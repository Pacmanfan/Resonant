#include "SoapyDriver.h"

#include "foundation/Time.h"

#include <map>
#include <mutex>

#ifdef RESONANT_HAVE_SOAPYSDR
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Version.hpp>
#endif

namespace resonant::hardware {

bool soapyBuilt()
{
#ifdef RESONANT_HAVE_SOAPYSDR
    return true;
#else
    return false;
#endif
}

#ifndef RESONANT_HAVE_SOAPYSDR

std::shared_ptr<experiments::DeviceDriver> makeSoapyDriver() { return nullptr; }

#else

namespace {

using namespace experiments;

// SoapySDR's module loader writes errors to stderr; they are collected instead.
std::mutex& logMutex()
{
    static std::mutex m;
    return m;
}
std::vector<std::string>& logLines()
{
    static std::vector<std::string> lines;
    return lines;
}
void collect(const SoapySDRLogLevel level, const char* message)
{
    if (level > SOAPY_SDR_WARNING)
        return;
    std::lock_guard lock(logMutex());
    logLines().push_back(message);
}

class SoapyDevice final : public HardwareDevice {
public:
    explicit SoapyDevice(DeviceInfo info, DeviceCapabilities caps) : info_(std::move(info)), caps_(std::move(caps)) {}
    const DeviceInfo& info() const override { return info_; }
    DeviceCapabilities capabilities() const override { return caps_; }
    DeviceStatus status() const override { return status_; }
    Status configure(const DeviceConfiguration& c) override
    {
        config_ = c;
        status_ = DeviceStatus::Configured;
        return Status::success();
    }
    const DeviceConfiguration& configuration() const override { return config_; }
    Result<std::unique_ptr<AcquisitionSession>> startAcquisition(size_t) override
    {
        return Result<std::unique_ptr<AcquisitionSession>>::failure(
            "SoapySDR streaming is not implemented in this version (no SDR was available to test it); discovery and "
            "capability inspection are");
    }
    Result<std::unique_ptr<OutputSession>> startOutput(const OutputSignal&, const OutputAuthorization&) override
    {
        return Result<std::unique_ptr<OutputSession>>::failure("SoapySDR transmission is not implemented in this version");
    }
    void disconnect() override { status_ = DeviceStatus::Disconnected; }
    std::vector<DeviceError> errors() const override { return {}; }

private:
    DeviceInfo info_;
    DeviceCapabilities caps_;
    DeviceStatus status_ = DeviceStatus::Connected;
    DeviceConfiguration config_;
};

class SoapyDriver final : public DeviceDriver {
public:
    SoapyDriver() { SoapySDR::registerLogHandler(collect); }
    std::string name() const override { return "soapysdr"; }
    std::string version() const override { return "SoapySDR " + SoapySDR::getLibVersion(); }
    ConnectionType connection() const override { return ConnectionType::VendorSdk; }
    DriverAvailability availability() const override
    {
        return {true, "SoapySDR " + SoapySDR::getLibVersion() + " (discovery and capabilities only)", "BSL-1.0 (dynamically linked)"};
    }
    Result<std::vector<DeviceInfo>> discover() override
    {
        std::vector<DeviceInfo> out;
        try {
            for (const auto& args : SoapySDR::Device::enumerate()) {
                DeviceInfo i;
                const auto get = [&](const char* key) { auto it = args.find(key); return it == args.end() ? std::string() : it->second; };
                i.driver = "soapysdr";
                i.serial = get("serial");
                i.model = get("driver");
                i.name = get("label").empty() ? "SoapySDR " + i.model : get("label");
                i.vendor = get("manufacturer");
                i.address = SoapySDR::KwargsToString(args);
                i.id = "soapysdr:" + (i.serial.empty() ? i.address : i.model + ":" + i.serial);
                i.connection = ConnectionType::VendorSdk;
                i.driverVersion = version();
                args_[i.id] = args;
                out.push_back(i);
            }
        } catch (const std::exception& e) {
            return Result<std::vector<DeviceInfo>>::failure(std::string("SoapySDR enumeration failed: ") + e.what());
        }
        return out;
    }
    Result<std::unique_ptr<HardwareDevice>> open(const DeviceInfo& info) override
    {
        using R = Result<std::unique_ptr<HardwareDevice>>;
        auto it = args_.find(info.id);
        if (it == args_.end())
            return R::failure("SoapySDR device '" + info.id + "' was not found by discovery");
        SoapySDR::Device* device = nullptr;
        try {
            device = SoapySDR::Device::make(it->second);
        } catch (const std::exception& e) {
            return R::failure(std::string("SoapySDR: cannot open ") + info.name + ": " + e.what());
        }
        DeviceCapabilities c;
        const size_t rx = device->getNumChannels(SOAPY_SDR_RX);
        for (size_t ch = 0; ch < rx; ++ch)
            c.inputs.push_back({"rx" + std::to_string(ch), "RX " + std::to_string(ch + 1), ChannelDirection::Input,
                                Quantity::DigitalFullScale, -1.0, 1.0, true});
        if (rx > 0) {
            c.sampleRates = device->listSampleRates(SOAPY_SDR_RX, 0);
            const auto ranges = device->getFrequencyRange(SOAPY_SDR_RX, 0);
            if (!ranges.empty()) {
                c.minTuneHz = ranges.front().minimum();
                c.maxTuneHz = ranges.back().maximum();
            }
        }
        for (const auto& source : device->listClockSources())
            c.clocks.push_back({source, "clock source " + source, 0.0, source != "internal", 0.0});
        c.hardwareTimestamps = device->hasHardwareTime();
        c.formats = {SampleFormat::ComplexFloat32};
        c.modes = {"discovery-only"};
        c.reachableStates = {DeviceStatus::Connected, DeviceStatus::Configured, DeviceStatus::Disconnected};
        SoapySDR::Device::unmake(device);
        return std::unique_ptr<HardwareDevice>(std::make_unique<SoapyDevice>(info, c));
    }

private:
    std::map<std::string, SoapySDR::Kwargs> args_;
};

}  // namespace

std::shared_ptr<experiments::DeviceDriver> makeSoapyDriver() { return std::make_shared<SoapyDriver>(); }

#endif

}  // namespace resonant::hardware
