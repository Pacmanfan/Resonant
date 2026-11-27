#include "experiments/Hardware.h"

#include "foundation/Log.h"

#include <algorithm>
#include <cmath>

namespace resonant::experiments {

std::string_view toString(DeviceStatus status)
{
    switch (status) {
    case DeviceStatus::Disconnected: return "disconnected";
    case DeviceStatus::Discovering: return "discovering";
    case DeviceStatus::Connecting: return "connecting";
    case DeviceStatus::Connected: return "connected";
    case DeviceStatus::Configured: return "configured";
    case DeviceStatus::Acquiring: return "acquiring";
    case DeviceStatus::Transmitting: return "transmitting";
    case DeviceStatus::Error: return "error";
    }
    return "error";
}

std::string_view toString(ConnectionType connection)
{
    switch (connection) {
    case ConnectionType::Simulated: return "simulated";
    case ConnectionType::Usb: return "usb";
    case ConnectionType::Ethernet: return "ethernet";
    case ConnectionType::Pcie: return "pcie";
    case ConnectionType::Serial: return "serial";
    case ConnectionType::NetworkApi: return "network-api";
    case ConnectionType::VendorSdk: return "vendor-sdk";
    case ConnectionType::SystemAudio: return "system-audio";
    case ConnectionType::File: return "file";
    }
    return "simulated";
}

std::string_view toString(ChannelDirection direction) { return direction == ChannelDirection::Input ? "input" : "output"; }

bool DeviceCapabilities::supportsSampleRate(double hz) const
{
    if (!sampleRates.empty())
        return std::any_of(sampleRates.begin(), sampleRates.end(), [&](double r) { return std::abs(r - hz) <= 1e-9 * r; });
    return hz >= minSampleRateHz && hz <= maxSampleRateHz && hz > 0.0;
}

const ChannelInfo* DeviceCapabilities::input(const std::string& id) const
{
    for (const auto& c : inputs)
        if (c.id == id)
            return &c;
    return nullptr;
}

const ChannelInfo* DeviceCapabilities::output(const std::string& id) const
{
    for (const auto& c : outputs)
        if (c.id == id)
            return &c;
    return nullptr;
}

namespace {

nlohmann::json channelsJson(const std::vector<ChannelInfo>& channels)
{
    nlohmann::json list = nlohmann::json::array();
    for (const auto& c : channels)
        list.push_back({{"id", c.id},
                        {"name", c.name},
                        {"quantity", toString(c.quantity)},
                        {"unit", siUnit(c.quantity)},
                        {"range", {c.rangeMin, c.rangeMax}},
                        {"complex", c.complex}});
    return list;
}

}  // namespace

nlohmann::json toJson(const DeviceCapabilities& c)
{
    nlohmann::json formats = nlohmann::json::array();
    for (auto f : c.formats)
        formats.push_back(toString(f));
    nlohmann::json clocks = nlohmann::json::array();
    for (const auto& k : c.clocks)
        clocks.push_back({{"id", k.id}, {"description", k.description}, {"nominalHz", k.nominalHz}, {"external", k.external},
                          {"accuracyPpm", k.accuracyPpm}});
    nlohmann::json triggers = nlohmann::json::array();
    for (const auto& t : c.triggers)
        triggers.push_back({{"id", t.id}, {"description", t.description}, {"hardware", t.hardware}});
    nlohmann::json states = nlohmann::json::array();
    for (auto s : c.reachableStates)
        states.push_back(toString(s));
    return {{"inputs", channelsJson(c.inputs)},
            {"outputs", channelsJson(c.outputs)},
            {"sampleRates", c.sampleRates},
            {"sampleRateRangeHz", {c.minSampleRateHz, c.maxSampleRateHz}},
            {"formats", formats},
            {"tuneRangeHz", {c.minTuneHz, c.maxTuneHz}},
            {"outputPowerRangeDbm", {c.minOutputPowerDbm, c.maxOutputPowerDbm}},
            {"clocks", clocks},
            {"triggers", triggers},
            {"hardwareTimestamps", c.hardwareTimestamps},
            {"sharedSampleClock", c.sharedSampleClock},
            {"frequencySweep", c.frequencySweep},
            {"positioner", c.positioner},
            {"interlock", c.interlock},
            {"modes", c.modes},
            {"reachableStates", states}};
}

nlohmann::json toJson(const DeviceInfo& i)
{
    return {{"id", i.id},           {"driver", i.driver},   {"name", i.name},
            {"vendor", i.vendor},   {"model", i.model},     {"serial", i.serial},
            {"connection", toString(i.connection)},         {"address", i.address},
            {"driverVersion", i.driverVersion},             {"simulated", i.simulated}};
}

nlohmann::json toJson(const DeviceConfiguration& c)
{
    return {{"sampleRateHz", c.sampleRateHz},       {"format", toString(c.format)},
            {"inputChannels", c.inputChannels},     {"outputChannels", c.outputChannels},
            {"clockSource", c.clockSource},         {"triggerSource", c.triggerSource},
            {"centerFrequencyHz", c.centerFrequencyHz}, {"gainDb", c.gainDb},
            {"blockFrames", c.blockFrames},         {"options", c.options}};
}

Result<DeviceConfiguration> deviceConfigurationFromJson(const nlohmann::json& v)
{
    DeviceConfiguration c;
    if (v.is_null())
        return c;
    if (!v.is_object())
        return Result<DeviceConfiguration>::failure("a device configuration must be an object");
    c.sampleRateHz = v.value("sampleRateHz", c.sampleRateHz);
    auto format = sampleFormatFromString(v.value("format", std::string("float32")));
    if (!format)
        return Result<DeviceConfiguration>::failure(format.message());
    c.format = format.value();
    c.inputChannels = v.value("inputChannels", std::vector<std::string>{});
    c.outputChannels = v.value("outputChannels", std::vector<std::string>{});
    c.clockSource = v.value("clockSource", c.clockSource);
    c.triggerSource = v.value("triggerSource", c.triggerSource);
    c.centerFrequencyHz = v.value("centerFrequencyHz", 0.0);
    c.gainDb = v.value("gainDb", 0.0);
    c.blockFrames = v.value("blockFrames", static_cast<size_t>(1024));
    c.options = v.value("options", nlohmann::json::object());
    if (!(c.sampleRateHz > 0.0) || c.blockFrames == 0)
        return Result<DeviceConfiguration>::failure("the sample rate and the block length must be positive");
    return c;
}

Result<std::unique_ptr<OutputSession>> HardwareDevice::startOutput(const OutputSignal&, const OutputAuthorization&)
{
    return Result<std::unique_ptr<OutputSession>>::failure(info().name + " has no output channels");
}

DeviceRegistry::~DeviceRegistry()
{
    if (discoveryThread_.joinable())
        discoveryThread_.join();
    disconnectAll();
}

void DeviceRegistry::registerDriver(std::shared_ptr<DeviceDriver> driver)
{
    std::lock_guard lock(mutex_);
    for (const auto& d : drivers_)
        if (d->name() == driver->name())
            return;
    drivers_.push_back(std::move(driver));
}

std::vector<std::shared_ptr<DeviceDriver>> DeviceRegistry::drivers() const
{
    std::lock_guard lock(mutex_);
    return drivers_;
}

DiscoveryReport DeviceRegistry::discover()
{
    const auto start = std::chrono::steady_clock::now();
    DiscoveryReport report;
    for (const auto& driver : drivers()) {
        const DriverAvailability a = driver->availability();
        if (!a.available) {
            report.problems.push_back(driver->name() + ": not available (" + a.reason + ")");
            continue;
        }
        try {
            auto found = driver->discover();
            if (!found) {
                report.problems.push_back(driver->name() + ": " + found.message());
                continue;
            }
            for (auto& d : found.value())
                report.devices.push_back(std::move(d));
        } catch (const std::exception& e) {
            report.problems.push_back(driver->name() + ": discovery failed (" + e.what() + ")");
        }
    }
    report.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    {
        std::lock_guard lock(mutex_);
        known_ = report.devices;
    }
    return report;
}

void DeviceRegistry::discoverAsync(std::function<void(const DiscoveryReport&)> done)
{
    if (discovering_.exchange(true))
        return;
    if (discoveryThread_.joinable())
        discoveryThread_.join();
    discoveryThread_ = std::thread([this, done = std::move(done)] {
        DiscoveryReport report = discover();
        discovering_.store(false);
        if (done)
            done(report);
    });
}

std::vector<DeviceInfo> DeviceRegistry::knownDevices() const
{
    std::lock_guard lock(mutex_);
    return known_;
}

Result<std::shared_ptr<HardwareDevice>> DeviceRegistry::connect(const std::string& deviceId)
{
    std::shared_ptr<DeviceDriver> driver;
    DeviceInfo info;
    {
        std::lock_guard lock(mutex_);
        if (auto it = open_.find(deviceId); it != open_.end())
            return it->second;
        auto found = std::find_if(known_.begin(), known_.end(), [&](const DeviceInfo& d) { return d.id == deviceId; });
        if (found == known_.end())
            return Result<std::shared_ptr<HardwareDevice>>::failure("device '" + deviceId +
                                                                     "' was not found by discovery (is it connected, and has "
                                                                     "discovery run?)");
        info = *found;
        for (const auto& d : drivers_)
            if (d->name() == info.driver)
                driver = d;
    }
    if (!driver)
        return Result<std::shared_ptr<HardwareDevice>>::failure("no driver '" + info.driver + "' for device '" + deviceId + "'");
    auto device = driver->open(info);
    if (!device)
        return Result<std::shared_ptr<HardwareDevice>>::failure(device.message());
    std::shared_ptr<HardwareDevice> shared(device.take());
    std::lock_guard lock(mutex_);
    open_[deviceId] = shared;
    return shared;
}

std::shared_ptr<HardwareDevice> DeviceRegistry::connected(const std::string& deviceId) const
{
    std::lock_guard lock(mutex_);
    auto it = open_.find(deviceId);
    return it == open_.end() ? nullptr : it->second;
}

void DeviceRegistry::disconnect(const std::string& deviceId)
{
    std::shared_ptr<HardwareDevice> device;
    {
        std::lock_guard lock(mutex_);
        auto it = open_.find(deviceId);
        if (it == open_.end())
            return;
        device = it->second;
        open_.erase(it);
    }
    device->disconnect();
}

void DeviceRegistry::disconnectAll()
{
    std::map<std::string, std::shared_ptr<HardwareDevice>> all;
    {
        std::lock_guard lock(mutex_);
        all.swap(open_);
    }
    for (auto& [id, device] : all)
        device->disconnect();
}

DeviceStatus DeviceRegistry::status(const std::string& deviceId) const
{
    if (auto device = connected(deviceId))
        return device->status();
    return DeviceStatus::Disconnected;
}

}  // namespace resonant::experiments
