#include "AlsaDriver.h"

#include "foundation/Time.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>

#ifdef RESONANT_HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

namespace resonant::hardware {

bool alsaBuilt()
{
#ifdef RESONANT_HAVE_ALSA
    return true;
#else
    return false;
#endif
}

#ifndef RESONANT_HAVE_ALSA

std::shared_ptr<experiments::DeviceDriver> makeAlsaDriver() { return nullptr; }

#else

namespace {

using namespace experiments;

constexpr unsigned kMaxChannels = 32;
constexpr unsigned kRates[] = {8000, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000};

struct Probe {
    bool ok = false;
    std::string error;
    unsigned maxChannels = 0;
    std::vector<double> rates;
    std::vector<SampleFormat> formats;
};

snd_pcm_format_t alsaFormat(SampleFormat f)
{
    switch (f) {
    case SampleFormat::Int16: return SND_PCM_FORMAT_S16_LE;
    case SampleFormat::Int32: return SND_PCM_FORMAT_S32_LE;
    case SampleFormat::Float32: return SND_PCM_FORMAT_FLOAT_LE;
    default: return SND_PCM_FORMAT_UNKNOWN;
    }
}

size_t bytesOf(SampleFormat f) { return f == SampleFormat::Int16 ? 2 : 4; }

Probe probe(const std::string& name, snd_pcm_stream_t stream)
{
    Probe p;
    snd_pcm_t* pcm = nullptr;
    int err = snd_pcm_open(&pcm, name.c_str(), stream, SND_PCM_NONBLOCK);
    if (err < 0) {
        p.error = snd_strerror(err);
        return p;
    }
    snd_pcm_hw_params_t* hw = nullptr;
    snd_pcm_hw_params_alloca(&hw);
    if ((err = snd_pcm_hw_params_any(pcm, hw)) < 0) {
        p.error = snd_strerror(err);
        snd_pcm_close(pcm);
        return p;
    }
    unsigned channels = 0;
    snd_pcm_hw_params_get_channels_max(hw, &channels);
    p.maxChannels = std::min(channels, kMaxChannels);
    for (unsigned rate : kRates)
        if (snd_pcm_hw_params_test_rate(pcm, hw, rate, 0) == 0)
            p.rates.push_back(rate);
    for (SampleFormat f : {SampleFormat::Int16, SampleFormat::Int32, SampleFormat::Float32})
        if (snd_pcm_hw_params_test_format(pcm, hw, alsaFormat(f)) == 0)
            p.formats.push_back(f);
    snd_pcm_close(pcm);
    p.ok = p.maxChannels > 0 && !p.rates.empty() && !p.formats.empty();
    if (!p.ok)
        p.error = "the device reports no usable channel count, rate or sample format";
    return p;
}

// Opens a PCM and sets interleaved hardware parameters.
Result<snd_pcm_t*> openPcm(const std::string& name, snd_pcm_stream_t stream, const DeviceConfiguration& c, unsigned channels,
                           snd_pcm_uframes_t& period)
{
    using R = Result<snd_pcm_t*>;
    snd_pcm_t* pcm = nullptr;
    int err = snd_pcm_open(&pcm, name.c_str(), stream, 0);
    if (err < 0)
        return R::failure("ALSA: cannot open '" + name + "': " + snd_strerror(err));
    snd_pcm_hw_params_t* hw = nullptr;
    snd_pcm_hw_params_alloca(&hw);
    auto fail = [&](const char* what) {
        const std::string message = std::string("ALSA '") + name + "': " + what + ": " + snd_strerror(err);
        snd_pcm_close(pcm);
        return R::failure(message);
    };
    if ((err = snd_pcm_hw_params_any(pcm, hw)) < 0)
        return fail("no hardware parameters");
    if ((err = snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        return fail("interleaved access refused");
    if ((err = snd_pcm_hw_params_set_format(pcm, hw, alsaFormat(c.format))) < 0)
        return fail("sample format refused");
    if ((err = snd_pcm_hw_params_set_channels(pcm, hw, channels)) < 0)
        return fail("channel count refused");
    if ((err = snd_pcm_hw_params_set_rate(pcm, hw, static_cast<unsigned>(std::lround(c.sampleRateHz)), 0)) < 0)
        return fail("sample rate refused (no resampling is done: the rate must be exact)");
    period = c.blockFrames;
    if ((err = snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, nullptr)) < 0)
        return fail("period size refused");
    snd_pcm_uframes_t buffer = period * 8;
    if ((err = snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &buffer)) < 0)
        return fail("buffer size refused");
    if ((err = snd_pcm_hw_params(pcm, hw)) < 0)
        return fail("hardware parameters not accepted");
    if ((err = snd_pcm_prepare(pcm)) < 0)
        return fail("prepare failed");
    return pcm;
}

class AlsaDevice;

class AlsaCapture final : public AcquisitionSession {
public:
    AlsaCapture(AlsaDevice& device, snd_pcm_t* pcm, std::vector<unsigned> channels, unsigned deviceChannels, size_t frames,
                snd_pcm_uframes_t period, SampleFormat format, double rate)
        : device_(device), pcm_(pcm), channels_(std::move(channels)), deviceChannels_(deviceChannels), total_(frames),
          period_(period), format_(format), rate_(rate), started_(std::chrono::steady_clock::now()),
          buffer_(period * deviceChannels * bytesOf(format))
    {
        snd_pcm_start(pcm_);
    }
    ~AlsaCapture() override { stop(); }

