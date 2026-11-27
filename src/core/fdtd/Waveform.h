#pragma once

// Time-domain source signals for transient solvers.
//
// A waveform is a dimensionless function of time w(t), zero for t < 0, with
// a peak magnitude of about one. A source multiplies it by its amplitude and
// shifts it by its time delay: a delay is a real shift of the signal, at
// every frequency, never a phase.
#include "foundation/Result.h"

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <vector>

namespace resonant::fdtd {

enum class WaveformKind {
    Continuous,     // sinusoid switched on with a smooth ramp
    ToneBurst,      // a whole number of cycles under a Hann envelope
    GaussianPulse,  // sinusoid under a Gaussian envelope
    Ricker,         // second derivative of a Gaussian: broadband, zero mean
    Chirp,          // linear frequency sweep under a Tukey envelope
    Sampled         // a recorded signal, linearly interpolated
};

std::string_view toString(WaveformKind kind);
Result<WaveformKind> waveformKindFromString(std::string_view text);

struct Waveform {
    WaveformKind kind = WaveformKind::Continuous;
    double frequencyHz = 40000.0;    // carrier / centre / peak / start frequency
    double phase = 0.0;              // rad, phase of the carrier
    double rampCycles = 4.0;         // Continuous: length of the switch-on ramp
    double cycles = 5.0;             // ToneBurst: number of cycles
    double fractionalBandwidth = 0.5;  // GaussianPulse: -6 dB bandwidth / centre frequency
    double endFrequencyHz = 80000.0; // Chirp: frequency at the end of the sweep
    double duration = 1e-3;          // Chirp: length of the sweep, s
    double repetitionPeriod = 0.0;   // s; 0 = not repeated. Not for Continuous.
    std::vector<double> samples;     // Sampled
    double sampleRateHz = 0.0;       // Sampled

    Status validate() const;

    // w(t). Zero before t = 0.
    double value(double t) const;
    // Length of one occurrence of the signal, s; infinity for Continuous.
    double length() const;
    // The highest frequency that carries a significant part of the signal's
    // energy: what the grid has to resolve.
    double highestFrequency() const;
    // A sentence describing the signal, for records.
    std::string describe() const;

    friend bool operator==(const Waveform&, const Waveform&) = default;
};

nlohmann::json toJson(const Waveform& waveform);
Result<Waveform> waveformFromJson(const nlohmann::json& value);

}  // namespace resonant::fdtd
