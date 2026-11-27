// The Yee CUDA functions for builds without a CUDA toolkit: every call
// reports that CUDA is not part of the build. Builds with CUDA compile
// YeeDevice.cu instead (RESONANT_HAVE_CUDA).
#include "electromagnetics/YeeDevice.h"

#ifndef RESONANT_HAVE_CUDA
namespace resonant::em {
namespace {
const char* const kNoCuda = "this build has no CUDA support";
}
YeeCudaState* yeeCudaCreate(const YeeDeviceLayout&, int, std::string& error)
{
    error = kNoCuda;
    return nullptr;
}
void yeeCudaDestroy(YeeCudaState*) {}
bool yeeCudaStep(YeeCudaState*, const double*, std::string& error) { error = kNoCuda; return false; }
bool yeeCudaGather(YeeCudaState*, const int*, const size_t*, int, double*, std::string& error) { error = kNoCuda; return false; }
bool yeeCudaAccumulateHarmonic(YeeCudaState*, double, double, std::string& error) { error = kNoCuda; return false; }
bool yeeCudaHarmonic(YeeCudaState*, std::vector<double> (&)[3], std::vector<double> (&)[3], std::string& error) { error = kNoCuda; return false; }
bool yeeCudaNodeField(YeeCudaState*, int, std::vector<float>&, std::string& error) { error = kNoCuda; return false; }
bool yeeCudaEnergy(YeeCudaState*, double&, std::string& error) { error = kNoCuda; return false; }
std::string yeeCudaDeviceName(const YeeCudaState*) { return {}; }
}  // namespace resonant::em
#endif
