#include "experiments/SimulatedHardware.h"

#include "foundation/Time.h"
#include "foundation/Units.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <thread>

namespace resonant::experiments {

namespace {

using units::pi;

// --- small signal-processing pieces of the simulated world -------------------------------

struct Random {
    uint64_t state = 1;
    double uniform()
    {
        uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        z ^= z >> 31;
        return (static_cast<double>(z >> 11) + 0.5) * (1.0 / 9007199254740992.0);
    }
    double gaussian()  // Box-Muller
    {
        return std::sqrt(-2.0 * std::log(uniform())) * std::cos(2.0 * pi * uniform());
    }
};

// Transposed direct form II biquad.
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;
    double process(double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Bilinear transform of (c2 s^2 + c1 s + c0) / (s^2 + d1 s + d0), prewarped at w0.
Biquad bilinear(double c2, double c1, double c0, double d1, double d0, double w0, double fs)
{
    const double K = w0 / std::tan(w0 / (2.0 * fs));
    const double K2 = K * K;
    const double a0 = K2 + d1 * K + d0;
    Biquad q;
    q.b0 = (c2 * K2 + c1 * K + c0) / a0;
    q.b1 = (2.0 * c0 - 2.0 * c2 * K2) / a0;
    q.b2 = (c2 * K2 - c1 * K + c0) / a0;
    q.a1 = (2.0 * d0 - 2.0 * K2) / a0;
    q.a2 = (K2 - d1 * K + d0) / a0;
    return q;
}

Biquad highPass(double fc, double q, double fs)
{
    const double w = 2.0 * pi * fc;
    return bilinear(1.0, 0.0, 0.0, w / q, w * w, w, fs);
}

double sinc(double x) { return std::abs(x) < 1e-12 ? 1.0 : std::sin(pi * x) / (pi * x); }

// A signal history with fractional-delay reads (windowed sinc, 8 taps each side;
// taps later than the newest sample are left out).
class History {
public:
    explicit History(size_t capacity = 32768) : data_(capacity, 0.0) {}
    void push(double v)
    {
        data_[static_cast<size_t>(count_ % data_.size())] = v;
        ++count_;
    }
    // The value `delay` samples before the newest sample.
    double delayed(double delay) const
    {
        if (count_ == 0)
            return 0.0;
        const double position = static_cast<double>(count_ - 1) - delay;
        const long centre = static_cast<long>(std::floor(position));
        double sum = 0.0;
        for (long j = centre - 7; j <= centre + 8; ++j) {
            if (j < 0 || j > static_cast<long>(count_) - 1 || static_cast<long>(count_) - 1 - j >= static_cast<long>(data_.size()))
                continue;
            const double d = position - static_cast<double>(j);
            const double w = 0.5 * (1.0 + std::cos(pi * d / 8.5));
            sum += data_[static_cast<size_t>(j) % data_.size()] * sinc(d) * w;
        }
        return sum;
    }

private:
    std::vector<double> data_;
    uint64_t count_ = 0;
};

template <typename T>
T option(const nlohmann::json& options, const char* key, T fallback)
{
    if (!options.is_object() || !options.contains(key))
        return fallback;
    try {
        return options.at(key).get<T>();
    } catch (...) {
        return fallback;
    }
}

std::vector<double> optionList(const nlohmann::json& options, const char* key, size_t count, double fallback)
{
    std::vector<double> v = option(options, key, std::vector<double>{});
    v.resize(count, fallback);
    return v;
}

ChannelInfo channel(std::string id, std::string name, ChannelDirection direction, Quantity q, double range, bool complex = false)
{
    return {std::move(id), std::move(name), direction, q, -range, range, complex};
}

double distance(const Pose& p, double x, double y, double z)
{
    return std::sqrt((p.x - x) * (p.x - x) + (p.y - y) * (p.y - y) + (p.z - z) * (p.z - z));
}

// --- the device base ---------------------------------------------------------------------

class SimulatedDevice : public HardwareDevice {
public:
    SimulatedDevice(DeviceInfo info, std::shared_ptr<SimulatedBench> bench) : info_(std::move(info)), bench_(std::move(bench)) {}
    const DeviceInfo& info() const override { return info_; }
    DeviceStatus status() const override
    {
        std::lock_guard lock(mutex_);
        return status_;
    }
    const DeviceConfiguration& configuration() const override { return config_; }
    std::vector<DeviceError> errors() const override
    {
        std::lock_guard lock(mutex_);
        return errors_;
    }
    void disconnect() override
    {
        stopOutput("disconnected");
        std::lock_guard lock(mutex_);
        status_ = DeviceStatus::Disconnected;
        disconnected_ = true;
    }

    Status configure(const DeviceConfiguration& c) override
    {
        const DeviceCapabilities caps = capabilities();
        if (!caps.supportsSampleRate(c.sampleRateHz))
            return Status::failure(info_.name + " does not support " + std::to_string(c.sampleRateHz) + " Hz");
        for (const auto& id : c.inputChannels)
            if (!caps.input(id))
                return Status::failure(info_.name + " has no input channel '" + id + "'");
        for (const auto& id : c.outputChannels)
            if (!caps.output(id))
                return Status::failure(info_.name + " has no output channel '" + id + "'");
        if (!caps.formats.empty() && std::find(caps.formats.begin(), caps.formats.end(), c.format) == caps.formats.end())
            return Status::failure(info_.name + " does not deliver " + std::string(toString(c.format)) + " samples");
        if (c.blockFrames == 0 || c.blockFrames > 1u << 20)
            return Status::failure("the block length must lie in 1 ... 1048576 frames");
        std::lock_guard lock(mutex_);
        if (disconnected_)
            return Status::failure(info_.name + " is disconnected");
        config_ = c;
        random_.state = option<uint64_t>(c.options, "seed", 12345) ^ std::hash<std::string>()(info_.id);
        configured_ = true;
        status_ = DeviceStatus::Configured;
        onConfigured();
        return Status::success();
    }

    Result<std::unique_ptr<AcquisitionSession>> startAcquisition(size_t frames) override;

    Result<std::unique_ptr<OutputSession>> startOutput(const OutputSignal& signal, const OutputAuthorization& authorization) override;

    // Called with the lock held, frame by frame in order (also for frames
    // that are then dropped, so the simulated physics keeps running).
    virtual void generate(uint64_t frame, double* real, std::complex<double>* iq) = 0;
    virtual bool complexSamples() const { return false; }
    virtual std::optional<double> hardwareTime(uint64_t) const { return std::nullopt; }
    virtual void onConfigured() {}
    virtual Status acceptOutput(const OutputSignal&) { return Status::success(); }

    // Output sample of output channel k at device frame n (0 when no
    // authorized output is running).
    double outputSample(size_t k, uint64_t n) const
    {
        if (!outputLive_ || n < outputStart_ || k >= output_.channels.size())
            return 0.0;
        const auto& s = output_.channels[k];
        if (s.empty())
            return 0.0;
        uint64_t m = n - outputStart_;
        if (m >= s.size()) {
            if (!output_.repeat)
                return 0.0;
            m %= s.size();
        }
        return s[static_cast<size_t>(m)];
    }

    void stopOutput(const std::string& why)
    {
        bool wasActive = false;
        {
            std::lock_guard lock(mutex_);
            wasActive = outputActive_;
            outputActive_ = outputLive_ = false;
            if (status_ == DeviceStatus::Transmitting)
                status_ = acquiring_ ? DeviceStatus::Acquiring : DeviceStatus::Configured;
        }
        if (wasActive) {
            OutputAuthority::instance().record("output-stopped", info_.id + ": " + why);
            onOutputStopped();
        }
    }
    virtual void onOutputStopped() {}

    // At the start of every block: is the output still authorized?
    void checkOutputAuthorization()
    {
        if (!outputActive_)
            return;
        if (!OutputAuthority::instance().isValid(authorization_)) {
            outputActive_ = outputLive_ = false;
            status_ = acquiring_ ? DeviceStatus::Acquiring : DeviceStatus::Configured;
            OutputAuthority::instance().record("output-stopped", info_.id + ": the authorization ended (released, revoked or "
                                                                            "emergency stop)");
        }
    }

    uint64_t outputFramesWritten() const
    {
        std::lock_guard lock(mutex_);
        if (!outputActive_ || frame_ < outputStart_)
            return outputWritten_;
        return frame_ - outputStart_;
    }

    mutable std::mutex mutex_;
    DeviceInfo info_;
    std::shared_ptr<SimulatedBench> bench_;
    DeviceStatus status_ = DeviceStatus::Connected;
    DeviceConfiguration config_;
    bool configured_ = false;
    bool disconnected_ = false;
    bool acquiring_ = false;
    std::vector<DeviceError> errors_;
    uint64_t frame_ = 0;
    Random random_;
    // output
    OutputSignal output_;
    OutputAuthorization authorization_;
    uint64_t outputStart_ = 0;
    uint64_t outputWritten_ = 0;
    bool outputActive_ = false;
    bool outputLive_ = false;
};

class SimulatedSession final : public AcquisitionSession {
public:
    SimulatedSession(SimulatedDevice& device, size_t frames) : device_(device), total_(frames), started_(std::chrono::steady_clock::now())
    {
        const auto& o = device_.config_.options;
        realTime_ = option(o, "realTime", false);
        dropAfter_ = option<long>(o, "dropAfterBlocks", -1);
        dropFrames_ = option<uint64_t>(o, "dropFrames", 0);
        disconnectAfter_ = option<long>(o, "disconnectAfterBlocks", -1);
        errorAfter_ = option<long>(o, "errorAfterBlocks", -1);
        glitchAfter_ = option<long>(o, "timestampGlitchAfterBlocks", -1);
    }
    ~SimulatedSession() override { stop(); }

    Result<SampleBlock> read(std::chrono::milliseconds) override
    {
        SimulatedDevice& d = device_;
        std::unique_lock lock(d.mutex_);
        if (stopped_)
            return Result<SampleBlock>::failure("the acquisition was stopped");
        if (d.disconnected_)
            return Result<SampleBlock>::failure(d.info_.name + " is disconnected");
        const long block = static_cast<long>(blocks_);
        if (block == disconnectAfter_) {
            d.disconnected_ = true;
            d.status_ = DeviceStatus::Error;
            const bool output = d.outputActive_;
            d.errors_.push_back({"disconnected", "the device disconnected (simulated fault)", output, utcNowIso8601()});
            lock.unlock();
            if (output)
                d.stopOutput("connection lost during output (safety-relevant)");
            return Result<SampleBlock>::failure(d.info_.name + " disconnected (simulated fault)");
        }
        if (block == errorAfter_) {
            d.status_ = DeviceStatus::Error;
            d.errors_.push_back({"acquisition-error", "the acquisition failed (simulated fault)", false, utcNowIso8601()});
            return Result<SampleBlock>::failure(d.info_.name + ": acquisition error (simulated fault)");
        }
        d.checkOutputAuthorization();
        SampleBlock b;
        b.sequence = blocks_;
        const size_t channels = d.config_.inputChannels.size();
        b.channels = channels;
        // Injected overrun: frames pass in the simulated world but are not delivered.
        if (block == dropAfter_ && dropFrames_ > 0) {
            std::vector<double> scratch(channels);
            std::vector<std::complex<double>> scratchIq(channels);
            for (uint64_t i = 0; i < dropFrames_; ++i)
                d.generate(d.frame_++, scratch.data(), scratchIq.data());
            b.droppedBefore = dropFrames_;
        }
        size_t frames = d.config_.blockFrames;
        if (total_ > 0)
            frames = static_cast<size_t>(std::min<uint64_t>(frames, total_ - delivered_));
        b.frames = frames;
        b.firstFrame = d.frame_;
        if (d.complexSamples())
            b.iq.resize(frames * channels);
        else
            b.samples.resize(frames * channels);
        for (size_t f = 0; f < frames; ++f)
            d.generate(d.frame_++, b.samples.empty() ? nullptr : &b.samples[f * channels], b.iq.empty() ? nullptr : &b.iq[f * channels]);
        b.hardwareTimestamp = d.hardwareTime(b.firstFrame);
        if (b.hardwareTimestamp && block == glitchAfter_)
            *b.hardwareTimestamp += 1e-3;  // injected timestamp jump
        b.softwareTimestamp = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count();
        delivered_ += frames;
        ++blocks_;
        b.endOfStream = total_ > 0 && delivered_ >= total_;
        const double fs = d.config_.sampleRateHz;
        lock.unlock();
        if (realTime_)
            std::this_thread::sleep_until(started_ + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                                         std::chrono::duration<double>(static_cast<double>(delivered_) / fs)));
        return b;
    }

    void stop() override
    {
        std::lock_guard lock(device_.mutex_);
        if (stopped_)
            return;
        stopped_ = true;
        device_.acquiring_ = false;
        if (device_.status_ == DeviceStatus::Acquiring)
            device_.status_ = DeviceStatus::Configured;
    }

private:
    SimulatedDevice& device_;
    uint64_t total_;
    uint64_t delivered_ = 0;
    uint64_t blocks_ = 0;
    bool stopped_ = false;
    bool realTime_ = false;
    long dropAfter_ = -1, disconnectAfter_ = -1, errorAfter_ = -1, glitchAfter_ = -1;
    uint64_t dropFrames_ = 0;
    std::chrono::steady_clock::time_point started_;
};

class SimulatedOutputSession final : public OutputSession {
public:
    explicit SimulatedOutputSession(SimulatedDevice& device) : device_(device) {}
    ~SimulatedOutputSession() override { stop(); }
    bool running() const override
    {
        std::lock_guard lock(device_.mutex_);
        return device_.outputActive_;
    }
    void stop() override { device_.stopOutput("stopped by the run"); }
    uint64_t framesWritten() const override { return device_.outputFramesWritten(); }

private:
    SimulatedDevice& device_;
};

Result<std::unique_ptr<AcquisitionSession>> SimulatedDevice::startAcquisition(size_t frames)
{
    using R = Result<std::unique_ptr<AcquisitionSession>>;
    std::lock_guard lock(mutex_);
    if (disconnected_)
        return R::failure(info_.name + " is disconnected");
    if (!configured_)
        return R::failure(info_.name + " is not configured");
    if (config_.inputChannels.empty())
        return R::failure(info_.name + ": no input channel is configured");
    if (acquiring_)
        return R::failure(info_.name + " is already acquiring");
    acquiring_ = true;
    if (status_ != DeviceStatus::Transmitting)
        status_ = DeviceStatus::Acquiring;
    return std::unique_ptr<AcquisitionSession>(std::make_unique<SimulatedSession>(*this, frames));
}

Result<std::unique_ptr<OutputSession>> SimulatedDevice::startOutput(const OutputSignal& signal, const OutputAuthorization& authorization)
{
    using R = Result<std::unique_ptr<OutputSession>>;
    if (!OutputAuthority::instance().isValid(authorization)) {
        OutputAuthority::instance().record("refused", info_.id + ": output without a valid authorization");
        return R::failure(info_.name + ": output refused, no valid authorization (physical output needs an explicit operator "
                          "authorization for this run)");
    }
    if (Status ok = acceptOutput(signal); !ok)
        return R::failure(ok.message());
    std::lock_guard lock(mutex_);
    if (disconnected_)
        return R::failure(info_.name + " is disconnected");
    if (!configured_)
        return R::failure(info_.name + " is not configured");
    if (outputActive_)
        return R::failure(info_.name + " is already outputting");
    output_ = signal;
    authorization_ = authorization;
    outputStart_ = frame_;
    outputActive_ = outputLive_ = true;
    status_ = DeviceStatus::Transmitting;
    return std::unique_ptr<OutputSession>(std::make_unique<SimulatedOutputSession>(*this));
}

// --- sim:audio-8x2 -----------------------------------------------------------------------

class SimulatedAudioInterface final : public SimulatedDevice {
public:
    using SimulatedDevice::SimulatedDevice;
    DeviceCapabilities capabilities() const override
    {
        DeviceCapabilities c;
        for (int i = 0; i < 7; ++i)
            c.inputs.push_back(channel("in" + std::to_string(i), "Microphone input " + std::to_string(i + 1), ChannelDirection::Input,
                                       Quantity::DigitalFullScale, 1.0));
        c.inputs.push_back(channel("in7", "Loopback of output 1 (electrical reference)", ChannelDirection::Input,
                                   Quantity::DigitalFullScale, 1.0));
        c.outputs.push_back(channel("out0", "Output 1 (loudspeaker amplifier)", ChannelDirection::Output, Quantity::DigitalFullScale, 1.0));
        c.outputs.push_back(channel("out1", "Output 2 (second loudspeaker)", ChannelDirection::Output, Quantity::DigitalFullScale, 1.0));
        c.sampleRates = {44100.0, 48000.0, 96000.0, 192000.0};
        c.formats = {SampleFormat::Int16, SampleFormat::Int24, SampleFormat::Float32};
        c.clocks = {{"internal", "internal crystal (simulated)", 0.0, false, 20.0}};
        c.triggers = {{"immediate", "start at once", false}, {"software", "software trigger", false}};
        c.sharedSampleClock = true;
        c.modes = {"duplex"};
        c.reachableStates = {DeviceStatus::Connected, DeviceStatus::Configured, DeviceStatus::Acquiring, DeviceStatus::Transmitting,
                             DeviceStatus::Error, DeviceStatus::Disconnected};
        return c;
    }

