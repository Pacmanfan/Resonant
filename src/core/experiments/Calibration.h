#pragma once

// Calibration records and the calibration library.
//
// A record states what was calibrated (device channel, sensor or actuator),
// how, against what reference, with which result (a scalar, a
// frequency-dependent complex response, a delay, a position offset), over
// which frequency and amplitude range, with which uncertainty, and until
// when it is valid.
//
// The library is append-only: a new calibration is a new record that names
// the one it supersedes; no record is ever changed or deleted. An
// experiment selects the record that applies by its id.
//
// Sensitivities are "raw reading per physical unit" (V/Pa, V/(m/s^2),
// FS/Pa, ...). Applying one divides the raw reading by it. A frequency-
// dependent response is applied in the frequency domain; it is never
// reduced to a scalar unless the record states the justification.
#include "experiments/ExperimentTypes.h"
#include "experiments/Uncertainty.h"
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <complex>
#include <string>
#include <vector>

namespace resonant::experiments {

enum class CalibrationType {
    Amplitude, Phase, Timing, Frequency, Position, Orientation, SensorSensitivity, ActuatorTransferFunction, ChannelToChannel
};
std::string_view toString(CalibrationType type);
Result<CalibrationType> calibrationTypeFromString(std::string_view text);

struct ResponsePoint {
    double frequencyHz = 0.0;
    std::complex<double> value;
    double relativeUncertainty = 0.0;  // standard, of |value|
};

struct CalibrationRecord {
    std::string id;
    std::string supersedes;       // the record this one replaces (its history)
    int version = 1;
    CalibrationType type = CalibrationType::SensorSensitivity;
    std::string deviceId, channelId, sensorId, actuatorId;
    std::string dateUtc;
    std::string procedure;        // what was done
    std::string referenceInstrument;
    std::string operatorName;
    std::string source;           // laboratory, certificate number, or "derived by Resonant from measurement <id>"
    // Results (whichever apply):
    double scalar = 0.0;          // sensitivity or gain, in `unit`; 0 = not given
    std::string unit;             // e.g. "V/Pa"
    std::vector<ResponsePoint> response;  // frequency-dependent complex value, in `unit`
    std::string scalarJustification;      // why a scalar represents a frequency-dependent quantity, if it does
    double delaySeconds = 0.0;    // timing: subtract from the channel's time axis
    double phaseRad = 0.0;        // phase calibration at referenceFrequencyHz
    double referenceFrequencyHz = 0.0;
    double positionOffset[3] = {0.0, 0.0, 0.0};  // m, add to the nominal position
    double frequencyErrorPpm = 0.0;
    // Validity:
    double minFrequencyHz = 0.0, maxFrequencyHz = 0.0;  // 0 / 0 = not restricted (scalars only)
    double minAmplitude = 0.0, maxAmplitude = 0.0;      // 0 / 0 = not restricted
    double temperatureC = 0.0;
    bool temperatureRecorded = false;
    std::string validUntilUtc;    // empty = no expiry stated
    std::vector<std::string> files;
    UncertaintyBudget uncertainty;
    bool fromSimulatedDevice = false;  // derived from simulated data: for testing the procedure only

    bool frequencyDependent() const { return !response.empty(); }
    // The value at a frequency: interpolated (linear in magnitude and in
    // unwrapped phase) inside the response table, never extrapolated; the
    // scalar when there is no table.
    Result<std::complex<double>> valueAt(double frequencyHz) const;
    bool covers(double fromHz, double toHz) const;
    bool expiredAt(const std::string& nowUtc) const;
};

nlohmann::json toJson(const CalibrationRecord& record);
Result<CalibrationRecord> calibrationRecordFromJson(const nlohmann::json& value);

class CalibrationLibrary {
public:
    // Adds a record (assigning an id and the version when absent). Fails when
    // the id exists (records are never replaced) or the superseded record is unknown.
    Result<std::string> add(CalibrationRecord record);
    const CalibrationRecord* find(const std::string& id) const;
    const std::vector<CalibrationRecord>& all() const { return records_; }
    // The chain of records ending at `id`, oldest first.
    std::vector<const CalibrationRecord*> history(const std::string& id) const;
    // The newest record of a type for a sensor / device channel (not superseded).
    const CalibrationRecord* latest(CalibrationType type, const std::string& sensorId, const std::string& deviceId = {},
                                    const std::string& channelId = {}) const;

    nlohmann::json toJson() const;
    static Result<CalibrationLibrary> fromJson(const nlohmann::json& value);

private:
    std::vector<CalibrationRecord> records_;
};

struct CalibrationStatus {
    bool usable = false;
    std::vector<std::string> problems;
    std::vector<std::string> warnings;
};

// Whether a record can be applied to data spanning [fromHz, toHz] now.
CalibrationStatus checkCalibration(const CalibrationRecord& record, double fromHz, double toHz, const std::string& nowUtc);

// --- applying ----------------------------------------------------------------

// Raw time series / scalar sensitivity: physical = raw / sensitivity. Refused
// for a frequency-dependent record without a stated scalar justification.
Result<std::vector<double>> applySensitivity(const std::vector<double>& raw, const CalibrationRecord& record);
// Spectrum (complex, on frequencies): divides by the complex sensitivity at
// each frequency. Frequencies outside the record's range fail.
Result<std::vector<std::complex<double>>> applyResponse(const std::vector<double>& frequencyHz,
                                                        const std::vector<std::complex<double>>& values,
                                                        const CalibrationRecord& record);

// --- deriving -----------------------------------------------------------------

// A sensor's sensitivity from a recording of a reference source of known
// RMS level at one frequency (e.g. an acoustic calibrator at 1 kHz): the
// amplitude of the recorded tone divided by the reference amplitude.
Result<CalibrationRecord> sensitivityFromReference(const std::vector<double>& recording, double sampleRateHz,
                                                   double referenceRms, double referenceFrequencyHz, Quantity physical,
                                                   Quantity raw, double referenceRelativeUncertainty);

// A frequency-dependent sensitivity by comparison with a calibrated
// reference sensor exposed to the same excitation: S_dut(f) = H(f) S_ref,
// H the transfer function reference -> device under test.
Result<CalibrationRecord> responseByComparison(const std::vector<double>& reference, const std::vector<double>& deviceUnderTest,
                                               double sampleRateHz, double referenceSensitivity, const std::string& unit,
                                               double minHz, double maxHz, size_t segmentLength = 4096,
                                               double minCoherence = 0.95);

// Channel-to-channel alignment from channels that recorded the same signal:
// per channel, the gain ratio and the phase relative to the reference channel
// at `frequencyHz`, and the delay. One record per non-reference channel.
struct ChannelRecording {
    std::string deviceId, channelId;
    std::vector<double> samples;
};
Result<std::vector<CalibrationRecord>> channelToChannel(const std::vector<ChannelRecording>& channels, size_t referenceIndex,
                                                        double sampleRateHz, double frequencyHz, long maxLagSamples);

// An actuator's transfer function, command -> physical output (measured with
// a calibrated sensor), over [minHz, maxHz].
Result<CalibrationRecord> actuatorTransferFunction(const std::vector<double>& command, const std::vector<double>& physicalOutput,
                                                   double sampleRateHz, const std::string& unit, double minHz, double maxHz,
                                                   size_t segmentLength = 4096, double minCoherence = 0.9);

}  // namespace resonant::experiments
