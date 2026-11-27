#pragma once

// The unified measurement record: the metadata of one data set, raw or
// derived. The numbers themselves live in the measurement store (HDF5 next
// to the project); the record says what they are.
//
// Raw records are written once and never changed. A derived record names
// its sources and the complete processing history that produced it, so its
// values can be reproduced from the raw data.
#include "experiments/ExperimentTypes.h"
#include "experiments/Uncertainty.h"
#include "foundation/Result.h"
#include "foundation/Types.h"

#include <nlohmann/json.hpp>

#include <complex>
#include <optional>
#include <string>
#include <vector>

namespace resonant::experiments {

enum class MeasurementKind {
    TimeSeries,        // samples on a uniform time base
    Spectrum,          // complex amplitudes on a frequency axis
    PowerSpectrum,     // one-sided density on a frequency axis (real)
    TransferFunction,  // complex ratio on a frequency axis, with coherence
    NetworkParameters, // S-parameters on a frequency axis (ports x ports per frequency)
    Scalar,            // one value (RMS, level, delay, ...)
    Vector             // a few values with names
};
std::string_view toString(MeasurementKind kind);
Result<MeasurementKind> measurementKindFromString(std::string_view text);

struct DataQuality {
    uint64_t expectedFrames = 0;
    uint64_t recordedFrames = 0;
    uint64_t droppedFrames = 0;           // reported lost by the device or the recorder
    uint64_t gaps = 0;                    // separate places where frames are missing
    uint64_t clippedSamples = 0;
    bool saturated = false;               // a run of clipped samples (the reading is not usable there)
    uint64_t timestampDiscontinuities = 0;
    uint64_t invalidTimestamps = 0;
    std::vector<std::string> deviceErrors;
    bool complete = true;                 // false when cancelled or ended by an error
    double peakAbsolute = 0.0;            // raw units
    std::vector<std::string> notes;

    bool clean() const;
    std::string summary() const;
};

nlohmann::json toJson(const DataQuality& quality);
DataQuality dataQualityFromJson(const nlohmann::json& value);

struct TimeBase {
    double sampleRateHz = 0.0;
    std::string startUtc;
    double startSeconds = 0.0;       // device clock (hardware timestamp) or software clock
    std::string clockDomain;         // the clock domain id the samples were taken in
    bool hardwareTimestamps = false;
    double timingCorrectionSeconds = 0.0;  // applied delay correction (from a timing calibration)
};

struct MeasurementRecord {
    std::string id;
    std::string experimentId;
    std::string runId;
    std::string bindingId;            // the channel binding
    std::string sensorId;
    std::string deviceId, channelId;
    PhysicsDomain domain = PhysicsDomain::Acoustic;
    MeasurementKind kind = MeasurementKind::TimeSeries;
    DataCategory category = DataCategory::Raw;
    QuantitySpec quantity;
    TimeBase timeBase;
    std::optional<Pose> pose;
    std::string dataRef;              // the data set in the measurement store
    std::vector<size_t> shape;        // e.g. {frames} or {frequencies}
    SampleFormat nativeFormat = SampleFormat::Float64;
    bool simulatedDevice = false;     // acquired from a simulated device: generated, not measured
    std::string calibrationId;        // the calibration applied (calibrated / processed data)
    std::vector<std::string> sourceIds;
    std::vector<ProcessingStep> history;
    UncertaintyBudget uncertainty;
    DataQuality quality;
    nlohmann::json acquisition = nlohmann::json::object();  // device configuration and settings used
    std::string createdUtc;
    std::string note;
};

nlohmann::json toJson(const MeasurementRecord& record);
Result<MeasurementRecord> measurementRecordFromJson(const nlohmann::json& value);

// The values of a record in memory.
struct MeasurementData {
    std::vector<double> real;                  // TimeSeries, PowerSpectrum, Scalar, Vector
    std::vector<std::complex<double>> complex; // Spectrum, TransferFunction, NetworkParameters, I/Q time series
    std::vector<double> axis;                  // frequency (Hz) or angle for frequency-domain kinds
    std::vector<double> auxiliary;             // coherence of a transfer function, ...
    std::vector<std::string> labels;           // Vector
    bool isComplex() const { return !complex.empty(); }
    size_t size() const { return isComplex() ? complex.size() : real.size(); }
};

}  // namespace resonant::experiments
