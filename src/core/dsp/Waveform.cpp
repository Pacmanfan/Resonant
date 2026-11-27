#include "dsp/Waveform.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace resonant::dsp {
namespace {

constexpr double pi = 3.14159265358979323846;
constexpr double twoPi = 2.0 * pi;

struct KindName {
    WaveformKind kind;
    std::string_view name;
};
constexpr KindName kNames[] = {{WaveformKind::Continuous, "continuous"}, {WaveformKind::ToneBurst, "toneBurst"},
                               {WaveformKind::GaussianPulse, "gaussianPulse"}, {WaveformKind::Ricker, "ricker"},
                               {WaveformKind::Chirp, "chirp"}, {WaveformKind::Sampled, "sampled"}};

// Standard deviation of the Gaussian envelope, s, for a -6 dB (amplitude)
// spectral bandwidth of fractionalBandwidth * f. The spectrum of
// exp(-t^2 / (2 s^2)) is exp(-(2 pi f s)^2 / 2); half amplitude at
// f_h = sqrt(2 ln 2) / (2 pi s), and the bandwidth is 2 f_h.
double gaussianSigma(const Waveform& w)
{
    return std::sqrt(2.0 * std::log(2.0)) / (pi * w.fractionalBandwidth * w.frequencyHz);
}

// The envelope is started 4 sigma before its peak (value 3e-4 there).
constexpr double kGaussianLead = 4.0;

double once(const Waveform& w, double t)
{
    switch (w.kind) {
    case WaveformKind::Continuous: {
        const double rampTime = w.rampCycles / w.frequencyHz;
        const double ramp = rampTime > 0.0 && t < rampTime ? 0.5 * (1.0 - std::cos(pi * t / rampTime)) : 1.0;
        return ramp * std::cos(twoPi * w.frequencyHz * t + w.phase);
    }
    case WaveformKind::ToneBurst: {
        const double length = w.cycles / w.frequencyHz;
        if (t > length)
            return 0.0;
        return 0.5 * (1.0 - std::cos(twoPi * t / length)) * std::cos(twoPi * w.frequencyHz * t + w.phase);
    }
    case WaveformKind::GaussianPulse: {
        const double sigma = gaussianSigma(w);
        const double tau = t - kGaussianLead * sigma;
        if (tau > kGaussianLead * sigma)
            return 0.0;
        return std::exp(-0.5 * tau * tau / (sigma * sigma)) * std::cos(twoPi * w.frequencyHz * tau + w.phase);
    }
    case WaveformKind::Ricker: {
        // Peak frequency f: (1 - 2 a) exp(-a), a = (pi f tau)^2, centred 1.5 / f after the start.
        const double tau = t - 1.5 / w.frequencyHz;
        if (tau > 1.5 / w.frequencyHz)
            return 0.0;
        const double a = pi * w.frequencyHz * tau;
        return (1.0 - 2.0 * a * a) * std::exp(-a * a);
    }
    case WaveformKind::Chirp: {
        if (t > w.duration)
            return 0.0;
        const double rate = (w.endFrequencyHz - w.frequencyHz) / w.duration;
        // Tukey envelope: 10 % cosine tapers at both ends.
        const double taper = 0.1 * w.duration;
        double envelope = 1.0;
        if (t < taper)
            envelope = 0.5 * (1.0 - std::cos(pi * t / taper));
        else if (t > w.duration - taper)
            envelope = 0.5 * (1.0 - std::cos(pi * (w.duration - t) / taper));
        return envelope * std::cos(twoPi * (w.frequencyHz * t + 0.5 * rate * t * t) + w.phase);
    }
    case WaveformKind::Sampled: {
        const double position = t * w.sampleRateHz;
        const auto index = static_cast<size_t>(position);
        if (index + 1 >= w.samples.size())
            return index < w.samples.size() && position == static_cast<double>(index) ? w.samples[index] : 0.0;
        const double f = position - static_cast<double>(index);
        return (1.0 - f) * w.samples[index] + f * w.samples[index + 1];
    }
    }
    return 0.0;
}

}  // namespace