    void onConfigured() override
    {
        const auto& o = config_.options;
        const double fs = config_.sampleRateHz;
        speakers_.clear();
        const auto positions = option(o, "speakerPositions", std::vector<std::vector<double>>{{0.0, 0.0, 0.0}, {0.5, 0.0, 0.0}});
        for (size_t k = 0; k < 2; ++k) {
            Speaker s;
            const auto p = k < positions.size() && positions[k].size() == 3 ? positions[k] : std::vector<double>{0.0, 0.0, 0.0};
            s.x = p[0];
            s.y = p[1];
            s.z = p[2];
            s.highPass = highPass(option(o, "speakerHighPassHz", simulated::kSpeakerHighPassHz), 0.7071, fs);
            speakers_.push_back(s);
        }
        auto mics = option(o, "micPositions", std::vector<std::vector<double>>{});
        mics_.clear();
        for (int i = 0; i < 7; ++i) {
            Pose p;
            if (static_cast<size_t>(i) < mics.size() && mics[static_cast<size_t>(i)].size() == 3) {
                p.x = mics[static_cast<size_t>(i)][0];
                p.y = mics[static_cast<size_t>(i)][1];
                p.z = mics[static_cast<size_t>(i)][2];
            } else {
                p.x = -0.3 + 0.1 * i;  // a line 1 m in front of the speaker
                p.y = 1.0;
            }
            mics_.push_back(p);
        }
        gain_ = optionList(o, "gainMismatch", 8, 0.0);
        channelDelay_ = optionList(o, "channelDelaySamples", 8, 0.0);
        noise_ = option(o, "noiseRms", 1e-4);
        micSensitivity_ = option(o, "micFsPerPa", simulated::kMicFsPerPa);
        speakerGain_ = option(o, "speakerPaPerFsAt1m", simulated::kSpeakerPaPerFsAt1m);
        speedOfSound_ = option(o, "speedOfSound", simulated::kSpeedOfSound);
        probeInput_ = option(o, "probeInput", 0);
        // Map configured input ids to indices.
        inputIndex_.clear();
        for (const auto& id : config_.inputChannels)
            inputIndex_.push_back(std::stoi(id.substr(2)));
        histories_.assign(2, History());
        loopback_ = History(64);
    }

