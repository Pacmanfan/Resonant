#pragma once

// The CUDA runtime as seen from the rest of the core: compiled only when the
// build found a CUDA toolkit (RESONANT_HAVE_CUDA); otherwise these report
// that CUDA is not part of the build. No CUDA header is included here.
#include <cstddef>
#include <string>
#include <vector>

namespace resonant::compute {

struct CudaDeviceInfo {
    int index = 0;
    std::string name;
    int major = 0, minor = 0;  // compute capability
    size_t totalBytes = 0, freeBytes = 0;
    bool usable = false;       // a context was created and its memory queried
    std::string problem;       // why not, when not usable
};

namespace cuda {
bool built();
std::string runtimeDescription();             // runtime and driver versions, or why there is none
std::vector<CudaDeviceInfo> devices();        // every visible device, probed
std::string architectures();                  // the GPU architectures compiled into this build
}  // namespace cuda

}  // namespace resonant::compute