std::string_view toString(WaveformKind kind)
{
    for (const KindName& n : kNames)
        if (n.kind == kind)
            return n.name;
    return "continuous";
}

Result<WaveformKind> waveformKindFromString(std::string_view text)
{
    for (const KindName& n : kNames)
        if (n.name == text)
            return n.kind;
    return Result<WaveformKind>::failure("unknown waveform '" + std::string(text) + "'");
}

Status Waveform::validate() const
{
    if (kind == WaveformKind::Sampled) {
        if (samples.size() < 2)
            return Status::failure("a sampled waveform needs at least two samples");
        if (!(std::isfinite(sampleRateHz) && sampleRateHz > 0.0))
            return Status::failure("the sample rate of a sampled waveform must be positive");
        for (const double s : samples)
            if (!std::isfinite(s))
                return Status::failure("a sampled waveform contains a value that is not a finite number");
    } else if (!(std::isfinite(frequencyHz) && frequencyHz > 0.0)) {
        return Status::failure("the waveform frequency must be positive");
    }
    if (!std::isfinite(phase))
        return Status::failure("the waveform phase must be finite");
    if (kind == WaveformKind::Continuous && !(std::isfinite(rampCycles) && rampCycles >= 0.0))
        return Status::failure("the ramp length must be zero or positive");
    if (kind == WaveformKind::ToneBurst && !(std::isfinite(cycles) && cycles >= 1.0))
        return Status::failure("a tone burst needs at least one cycle");
    if (kind == WaveformKind::GaussianPulse && !(std::isfinite(fractionalBandwidth) && fractionalBandwidth >= 0.05 && fractionalBandwidth <= 2.0))
        return Status::failure("the fractional bandwidth must be between 0.05 and 2");
    if (kind == WaveformKind::Chirp) {
        if (!(std::isfinite(endFrequencyHz) && endFrequencyHz > 0.0))
            return Status::failure("the end frequency of a sweep must be positive");
        if (!(std::isfinite(duration) && duration > 0.0))
            return Status::failure("the duration of a sweep must be positive");
    }
    if (!(std::isfinite(repetitionPeriod) && repetitionPeriod >= 0.0))
        return Status::failure("the repetition period must be zero or positive");
    if (repetitionPeriod > 0.0 && kind == WaveformKind::Continuous)
        return Status::failure("a continuous signal cannot be repeated");
    if (repetitionPeriod > 0.0 && repetitionPeriod < length())
        return Status::failure("the repetition period is shorter than the signal");
    return Status::success();
}

double Waveform::length() const
{
    switch (kind) {
    case WaveformKind::Continuous:
        return std::numeric_limits<double>::infinity();
    case WaveformKind::ToneBurst:
        return cycles / frequencyHz;
    case WaveformKind::GaussianPulse:
        return 2.0 * kGaussianLead * gaussianSigma(*this);
    case WaveformKind::Ricker:
        return 3.0 / frequencyHz;
    case WaveformKind::Chirp:
        return duration;
    case WaveformKind::Sampled:
        return sampleRateHz > 0.0 && !samples.empty() ? static_cast<double>(samples.size() - 1) / sampleRateHz : 0.0;
    }
    return 0.0;
}

double Waveform::value(double t) const
{
    if (t < 0.0)
        return 0.0;
    if (repetitionPeriod > 0.0)
        t = std::fmod(t, repetitionPeriod);
    return once(*this, t);
}

double Waveform::highestFrequency() const
{
    switch (kind) {
    case WaveformKind::Continuous:
        return frequencyHz;
    case WaveformKind::ToneBurst:
        // Hann-windowed burst: main lobe reaches f (1 + 2 / cycles).
        return frequencyHz * (1.0 + 2.0 / cycles);
    case WaveformKind::GaussianPulse:
        // Twice the -6 dB half-bandwidth above the centre: about -24 dB.
        return frequencyHz * (1.0 + fractionalBandwidth);
    case WaveformKind::Ricker:
        // The spectrum of a Ricker wavelet is below -40 dB of its peak at 3 f.
        return 2.5 * frequencyHz;
    case WaveformKind::Chirp:
        return std::max(frequencyHz, endFrequencyHz);
    case WaveformKind::Sampled:
        return 0.5 * sampleRateHz;  // nothing more is known than the Nyquist limit
    }
    return frequencyHz;
}