    void generate(uint64_t n, double* real, std::complex<double>*) override
    {
        const double fs = config_.sampleRateHz;
        for (size_t k = 0; k < speakers_.size(); ++k) {
            const double drive = outputSample(k, n);
            histories_[k].push(speakers_[k].highPass.process(drive));
            if (k == 0)
                loopback_.push(drive);
        }
        const std::optional<Pose> probe = bench_->probe();
        for (size_t c = 0; c < inputIndex_.size(); ++c) {
            const int i = inputIndex_[c];
            double v = 0.0;
            if (i == 7) {
                v = loopback_.delayed(channelDelay_[7]);
            } else {
                const Pose& mic = (probe && i == probeInput_) ? *probe : mics_[static_cast<size_t>(i)];
                double pressure = 0.0;
                for (size_t k = 0; k < speakers_.size(); ++k) {
                    const double r = std::max(0.01, distance(mic, speakers_[k].x, speakers_[k].y, speakers_[k].z));
                    pressure += speakerGain_ / r * histories_[k].delayed(r / speedOfSound_ * fs + channelDelay_[static_cast<size_t>(i)]);
                }
                v = pressure * micSensitivity_;
            }
            v = v * (1.0 + gain_[static_cast<size_t>(i)]) + noise_ * random_.gaussian();
            real[c] = std::clamp(v, -1.0, 1.0);
        }
    }

private:
    struct Speaker {
        double x = 0, y = 0, z = 0;
        Biquad highPass;
    };
    std::vector<Speaker> speakers_;
    std::vector<Pose> mics_;
    std::vector<double> gain_, channelDelay_;
    std::vector<int> inputIndex_;
    std::vector<History> histories_;
    History loopback_{64};
    double noise_ = 1e-4, micSensitivity_ = simulated::kMicFsPerPa, speakerGain_ = simulated::kSpeakerPaPerFsAt1m;
    double speedOfSound_ = simulated::kSpeedOfSound;
    int probeInput_ = 0;
};

// --- sim:daq-4x1 -------------------------------------------------------------------------

class SimulatedVibrationDaq final : public SimulatedDevice {
public:
    using SimulatedDevice::SimulatedDevice;
    DeviceCapabilities capabilities() const override
    {
        DeviceCapabilities c;
        c.inputs.push_back(channel("ai0", "Input 1 (load cell)", ChannelDirection::Input, Quantity::Voltage, 10.0));
        for (int i = 1; i < 4; ++i)
            c.inputs.push_back(channel("ai" + std::to_string(i), "Input " + std::to_string(i + 1) + " (accelerometer)",
                                       ChannelDirection::Input, Quantity::Voltage, 10.0));
        c.outputs.push_back(channel("ao0", "Output 1 (shaker amplifier)", ChannelDirection::Output, Quantity::Voltage, 10.0));
        c.minSampleRateHz = 1000.0;
        c.maxSampleRateHz = 102400.0;
        c.formats = {SampleFormat::Int24, SampleFormat::Float32};
        c.clocks = {{"internal", "internal oscillator (simulated)", 0.0, false, 25.0}};
        c.triggers = {{"immediate", "start at once", false}, {"external", "external TTL trigger (simulated)", true}};
        c.hardwareTimestamps = true;
        c.interlock = true;
        c.modes = {"IEPE", "voltage"};
        c.reachableStates = {DeviceStatus::Connected, DeviceStatus::Configured, DeviceStatus::Acquiring, DeviceStatus::Transmitting,
                             DeviceStatus::Error, DeviceStatus::Disconnected};
        return c;
    }
    bool interlockClosed() const override { return option(config_.options, "interlockClosed", true); }
    std::optional<double> hardwareTime(uint64_t frame) const override
    {
        return static_cast<double>(frame) / config_.sampleRateHz * (1.0 + driftPpm_ * 1e-6);
    }

