#pragma once

// Reproducible processing of measurements.
//
// A processing step is an operation name and its parameters. Running a step
// on input data sets produces a new data set whose record names its sources
// and carries the complete history (the inputs' history plus this step,
// with a description that states every parameter it used). The raw data is
// never changed. Nothing is applied silently: no window, normalization,
// filter, calibration or unit conversion happens unless a step asks for it.
//
// Operations
//   segment            {startSeconds, durationSeconds}                time series -> time series
//   calibrate          {calibrationId}                                raw -> calibrated (time series or spectrum);
//                                                                     without an id: the sensor's selected calibration
//   spectrum           {window}                                       time series -> complex spectrum (tones read A e^{j phi})
//   psd                {segmentLength, overlap, window}               time series -> one-sided density, unit^2/Hz
//   transfer-function  {reference, segmentLength, overlap, window}    (response, reference) -> H1 with coherence
//   impulse-response   {}                                             transfer function -> time series
//   filter             {band, lowHz, highHz, taps, window, compensateDelay}
//   decimate           {factor}
//   resample           {sampleRateHz}
//   cross-correlation  {reference, maxLagSeconds}                     -> correlation over lag (s)
//   autocorrelation    {maxLagSeconds}
//   delay              {reference, maxLagSeconds}                     -> scalar delay (s), with its uncertainty
//   rms                {}                                             -> scalar
//   tone               {frequencyHz, window}                          -> complex amplitude at one frequency
//   average            {}                                             several time series -> mean, with its standard error
//   integrate-frequency {times}                                       spectrum / transfer function divided by (j omega)^times:
//                                                                     acceleration -> velocity -> displacement (DC left out)
//   scale              {factor, quantity, reason}                     multiplies by a stated factor that changes the
//                                                                     quantity (e.g. stress = E * strain), with the reason
//
// A time series with dropped frames (stored as NaN) is refused by every
// step except `segment`: choose a gap-free segment first.
#include "experiments/Calibration.h"
#include "experiments/MeasurementRecord.h"

#include <map>
#include <string>
#include <vector>

namespace resonant::experiments {

struct ProcessingItem {
    MeasurementRecord record;
    MeasurementData data;
};

struct ProcessingContext {
    const CalibrationLibrary* calibrations = nullptr;
    // The physical quantity each sensor measures (for calibrate).
    std::map<std::string, Quantity> sensorQuantity;
    // Conditioning gain between transducer and channel, per binding (calibrate divides it out).
    std::map<std::string, double> bindingGain;
    // The calibration each sensor has selected (calibrate without a calibrationId uses it).
    std::map<std::string, std::string> sensorCalibration;
};

std::vector<std::string> processingOperations();

// inputs[0] is the data set processed; operations with a "reference" take it as inputs[1].
Result<ProcessingItem> runStep(const ProcessingStep& step, const std::vector<const ProcessingItem*>& inputs,
                               const ProcessingContext& context);

// Applies a chain to the raw items of one run (by binding id). Each step
// applies to the bindings in its "bindings" parameter (default: every
// recorded binding, not the commanded signals; steps with a reference skip
// the reference binding), in order;
// a step's reference is the latest item of the reference binding. Returns
// every derived item, in order.
Result<std::vector<ProcessingItem>> runChain(const std::vector<ProcessingStep>& chain,
                                             const std::map<std::string, ProcessingItem>& rawByBinding,
                                             const ProcessingContext& context);

// A deterministic id for a derived data set: the same sources and step give
// the same id (so re-running a chain reproduces, and never duplicates, its results).
std::string derivedId(const std::vector<std::string>& sourceIds, const ProcessingStep& step);

}  // namespace resonant::experiments