    Result<SampleBlock> read(std::chrono::milliseconds timeout) override;

    void stop() override;

private:
    AlsaDevice& device_;
    snd_pcm_t* pcm_;
    std::vector<unsigned> channels_;
    unsigned deviceChannels_;
    uint64_t total_;
    uint64_t delivered_ = 0, frame_ = 0, sequence_ = 0;
    snd_pcm_uframes_t period_;
    SampleFormat format_;
    double rate_;
    std::chrono::steady_clock::time_point started_;
    std::vector<unsigned char> buffer_;
    std::mutex mutex_;
};

class AlsaPlayback final : public OutputSession {
public:
    AlsaPlayback(snd_pcm_t* pcm, OutputSignal signal, std::vector<unsigned> channels, unsigned deviceChannels,
                 snd_pcm_uframes_t period, SampleFormat format, OutputAuthorization authorization, std::string deviceId)
        : pcm_(pcm), signal_(std::move(signal)), channels_(std::move(channels)), deviceChannels_(deviceChannels), period_(period),
          format_(format), authorization_(std::move(authorization)), deviceId_(std::move(deviceId))
    {
        running_.store(true);
        thread_ = std::thread([this] { loop(); });
    }
    ~AlsaPlayback() override { stop(); }
    bool running() const override { return running_.load(); }
    void stop() override
    {
        stop_.store(true);
        if (thread_.joinable())
            thread_.join();
        if (pcm_) {
            snd_pcm_drop(pcm_);
            snd_pcm_close(pcm_);
            pcm_ = nullptr;
        }
    }
    uint64_t framesWritten() const override { return written_.load(); }

private:
    void loop()
    {
        size_t length = 0;
        for (const auto& c : signal_.channels)
            length = std::max(length, c.size());
        std::vector<unsigned char> buffer(period_ * deviceChannels_ * bytesOf(format_));
        uint64_t position = 0;
        while (!stop_.load()) {
            if (!OutputAuthority::instance().isValid(authorization_)) {
                OutputAuthority::instance().record("output-stopped", deviceId_ + ": the authorization ended; playback stopped");
                break;
            }
            if (length == 0 || (!signal_.repeat && position >= length))
                break;
            std::fill(buffer.begin(), buffer.end(), 0);
            for (snd_pcm_uframes_t f = 0; f < period_; ++f) {
                uint64_t m = position + f;
                if (signal_.repeat)
                    m %= length;
                for (size_t k = 0; k < channels_.size() && k < signal_.channels.size(); ++k) {
                    const auto& s = signal_.channels[k];
                    const double v = m < s.size() ? std::clamp(s[m], -1.0, 1.0) : 0.0;
                    const size_t at = (f * deviceChannels_ + channels_[k]) * bytesOf(format_);
                    if (format_ == SampleFormat::Int16) {
                        const int16_t x = static_cast<int16_t>(std::lround(v * 32767.0));
                        std::memcpy(&buffer[at], &x, 2);
                    } else if (format_ == SampleFormat::Int32) {
                        const int32_t x = static_cast<int32_t>(std::llround(v * 2147483647.0));
                        std::memcpy(&buffer[at], &x, 4);
                    } else {
                        const float x = static_cast<float>(v);
                        std::memcpy(&buffer[at], &x, 4);
                    }
                }
            }
            snd_pcm_sframes_t n = snd_pcm_writei(pcm_, buffer.data(), period_);
            if (n == -EPIPE) {
                snd_pcm_prepare(pcm_);  // underrun: the output paused; carry on
                continue;
            }
            if (n < 0) {
                OutputAuthority::instance().record("device-error", deviceId_ + ": playback failed: " + snd_strerror(static_cast<int>(n)));
                break;
            }
            position += static_cast<uint64_t>(n);
            written_.store(position);
        }
        if (!stop_.load() && pcm_)
            snd_pcm_drain(pcm_);
        running_.store(false);
    }