    void onConfigured() override
    {
        const auto& o = config_.options;
        const double fs = config_.sampleRateHz;
        // Modes: frequency, damping ratio, modal mass and the mode shape at the
        // drive point and the three accelerometers.
        struct ModeSpec {
            double f, zeta, mass;
            std::vector<double> shape;
        };
        std::vector<ModeSpec> specs;
        for (const auto& m : option(o, "modes", nlohmann::json::array()))
            specs.push_back({m.value("frequencyHz", 100.0), m.value("dampingRatio", 0.02), m.value("modalMass", 1.0),
                             m.value("shape", std::vector<double>{1.0, 1.0, 1.0, 1.0})});
        if (specs.empty())
            specs = {{120.0, 0.02, 0.5, {0.9, 1.0, 0.7, 0.3}}, {410.0, 0.015, 0.4, {0.6, -0.5, 0.8, 1.0}}};
        modes_.clear();
        for (const auto& s : specs) {
            Mode m;
            const double w = 2.0 * pi * s.f;
            for (int j = 0; j < 3; ++j) {
                const double participation = s.shape.at(0) * s.shape.at(static_cast<size_t>(j + 1)) / s.mass;
                // Accelerance a/F = phi_d phi_j s^2 / (m (s^2 + 2 zeta w s + w^2)).
                m.filters[j] = bilinear(participation, 0.0, 0.0, 2.0 * s.zeta * w, w * w, w, fs);
            }
            modes_.push_back(m);
        }
        shakerHighPass_ = highPass(5.0, 0.7071, fs);
        shakerGain_ = option(o, "shakerNewtonPerVolt", simulated::kShakerNewtonPerVolt);
        loadCell_ = option(o, "loadCellVoltPerNewton", simulated::kLoadCellVoltPerNewton);
        accelerometers_ = optionList(o, "accelerometerVoltPerMs2", 3, simulated::kAccelerometerVoltPerMs2);
        noise_ = option(o, "noiseRmsVolt", 2e-4);
        driftPpm_ = option(o, "clockDriftPpm", 0.0);
        inputIndex_.clear();
        for (const auto& id : config_.inputChannels)
            inputIndex_.push_back(std::stoi(id.substr(2)));
    }