std::string Waveform::describe() const
{
    const auto number = [](double v) {
        char buffer[40];
        std::snprintf(buffer, sizeof buffer, "%.6g", v);
        return std::string(buffer);
    };
    std::string text;
    switch (kind) {
    case WaveformKind::Continuous:
        text = "continuous sinusoid at " + number(frequencyHz) + " Hz, switched on over " + number(rampCycles) + " cycles";
        break;
    case WaveformKind::ToneBurst:
        text = "tone burst of " + number(cycles) + " cycles at " + number(frequencyHz) + " Hz (Hann envelope)";
        break;
    case WaveformKind::GaussianPulse:
        text = "Gaussian pulse at " + number(frequencyHz) + " Hz, -6 dB bandwidth " + number(fractionalBandwidth * frequencyHz) + " Hz";
        break;
    case WaveformKind::Ricker:
        text = "Ricker wavelet with peak frequency " + number(frequencyHz) + " Hz";
        break;
    case WaveformKind::Chirp:
        text = "linear sweep from " + number(frequencyHz) + " Hz to " + number(endFrequencyHz) + " Hz in " + number(duration) + " s";
        break;
    case WaveformKind::Sampled:
        text = "sampled signal, " + std::to_string(samples.size()) + " samples at " + number(sampleRateHz) + " Hz";
        break;
    }
    if (repetitionPeriod > 0.0)
        text += ", repeated every " + number(repetitionPeriod) + " s";
    return text;
}

nlohmann::json toJson(const Waveform& w)
{
    nlohmann::json value = {{"kind", std::string(toString(w.kind))},
                            {"frequencyHz", w.frequencyHz},
                            {"phase", w.phase},
                            {"rampCycles", w.rampCycles},
                            {"cycles", w.cycles},
                            {"fractionalBandwidth", w.fractionalBandwidth},
                            {"endFrequencyHz", w.endFrequencyHz},
                            {"duration", w.duration},
                            {"repetitionPeriod", w.repetitionPeriod}};
    if (w.kind == WaveformKind::Sampled) {
        value["samples"] = w.samples;
        value["sampleRateHz"] = w.sampleRateHz;
    }
    return value;
}

Result<Waveform> waveformFromJson(const nlohmann::json& value)
{
    if (!value.is_object())
        return Result<Waveform>::failure("a waveform must be an object");
    Waveform w;
    try {
        auto kind = waveformKindFromString(value.value("kind", std::string("continuous")));
        if (!kind)
            return Result<Waveform>::failure(kind.message());
        w.kind = kind.value();
        w.frequencyHz = value.value("frequencyHz", w.frequencyHz);
        w.phase = value.value("phase", w.phase);
        w.rampCycles = value.value("rampCycles", w.rampCycles);
        w.cycles = value.value("cycles", w.cycles);
        w.fractionalBandwidth = value.value("fractionalBandwidth", w.fractionalBandwidth);
        w.endFrequencyHz = value.value("endFrequencyHz", w.endFrequencyHz);
        w.duration = value.value("duration", w.duration);
        w.repetitionPeriod = value.value("repetitionPeriod", w.repetitionPeriod);
        if (w.kind == WaveformKind::Sampled) {
            w.samples = value.value("samples", std::vector<double>{});
            w.sampleRateHz = value.value("sampleRateHz", 0.0);
        }
    } catch (const nlohmann::json::exception& e) {
        return Result<Waveform>::failure(std::string("invalid waveform: ") + e.what());
    }
    if (const Status status = w.validate(); !status)
        return Result<Waveform>::failure(status.message());
    return w;
}

}  // namespace resonant::dsp
