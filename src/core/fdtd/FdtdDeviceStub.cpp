// The acoustic FDTD CUDA functions for builds without a CUDA toolkit: every
// call reports that CUDA is not part of the build. Builds with CUDA compile
// FdtdDevice.cu instead (RESONANT_HAVE_CUDA).
#include "fdtd/FdtdDevice.h"

#ifndef RESONANT_HAVE_CUDA
namespace resonant::fdtd {
namespace {
const char* const kNoCuda = "this build has no CUDA support";
}
FdtdCudaState* fdtdCudaCreate(const FdtdDeviceLayout&, int, std::string& error)
{
    error = kNoCuda;
    return nullptr;
}
void fdtdCudaDestroy(FdtdCudaState*) {}
bool fdtdCudaStep(FdtdCudaState*, const double*, std::string& error) { error = kNoCuda; return false; }
bool fdtdCudaGather(FdtdCudaState*, const size_t*, int, double*, std::string& error) { error = kNoCuda; return false; }
bool fdtdCudaAccumulateHarmonic(FdtdCudaState*, double, double, std::string& error) { error = kNoCuda; return false; }
bool fdtdCudaHarmonic(FdtdCudaState*, std::vector<double>&, std::vector<double>&, std::string& error) { error = kNoCuda; return false; }
bool fdtdCudaPressureField(FdtdCudaState*, std::vector<float>&, std::string& error) { error = kNoCuda; return false; }
bool fdtdCudaEnergy(FdtdCudaState*, double&, std::string& error) { error = kNoCuda; return false; }
bool fdtdCudaMaxAbsPressure(FdtdCudaState*, double&, std::string& error) { error = kNoCuda; return false; }
std::string fdtdCudaDeviceName(const FdtdCudaState*) { return {}; }
}  // namespace resonant::fdtd
#endif
