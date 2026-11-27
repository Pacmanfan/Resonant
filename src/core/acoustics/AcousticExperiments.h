#pragma once

// The acoustic interpretation of experiments: sound pressure and its level,
// microphone calibration with an acoustic calibrator, the acoustic field the
// simulation predicts at measured positions, focus metrics, and templates
// for the acoustic workflows (speaker characterization, field mapping,
// ultrasonic element calibration, focusing validation).
//
// Pressures are complex peak amplitudes in Pa with the exp(+j omega t)
// convention of the acoustic domain (Conventions.h); a measured tone
// amplitude A e^{j phi} of A cos(omega t + phi) is the same quantity. Sound
// pressure is what microphones measure; intensity and power are calculated
// from it with a model and are never reported as measured.
#include "acoustics/FieldEvaluator.h"
#include "experiments/Calibration.h"
#include "experiments/Comparison.h"
#include "experiments/Experiment.h"

#include <memory>

namespace resonant::acoustics {

// 20 log10(rms / 20 uPa).
double soundPressureLevelDb(double rmsPa);

// A microphone's sensitivity (raw unit per Pa) from a recording of an
// acoustic calibrator (a known level at one frequency, e.g. 94 dB at 1 kHz).
Result<experiments::CalibrationRecord> calibratorSensitivity(const std::vector<double>& recording, double sampleRateHz,
                                                             double calibratorLevelDb, double calibratorFrequencyHz,
                                                             double levelUncertaintyDb, experiments::Quantity rawQuantity);

// The predicted complex peak pressure (Pa) at a point, for comparisons with measured fields.
experiments::FieldSampler fieldSampler(std::shared_ptr<const FieldEvaluator> evaluator);

// The free-field response (Pa per command unit) of a point source of
// `paAt1mPerUnit` at `source`, at `receiver`, over frequencies: what a
// speaker-characterization measurement would show for an ideal, flat source.
experiments::FrequencySeries freeFieldResponse(const Vec3& source, const Vec3& receiver, double paAt1mPerUnit,
                                               experiments::Quantity commandQuantity, const std::vector<double>& frequenciesHz,
                                               const Medium& medium);

struct FocusMetrics {
    Vec3 peakPosition = Vec3::Zero();
    double peakMagnitude = 0.0;
    double widthMinus6dB = 0.0;   // m, along the scan line (interpolated between samples)
    double peakSidelobeDb = 0.0;  // the largest secondary maximum relative to the peak (dB, < 0); 0 = none found
    size_t points = 0;
    std::string note;
};

// For samples along a line (in scan order).
Result<FocusMetrics> focusMetrics(const experiments::SpatialDataset& line);

struct FocusComparison {
    FocusMetrics measured, simulated;
    double locationErrorM = 0.0;
    double widthErrorM = 0.0;
    double sidelobeDifferenceDb = 0.0;
};

// Both evaluated at the measured positions only.
Result<FocusComparison> compareFocus(const experiments::SpatialDataset& measured, const experiments::FieldSampler& simulated);

// Per-element amplitude and phase of an ultrasonic array relative to a
// reference element, from the complex tone each element alone produced at
// one microphone. `pathPhaseRad` is the propagation phase from each element to
// the microphone (from the geometry), removed so that only the element's own
// response remains.
Result<std::vector<experiments::CalibrationRecord>> elementCalibration(const std::vector<std::complex<double>>& responses,
                                                                       const std::vector<std::string>& actuatorIds,
                                                                       const std::vector<double>& pathPhaseRad,
                                                                       size_t referenceIndex, double frequencyHz);

// --- templates (on the simulated devices; change the devices to use real ones) ---

experiments::Experiment speakerCharacterizationExperiment(const std::string& id);
experiments::Experiment fieldMappingExperiment(const std::string& id, double frequencyHz);
experiments::Experiment ultrasonicElementExperiment(const std::string& id, const std::string& elementBinding);
experiments::Experiment focusingValidationExperiment(const std::string& id, const Vec3& focus);

}  // namespace resonant::acoustics
