#pragma once

// The hardware abstraction layer: instruments behind stable interfaces,
// independent of Qt, of any vendor SDK and of the physics domains.
//
//   DeviceDriver      finds devices of one kind (simulated, ALSA, SoapySDR, ...)
//   HardwareDevice    one instrument: capabilities, configuration, status, errors
//   AcquisitionSession  a running input stream: blocks of samples
//   OutputSession     a running output: only with a valid OutputAuthorization
//   FrequencySweepInstrument  an instrument that measures network parameters (a VNA)
//   Positioner        moves a probe or the device under test
//   DeviceRegistry    the drivers, discovery (on its own thread) and connections
//
// A device reports only the states its driver can establish. A sensor is not
// a device channel: experiments bind sensors to channels (Experiment.h).
#include "experiments/ExperimentTypes.h"
#include "experiments/Safety.h"
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <complex>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace resonant::experiments {

enum class DeviceStatus { Disconnected, Discovering, Connecting, Connected, Configured, Acquiring, Transmitting, Error };
enum class ConnectionType { Simulated, Usb, Ethernet, Pcie, Serial, NetworkApi, VendorSdk, SystemAudio, File };
enum class ChannelDirection { Input, Output };

std::string_view toString(DeviceStatus status);
std::string_view toString(ConnectionType connection);
std::string_view toString(ChannelDirection direction);

struct ChannelInfo {
    std::string id;
    std::string name;
    ChannelDirection direction = ChannelDirection::Input;
    Quantity quantity = Quantity::Voltage;  // of the raw reading / the command
    double rangeMin = -1.0, rangeMax = 1.0; // in that quantity
    bool complex = false;                   // I/Q samples
};

struct ClockSource {
    std::string id;           // "internal", "external-10MHz", "pps", ...
    std::string description;
    double nominalHz = 0.0;
    bool external = false;
    double accuracyPpm = 0.0; // as specified, 0 = not stated
};

struct TriggerSource {
    std::string id;           // "immediate", "software", "external", "level"
    std::string description;
    bool hardware = false;
};

struct DeviceCapabilities {
    std::vector<ChannelInfo> inputs, outputs;
    std::vector<double> sampleRates;             // discrete rates; empty = the range below
    double minSampleRateHz = 0.0, maxSampleRateHz = 0.0;
    std::vector<SampleFormat> formats;
    double minTuneHz = 0.0, maxTuneHz = 0.0;     // RF centre frequency range; 0 / 0 = baseband
    double minOutputPowerDbm = 0.0, maxOutputPowerDbm = 0.0;  // RF sources
    std::vector<ClockSource> clocks;
    std::vector<TriggerSource> triggers;
    bool hardwareTimestamps = false;
    bool sharedSampleClock = true;               // all channels of the device share one sample clock
    bool frequencySweep = false;                 // FrequencySweepInstrument
    bool positioner = false;                     // Positioner
    bool interlock = false;                      // reports a hardware interlock
    std::vector<std::string> modes;
    std::vector<DeviceStatus> reachableStates;

    bool supportsSampleRate(double hz) const;
    const ChannelInfo* input(const std::string& id) const;
    const ChannelInfo* output(const std::string& id) const;
};

nlohmann::json toJson(const DeviceCapabilities& capabilities);

struct DeviceInfo {
    std::string id;          // stable: "<driver>:<serial or address>"
    std::string driver;
    std::string name;
    std::string vendor, model, serial;
    ConnectionType connection = ConnectionType::Simulated;
    std::string address;
    std::string driverVersion;
    bool simulated = false;  // the data is generated, not measured
};

nlohmann::json toJson(const DeviceInfo& info);

struct DeviceError {
    std::string code;
    std::string message;
    bool safetyRelevant = false;  // loss of control of an output, interlock opened, ...
    std::string timeUtc;
};

struct DeviceConfiguration {
    double sampleRateHz = 48000.0;
    SampleFormat format = SampleFormat::Float32;
    std::vector<std::string> inputChannels;   // device channel ids, in this order in each block
    std::vector<std::string> outputChannels;
    std::string clockSource = "internal";
    std::string triggerSource = "immediate";
    double centerFrequencyHz = 0.0;           // RF
    double gainDb = 0.0;                      // RF receiver gain
    size_t blockFrames = 1024;
    nlohmann::json options = nlohmann::json::object();  // driver specific
};

nlohmann::json toJson(const DeviceConfiguration& configuration);
Result<DeviceConfiguration> deviceConfigurationFromJson(const nlohmann::json& value);

// One block of an input stream, interleaved by frame: real samples in
// `samples` (frames x channels) or complex samples in `iq`. Values are the
// instrument's readings in the channels' raw quantity.
struct SampleBlock {
    uint64_t sequence = 0;
    uint64_t firstFrame = 0;        // the device's frame counter at the first frame
    size_t frames = 0;
    size_t channels = 0;
    std::vector<double> samples;
    std::vector<std::complex<double>> iq;
    std::optional<double> hardwareTimestamp;  // s, device clock, if the device has one
    double softwareTimestamp = 0.0;           // s since the session started (steady clock)
    uint64_t droppedBefore = 0;               // frames the device reports lost before this block (overrun)
    bool endOfStream = false;                 // a finite acquisition delivered everything
};

class AcquisitionSession {
public:
    virtual ~AcquisitionSession() = default;
    // The next block, waiting up to `timeout`. A failure is a device error
    // (disconnection, overrun the device cannot report as a gap, ...).
    virtual Result<SampleBlock> read(std::chrono::milliseconds timeout) = 0;
    virtual void stop() = 0;
};

// Output samples per output channel, in the channel's command quantity.
struct OutputSignal {
    std::vector<std::vector<double>> channels;
    double sampleRateHz = 0.0;
    bool repeat = false;
    // RF sources: a carrier instead of samples.
    double carrierHz = 0.0;
    double powerDbm = -1000.0;
};

class OutputSession {
public:
    virtual ~OutputSession() = default;
    virtual bool running() const = 0;
    // Stops at once (also called on an emergency stop).
    virtual void stop() = 0;
    virtual uint64_t framesWritten() const = 0;
};

struct SweepSettings {
    double startHz = 0.0, stopHz = 0.0;
    int points = 201;
    double powerDbm = -10.0;     // the stimulus power at the test ports
    double ifBandwidthHz = 1000.0;
    std::vector<int> ports{1};   // measured ports (1-based)
};

struct NetworkData {
    int ports = 1;
    std::vector<double> frequencyHz;
    std::vector<std::vector<std::complex<double>>> s;  // per frequency, row-major ports x ports
    double referenceImpedance = 50.0;
    std::string note;
};

class FrequencySweepInstrument {
public:
    virtual ~FrequencySweepInstrument() = default;
    // A sweep drives the test ports: it is a physical output and needs an authorization.
    virtual Result<NetworkData> sweep(const SweepSettings& settings, const OutputAuthorization& authorization) = 0;
};

class Positioner {
public:
    virtual ~Positioner() = default;
    virtual Status moveTo(const Pose& pose) = 0;
    virtual Pose current() const = 0;
};

class HardwareDevice {
public:
    virtual ~HardwareDevice() = default;
    virtual const DeviceInfo& info() const = 0;
    virtual DeviceCapabilities capabilities() const = 0;
    virtual DeviceStatus status() const = 0;
    virtual Status configure(const DeviceConfiguration& configuration) = 0;
    virtual const DeviceConfiguration& configuration() const = 0;
    // frames = 0: continuous until stop().
    virtual Result<std::unique_ptr<AcquisitionSession>> startAcquisition(size_t frames) = 0;
    // Checks the authorization with the OutputAuthority before anything is output.
    virtual Result<std::unique_ptr<OutputSession>> startOutput(const OutputSignal& signal, const OutputAuthorization& authorization);
    virtual FrequencySweepInstrument* sweepInstrument() { return nullptr; }
    virtual Positioner* positioner() { return nullptr; }
    virtual bool interlockClosed() const { return false; }
    virtual void disconnect() = 0;
    virtual std::vector<DeviceError> errors() const = 0;
};

struct DriverAvailability {
    bool available = false;
    std::string reason;    // why not, or what was found
    std::string license;   // of the library the driver uses
};

class DeviceDriver {
public:
    virtual ~DeviceDriver() = default;
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual ConnectionType connection() const = 0;
    virtual DriverAvailability availability() const = 0;
    // Must not block for long; called on the registry's discovery thread.
    virtual Result<std::vector<DeviceInfo>> discover() = 0;
    virtual Result<std::unique_ptr<HardwareDevice>> open(const DeviceInfo& info) = 0;
};

struct DiscoveryReport {
    std::vector<DeviceInfo> devices;
    std::vector<std::string> problems;  // per driver
    double seconds = 0.0;
};

class DeviceRegistry {
public:
    DeviceRegistry() = default;
    ~DeviceRegistry();
    DeviceRegistry(const DeviceRegistry&) = delete;
    DeviceRegistry& operator=(const DeviceRegistry&) = delete;

    void registerDriver(std::shared_ptr<DeviceDriver> driver);
    std::vector<std::shared_ptr<DeviceDriver>> drivers() const;

    // Blocking discovery over every driver (problems are collected, not fatal).
    DiscoveryReport discover();
    // Discovery on a separate thread; `done` runs on that thread. Never
    // blocks the caller (the application starts without waiting for it).
    void discoverAsync(std::function<void(const DiscoveryReport&)> done);
    bool discovering() const { return discovering_.load(); }
    std::vector<DeviceInfo> knownDevices() const;

    // Opens (once) and returns the device; later calls return the same one.
    Result<std::shared_ptr<HardwareDevice>> connect(const std::string& deviceId);
    std::shared_ptr<HardwareDevice> connected(const std::string& deviceId) const;
    void disconnect(const std::string& deviceId);
    void disconnectAll();
    DeviceStatus status(const std::string& deviceId) const;

private:
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<DeviceDriver>> drivers_;
    std::vector<DeviceInfo> known_;
    std::map<std::string, std::shared_ptr<HardwareDevice>> open_;
    std::atomic<bool> discovering_{false};
    std::thread discoveryThread_;
};

}  // namespace resonant::experiments
