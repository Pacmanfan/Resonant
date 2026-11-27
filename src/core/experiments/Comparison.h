#pragma once

// Comparison of measured with simulated data, independent of the physics
// domain. Domain modules supply the simulated side (a frequency response, a
// field sampler) and add their own metrics (focal width, modal assurance,
// return loss, beam pointing, ...).
//
// Rules:
//  * Quantities must agree (quantity, ratio and unit). An uncalibrated
//    reading (V, FS) can only be compared in shape ("relative": both
//    normalized to their own maximum), and only when that is asked for; the
//    result then says that absolute levels were not compared.
//  * Only the overlap of the frequency ranges is compared; nothing is
//    extrapolated. Simulated values are interpolated onto the measured
//    frequencies (linear in magnitude and in unwrapped phase).
//  * Spatial comparisons need the positions in the simulation's frame
//    (a transform may be given) and the same frequency.
//  * Where the measurement has an uncertainty, the fraction of points whose
//    difference lies within the expanded uncertainty is reported, and so is
//    the simulation uncertainty when it is stated (it is not assumed zero).
#include "experiments/ExperimentTypes.h"
#include "experiments/Spatial.h"
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <complex>
#include <functional>
#include <string>
#include <vector>

namespace resonant::experiments {

struct FrequencySeries {
    std::vector<double> frequencyHz;
    std::vector<std::complex<double>> values;
    std::vector<double> standardUncertainty;  // absolute, of |value|; empty = unknown
    QuantitySpec quantity;
    DataCategory category = DataCategory::Processed;
    std::string label;
    std::string model;                        // the simulation's model, for a simulated series
};

struct ComparisonOptions {
    bool comparePhase = true;
    bool relative = false;              // normalize both to their maximum magnitude (shape only)
    double coverageFactor = 2.0;
    double minMagnitudeFraction = 1e-3; // points where both are below this fraction of the maximum are left out of dB metrics
};

struct ComparisonMetrics {
    size_t points = 0;
    double fromHz = 0.0, toHz = 0.0;
    double rmsMagnitudeErrorDb = 0.0, maxMagnitudeErrorDb = 0.0;
    double rmsPhaseErrorDeg = 0.0;      // phase difference wrapped to (-180, 180]
    double normalizedMeanSquareError = 0.0;  // sum |m - s|^2 / sum |m|^2
    double magnitudeCorrelation = 0.0;  // Pearson, of the magnitudes
    double peakMeasuredHz = 0.0, peakSimulatedHz = 0.0;
    double fractionWithinUncertainty = -1.0;  // -1 = no uncertainty available
    bool relative = false;
    std::vector<double> errorDb;        // per compared point
    std::vector<double> comparedHz;
    std::vector<std::string> notes;
};

nlohmann::json toJson(const ComparisonMetrics& metrics);

Result<ComparisonMetrics> compareFrequencySeries(const FrequencySeries& measured, const FrequencySeries& simulated,
                                                 const ComparisonOptions& options = {});

// The simulated field at a position (in the simulation's frame) for the data
// set's frequency; supplied by a domain.
using FieldSampler = std::function<Result<std::complex<double>>(double x, double y, double z)>;

struct SpatialComparisonOptions {
    bool relative = false;
    bool comparePhase = true;
    std::string simulationFrame = "scene";
    double transform[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};  // measurement frame -> simulation frame, row-major
    double coverageFactor = 2.0;
};

struct SpatialComparison {
    size_t points = 0;
    double rmsMagnitudeErrorDb = 0.0, maxMagnitudeErrorDb = 0.0, rmsPhaseErrorDeg = 0.0;
    double normalizedMeanSquareError = 0.0;
    double fractionWithinUncertainty = -1.0;
    size_t worstPoint = 0;
    std::vector<double> errorDb;              // per sample
    std::vector<std::complex<double>> simulated;
    bool relative = false;
    std::vector<std::string> notes;
};

nlohmann::json toJson(const SpatialComparison& comparison);

Result<SpatialComparison> compareSpatial(const SpatialDataset& measured, const FieldSampler& simulated,
                                         const QuantitySpec& simulatedQuantity, const SpatialComparisonOptions& options = {});

// Whether two quantities may be compared (and why not).
Status comparable(const QuantitySpec& measured, const QuantitySpec& simulated, bool relative);

}  // namespace resonant::experiments