    void generate(uint64_t n, double* real, std::complex<double>*) override
    {
        const double force = shakerGain_ * shakerHighPass_.process(outputSample(0, n));
        double accel[3] = {0.0, 0.0, 0.0};
        for (auto& m : modes_)
            for (int j = 0; j < 3; ++j)
                accel[j] += m.filters[j].process(force);
        for (size_t c = 0; c < inputIndex_.size(); ++c) {
            const int i = inputIndex_[c];
            const double v = (i == 0 ? force * loadCell_ : accel[i - 1] * accelerometers_[static_cast<size_t>(i - 1)]) +
                             noise_ * random_.gaussian();
            real[c] = std::clamp(v, -10.0, 10.0);
        }
    }

    Status acceptOutput(const OutputSignal&) override
    {
        if (option(config_.options, "requireInterlock", false) && !interlockClosed())
            return Status::failure(info_.name + ": the hardware interlock is open; output refused");
        return Status::success();
    }

private:
    struct Mode {
        Biquad filters[3];
    };
    std::vector<Mode> modes_;
    Biquad shakerHighPass_;
    double shakerGain_ = simulated::kShakerNewtonPerVolt, loadCell_ = simulated::kLoadCellVoltPerNewton;
    std::vector<double> accelerometers_;
    double noise_ = 2e-4, driftPpm_ = 0.0;
    std::vector<int> inputIndex_;
};

// --- sim:rf-rx-4 -------------------------------------------------------------------------

class SimulatedRfReceiver final : public SimulatedDevice {
public:
    using SimulatedDevice::SimulatedDevice;
    bool complexSamples() const override { return true; }
    DeviceCapabilities capabilities() const override
    {
        DeviceCapabilities c;
        for (int i = 0; i < 4; ++i)
            c.inputs.push_back(channel("rx" + std::to_string(i), "Receiver " + std::to_string(i + 1), ChannelDirection::Input,
                                       Quantity::DigitalFullScale, 1.0, true));
        c.sampleRates = {1e6, 2e6, 5e6, 10e6, 20e6};
        c.formats = {SampleFormat::ComplexInt16, SampleFormat::ComplexFloat32};
        c.minTuneHz = 70e6;
        c.maxTuneHz = 6e9;
        c.clocks = {{"internal", "internal TCXO (simulated)", 10e6, false, 2.0},
                    {"external-10MHz", "external 10 MHz reference (simulated)", 10e6, true, 0.0}};
        c.triggers = {{"immediate", "start at once", false}, {"pps", "PPS input (simulated)", true}};
        c.hardwareTimestamps = true;
        c.sharedSampleClock = true;
        c.modes = {"coherent", "independent-lo"};
        c.reachableStates = {DeviceStatus::Connected, DeviceStatus::Configured, DeviceStatus::Acquiring, DeviceStatus::Error,
                             DeviceStatus::Disconnected};
        return c;
    }
    std::optional<double> hardwareTime(uint64_t frame) const override { return static_cast<double>(frame) / config_.sampleRateHz; }

