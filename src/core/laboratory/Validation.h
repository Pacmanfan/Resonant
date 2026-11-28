#pragma once

// Comparison of observation and prediction (REQ-CHAT-20261126-023, RES-LAB-005, -006).
// Every comparison first checks that the two sides are comparable (quantity and unit,
// frame, overlapping range) and records each alignment; metrics are chosen by what is
// compared, and uncertainty decides:
//   scalars      difference, relative difference, E_n = |m - p| / sqrt(U_m^2 + U_p^2)
//                (consistent for E_n <= 1; not assessable without uncertainties)
//   responses    the experiment framework's comparison (dB and phase errors, NMSE, correlation)
//                plus the peaks' frequency and amplitude offsets, the spectral similarity and the
//                fraction within the combined expanded uncertainty
//   transients   on the overlap of the two time ranges (the prediction interpolated onto the
//                measured times): RMS, normalized RMS, maximum deviation, correlation, the time
//                shift that best aligns them (reported, not applied)
//   fields       measured positions transformed into the prediction's frame and the prediction
//                interpolated there (trilinear on its grid): per-point residuals, RMS, normalized
//                RMS, the worst point, the fraction within uncertainty; different frames without a
//                transform are refused

#include "experiments/Comparison.h"
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace resonant::laboratory {

// The smallest fraction of points within k standard uncertainties that is consistent with the
// model at n points: the normal coverage p(k) less three binomial standard deviations (a
// correct model at k = 2 and 160 points lies above about 0.905, not always above 0.95).
double consistencyThreshold(size_t points, double coverage);

struct ScalarValue {
    double value = 0.0;
    double standard = 0.0;  // standard uncertainty; 0: unknown
    double coverage = 2.0;
    std::string unit, label;
};

struct ScalarComparison {
    double measured = 0.0, predicted = 0.0, difference = 0.0;
    std::optional<double> relative, normalizedError;  // relative to the measured value; E_n
    double expandedMeasured = 0.0, expandedPredicted = 0.0;
    std::string verdict;  // consistent, inconsistent, not assessable
    std::string unit;
    std::vector<std::string> notes;
    nlohmann::json toJson() const;
};
Result<ScalarComparison> compareScalars(const ScalarValue& measured, const ScalarValue& predicted);

struct ResponseComparison {
    experiments::ComparisonMetrics base;
    double peakOffsetHz = 0.0, peakOffsetRelative = 0.0, peakAmplitudeOffsetDb = 0.0;
    double spectralSimilarity = 0.0;          // cosine of the magnitude spectra
    double fractionWithinCombined = -1.0;     // with both sides' uncertainties (-1: none stated)
    std::vector<double> frequencyHz, measuredMagnitude, predictedMagnitude, residual;  // |m| - |p| on the compared points
    std::string verdict;
    nlohmann::json toJson() const;
};
Result<ResponseComparison> compareResponses(const experiments::FrequencySeries& measured, const experiments::FrequencySeries& predicted,
                                            const experiments::ComparisonOptions& options = {});

struct TimeComparison {
    size_t points = 0;
    double from = 0.0, to = 0.0;
    double rms = 0.0, normalizedRms = 0.0, maxDeviation = 0.0, maxAt = 0.0, correlation = 0.0, shiftSeconds = 0.0;
    double fractionWithin = -1.0;
    std::vector<double> times, measured, predicted, residual;
    std::string unit, verdict;
    std::vector<std::string> notes;
    nlohmann::json toJson() const;
};
// measuredStandard: per point, one value for all, or empty (unknown); predictedStandard: one value (0 unknown).
Result<TimeComparison> compareTransients(const std::vector<double>& measuredTimes, const std::vector<double>& measured,
                                         const std::vector<double>& measuredStandard, const std::vector<double>& predictedTimes,
                                         const std::vector<double>& predicted, double predictedStandard, const std::string& unit,
                                         double coverage = 2.0);

struct FieldGrid {
    std::array<double, 3> origin{}, spacing{1, 1, 1};
    std::array<int, 3> size{1, 1, 1};
    std::vector<double> values;  // x fastest
    std::string frame = "scene", unit;
    std::optional<double> at(double x, double y, double z) const;  // trilinear; none outside
};

struct FieldComparison {
    size_t points = 0, outside = 0, worst = 0;
    double rms = 0.0, normalizedRms = 0.0, maxDeviation = 0.0, fractionWithin = -1.0;
    std::vector<std::array<double, 3>> positions;  // in the prediction's frame
    std::vector<double> measured, predicted, residual;
    std::string frame, unit, verdict;
    std::vector<std::string> notes;
    nlohmann::json toJson() const;
};
// positions: x, y, z per point in `measuredFrame`; transform: measurement frame -> prediction frame (row-major 4x4).
Result<FieldComparison> compareFields(const std::vector<std::array<double, 3>>& positions, const std::vector<double>& measured,
                                      const std::vector<double>& measuredStandard, const std::string& measuredFrame, const FieldGrid& predicted,
                                      const std::array<double, 16>& transform, double coverage = 2.0);

}  // namespace resonant::laboratory
