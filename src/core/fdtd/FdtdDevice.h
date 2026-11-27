#pragma once

// The acoustic FDTD solver's state as plain arrays, and the CUDA functions
// that run it on a GPU (FdtdDevice.cu, compiled only with RESONANT_HAVE_CUDA;
// FdtdDeviceStub.cpp otherwise). This header includes nothing of the rest of
// the core, so nvcc never sees it.
//
// The kernels repeat FdtdSolver::updateVelocity / updatePressure operation
// for operation in double precision without fused multiply-add, so the
// fields are the CPU reference's numbers.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace resonant::fdtd {

struct FdtdDeviceLayout {
    int nx = 1, ny = 1, nz = 1;        // cells including absorbing layers
    int is2D = 0;
    int padLo[3] = {0, 0, 0};
    int interior[3] = {1, 1, 1};       // region cells
    double dx = 0.0, dt = 0.0;
    const std::uint8_t* material = nullptr;
    const double* p = nullptr;
    const double* px = nullptr;
    const double* py = nullptr;
    const double* pz = nullptr;        // null in 2D
    const double* vx = nullptr;
    const double* vy = nullptr;
    const double* vz = nullptr;        // null in 2D
    std::vector<double> kappa, loss, cellInverseDensity, faceCoefficient;  // per material / material pair
    std::vector<double> bulkModulus;   // per material, 0 for a non-fluid
    std::vector<double> decayCell[3], decayFace[3];
    int boundary[6] = {0, 0, 0, 0, 0, 0};  // 0 rigid, 1 pressure release, 2 impedance, 3 absorbing
    double impedance[6] = {0, 0, 0, 0, 0, 0};
    std::vector<size_t> sourceCell;
};

struct FdtdCudaState;

FdtdCudaState* fdtdCudaCreate(const FdtdDeviceLayout& layout, int device, std::string& error);
void fdtdCudaDestroy(FdtdCudaState* state);
// Velocity, pressure, then the sources (increments: scale * integral, in order).
bool fdtdCudaStep(FdtdCudaState* state, const double* sourceIncrements, std::string& error);
// Pressure at padded cells, for probes.
bool fdtdCudaGather(FdtdCudaState* state, const size_t* cells, int count, double* out, std::string& error);
// Harmonic accumulation over the region's cells, as on the CPU.
bool fdtdCudaAccumulateHarmonic(FdtdCudaState* state, double kernelReal, double kernelImag, std::string& error);
bool fdtdCudaHarmonic(FdtdCudaState* state, std::vector<double>& real, std::vector<double>& imag, std::string& error);
bool fdtdCudaPressureField(FdtdCudaState* state, std::vector<float>& out, std::string& error);
bool fdtdCudaEnergy(FdtdCudaState* state, double& energy, std::string& error);
bool fdtdCudaMaxAbsPressure(FdtdCudaState* state, double& value, std::string& error);
std::string fdtdCudaDeviceName(const FdtdCudaState* state);

}  // namespace resonant::fdtd