    void onConfigured() override
    {
        const auto& o = config_.options;
        phases_ = optionList(o, "channelPhaseDeg", 4, 0.0);
        if (!o.contains("channelPhaseDeg"))
            phases_ = {0.0, 37.0, -52.0, 101.0};
        gainsDb_ = optionList(o, "channelGainDb", 4, 0.0);
        if (!o.contains("channelGainDb"))
            gainsDb_ = {0.0, -0.4, 0.3, -0.8};
        coherent_ = option(o, "coherent", true);
        noise_ = option(o, "noiseRms", 1e-3);
        pathGainDb_ = option(o, "pathGainDb", simulated::kRfPathGainDb);
        const double lambda = 299792458.0 / std::max(1.0, config_.centerFrequencyHz);
        spacing_ = option(o, "elementSpacing", lambda / 2.0);
        azimuth_ = option(o, "sourceAzimuthDeg", 0.0);
        autPattern_ = option(o, "autPattern", false);
        loPhase_.assign(4, 0.0);
        inputIndex_.clear();
        for (const auto& id : config_.inputChannels)
            inputIndex_.push_back(std::stoi(id.substr(2)));
    }

    void generate(uint64_t n, double*, std::complex<double>* iq) override
    {
        if (n % 1024 == 0)
            carriers_ = bench_->carriers();
        const double fs = config_.sampleRateHz;
        const double t = static_cast<double>(n) / fs;
        const double k = 2.0 * pi * config_.centerFrequencyHz / 299792458.0;
        double pattern = 1.0;
        if (autPattern_) {
            if (auto probe = bench_->probe()) {
                const double angle = std::atan2(probe->y, probe->x);  // the antenna under test faces +x
                const double c = std::cos(angle);
                pattern = std::max(c > 0.0 ? std::pow(c, 1.5) : 0.0, std::pow(10.0, -25.0 / 20.0));
            }
        }
        for (size_t c = 0; c < inputIndex_.size(); ++c) {
            const int i = inputIndex_[c];
            if (!coherent_)
                loPhase_[static_cast<size_t>(i)] += 0.01 * random_.gaussian();  // independent LOs: phase random walk
            std::complex<double> v(0.0, 0.0);
            for (const auto& carrier : carriers_) {
                const double offset = carrier.frequencyHz - config_.centerFrequencyHz;
                if (std::abs(offset) > fs / 2.0)
                    continue;
                const double powerDbm = carrier.powerDbm + pathGainDb_ + config_.gainDb - simulated::kRfFullScaleDbm;
                const double amplitude = std::pow(10.0, powerDbm / 20.0) * pattern * std::pow(10.0, gainsDb_[static_cast<size_t>(i)] / 20.0);
                const double geometric = -k * spacing_ * i * std::sin(units::degreesToRadians(azimuth_));
                const double phase = 2.0 * pi * offset * t + units::degreesToRadians(phases_[static_cast<size_t>(i)]) + geometric +
                                     loPhase_[static_cast<size_t>(i)];
                v += std::polar(amplitude, phase);
            }
            v += noise_ / std::sqrt(2.0) * std::complex<double>(random_.gaussian(), random_.gaussian());
            iq[c] = {std::clamp(v.real(), -1.0, 1.0), std::clamp(v.imag(), -1.0, 1.0)};
        }
    }

private:
    std::vector<double> phases_, gainsDb_, loPhase_;
    std::vector<int> inputIndex_;
    std::vector<SimulatedBench::Carrier> carriers_;
    bool coherent_ = true, autPattern_ = false;
    double noise_ = 1e-3, pathGainDb_ = simulated::kRfPathGainDb, spacing_ = 0.15, azimuth_ = 0.0;
};

// --- sim:siggen --------------------------------------------------------------------------

class SimulatedSignalGenerator final : public SimulatedDevice {
public:
    using SimulatedDevice::SimulatedDevice;
    DeviceCapabilities capabilities() const override
    {
        DeviceCapabilities c;
        c.outputs.push_back(channel("rf0", "RF output", ChannelDirection::Output, Quantity::Power, 0.1));
        c.minSampleRateHz = 1.0;
        c.maxSampleRateHz = 1e9;
        c.minTuneHz = 9e3;
        c.maxTuneHz = 6e9;
        c.minOutputPowerDbm = -120.0;
        c.maxOutputPowerDbm = 20.0;
        c.clocks = {{"internal", "internal OCXO (simulated)", 10e6, false, 0.05}};
        c.modes = {"cw"};
        c.reachableStates = {DeviceStatus::Connected, DeviceStatus::Configured, DeviceStatus::Transmitting, DeviceStatus::Error,
                             DeviceStatus::Disconnected};
        return c;
    }
    void generate(uint64_t, double*, std::complex<double>*) override {}
    Status acceptOutput(const OutputSignal& s) override
    {
        const auto c = capabilities();
        if (!(s.carrierHz >= c.minTuneHz && s.carrierHz <= c.maxTuneHz))
            return Status::failure(info_.name + ": carrier " + std::to_string(s.carrierHz) + " Hz outside 9 kHz ... 6 GHz");
        if (!(s.powerDbm >= c.minOutputPowerDbm && s.powerDbm <= c.maxOutputPowerDbm))
            return Status::failure(info_.name + ": power " + std::to_string(s.powerDbm) + " dBm outside -120 ... +20 dBm");
        return Status::success();
    }
    Result<std::unique_ptr<OutputSession>> startOutput(const OutputSignal& signal, const OutputAuthorization& authorization) override
    {
        auto session = SimulatedDevice::startOutput(signal, authorization);
        if (session)
            bench_->setCarrier(info_.id, SimulatedBench::Carrier{signal.carrierHz, signal.powerDbm}, authorization);
        return session;
    }
    void onOutputStopped() override { bench_->setCarrier(info_.id, std::nullopt, {}); }
};

// --- sim:vna-2 ---------------------------------------------------------------------------

class SimulatedVna final : public SimulatedDevice, public FrequencySweepInstrument {
public:
    using SimulatedDevice::SimulatedDevice;
    DeviceCapabilities capabilities() const override
    {
        DeviceCapabilities c;
        c.outputs.push_back(channel("port1", "Test port 1", ChannelDirection::Output, Quantity::Power, 0.01));
        c.outputs.push_back(channel("port2", "Test port 2", ChannelDirection::Output, Quantity::Power, 0.01));
        c.minSampleRateHz = 1.0;
        c.maxSampleRateHz = 1e6;
        c.minTuneHz = 100e3;
        c.maxTuneHz = 6e9;
        c.minOutputPowerDbm = -40.0;
        c.maxOutputPowerDbm = 0.0;
        c.frequencySweep = true;
        c.modes = {"s-parameters"};
        c.reachableStates = {DeviceStatus::Connected, DeviceStatus::Configured, DeviceStatus::Transmitting, DeviceStatus::Error,
                             DeviceStatus::Disconnected};
        return c;
    }
    void generate(uint64_t, double*, std::complex<double>*) override {}
    FrequencySweepInstrument* sweepInstrument() override { return this; }

