#include "compute/Compute.h"

#include "compute/CudaRuntime.h"
#include "compute/ThreadPool.h"
#include "compute/VectorKernels.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <mutex>

namespace resonant::compute {

std::string_view toString(ComputeBackend b)
{
    return b == ComputeBackend::Cuda ? "cuda" : "cpu";
}

Result<ComputeBackend> computeBackendFromString(std::string_view text)
{
    if (text == "cpu")
        return ComputeBackend::Cpu;
    if (text == "cuda")
        return ComputeBackend::Cuda;
    return Result<ComputeBackend>::failure("unknown compute backend '" + std::string(text) + "'");
}

Status ComputeOptions::validate() const
{
    if (threads < 0 || threads > 4096)
        return Status::failure("the thread count must be between 0 (all) and 4096");
    if (device < 0 || device > 64)
        return Status::failure("invalid CUDA device index");
    return Status::success();
}

nlohmann::json toJson(const ComputeOptions& o)
{
    return {{"backend", std::string(toString(o.backend))}, {"threads", o.threads}, {"device", o.device}};
}

Result<ComputeOptions> computeOptionsFromJson(const nlohmann::json& v)
{
    using Failure = Result<ComputeOptions>;
    if (!v.is_object())
        return Failure::failure("compute options must be an object");
    ComputeOptions o;
    try {
        auto backend = computeBackendFromString(v.value("backend", std::string("cpu")));
        if (!backend)
            return Failure::failure(backend.message());
        o.backend = backend.value();
        o.threads = v.value("threads", 0);
        o.device = v.value("device", 0);
    } catch (const nlohmann::json::exception& e) {
        return Failure::failure(std::string("invalid compute options: ") + e.what());
    }
    if (Status status = o.validate(); !status)
        return Failure::failure(status.message());
    return o;
}

ComputeCapabilities detectCompute()
{
    ComputeCapabilities c;
    c.hardwareThreads = hardwareThreads();
    c.volkBuilt = volkAvailable();
    c.volkVersion = volkVersion();
    c.volkMachine = volkMachine();
    c.cudaBuilt = cuda::built();
    c.cudaRuntime = cuda::runtimeDescription();
    c.cudaDevices = cuda::devices();
    return c;
}

std::string ComputeCapabilities::summary() const
{
    std::string s = "CPU: " + std::to_string(hardwareThreads) + " hardware threads. VOLK: ";
    s += volkBuilt ? "version " + volkVersion + ", kernels for '" + volkMachine + "'" : "not in this build";
    s += ". CUDA: ";
    if (!cudaBuilt) {
        s += "not in this build";
    } else {
        s += cudaRuntime;
        if (cudaDevices.empty())
            s += "; no device";
        for (const CudaDeviceInfo& d : cudaDevices) {
            char text[400];
            std::snprintf(text, sizeof text, "; device %d %s (sm_%d%d, %.1f GiB): %s", d.index, d.name.c_str(), d.major, d.minor,
                          static_cast<double>(d.totalBytes) / 1073741824.0,
                          d.usable ? (std::to_string(d.freeBytes >> 20) + " MiB free").c_str() : d.problem.c_str());
            s += text;
        }
    }
    return s + ".";
}

Status checkBackend(const ComputeOptions& o, size_t bytes)
{
    if (Status status = o.validate(); !status)
        return status;
    if (o.backend == ComputeBackend::Cpu)
        return Status::success();
    if (!cuda::built())
        return Status::failure("CUDA was chosen, but " + cuda::runtimeDescription());
    const auto devices = cuda::devices();
    if (o.device >= static_cast<int>(devices.size()))
        return Status::failure("CUDA was chosen, but there is no CUDA device " + std::to_string(o.device) + " (" + cuda::runtimeDescription() + ")");
    const CudaDeviceInfo& d = devices[static_cast<size_t>(o.device)];
    if (!d.usable)
        return Status::failure("CUDA device " + std::to_string(o.device) + " (" + d.name + ") cannot be used: " + d.problem);
    const size_t margin = 64ull * 1024ull * 1024ull;
    if (bytes + margin > d.freeBytes)
        return Status::failure("the problem needs " + std::to_string((bytes + margin) >> 20) + " MiB on the GPU, but " + d.name + " has " +
                               std::to_string(d.freeBytes >> 20) + " MiB free");
    return Status::success();
}

}  // namespace resonant::compute
