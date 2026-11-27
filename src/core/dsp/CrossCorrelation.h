#pragma once

// Correlation of sampled records and time-delay estimation.
//
//   r_xy[k] = sum_n x[n] y[n + k],   k = -maxLag ... maxLag
//
// computed through zero-padded FFTs (no circular wrap-around). A positive
// lag means y lags x: y[n] = x[n - d] peaks at k = d.
#include "foundation/Result.h"

#include <string>
#include <vector>

namespace resonant::dsp {

struct Correlation {
    std::vector<double> values;  // index i is lag (i - maxLag)
    long maxLag = 0;
    double normalization = 1.0;  // sqrt(sum x^2 sum y^2): values / normalization lie in [-1, 1]
    double at(long lag) const { return values[static_cast<size_t>(lag + maxLag)]; }
};

Result<Correlation> crossCorrelation(const std::vector<double>& x, const std::vector<double>& y, long maxLag);
Result<Correlation> autoCorrelation(const std::vector<double>& x, long maxLag);

struct DelayEstimate {
    double delaySamples = 0.0;
    double delaySeconds = 0.0;
    long integerLag = 0;           // the correlation peak
    double peakCorrelation = 0.0;  // normalized, at the integer peak
    // How the fractional part was found: the slope of the cross-spectrum
    // phase after removing the integer lag, weighted by |G_xy|, over the
    // bins whose magnitude is at least 1 % of the largest.
    std::string method;
    double standardUncertaintySeconds = 0.0;  // from the phase-fit residual
};

// The delay of `delayed` relative to `reference` (both sampled at fs), searched
// within +-maxLagSamples. Fails when the records are too short or carry no
// common signal (normalized peak below 0.1).
Result<DelayEstimate> estimateDelay(const std::vector<double>& reference, const std::vector<double>& delayed, double sampleRateHz,
                                    long maxLagSamples);

}  // namespace resonant::dsp