    Result<NetworkData> sweep(const SweepSettings& s, const OutputAuthorization& authorization) override
    {
        using R = Result<NetworkData>;
        if (!OutputAuthority::instance().isValid(authorization))
            return R::failure(info_.name + ": a sweep drives the test ports; it needs a valid output authorization");
        const auto caps = capabilities();
        if (!(s.startHz >= caps.minTuneHz && s.stopHz <= caps.maxTuneHz && s.stopHz > s.startHz) || s.points < 2)
            return R::failure(info_.name + ": the sweep must lie within 100 kHz ... 6 GHz with at least two points");
        if (s.powerDbm > caps.maxOutputPowerDbm || s.powerDbm < caps.minOutputPowerDbm)
            return R::failure(info_.name + ": the port power must lie within -40 ... 0 dBm");
        std::lock_guard lock(mutex_);
        if (disconnected_)
            return R::failure(info_.name + " is disconnected");
        const auto& o = config_.options;
        // DUT on port 1: series RLC (a resonant antenna near f0); port 1 -> 2: a lossy matched line.
        const double f0 = option(o, "dutResonanceHz", 1e9), q = option(o, "dutQ", 10.0), resistance = option(o, "dutResistance", 73.0);
        const double inductance = q * resistance / (2.0 * pi * f0);
        const double capacitance = 1.0 / ((2.0 * pi * f0) * (2.0 * pi * f0) * inductance);
        const double lineLossDb = option(o, "lineLossDb", 1.5), lineDelay = option(o, "lineDelaySeconds", 2e-9);
        const double z0 = 50.0;
        const double traceNoise = option(o, "traceNoise", 1e-3);
        NetworkData d;
        d.ports = static_cast<int>(std::max<size_t>(1, s.ports.size()));
        d.referenceImpedance = z0;
        d.note = "simulated DUT: series RLC (R " + std::to_string(resistance) + " ohm, f0 " + std::to_string(f0) + " Hz, Q " +
                 std::to_string(q) + ") on port 1; matched line with " + std::to_string(lineLossDb) + " dB loss between ports";
        status_ = DeviceStatus::Transmitting;
        for (int p = 0; p < s.points; ++p) {
            const double f = s.startHz + (s.stopHz - s.startHz) * p / (s.points - 1);
            const double w = 2.0 * pi * f;
            const std::complex<double> z(resistance, w * inductance - 1.0 / (w * capacitance));
            const std::complex<double> s11 = (z - z0) / (z + z0);
            const std::complex<double> s21 = std::pow(10.0, -lineLossDb / 20.0) * std::polar(1.0, -w * lineDelay);
            auto noisy = [&](std::complex<double> v) {
                return v + traceNoise * std::complex<double>(random_.gaussian(), random_.gaussian());
            };
            if (d.ports == 1)
                d.s.push_back({noisy(s11)});
            else
                d.s.push_back({noisy(s11), noisy(s21), noisy(s21), noisy(std::complex<double>(0.0, 0.0))});
            d.frequencyHz.push_back(f);
        }
        status_ = DeviceStatus::Configured;
        return d;
    }
};

// --- sim:positioner ----------------------------------------------------------------------

class SimulatedPositioner final : public SimulatedDevice, public Positioner {
public:
    using SimulatedDevice::SimulatedDevice;
    DeviceCapabilities capabilities() const override
    {
        DeviceCapabilities c;
        c.minSampleRateHz = 1.0;
        c.maxSampleRateHz = 1e9;
        c.positioner = true;
        c.modes = {"cartesian"};
        c.reachableStates = {DeviceStatus::Connected, DeviceStatus::Configured, DeviceStatus::Disconnected, DeviceStatus::Error};
        return c;
    }
    void generate(uint64_t, double*, std::complex<double>*) override {}
    Positioner* positioner() override { return this; }
    Status moveTo(const Pose& pose) override
    {
        const double travel = option(config_.options, "travel", 5.0);
        if (std::abs(pose.x) > travel || std::abs(pose.y) > travel || std::abs(pose.z) > travel)
            return Status::failure(info_.name + ": position outside the +-" + std::to_string(travel) + " m travel");
        std::lock_guard lock(mutex_);
        if (disconnected_)
            return Status::failure(info_.name + " is disconnected");
        current_ = pose;
        bench_->setProbe(pose);
        return Status::success();
    }
    Pose current() const override
    {
        std::lock_guard lock(mutex_);
        return current_;
    }

private:
    Pose current_;
};

DeviceInfo simulatedInfo(const std::string& id, const std::string& name, const std::string& model)
{
    DeviceInfo i;
    i.id = id;
    i.driver = "simulated";
    i.name = name;
    i.vendor = "Resonant (simulated)";
    i.model = model;
    i.serial = id.substr(4);
    i.connection = ConnectionType::Simulated;
    i.address = "simulated";
    i.driverVersion = "1.0";
    i.simulated = true;
    return i;
}

}  // namespace

void SimulatedBench::setCarrier(const std::string& deviceId, std::optional<Carrier> carrier, const OutputAuthorization& authorization)
{
    std::lock_guard lock(mutex_);
    if (carrier)
        carriers_[deviceId] = {*carrier, authorization};
    else
        carriers_.erase(deviceId);
}

std::vector<SimulatedBench::Carrier> SimulatedBench::carriers() const
{
    std::lock_guard lock(mutex_);
    std::vector<Carrier> out;
    for (auto it = carriers_.begin(); it != carriers_.end();) {
        if (!OutputAuthority::instance().isValid(it->second.second)) {
            it = carriers_.erase(it);  // the authorization ended: the carrier is off
            continue;
        }
        out.push_back(it->second.first);
        ++it;
    }
    return out;
}

void SimulatedBench::setProbe(const Pose& pose)
{
    std::lock_guard lock(mutex_);
    probe_ = pose;
}

std::optional<Pose> SimulatedBench::probe() const
{
    std::lock_guard lock(mutex_);
    return probe_;
}

SimulatedDriver::SimulatedDriver() : bench_(std::make_shared<SimulatedBench>()) {}

DriverAvailability SimulatedDriver::availability() const
{
    return {true, "built in; generates data from synthetic models (no physical measurement)", "part of Resonant"};
}

Result<std::vector<DeviceInfo>> SimulatedDriver::discover()
{
    return std::vector<DeviceInfo>{
        simulatedInfo("sim:audio-8x2", "Simulated audio interface (8 in, 2 out)", "AUDIO-8x2"),
        simulatedInfo("sim:daq-4x1", "Simulated vibration DAQ (4 in, 1 out)", "DAQ-4x1"),
        simulatedInfo("sim:rf-rx-4", "Simulated 4-channel RF receiver", "RFRX-4"),
        simulatedInfo("sim:siggen", "Simulated RF signal generator", "SG-6G"),
        simulatedInfo("sim:vna-2", "Simulated 2-port vector network analyser", "VNA-2P"),
        simulatedInfo("sim:positioner", "Simulated probe positioner", "POS-3"),
    };
}

Result<std::unique_ptr<HardwareDevice>> SimulatedDriver::open(const DeviceInfo& info)
{
    using R = Result<std::unique_ptr<HardwareDevice>>;
    std::unique_ptr<HardwareDevice> device;
    if (info.id == "sim:audio-8x2")
        device = std::make_unique<SimulatedAudioInterface>(info, bench_);
    else if (info.id == "sim:daq-4x1")
        device = std::make_unique<SimulatedVibrationDaq>(info, bench_);
    else if (info.id == "sim:rf-rx-4")
        device = std::make_unique<SimulatedRfReceiver>(info, bench_);
    else if (info.id == "sim:siggen")
        device = std::make_unique<SimulatedSignalGenerator>(info, bench_);
    else if (info.id == "sim:vna-2")
        device = std::make_unique<SimulatedVna>(info, bench_);
    else if (info.id == "sim:positioner")
        device = std::make_unique<SimulatedPositioner>(info, bench_);
    else
        return R::failure("unknown simulated device '" + info.id + "'");
    return device;
}

}  // namespace resonant::experiments
