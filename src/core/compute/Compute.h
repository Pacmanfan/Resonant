#pragma once

// Where a numerical solver runs, and what this machine offers.
//
// The CPU path in double precision is the reference (ADR-000205). Threads
// split independent cell updates and give bit-identical results; the CUDA
// path is an optional backend, used only when chosen, and validated against
// the CPU path (ADR-000212). Nothing switches backend automatically.
#include "compute/CudaRuntime.h"
#include "foundation/Result.h"

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace resonant::compute {

enum class ComputeBackend { Cpu, Cuda };
std::string_view toString(ComputeBackend backend);
Result<ComputeBackend> computeBackendFromString(std::string_view text);

struct ComputeOptions {
    ComputeBackend backend = ComputeBackend::Cpu;
    int threads = 0;  // CPU: 0 = every hardware thread
    int device = 0;   // CUDA device index
    Status validate() const;
    friend bool operator==(const ComputeOptions&, const ComputeOptions&) = default;
};
nlohmann::json toJson(const ComputeOptions& options);
Result<ComputeOptions> computeOptionsFromJson(const nlohmann::json& value);


struct ComputeCapabilities {
    int hardwareThreads = 1;
    bool volkBuilt = false;
    std::string volkVersion, volkMachine;  // the kernel set VOLK selected for this CPU
    bool cudaBuilt = false;
    std::string cudaRuntime;               // runtime / driver versions, or why there is none
    std::vector<CudaDeviceInfo> cudaDevices;
    std::string summary() const;
};

// Queries the machine (CUDA devices are probed each call; cheap after the first).
ComputeCapabilities detectCompute();

// Checks that a backend can run a problem needing `bytes` of state, with a
// reason when it cannot (no CUDA in the build, no usable device, not enough
// free memory).
Status checkBackend(const ComputeOptions& options, size_t bytes);

}  // namespace resonant::compute