    snd_pcm_t* pcm_;
    OutputSignal signal_;
    std::vector<unsigned> channels_;
    unsigned deviceChannels_;
    snd_pcm_uframes_t period_;
    SampleFormat format_;
    OutputAuthorization authorization_;
    std::string deviceId_;
    std::atomic<bool> running_{false}, stop_{false};
    std::atomic<uint64_t> written_{0};
    std::thread thread_;
};

class AlsaDevice final : public HardwareDevice {
public:
    AlsaDevice(DeviceInfo info, bool capture, bool playback) : info_(std::move(info))
    {
        if (capture)
            capture_ = probe(info_.address, SND_PCM_STREAM_CAPTURE);
        if (playback)
            playback_ = probe(info_.address, SND_PCM_STREAM_PLAYBACK);
    }
    const DeviceInfo& info() const override { return info_; }
    DeviceCapabilities capabilities() const override
    {
        DeviceCapabilities c;
        for (unsigned i = 0; capture_.ok && i < capture_.maxChannels; ++i)
            c.inputs.push_back({"in" + std::to_string(i), "Capture channel " + std::to_string(i + 1), ChannelDirection::Input,
                                Quantity::DigitalFullScale, -1.0, 1.0, false});
        for (unsigned i = 0; playback_.ok && i < playback_.maxChannels; ++i)
            c.outputs.push_back({"out" + std::to_string(i), "Playback channel " + std::to_string(i + 1), ChannelDirection::Output,
                                 Quantity::DigitalFullScale, -1.0, 1.0, false});
        c.sampleRates = capture_.ok ? capture_.rates : playback_.rates;
        c.formats = capture_.ok ? capture_.formats : playback_.formats;
        c.clocks = {{"internal", "the interface's sample clock", 0.0, false, 0.0}};
        c.triggers = {{"immediate", "start at once", false}};
        c.sharedSampleClock = true;  // the channels of one PCM are sampled together
        c.modes = {"interleaved"};
        c.reachableStates = {DeviceStatus::Connected, DeviceStatus::Configured, DeviceStatus::Acquiring, DeviceStatus::Transmitting,
                             DeviceStatus::Error, DeviceStatus::Disconnected};
        return c;
    }
    DeviceStatus status() const override
    {
        std::lock_guard lock(mutex_);
        return status_;
    }
    Status configure(const DeviceConfiguration& c) override
    {
        const auto caps = capabilities();
        if (caps.inputs.empty() && caps.outputs.empty())
            return Status::failure("ALSA '" + info_.address + "' could not be opened: " +
                                   (capture_.error.empty() ? playback_.error : capture_.error));
        if (!caps.supportsSampleRate(c.sampleRateHz))
            return Status::failure("ALSA '" + info_.address + "' does not run at " + std::to_string(c.sampleRateHz) + " Hz");
        if (std::find(caps.formats.begin(), caps.formats.end(), c.format) == caps.formats.end())
            return Status::failure("ALSA '" + info_.address + "' does not take " + std::string(toString(c.format)) + " samples");
        for (const auto& id : c.inputChannels)
            if (!caps.input(id))
                return Status::failure("ALSA '" + info_.address + "' has no capture channel '" + id + "'");
        for (const auto& id : c.outputChannels)
            if (!caps.output(id))
                return Status::failure("ALSA '" + info_.address + "' has no playback channel '" + id + "'");
        std::lock_guard lock(mutex_);
        config_ = c;
        configured_ = true;
        status_ = DeviceStatus::Configured;
        return Status::success();
    }
    const DeviceConfiguration& configuration() const override { return config_; }

