// CUDA runtime queries for builds without a CUDA toolkit. Builds with CUDA
// compile CudaRuntime.cu instead (RESONANT_HAVE_CUDA).
#include "compute/CudaRuntime.h"

#ifndef RESONANT_HAVE_CUDA
namespace resonant::compute::cuda {
bool built() { return false; }
std::string runtimeDescription() { return "this build has no CUDA support (no CUDA toolkit was found when it was configured)"; }
std::vector<CudaDeviceInfo> devices() { return {}; }
std::string architectures() { return {}; }
}  // namespace resonant::compute::cuda
#endif
