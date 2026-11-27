#pragma once

// Linear-phase FIR filters (windowed sinc), decimation and resampling of
// measured records.
//
// Every operation here changes the data and is named in the processing
// history of what it produced. A linear-phase FIR of N taps delays its
// output by (N - 1) / 2 samples; `compensateDelay` removes that delay by
// shifting the output (the first and last (N - 1) / 2 samples are then
// affected by the record's ends and are reported as such).
#include "dsp/Windows.h"
#include "foundation/Result.h"

#include <string>
#include <string_view>
#include <vector>

namespace resonant::dsp {

enum class FilterBand { LowPass, HighPass, BandPass, BandStop };
std::string_view toString(FilterBand band);
Result<FilterBand> filterBandFromString(std::string_view text);

struct FirFilter {
    std::vector<double> taps;
    FilterBand band = FilterBand::LowPass;
    double sampleRateHz = 0.0;
    double lowHz = 0.0, highHz = 0.0;  // cut-off (LowPass/HighPass use lowHz)
    WindowKind window = WindowKind::Hamming;

    size_t groupDelaySamples() const { return taps.empty() ? 0 : (taps.size() - 1) / 2; }
    std::string describe() const;
};

// numTaps must be odd (a type I filter, so every band shape is possible).
// Cut-offs are the -6 dB points of the windowed-sinc design.
Result<FirFilter> designFir(FilterBand band, double sampleRateHz, double lowHz, double highHz, size_t numTaps,
                            WindowKind window = WindowKind::Hamming);

struct FilteredRecord {
    std::vector<double> samples;
    size_t edgeSamples = 0;  // at each end, influenced by the zero extension of the record
    std::string description;
};

FilteredRecord applyFir(const FirFilter& filter, const std::vector<double>& x, bool compensateDelay);

// Low-pass at 0.8 of the new Nyquist frequency, then every factor-th sample.
struct DecimatedRecord {
    std::vector<double> samples;
    double sampleRateHz = 0.0;
    std::string description;
};
Result<DecimatedRecord> decimate(const std::vector<double>& x, double sampleRateHz, int factor);

// Band-limited interpolation onto a new sample rate (windowed sinc with
// `halfWidth` zero crossings on each side, cut-off 0.9 of the lower of the
// two Nyquist frequencies).
Result<DecimatedRecord> resample(const std::vector<double>& x, double sampleRateHz, double newSampleRateHz, int halfWidth = 16);

}  // namespace resonant::dsp