    Result<std::unique_ptr<AcquisitionSession>> startAcquisition(size_t frames) override
    {
        using R = Result<std::unique_ptr<AcquisitionSession>>;
        if (!configured_ || config_.inputChannels.empty())
            return R::failure("ALSA '" + info_.address + "' is not configured for capture");
        std::vector<unsigned> channels;
        unsigned count = 0;
        for (const auto& id : config_.inputChannels) {
            channels.push_back(static_cast<unsigned>(std::stoul(id.substr(2))));
            count = std::max(count, channels.back() + 1);
        }
        snd_pcm_uframes_t period = 0;
        auto pcm = openPcm(info_.address, SND_PCM_STREAM_CAPTURE, config_, count, period);
        if (!pcm) {
            std::lock_guard lock(mutex_);
            errors_.push_back({"open", pcm.message(), false, utcNowIso8601()});
            return R::failure(pcm.message());
        }
        {
            std::lock_guard lock(mutex_);
            status_ = DeviceStatus::Acquiring;
        }
        return std::unique_ptr<AcquisitionSession>(
            std::make_unique<AlsaCapture>(*this, pcm.value(), channels, count, frames, period, config_.format, config_.sampleRateHz));
    }

    Result<std::unique_ptr<OutputSession>> startOutput(const OutputSignal& signal, const OutputAuthorization& authorization) override
    {
        using R = Result<std::unique_ptr<OutputSession>>;
        if (!OutputAuthority::instance().isValid(authorization)) {
            OutputAuthority::instance().record("refused", info_.id + ": playback without a valid authorization");
            return R::failure("ALSA '" + info_.address + "': playback refused, no valid output authorization");
        }
        if (!configured_ || config_.outputChannels.empty())
            return R::failure("ALSA '" + info_.address + "' is not configured for playback");
        std::vector<unsigned> channels;
        unsigned count = 0;
        for (const auto& id : config_.outputChannels) {
            channels.push_back(static_cast<unsigned>(std::stoul(id.substr(3))));
            count = std::max(count, channels.back() + 1);
        }
        snd_pcm_uframes_t period = 0;
        auto pcm = openPcm(info_.address, SND_PCM_STREAM_PLAYBACK, config_, count, period);
        if (!pcm)
            return R::failure(pcm.message());
        {
            std::lock_guard lock(mutex_);
            status_ = DeviceStatus::Transmitting;
        }
        return std::unique_ptr<OutputSession>(
            std::make_unique<AlsaPlayback>(pcm.value(), signal, channels, count, period, config_.format, authorization, info_.id));
    }

