// The acoustic FDTD solver on a CUDA device (compiled only with
// RESONANT_HAVE_CUDA).
//
// The kernels repeat FdtdSolver::updateVelocity (with its boundary faces),
// updatePressure, the source increments and the energy operation for
// operation in double precision; the build compiles them without fused
// multiply-add (--fmad=false), so the fields are the CPU reference's
// numbers. Reductions (energy, peak pressure) use a partial per row, combined
// on the host in row order, as the CPU does.
#include "fdtd/FdtdDevice.h"

#include <cuda_runtime.h>

#include <algorithm>
#include <cmath>

namespace resonant::fdtd {

namespace {

constexpr int kPressureRelease = 1, kImpedance = 2;  // 0 rigid and 3 absorbing both hold the face still

struct Dims {
    int nx, ny, nz, is2D;
    int padLo[3], interior[3];
    double dx, dt;
    int materials;
    int boundary[6];
    double impedance[6];
    size_t cells, interiorCells;
};

struct Arrays {
    unsigned char* material;
    double *p, *px, *py, *pz, *vx, *vy, *vz;
    double *kappa, *loss, *cellInverseDensity, *faceCoefficient, *bulkModulus;
    double *decayCell[3], *decayFace[3];
};

__device__ inline size_t index(const Dims& d, int i, int j, int k)
{
    return static_cast<size_t>(i) + static_cast<size_t>(d.nx) * (static_cast<size_t>(j) + static_cast<size_t>(d.ny) * static_cast<size_t>(k));
}

// `sign` is +1 on a maximum face (outward normal along +axis).
__device__ inline void boundaryFace(const Dims& d, const Arrays& a, double& v, int face, size_t cell, double sign)
{
    const double b = a.cellInverseDensity[a.material[cell]];
    const int kind = d.boundary[face];
    if (kind == kPressureRelease) {
        v += sign * 2.0 * b * a.p[cell];
    } else if (kind == kImpedance) {
        const double beta = b * d.impedance[face];
        const double outward = sign * v;
        v = sign * ((1.0 - beta) * outward + 2.0 * b * a.p[cell]) / (1.0 + beta);
    } else {  // rigid, absorbing (backed by a rigid wall)
        v = 0.0;
    }
}

__global__ void velocityXKernel(Dims d, Arrays a)
{
    const size_t f = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    const size_t faces = static_cast<size_t>(d.nx + 1) * static_cast<size_t>(d.ny) * static_cast<size_t>(d.nz);
    if (f >= faces)
        return;
    const int i = static_cast<int>(f % static_cast<size_t>(d.nx + 1));
    const size_t row = f / static_cast<size_t>(d.nx + 1);
    const int j = static_cast<int>(row % static_cast<size_t>(d.ny)), k = static_cast<int>(row / static_cast<size_t>(d.ny));
    const size_t rowP = index(d, 0, j, k);
    double& v = a.vx[f];
    if (i == 0) {
        boundaryFace(d, a, v, 0, rowP, -1.0);
    } else if (i == d.nx) {
        boundaryFace(d, a, v, 1, rowP + static_cast<size_t>(d.nx - 1), 1.0);
    } else {
        const size_t c = rowP + static_cast<size_t>(i);
        const double b = a.faceCoefficient[a.material[c - 1] * d.materials + a.material[c]];
        const double s = a.decayFace[0][i];
        v = s * (s * v - b * (a.p[c] - a.p[c - 1]));
    }
}

__global__ void velocityYKernel(Dims d, Arrays a)
{
    const size_t f = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    const size_t faces = static_cast<size_t>(d.nx) * static_cast<size_t>(d.ny + 1) * static_cast<size_t>(d.nz);
    if (f >= faces)
        return;
    const int i = static_cast<int>(f % static_cast<size_t>(d.nx));
    const int j = static_cast<int>((f / static_cast<size_t>(d.nx)) % static_cast<size_t>(d.ny + 1));
    const int k = static_cast<int>(f / (static_cast<size_t>(d.nx) * static_cast<size_t>(d.ny + 1)));
    double& v = a.vy[f];
    if (j == 0) {
        boundaryFace(d, a, v, 2, index(d, i, 0, k), -1.0);
    } else if (j == d.ny) {
        boundaryFace(d, a, v, 3, index(d, i, d.ny - 1, k), 1.0);
    } else {
        const size_t c = index(d, i, j, k);
        const double b = a.faceCoefficient[a.material[c - static_cast<size_t>(d.nx)] * d.materials + a.material[c]];
        const double s = a.decayFace[1][j];
        v = s * (s * v - b * (a.p[c] - a.p[c - static_cast<size_t>(d.nx)]));
    }
}

__global__ void velocityZKernel(Dims d, Arrays a)
{
    const size_t f = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    const size_t strideZ = static_cast<size_t>(d.nx) * static_cast<size_t>(d.ny);
    if (f >= strideZ * static_cast<size_t>(d.nz + 1))
        return;
    const int k = static_cast<int>(f / strideZ);
    double& v = a.vz[f];
    if (k == 0) {
        boundaryFace(d, a, v, 4, f, -1.0);
    } else if (k == d.nz) {
        boundaryFace(d, a, v, 5, f - strideZ, 1.0);
    } else {
        const size_t c = f;
        const double b = a.faceCoefficient[a.material[c - strideZ] * d.materials + a.material[c]];
        const double s = a.decayFace[2][k];
        v = s * (s * v - b * (a.p[c] - a.p[c - strideZ]));
    }
}

__global__ void pressureKernel(Dims d, Arrays a)
{
    const size_t c = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    if (c >= d.cells)
        return;
    const unsigned char m = a.material[c];
    const double kappa = a.kappa[m];
    if (kappa == 0.0)
        return;
    const int i = static_cast<int>(c % static_cast<size_t>(d.nx));
    const int j = static_cast<int>((c / static_cast<size_t>(d.nx)) % static_cast<size_t>(d.ny));
    const int k = static_cast<int>(c / (static_cast<size_t>(d.nx) * static_cast<size_t>(d.ny)));
    const double loss = a.loss[m];
    const double dxf = a.decayCell[0][i], dy = a.decayCell[1][j], dz = a.decayCell[2][k];
    const size_t ix = static_cast<size_t>(d.nx + 1) * (static_cast<size_t>(j) + static_cast<size_t>(d.ny) * static_cast<size_t>(k)) + static_cast<size_t>(i);
    const size_t iy = static_cast<size_t>(d.nx) * (static_cast<size_t>(j) + static_cast<size_t>(d.ny + 1) * static_cast<size_t>(k)) + static_cast<size_t>(i);
    a.px[c] = loss * dxf * (dxf * a.px[c] - kappa * (a.vx[ix + 1] - a.vx[ix]));
    a.py[c] = loss * dy * (dy * a.py[c] - kappa * (a.vy[iy + static_cast<size_t>(d.nx)] - a.vy[iy]));
    double sum = a.px[c] + a.py[c];
    if (!d.is2D) {
        const size_t strideZ = static_cast<size_t>(d.nx) * static_cast<size_t>(d.ny);
        a.pz[c] = loss * dz * (dz * a.pz[c] - kappa * (a.vz[c + strideZ] - a.vz[c]));
        sum += a.pz[c];
    }
    a.p[c] = sum;
}

// One thread, in order: two sources in one cell add exactly as on the CPU.
__global__ void sourcesKernel(Arrays a, const size_t* cells, const double* increments, int count)
{
    if (blockIdx.x != 0 || threadIdx.x != 0)
        return;
    for (int s = 0; s < count; ++s) {
        a.px[cells[s]] += increments[s];
        a.p[cells[s]] += increments[s];
    }
}

__global__ void gatherKernel(Arrays a, const size_t* cells, int count, double* out)
{
    const int q = blockIdx.x * blockDim.x + threadIdx.x;
    if (q < count)
        out[q] = a.p[cells[q]];
}

__device__ inline size_t paddedOfInterior(const Dims& d, size_t m)
{
    const int i = static_cast<int>(m % static_cast<size_t>(d.interior[0]));
    const int j = static_cast<int>((m / static_cast<size_t>(d.interior[0])) % static_cast<size_t>(d.interior[1]));
    const int k = static_cast<int>(m / (static_cast<size_t>(d.interior[0]) * static_cast<size_t>(d.interior[1])));
    return index(d, i + d.padLo[0], j + d.padLo[1], k + d.padLo[2]);
}

__global__ void harmonicKernel(Dims d, Arrays a, double kr, double ki, double* re, double* im)
{
    const size_t m = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    if (m >= d.interiorCells)
        return;
    const double p = a.p[paddedOfInterior(d, m)];
    re[m] += p * kr;
    im[m] += p * ki;
}

__global__ void pressureFieldKernel(Dims d, Arrays a, float* out)
{
    const size_t m = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    if (m < d.interiorCells)
        out[m] = static_cast<float>(a.p[paddedOfInterior(d, m)]);
}

__device__ inline double faceEnergy(const Dims& d, const Arrays& a, double v, size_t lower, size_t upper)
{
    const double coefficient = a.faceCoefficient[a.material[lower] * d.materials + a.material[upper]];
    if (coefficient <= 0.0)
        return 0.0;
    const double density = d.dt / (d.dx * coefficient);
    return density * v * (v - coefficient * (a.p[upper] - a.p[lower]));
}

__global__ void energyKernel(Dims d, Arrays a, double* partial)
{
    const size_t row = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    if (row >= static_cast<size_t>(d.ny) * static_cast<size_t>(d.nz))
        return;
    const int j = static_cast<int>(row % static_cast<size_t>(d.ny)), k = static_cast<int>(row / static_cast<size_t>(d.ny));
    const size_t strideZ = static_cast<size_t>(d.nx) * static_cast<size_t>(d.ny);
    double potential = 0.0, kinetic = 0.0;
    for (int i = 0; i < d.nx; ++i) {
        const size_t c = index(d, i, j, k);
        const double bulk = a.bulkModulus[a.material[c]];
        if (bulk > 0.0)
            potential += a.p[c] * a.p[c] / bulk;
        if (i >= 1)
            kinetic += faceEnergy(d, a, a.vx[static_cast<size_t>(i) + static_cast<size_t>(d.nx + 1) * (static_cast<size_t>(j) + static_cast<size_t>(d.ny) * static_cast<size_t>(k))],
                                  c - 1, c);
        if (j >= 1)
            kinetic += faceEnergy(d, a, a.vy[static_cast<size_t>(i) + static_cast<size_t>(d.nx) * (static_cast<size_t>(j) + static_cast<size_t>(d.ny + 1) * static_cast<size_t>(k))],
                                  c - static_cast<size_t>(d.nx), c);
        if (!d.is2D && k >= 1)
            kinetic += faceEnergy(d, a, a.vz[c], c - strideZ, c);
    }
    partial[row] = potential + kinetic;
}

__global__ void maxKernel(Dims d, Arrays a, double* partial)
{
    const size_t row = blockIdx.x * static_cast<size_t>(blockDim.x) + threadIdx.x;
    const size_t rows = static_cast<size_t>(d.interior[1]) * static_cast<size_t>(d.interior[2]);
    if (row >= rows)
        return;
    double largest = 0.0;
    const size_t first = row * static_cast<size_t>(d.interior[0]);
    for (size_t m = first; m < first + static_cast<size_t>(d.interior[0]); ++m)
        largest = fmax(largest, fabs(a.p[paddedOfInterior(d, m)]));
    partial[row] = largest;
}

unsigned blocks(size_t count, unsigned threads = 256)
{
    return static_cast<unsigned>((count + threads - 1) / threads);
}

}  // namespace

struct FdtdCudaState {
    Dims dims{};
    Arrays arrays{};
    std::vector<void*> allocations;
    int sourceCount = 0;
    size_t* sourceCells = nullptr;
    double* sourceIncrements = nullptr;
    double* harmonicRe = nullptr;
    double* harmonicIm = nullptr;
    double* partial = nullptr;
    float* field = nullptr;
    size_t* probeCells = nullptr;
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
bool allocate(FdtdCudaState* s, T*& pointer, size_t count, std::string& error)
{
    void* p = nullptr;
    if (!ok(cudaMalloc(&p, count * sizeof(T) + 1), "cudaMalloc", error))
        return false;
    s->allocations.push_back(p);
    pointer = static_cast<T*>(p);
    return true;
}

template <typename T>
bool upload(FdtdCudaState* s, T*& pointer, const T* host, size_t count, std::string& error)
{
    return allocate(s, pointer, count, error) && ok(cudaMemcpy(pointer, host, count * sizeof(T), cudaMemcpyHostToDevice), "cudaMemcpy", error);
}

bool launched(std::string& error)
{
    return ok(cudaGetLastError(), "kernel launch", error);
}

}  // namespace

FdtdCudaState* fdtdCudaCreate(const FdtdDeviceLayout& l, int device, std::string& error)
{
    if (!ok(cudaSetDevice(device), "cudaSetDevice", error))
        return nullptr;
    auto* s = new FdtdCudaState;
    cudaDeviceProp prop{};
    if (cudaGetDeviceProperties(&prop, device) == cudaSuccess)
        s->deviceName = prop.name;
    Dims& d = s->dims;
    d.nx = l.nx;
    d.ny = l.ny;
    d.nz = l.nz;
    d.is2D = l.is2D;
    for (int a = 0; a < 3; ++a) {
        d.padLo[a] = l.padLo[a];
        d.interior[a] = l.interior[a];
    }
    d.dx = l.dx;
    d.dt = l.dt;
    d.materials = static_cast<int>(l.kappa.size());
    for (int f = 0; f < 6; ++f) {
        d.boundary[f] = l.boundary[f];
        d.impedance[f] = l.impedance[f];
    }
    d.cells = static_cast<size_t>(l.nx) * static_cast<size_t>(l.ny) * static_cast<size_t>(l.nz);
    d.interiorCells = static_cast<size_t>(l.interior[0]) * static_cast<size_t>(l.interior[1]) * static_cast<size_t>(l.interior[2]);
    const size_t vxCount = static_cast<size_t>(l.nx + 1) * static_cast<size_t>(l.ny) * static_cast<size_t>(l.nz);
    const size_t vyCount = static_cast<size_t>(l.nx) * static_cast<size_t>(l.ny + 1) * static_cast<size_t>(l.nz);
    const size_t vzCount = static_cast<size_t>(l.nx) * static_cast<size_t>(l.ny) * static_cast<size_t>(l.nz + 1);
    Arrays& a = s->arrays;
    bool good = upload(s, a.material, l.material, d.cells, error) && upload(s, a.p, l.p, d.cells, error) && upload(s, a.px, l.px, d.cells, error)
                && upload(s, a.py, l.py, d.cells, error) && upload(s, a.vx, l.vx, vxCount, error) && upload(s, a.vy, l.vy, vyCount, error)
                && upload(s, a.kappa, l.kappa.data(), l.kappa.size(), error) && upload(s, a.loss, l.loss.data(), l.loss.size(), error)
                && upload(s, a.cellInverseDensity, l.cellInverseDensity.data(), l.cellInverseDensity.size(), error)
                && upload(s, a.faceCoefficient, l.faceCoefficient.data(), l.faceCoefficient.size(), error)
                && upload(s, a.bulkModulus, l.bulkModulus.data(), l.bulkModulus.size(), error);
    if (good && !l.is2D)
        good = upload(s, a.pz, l.pz, d.cells, error) && upload(s, a.vz, l.vz, vzCount, error);
    for (int axis = 0; axis < 3 && good; ++axis)
        good = upload(s, a.decayCell[axis], l.decayCell[axis].data(), l.decayCell[axis].size(), error)
               && upload(s, a.decayFace[axis], l.decayFace[axis].data(), l.decayFace[axis].size(), error);
    s->sourceCount = static_cast<int>(l.sourceCell.size());
    if (good && s->sourceCount > 0)
        good = upload(s, s->sourceCells, l.sourceCell.data(), l.sourceCell.size(), error)
               && allocate(s, s->sourceIncrements, l.sourceCell.size(), error);
    if (good)
        good = allocate(s, s->partial, std::max(static_cast<size_t>(l.ny) * static_cast<size_t>(l.nz),
                                                static_cast<size_t>(l.interior[1]) * static_cast<size_t>(l.interior[2])),
                        error);
    if (!good) {
        fdtdCudaDestroy(s);
        return nullptr;
    }
    return s;
}

void fdtdCudaDestroy(FdtdCudaState* s)
{
    if (!s)
        return;
    for (void* p : s->allocations)
        cudaFree(p);
    delete s;
}

bool fdtdCudaStep(FdtdCudaState* s, const double* increments, std::string& error)
{
    const Dims& d = s->dims;
    velocityXKernel<<<blocks(static_cast<size_t>(d.nx + 1) * static_cast<size_t>(d.ny) * static_cast<size_t>(d.nz)), 256>>>(d, s->arrays);
    velocityYKernel<<<blocks(static_cast<size_t>(d.nx) * static_cast<size_t>(d.ny + 1) * static_cast<size_t>(d.nz)), 256>>>(d, s->arrays);
    if (!d.is2D)
        velocityZKernel<<<blocks(static_cast<size_t>(d.nx) * static_cast<size_t>(d.ny) * static_cast<size_t>(d.nz + 1)), 256>>>(d, s->arrays);
    pressureKernel<<<blocks(d.cells), 256>>>(d, s->arrays);
    if (s->sourceCount > 0) {
        if (!ok(cudaMemcpy(s->sourceIncrements, increments, static_cast<size_t>(s->sourceCount) * sizeof(double), cudaMemcpyHostToDevice),
                "cudaMemcpy", error))
            return false;
        sourcesKernel<<<1, 1>>>(s->arrays, s->sourceCells, s->sourceIncrements, s->sourceCount);
    }
    return launched(error);
}

bool fdtdCudaGather(FdtdCudaState* s, const size_t* cells, int count, double* out, std::string& error)
{
    if (count > s->probeCapacity) {
        if (!allocate(s, s->probeCells, static_cast<size_t>(count), error) || !allocate(s, s->probeValues, static_cast<size_t>(count), error))
            return false;
        s->probeCapacity = count;
    }
    if (!ok(cudaMemcpy(s->probeCells, cells, static_cast<size_t>(count) * sizeof(size_t), cudaMemcpyHostToDevice), "cudaMemcpy", error))
        return false;
    gatherKernel<<<blocks(static_cast<size_t>(count), 64), 64>>>(s->arrays, s->probeCells, count, s->probeValues);
    return launched(error) && ok(cudaMemcpy(out, s->probeValues, static_cast<size_t>(count) * sizeof(double), cudaMemcpyDeviceToHost), "cudaMemcpy", error);
}

bool fdtdCudaAccumulateHarmonic(FdtdCudaState* s, double kr, double ki, std::string& error)
{
    if (!s->harmonicRe) {
        if (!allocate(s, s->harmonicRe, s->dims.interiorCells, error) || !allocate(s, s->harmonicIm, s->dims.interiorCells, error)
            || !ok(cudaMemset(s->harmonicRe, 0, s->dims.interiorCells * sizeof(double)), "cudaMemset", error)
            || !ok(cudaMemset(s->harmonicIm, 0, s->dims.interiorCells * sizeof(double)), "cudaMemset", error))
            return false;
    }
    harmonicKernel<<<blocks(s->dims.interiorCells), 256>>>(s->dims, s->arrays, kr, ki, s->harmonicRe, s->harmonicIm);
    return launched(error);
}

bool fdtdCudaHarmonic(FdtdCudaState* s, std::vector<double>& real, std::vector<double>& imag, std::string& error)
{
    real.assign(s->dims.interiorCells, 0.0);
    imag.assign(s->dims.interiorCells, 0.0);
    if (!s->harmonicRe)
        return true;
    return ok(cudaMemcpy(real.data(), s->harmonicRe, s->dims.interiorCells * sizeof(double), cudaMemcpyDeviceToHost), "cudaMemcpy", error)
           && ok(cudaMemcpy(imag.data(), s->harmonicIm, s->dims.interiorCells * sizeof(double), cudaMemcpyDeviceToHost), "cudaMemcpy", error);
}

bool fdtdCudaPressureField(FdtdCudaState* s, std::vector<float>& out, std::string& error)
{
    if (!s->field && !allocate(s, s->field, s->dims.interiorCells, error))
        return false;
    pressureFieldKernel<<<blocks(s->dims.interiorCells), 256>>>(s->dims, s->arrays, s->field);
    out.resize(s->dims.interiorCells);
    return launched(error) && ok(cudaMemcpy(out.data(), s->field, s->dims.interiorCells * sizeof(float), cudaMemcpyDeviceToHost), "cudaMemcpy", error);
}

bool fdtdCudaEnergy(FdtdCudaState* s, double& energy, std::string& error)
{
    const size_t rows = static_cast<size_t>(s->dims.ny) * static_cast<size_t>(s->dims.nz);
    energyKernel<<<blocks(rows, 128), 128>>>(s->dims, s->arrays, s->partial);
    std::vector<double> partial(rows);
    if (!launched(error) || !ok(cudaMemcpy(partial.data(), s->partial, rows * sizeof(double), cudaMemcpyDeviceToHost), "cudaMemcpy", error))
        return false;
    double total = 0.0;
    for (const double p : partial)
        total += p;
    const double cellVolume = s->dims.is2D ? s->dims.dx * s->dims.dx : s->dims.dx * s->dims.dx * s->dims.dx;
    energy = 0.5 * cellVolume * total;
    return true;
}

bool fdtdCudaMaxAbsPressure(FdtdCudaState* s, double& value, std::string& error)
{
    const size_t rows = static_cast<size_t>(s->dims.interior[1]) * static_cast<size_t>(s->dims.interior[2]);
    maxKernel<<<blocks(rows, 128), 128>>>(s->dims, s->arrays, s->partial);
    std::vector<double> partial(rows);
    if (!launched(error) || !ok(cudaMemcpy(partial.data(), s->partial, rows * sizeof(double), cudaMemcpyDeviceToHost), "cudaMemcpy", error))
        return false;
    value = 0.0;
    for (const double p : partial)
        value = std::max(value, p);
    return true;
}

std::string fdtdCudaDeviceName(const FdtdCudaState* s)
{
    return s ? s->deviceName : std::string();
}

}  // namespace resonant::fdtd
