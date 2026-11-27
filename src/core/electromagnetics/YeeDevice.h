#pragma once

// The Yee solver's state as plain arrays, and the CUDA functions that run
// it on a GPU (YeeDevice.cu, compiled only with RESONANT_HAVE_CUDA; the stub
// in YeeDeviceStub.cpp reports that CUDA is not in the build). This header
// includes nothing of the rest of the core, so nvcc never sees it.
//
// The device update is the same arithmetic as YeeSolver::updateH/updateE in
// double precision; results agree with the CPU reference to rounding (the
// GPU may contract a*b+c into one fused operation).
#include <cstddef>
#include <string>
#include <vector>

namespace resonant::em {

struct YeeDeviceLayout {
    int px = 1, py = 1, pz = 1;      // padded nodes per axis
    int periodic[3] = {0, 0, 0};
    int nx = 1, ny = 1, nz = 1;      // region cells
    int pad[3] = {0, 0, 0};          // absorbing nodes before the region
    int is2D = 0;
    double cell = 0.0, dt = 0.0;
    const double* e[3][3] = {};      // split E: [component][derivative axis], px*py*pz each
    const double* h[3][3] = {};
    const double* ca[3] = {};
    const double* cb[3] = {};
    const double* chb[3] = {};
    const double* decayNode[3] = {}; // per axis, px / py / pz values
    const double* decayHalf[3] = {};
    // Current sources: the E part they drive and the padded node.
    std::vector<int> sourceComponent, sourcePart;
    std::vector<size_t> sourceNode;
};

struct YeeCudaState;

// Uploads the state to `device`; null with the reason on failure.
YeeCudaState* yeeCudaCreate(const YeeDeviceLayout& layout, int device, std::string& error);
void yeeCudaDestroy(YeeCudaState* state);
// H, then E, then the sources (values: cb * scale * w(t) per source, in order).
bool yeeCudaStep(YeeCudaState* state, const double* sourceValues, std::string& error);
// E (sum of the split parts) at padded nodes, for probes.
bool yeeCudaGather(YeeCudaState* state, const int* components, const size_t* nodes, int count, double* out, std::string& error);
// Steady-state accumulation at the region nodes, as on the CPU.
bool yeeCudaAccumulateHarmonic(YeeCudaState* state, double kernelReal, double kernelImag, std::string& error);
// The accumulated harmonic E per component: real and imaginary parts, region nodes.
bool yeeCudaHarmonic(YeeCudaState* state, std::vector<double> (&real)[3], std::vector<double> (&imag)[3], std::string& error);
// One E component averaged to the region nodes (as YeeSolver::eAtNode).
bool yeeCudaNodeField(YeeCudaState* state, int component, std::vector<float>& out, std::string& error);
bool yeeCudaEnergy(YeeCudaState* state, double& energy, std::string& error);
std::string yeeCudaDeviceName(const YeeCudaState* state);

}  // namespace resonant::em