    void disconnect() override
    {
        std::lock_guard lock(mutex_);
        status_ = DeviceStatus::Disconnected;
    }
    std::vector<DeviceError> errors() const override
    {
        std::lock_guard lock(mutex_);
        return errors_;
    }
    void addError(DeviceError e)
    {
        std::lock_guard lock(mutex_);
        errors_.push_back(std::move(e));
        status_ = DeviceStatus::Error;
    }
    void idle()
    {
        std::lock_guard lock(mutex_);
        if (status_ == DeviceStatus::Acquiring)
            status_ = DeviceStatus::Configured;
    }

private:
    DeviceInfo info_;
    Probe capture_, playback_;
    mutable std::mutex mutex_;
    DeviceStatus status_ = DeviceStatus::Connected;
    DeviceConfiguration config_;
    bool configured_ = false;
    std::vector<DeviceError> errors_;
};

Result<SampleBlock> AlsaCapture::read(std::chrono::milliseconds timeout)
{
    std::lock_guard lock(mutex_);
    if (!pcm_)
        return Result<SampleBlock>::failure("the capture was stopped");
    SampleBlock b;
    b.sequence = sequence_++;
    b.channels = channels_.size();
    snd_pcm_uframes_t want = period_;
    if (total_ > 0)
        want = static_cast<snd_pcm_uframes_t>(std::min<uint64_t>(want, total_ - delivered_));
    for (int attempt = 0; attempt < 3; ++attempt) {
        const int ready = snd_pcm_wait(pcm_, static_cast<int>(timeout.count()));
        if (ready == 0)
            return Result<SampleBlock>::failure("ALSA: no samples within " + std::to_string(timeout.count()) + " ms (the device stalled)");
        snd_pcm_sframes_t n = snd_pcm_readi(pcm_, buffer_.data(), want);
        if (n == -EPIPE) {
            // Overrun: ALSA lost frames and does not say how many. Estimate them from the elapsed time.
            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count();
            const double expected = elapsed * rate_;
            const uint64_t lost = expected > static_cast<double>(frame_) ? static_cast<uint64_t>(expected - static_cast<double>(frame_)) : 0;
            b.droppedBefore += std::max<uint64_t>(lost, 1);
            frame_ += std::max<uint64_t>(lost, 1);
            device_.addError({"overrun", "capture overrun: about " + std::to_string(lost) + " frame(s) lost (estimated from the "
                                         "elapsed time)", false, utcNowIso8601()});
            snd_pcm_prepare(pcm_);
            snd_pcm_start(pcm_);
            continue;
        }
        if (n < 0)
            return Result<SampleBlock>::failure(std::string("ALSA capture failed: ") + snd_strerror(static_cast<int>(n)));
        b.frames = static_cast<size_t>(n);
        break;
    }
    b.firstFrame = frame_;
    b.samples.resize(b.frames * b.channels);
    const size_t width = bytesOf(format_);
    for (size_t f = 0; f < b.frames; ++f)
        for (size_t c = 0; c < channels_.size(); ++c) {
            const unsigned char* p = &buffer_[(f * deviceChannels_ + channels_[c]) * width];
            double v = 0.0;
            if (format_ == SampleFormat::Int16) {
                int16_t x;
                std::memcpy(&x, p, 2);
                v = x / 32768.0;
            } else if (format_ == SampleFormat::Int32) {
                int32_t x;
                std::memcpy(&x, p, 4);
                v = x / 2147483648.0;
            } else {
                float x;
                std::memcpy(&x, p, 4);
                v = x;
            }
            b.samples[f * channels_.size() + c] = v;
        }
    frame_ += b.frames;
    delivered_ += b.frames;
    b.softwareTimestamp = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count();
    b.endOfStream = total_ > 0 && delivered_ >= total_;
    return b;
}

void AlsaCapture::stop()
{
    std::lock_guard lock(mutex_);
    if (!pcm_)
        return;
    snd_pcm_drop(pcm_);
    snd_pcm_close(pcm_);
    pcm_ = nullptr;
    device_.idle();
}

class AlsaDriver final : public DeviceDriver {
public:
    std::string name() const override { return "alsa"; }
    std::string version() const override { return std::string("ALSA ") + snd_asoundlib_version(); }
    ConnectionType connection() const override { return ConnectionType::SystemAudio; }
    DriverAvailability availability() const override
    {
        return {true, std::string("libasound ") + snd_asoundlib_version(), "LGPL-2.1 (dynamically linked)"};
    }
    Result<std::vector<DeviceInfo>> discover() override
    {
        void** hints = nullptr;
        if (snd_device_name_hint(-1, "pcm", &hints) < 0)
            return Result<std::vector<DeviceInfo>>::failure("ALSA lists no PCM devices");
        std::vector<DeviceInfo> out;
        for (void** h = hints; *h; ++h) {
            char* name = snd_device_name_get_hint(*h, "NAME");
            char* description = snd_device_name_get_hint(*h, "DESC");
            char* io = snd_device_name_get_hint(*h, "IOID");
            if (name) {
                DeviceInfo i;
                i.id = std::string("alsa:") + name;
                i.driver = "alsa";
                i.address = name;
                std::string text = description ? description : name;
                std::replace(text.begin(), text.end(), '\n', ' ');
                i.name = text;
                i.vendor = "ALSA";
                i.model = name;
                i.serial = name;
                i.connection = ConnectionType::SystemAudio;
                i.driverVersion = version();
                const std::string ioid = io ? io : "";
                ioOf_[i.id] = ioid;
                out.push_back(i);
            }
            free(name);
            free(description);
            free(io);
        }
        snd_device_name_free_hint(hints);
        return out;
    }
    Result<std::unique_ptr<HardwareDevice>> open(const DeviceInfo& info) override
    {
        const std::string io = ioOf_.count(info.id) ? ioOf_.at(info.id) : "";
        return std::unique_ptr<HardwareDevice>(std::make_unique<AlsaDevice>(info, io != "Output", io != "Input"));
    }

private:
    std::map<std::string, std::string> ioOf_;
};

}  // namespace

std::shared_ptr<experiments::DeviceDriver> makeAlsaDriver() { return std::make_shared<AlsaDriver>(); }

#endif

}  // namespace resonant::hardware
