#pragma once

// The RF interpretation of experiments: S-parameter measurements (VNA sweeps
// and imported Touchstone files), return loss, VSWR and impedance, their
// comparison with the simulated antenna impedance, per-channel calibration
// of receiver arrays and its use on array excitations, measured radiation
// patterns and their comparison with simulated patterns, and the RF
// measurement chain.
//
// An SDR or a receiver reports levels relative to its full scale. An
// absolute received power needs the receiver's calibration and the cable
// loss; an absolute field strength also needs the antenna factor. Without
// them only relative levels are reported (and the functions say so).
#include "electromagnetics/Antennas.h"
#include "electromagnetics/EmComponents.h"
#include "electromagnetics/EmStudy.h"
#include "electromagnetics/Touchstone.h"
#include "experiments/Calibration.h"
#include "experiments/Comparison.h"
#include "experiments/Experiment.h"
#include "experiments/Processing.h"

#include <optional>

namespace resonant::em {

// A Touchstone file as a raw (imported) measurement record with its data.
Result<experiments::ProcessingItem> importTouchstoneMeasurement(const std::string& text, const std::string& fileName,
                                                                const std::string& experimentId);
// The network parameters of a NetworkParameters measurement item.
Result<NetworkParameters> networkOf(const experiments::ProcessingItem& item);

struct ReflectionMetrics {
    std::vector<double> frequencyHz;
    std::vector<Complex> impedance;      // ohm
    std::vector<double> returnLossDb;    // -20 log10 |S11|
    std::vector<double> vswr;            // (1 + |S11|) / (1 - |S11|)
    double bestFrequencyHz = 0.0, bestReturnLossDb = 0.0;
    double bandwidth10dBHz = 0.0;        // contiguous band around the best point with return loss >= 10 dB
};
ReflectionMetrics reflectionMetrics(const std::vector<double>& frequencyHz, const std::vector<Complex>& s11, double referenceImpedance);

// Measured S11 against the S11 of a simulated impedance sweep (same reference impedance).
Result<experiments::ComparisonMetrics> compareReflection(const NetworkParameters& measured,
                                                         const std::vector<std::pair<double, AntennaImpedance>>& simulated,
                                                         const std::string& model, int port = 1);

// The complex amplitude of a tone at `offsetHz` from the centre frequency in
// I/Q records (one per channel).
Result<std::vector<Complex>> channelPhasors(const std::vector<std::vector<Complex>>& iq, double sampleRateHz, double offsetHz);

// Per-channel gain and phase relative to a reference channel, with the
// expected (geometric) phase of each channel removed. One record per channel.
Result<std::vector<experiments::CalibrationRecord>> arrayChannelCalibration(const std::vector<Complex>& phasors,
                                                                            const std::vector<std::string>& channelIds,
                                                                            const std::string& deviceId, size_t referenceIndex,
                                                                            const std::vector<double>& expectedPhaseRad,
                                                                            double frequencyHz);

// Array excitation corrected for measured channel errors: each element's
// weight is divided by its channel's complex gain (pre-compensation), so
// that the excitation reaching the elements is the intended one.
Result<EmArraySettings> applyArrayCalibration(const EmArraySettings& settings,
                                              const std::vector<const experiments::CalibrationRecord*>& perElement);

struct MeasuredPattern {
    std::vector<double> angleDeg;
    std::vector<double> relativeDb;    // normalized to the maximum
    double peakAngleDeg = 0.0;
    double halfPowerBeamwidthDeg = 0.0; // 0 when the -3 dB points are not both inside the scan
    std::string note;
};
// From an angular scan (samples with angles). Relative only.
Result<MeasuredPattern> measuredPattern(const experiments::SpatialDataset& angular);
// The azimuth cut (theta = 90 deg) of a simulated pattern, phi in degrees.
MeasuredPattern patternCut(const PatternGrid& grid, double thetaDeg = 90.0);

struct PatternComparison {
    size_t points = 0;
    double rmsDifferenceDb = 0.0;       // over angles where both lie above -30 dB
    double beamPointingErrorDeg = 0.0;  // measured peak - simulated peak, wrapped
    double measuredBeamwidthDeg = 0.0, simulatedBeamwidthDeg = 0.0;
    std::string note;
};
Result<PatternComparison> comparePattern(const MeasuredPattern& measured, const MeasuredPattern& simulated);

struct RfMeasurementChain {
    std::optional<double> receiverDbmAtFullScale;  // receiver calibration: the input power that reads 0 dBFS
    std::optional<double> cableLossDb;
    std::optional<double> antennaFactorDbPerM;     // field strength (dBuV/m) = voltage (dBuV) + AF
    double referenceImpedance = 50.0;
};
// Power at the antenna terminals; fails without the receiver calibration and the cable loss.
Result<double> receivedPowerDbm(double readingDbfs, const RfMeasurementChain& chain);
// Field strength at the antenna; fails unless the whole chain is calibrated.
Result<double> fieldStrengthDbuvPerM(double readingDbfs, const RfMeasurementChain& chain);

// --- templates (on the simulated RF devices) ---

experiments::Experiment antennaImpedanceExperiment(const std::string& id, double centreHz);
experiments::Experiment arrayCalibrationExperiment(const std::string& id, double frequencyHz);
experiments::Experiment radiationPatternExperiment(const std::string& id, double frequencyHz);

}  // namespace resonant::em
