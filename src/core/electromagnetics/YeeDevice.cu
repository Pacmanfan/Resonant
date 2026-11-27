// The Yee solver on a CUDA device (compiled only with RESONANT_HAVE_CUDA).
//
// The kernels repeat YeeSolver::updateH / updateE, eAtNode and energy
// operation for operation in double precision, and the build compiles them
// without fused multiply-add contraction (--fmad=false), so the fields are
// the same numbers the CPU reference computes. The energy is reduced as a
// partial sum per row, added in row order on the host, like the CPU.
#include "electromagnetics/YeeDevice.h"

#include <cuda_runtime.h>

#include <cstdio>

namespace resonant::em {

namespace {

struct Dims {
    int px, py, pz;
    int periodic[3];
    int nx, ny, nz;
    int pad[3];
    int is2D;
    double cell, dt;
    size_t nodes;        // padded
    size_t regionNodes;  // (nx+1)(ny+1)(nz+1 or 1)
};

struct Arrays {
    double* e[3][3];
    double* h[3][3];
    double* ca[3];
    double* cb[3];
    double* chb[3];
    double* decayNode[3];
    double* decayHalf[3];
};

__device__ inline size_t at(const Dims& d, int i, int j, int k)
{
    return static_cast<size_t>(i) + static_cast<size_t>(d.px) * (static_cast<size_t>(j) + static_cast<size_t>(d.py) * static_cast<size_t>(k));
}

__device__ inline int wrapIndex(const Dims& d, int axis, int index)
{
    const int n = axis == 0 ? d.px : axis == 1 ? d.py : d.pz;
    return d.periodic[axis] ? (index % n + n) % n : index;
}

__device__ inline double eSum(const Arrays& a, int c, size_t n)
{
    return a.e[c][0][n] + a.e[c][1][n] + a.e[c][2][n];
}

__global__ void updateHKernel(Dims d, Arrays a, int c)
{
    const size_t n = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    if (n >= d.nodes)
        return;
    const int i = static_cast<int>(n % static_cast<size_t>(d.px));
    const int j = static_cast<int>((n / static_cast<size_t>(d.px)) % static_cast<size_t>(d.py));
    const int k = static_cast<int>(n / (static_cast<size_t>(d.px) * static_cast<size_t>(d.py)));
    const int a1 = (c + 1) % 3, a2 = (c + 2) % 3;
    const int idx[3] = {i, j, k};
    const int limits[3] = {d.px, d.py, d.pz};
    if ((!d.periodic[a1] && idx[a1] >= limits[a1] - 1) || (!d.periodic[a2] && idx[a2] >= limits[a2] - 1))
        return;
    int up1[3] = {i, j, k}, up2[3] = {i, j, k};
    up1[a1] = wrapIndex(d, a1, up1[a1] + 1);
    up2[a2] = wrapIndex(d, a2, up2[a2] + 1);
    const size_t n1 = at(d, up1[0], up1[1], up1[2]), n2 = at(d, up2[0], up2[1], up2[2]);
    const double ea2Up = eSum(a, a2, n1), ea2 = eSum(a, a2, n);
    const double ea1Up = eSum(a, a1, n2), ea1 = eSum(a, a1, n);
    const double d1 = a.decayHalf[a1][idx[a1]];
    const double d2 = a.decayHalf[a2][idx[a2]];
    const double chb = a.chb[c][n];
    double* part1 = a.h[c][a1];
    double* part2 = a.h[c][a2];
    part1[n] = d1 * (d1 * part1[n] - chb * (ea2Up - ea2));
    part2[n] = d2 * (d2 * part2[n] + chb * (ea1Up - ea1));
}

__global__ void updateEKernel(Dims d, Arrays a, int c)
{
    const size_t n = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    if (n >= d.nodes)
        return;
    const double cb = a.cb[c][n];
    if (cb == 0.0)
        return;
    const int i = static_cast<int>(n % static_cast<size_t>(d.px));
    const int j = static_cast<int>((n / static_cast<size_t>(d.px)) % static_cast<size_t>(d.py));
    const int k = static_cast<int>(n / (static_cast<size_t>(d.px) * static_cast<size_t>(d.py)));
    const int a1 = (c + 1) % 3, a2 = (c + 2) % 3;
    const int idx[3] = {i, j, k};
    int dn1[3] = {i, j, k}, dn2[3] = {i, j, k};
    dn1[a1] = wrapIndex(d, a1, dn1[a1] - 1);
    dn2[a2] = wrapIndex(d, a2, dn2[a2] - 1);
    const size_t n1 = at(d, dn1[0], dn1[1], dn1[2]), n2 = at(d, dn2[0], dn2[1], dn2[2]);
    const double* const* hA2 = a.h[a2];
    const double* const* hA1 = a.h[a1];
    const double ha2 = hA2[0][n] + hA2[1][n] + hA2[2][n], ha2Down = hA2[0][n1] + hA2[1][n1] + hA2[2][n1];
    const double ha1 = hA1[0][n] + hA1[1][n] + hA1[2][n], ha1Down = hA1[0][n2] + hA1[1][n2] + hA1[2][n2];
    const double d1 = a.decayNode[a1][idx[a1]];
    const double d2 = a.decayNode[a2][idx[a2]];
    const double ca = a.ca[c][n];
    double* part1 = a.e[c][a1];
    double* part2 = a.e[c][a2];
    part1[n] = d1 * (d1 * ca * part1[n] + cb * (ha2 - ha2Down));
    part2[n] = d2 * (d2 * ca * part2[n] - cb * (ha1 - ha1Down));
}

// One thread, in order: two sources on one edge add exactly as on the CPU.
__global__ void sourcesKernel(Arrays a, const int* components, const int* parts, const size_t* nodes, const double* values, int count)
{
    if (blockIdx.x != 0 || threadIdx.x != 0)
        return;
    for (int s = 0; s < count; ++s)
        a.e[components[s]][parts[s]][nodes[s]] -= values[s];
}

__global__ void gatherKernel(Arrays a, const int* components, const size_t* nodes, int count, double* out)
{
    const int q = blockIdx.x * blockDim.x + threadIdx.x;
    if (q < count)
        out[q] = eSum(a, components[q], nodes[q]);
}

__device__ inline double eRegion(const Dims& d, const Arrays& a, int c, int i, int j, int k)
{
    return eSum(a, c, at(d, wrapIndex(d, 0, i + d.pad[0]), wrapIndex(d, 1, j + d.pad[1]), d.is2D ? 0 : wrapIndex(d, 2, k + d.pad[2])));
}

__device__ double eAtNode(const Dims& d, const Arrays& a, int c, int i, int j, int k)
{
    if (c == 2 && d.is2D)
        return eRegion(d, a, 2, i, j, 0);
    int lo[3] = {i, j, k};
    lo[c] -= 1;
    if (d.periodic[c])
        return 0.5 * (eRegion(d, a, c, i, j, k) + eRegion(d, a, c, lo[0], lo[1], lo[2]));
    const int limit = c == 0 ? d.nx : c == 1 ? d.ny : d.nz;
    const int along = c == 0 ? i : c == 1 ? j : k;
    if (along >= limit)
        return eRegion(d, a, c, lo[0], lo[1], lo[2]);
    const double here = eRegion(d, a, c, i, j, k);
    return along > 0 ? 0.5 * (here + eRegion(d, a, c, lo[0], lo[1], lo[2])) : here;
}

__device__ inline void regionNode(const Dims& d, size_t m, int& i, int& j, int& k)
{
    i = static_cast<int>(m % static_cast<size_t>(d.nx + 1));
    j = static_cast<int>((m / static_cast<size_t>(d.nx + 1)) % static_cast<size_t>(d.ny + 1));
    k = static_cast<int>(m / (static_cast<size_t>(d.nx + 1) * static_cast<size_t>(d.ny + 1)));
}

__global__ void harmonicKernel(Dims d, Arrays a, double kr, double ki, double* re0, double* re1, double* re2, double* im0, double* im1, double* im2)
{
    const size_t m = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    if (m >= d.regionNodes)
        return;
    int i, j, k;
    regionNode(d, m, i, j, k);
    double* re[3] = {re0, re1, re2};
    double* im[3] = {im0, im1, im2};
    for (int c = 0; c < 3; ++c) {
        const double v = eAtNode(d, a, c, i, j, k);
        re[c][m] += v * kr;
        im[c][m] += v * ki;
    }
}

__global__ void nodeFieldKernel(Dims d, Arrays a, int component, float* out)
{
    const size_t m = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    if (m >= d.regionNodes)
        return;
    int i, j, k;
    regionNode(d, m, i, j, k);
    out[m] = static_cast<float>(eAtNode(d, a, component, i, j, k));
}

// A partial energy sum per row (j, k), over the row's nodes in order.
__global__ void energyKernel(Dims d, Arrays a, double* partial)
{
    const size_t row = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    const size_t rows = static_cast<size_t>(d.py) * static_cast<size_t>(d.pz);
    if (row >= rows)
        return;
    double sum = 0.0;
    const size_t first = row * static_cast<size_t>(d.px);
    for (size_t n = first; n < first + static_cast<size_t>(d.px); ++n)
        for (int c = 0; c < 3; ++c) {
            const double cb = a.cb[c][n];
            if (cb > 0.0) {
                const double eps = d.dt / (cb * d.cell);
                const double value = eSum(a, c, n);
                sum += 0.5 * eps * value * value;
            }
            const double chb = a.chb[c][n];
            if (chb > 0.0) {
                const double mu = d.dt / (chb * d.cell);
                const double value = a.h[c][0][n] + a.h[c][1][n] + a.h[c][2][n];
                sum += 0.5 * mu * value * value;
            }
        }
    partial[row] = sum;
}

unsigned blocks(size_t count, unsigned threads = 256)
{
    return static_cast<unsigned>((count + threads - 1) / threads);
}

}  // namespace

struct YeeCudaState {
    Dims dims{};
    Arrays arrays{};
    std::vector<void*> allocations;
    int sourceCount = 0;
    int* sourceComponents = nullptr;
    int* sourceParts = nullptr;
    size_t* sourceNodes = nullptr;
    double* sourceValues = nullptr;
    double* harmonic[6] = {};  // re0..2, im0..2
    double* partial = nullptr;
    float* nodeField = nullptr;
    int* probeComponents = nullptr;
    size_t* probeNodes = nullptr;
    double* probeValues = nullptr;
    int probeCapacity = 0;
    std::string deviceName;
};

namespace {

bool ok(cudaError_t e, const char* what, std::string& error)
{
    if (e == cudaSuccess)
        return true;
    error = std::string(what) + ": " + cudaGetErrorString(e);
    cudaGetLastError();
    return false;
}

template <typename T>
bool allocate(YeeCudaState* s, T*& pointer, size_t count, std::string& error)
{
    void* p = nullptr;
    if (!ok(cudaMalloc(&p, count * sizeof(T) + 1), "cudaMalloc", error))
        return false;
    s->allocations.push_back(p);
    pointer = static_cast<T*>(p);
    return true;
}

template <typename T>
bool upload(YeeCudaState* s, T*& pointer, const T* host, size_t count, std::string& error)
{
    return allocate(s, pointer, count, error) && ok(cudaMemcpy(pointer, host, count * sizeof(T), cudaMemcpyHostToDevice), "cudaMemcpy", error);
}

bool launched(std::string& error)
{
    return ok(cudaGetLastError(), "kernel launch", error);
}

}  // namespace

YeeCudaState* yeeCudaCreate(const YeeDeviceLayout& l, int device, std::string& error)
{
    if (!ok(cudaSetDevice(device), "cudaSetDevice", error))
        return nullptr;
    auto* s = new YeeCudaState;
    cudaDeviceProp p{};
    if (cudaGetDeviceProperties(&p, device) == cudaSuccess)
        s->deviceName = p.name;
    Dims& d = s->dims;
    d.px = l.px;
    d.py = l.py;
    d.pz = l.pz;
    for (int a = 0; a < 3; ++a) {
        d.periodic[a] = l.periodic[a];
        d.pad[a] = l.pad[a];
    }
    d.nx = l.nx;
    d.ny = l.ny;
    d.nz = l.nz;
    d.is2D = l.is2D;
    d.cell = l.cell;
    d.dt = l.dt;
    d.nodes = static_cast<size_t>(l.px) * static_cast<size_t>(l.py) * static_cast<size_t>(l.pz);
    d.regionNodes = static_cast<size_t>(l.nx + 1) * static_cast<size_t>(l.ny + 1) * static_cast<size_t>(l.is2D ? 1 : l.nz + 1);
    const size_t lengths[3] = {static_cast<size_t>(l.px), static_cast<size_t>(l.py), static_cast<size_t>(l.pz)};
    bool good = true;
    for (int c = 0; c < 3 && good; ++c) {
        for (int q = 0; q < 3 && good; ++q)
            good = upload(s, s->arrays.e[c][q], l.e[c][q], d.nodes, error) && upload(s, s->arrays.h[c][q], l.h[c][q], d.nodes, error);
        good = good && upload(s, s->arrays.ca[c], l.ca[c], d.nodes, error) && upload(s, s->arrays.cb[c], l.cb[c], d.nodes, error)
               && upload(s, s->arrays.chb[c], l.chb[c], d.nodes, error)
               && upload(s, s->arrays.decayNode[c], l.decayNode[c], lengths[c], error)
               && upload(s, s->arrays.decayHalf[c], l.decayHalf[c], lengths[c], error);
    }
    s->sourceCount = static_cast<int>(l.sourceNode.size());
    if (good && s->sourceCount > 0)
        good = upload(s, s->sourceComponents, l.sourceComponent.data(), l.sourceComponent.size(), error)
               && upload(s, s->sourceParts, l.sourcePart.data(), l.sourcePart.size(), error)
               && upload(s, s->sourceNodes, l.sourceNode.data(), l.sourceNode.size(), error)
               && allocate(s, s->sourceValues, l.sourceNode.size(), error);
    if (good)
        good = allocate(s, s->partial, static_cast<size_t>(l.py) * static_cast<size_t>(l.pz), error);
    if (!good) {
        yeeCudaDestroy(s);
        return nullptr;
    }
    return s;
}

void yeeCudaDestroy(YeeCudaState* s)
{
    if (!s)
        return;
    for (void* p : s->allocations)
        cudaFree(p);
    delete s;
}

bool yeeCudaStep(YeeCudaState* s, const double* values, std::string& error)
{
    const unsigned b = blocks(s->dims.nodes);
    for (int c = 0; c < 3; ++c)
        updateHKernel<<<b, 256>>>(s->dims, s->arrays, c);
    for (int c = 0; c < 3; ++c)
        updateEKernel<<<b, 256>>>(s->dims, s->arrays, c);
    if (s->sourceCount > 0) {
        if (!ok(cudaMemcpy(s->sourceValues, values, static_cast<size_t>(s->sourceCount) * sizeof(double), cudaMemcpyHostToDevice), "cudaMemcpy",
                error))
            return false;
        sourcesKernel<<<1, 1>>>(s->arrays, s->sourceComponents, s->sourceParts, s->sourceNodes, s->sourceValues, s->sourceCount);
    }
    return launched(error);
}

bool yeeCudaGather(YeeCudaState* s, const int* components, const size_t* nodes, int count, double* out, std::string& error)
{
    if (count > s->probeCapacity) {
        if (!allocate(s, s->probeComponents, static_cast<size_t>(count), error) || !allocate(s, s->probeNodes, static_cast<size_t>(count), error)
            || !allocate(s, s->probeValues, static_cast<size_t>(count), error))
            return false;
        s->probeCapacity = count;
    }
    if (!ok(cudaMemcpy(s->probeComponents, components, static_cast<size_t>(count) * sizeof(int), cudaMemcpyHostToDevice), "cudaMemcpy", error)
        || !ok(cudaMemcpy(s->probeNodes, nodes, static_cast<size_t>(count) * sizeof(size_t), cudaMemcpyHostToDevice), "cudaMemcpy", error))
        return false;
    gatherKernel<<<blocks(static_cast<size_t>(count), 64), 64>>>(s->arrays, s->probeComponents, s->probeNodes, count, s->probeValues);
    return launched(error) && ok(cudaMemcpy(out, s->probeValues, static_cast<size_t>(count) * sizeof(double), cudaMemcpyDeviceToHost), "cudaMemcpy", error);
}

bool yeeCudaAccumulateHarmonic(YeeCudaState* s, double kr, double ki, std::string& error)
{
    if (!s->harmonic[0]) {
        for (double*& h : s->harmonic)
            if (!allocate(s, h, s->dims.regionNodes, error) || !ok(cudaMemset(h, 0, s->dims.regionNodes * sizeof(double)), "cudaMemset", error))
                return false;
    }
    harmonicKernel<<<blocks(s->dims.regionNodes), 256>>>(s->dims, s->arrays, kr, ki, s->harmonic[0], s->harmonic[1], s->harmonic[2], s->harmonic[3],
                                                          s->harmonic[4], s->harmonic[5]);
    return launched(error);
}

bool yeeCudaHarmonic(YeeCudaState* s, std::vector<double> (&real)[3], std::vector<double> (&imag)[3], std::string& error)
{
    for (int c = 0; c < 3; ++c) {
        real[c].assign(s->dims.regionNodes, 0.0);
        imag[c].assign(s->dims.regionNodes, 0.0);
        if (!s->harmonic[0])
            continue;
        if (!ok(cudaMemcpy(real[c].data(), s->harmonic[c], s->dims.regionNodes * sizeof(double), cudaMemcpyDeviceToHost), "cudaMemcpy", error)
            || !ok(cudaMemcpy(imag[c].data(), s->harmonic[3 + c], s->dims.regionNodes * sizeof(double), cudaMemcpyDeviceToHost), "cudaMemcpy", error))
            return false;
    }
    return true;
}

bool yeeCudaNodeField(YeeCudaState* s, int component, std::vector<float>& out, std::string& error)
{
    if (!s->nodeField && !allocate(s, s->nodeField, s->dims.regionNodes, error))
        return false;
    nodeFieldKernel<<<blocks(s->dims.regionNodes), 256>>>(s->dims, s->arrays, component, s->nodeField);
    out.resize(s->dims.regionNodes);
    return launched(error) && ok(cudaMemcpy(out.data(), s->nodeField, s->dims.regionNodes * sizeof(float), cudaMemcpyDeviceToHost), "cudaMemcpy", error);
}

bool yeeCudaEnergy(YeeCudaState* s, double& energy, std::string& error)
{
    const size_t rows = static_cast<size_t>(s->dims.py) * static_cast<size_t>(s->dims.pz);
    energyKernel<<<blocks(rows, 128), 128>>>(s->dims, s->arrays, s->partial);
    std::vector<double> partial(rows);
    if (!launched(error) || !ok(cudaMemcpy(partial.data(), s->partial, rows * sizeof(double), cudaMemcpyDeviceToHost), "cudaMemcpy", error))
        return false;
    double total = 0.0;
    for (const double p : partial)
        total += p;
    const double volume = s->dims.cell * s->dims.cell * (s->dims.is2D ? 1.0 : s->dims.cell);
    energy = total * volume;
    return true;
}

std::string yeeCudaDeviceName(const YeeCudaState* s)
{
    return s ? s->deviceName : std::string();
}

}  // namespace resonant::em
