// CUDA runtime queries (compiled only with RESONANT_HAVE_CUDA).
#include "compute/CudaRuntime.h"

#include <cuda_runtime.h>

#include <cstdio>

#ifndef RESONANT_CUDA_ARCHITECTURES
#define RESONANT_CUDA_ARCHITECTURES "unknown"
#endif

namespace resonant::compute::cuda {

bool built()
{
    return true;
}

std::string architectures()
{
    return RESONANT_CUDA_ARCHITECTURES;
}

std::string runtimeDescription()
{
    int runtime = 0, driver = 0;
    cudaRuntimeGetVersion(&runtime);
    const cudaError_t e = cudaDriverGetVersion(&driver);
    char text[200];
    if (e != cudaSuccess || driver == 0)
        std::snprintf(text, sizeof text, "CUDA runtime %d.%d; no NVIDIA driver (%s)", runtime / 1000, (runtime % 1000) / 10, cudaGetErrorString(e));
    else
        std::snprintf(text, sizeof text, "CUDA runtime %d.%d, driver supports %d.%d", runtime / 1000, (runtime % 1000) / 10, driver / 1000,
                      (driver % 1000) / 10);
    return text;
}

std::vector<CudaDeviceInfo> devices()
{
    std::vector<CudaDeviceInfo> out;
    int count = 0;
    if (cudaGetDeviceCount(&count) != cudaSuccess) {
        cudaGetLastError();
        return out;
    }
    for (int i = 0; i < count; ++i) {
        CudaDeviceInfo d;
        d.index = i;
        cudaDeviceProp p{};
        if (cudaGetDeviceProperties(&p, i) == cudaSuccess) {
            d.name = p.name;
            d.major = p.major;
            d.minor = p.minor;
            d.totalBytes = p.totalGlobalMem;
        }
        // Creating the context is the real test: it needs device memory.
        cudaError_t e = cudaSetDevice(i);
        size_t freeBytes = 0, totalBytes = 0;
        if (e == cudaSuccess)
            e = cudaMemGetInfo(&freeBytes, &totalBytes);
        if (e == cudaSuccess) {
            d.usable = true;
            d.freeBytes = freeBytes;
            d.totalBytes = totalBytes;
        } else {
            d.problem = std::string("a CUDA context could not be created: ") + cudaGetErrorString(e) +
                        (e == cudaErrorMemoryAllocation ? " (the GPU's memory is taken by other programs)" : "");
            cudaGetLastError();
        }
        out.push_back(d);
    }
    return out;
}

}  // namespace resonant::compute::cuda
