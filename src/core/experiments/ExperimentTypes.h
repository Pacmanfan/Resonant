#pragma once

// Vocabulary shared by the experiment framework: what a number measures and
// in which unit, what kind of data it is (raw reading, calibrated, processed,
// simulated, reconstructed), and how an instrument delivers its samples.
//
// The framework is domain independent: it names quantities and units but
// never interprets them physically. Physical interpretation (sound pressure
// level, accelerance, return loss, ...) belongs to the domain modules.
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace resonant::experiments {

// A measured or simulated physical quantity. Each has one SI unit; values in
// the framework are always in that unit.
enum class Quantity {
    Voltage,              // V
    DigitalFullScale,     // FS: an instrument reading relative to its full scale (uncalibrated)
    Pressure,             // Pa
    Acceleration,         // m/s^2
    Velocity,             // m/s
    Displacement,         // m
    Force,                // N
    Strain,               // m/m
    Stress,               // Pa (never measured directly: inferred from strain and a material model)
    Power,                // W
    ElectricField,        // V/m
    MagneticField,        // A/m
    ScatteringParameter,  // 1 (S_ij)
    Impedance,            // ohm
    Temperature,          // K
    Angle,                // rad
    Time,                 // s (delays, time constants)
    Frequency,            // Hz (identified resonances)
    Dimensionless         // 1
};

std::string_view toString(Quantity quantity);
Result<Quantity> quantityFromString(std::string_view text);
std::string_view siUnit(Quantity quantity);

// A quantity, or a ratio of two (a transfer function, a sensitivity).
struct QuantitySpec {
    Quantity quantity = Quantity::Voltage;
    std::optional<Quantity> per;  // the denominator of a ratio
    bool spectralDensity = false; // a power spectral density: (unit)^2/Hz

    std::string unit() const;
    std::string describe() const;
    friend bool operator==(const QuantitySpec&, const QuantitySpec&) = default;
};

nlohmann::json toJson(const QuantitySpec& spec);
Result<QuantitySpec> quantitySpecFromJson(const nlohmann::json& value);

// What a data set is. These are never interchangeable and every report and
// display names them separately.
enum class DataCategory {
    Raw,            // instrument readings as acquired; immutable
    Calibrated,     // raw readings converted to physical units with a named calibration
    Processed,      // derived by a recorded processing chain (spectra, transfer functions, ...)
    Simulated,      // calculated by a model
    Reconstructed,  // estimated between measured points (interpolation); not measured
    Command         // the signal sent to an output: what was commanded, not what the actuator produced
};

std::string_view toString(DataCategory category);
Result<DataCategory> dataCategoryFromString(std::string_view text);

// How an instrument delivers its samples (the stored raw data is always the
// reading as double precision, losslessly converted from this format).
enum class SampleFormat { Int16, Int24, Int32, Float32, Float64, ComplexInt16, ComplexFloat32, ComplexFloat64 };

std::string_view toString(SampleFormat format);
Result<SampleFormat> sampleFormatFromString(std::string_view text);
bool isComplex(SampleFormat format);
int bitsOf(SampleFormat format);  // per real component

// The quantization step of a format relative to its full scale (2 / 2^bits
// for integer formats, 0 for floating point).
double quantizationStep(SampleFormat format, double fullScale);

// A 3-D pose in a named coordinate frame ("scene" is the project's world
// frame). The direction is the sensitive axis of a sensor or the pointing
// direction of an actuator.
struct Pose {
    double x = 0.0, y = 0.0, z = 0.0;     // m
    double dx = 0.0, dy = 0.0, dz = 1.0;  // unit vector
    std::string frame = "scene";
    double positionUncertainty = 0.0;     // m, standard
    double orientationUncertaintyDeg = 0.0;
    friend bool operator==(const Pose&, const Pose&) = default;
};

nlohmann::json toJson(const Pose& pose);
Result<Pose> poseFromJson(const nlohmann::json& value);

// One explicit, reproducible processing operation (Processing.h lists them).
struct ProcessingStep {
    std::string operation;
    nlohmann::json parameters = nlohmann::json::object();
    std::string description;  // filled in when the step runs: what it did, with every parameter
    friend bool operator==(const ProcessingStep&, const ProcessingStep&) = default;
};

nlohmann::json toJson(const ProcessingStep& step);
Result<ProcessingStep> processingStepFromJson(const nlohmann::json& value);

}  // namespace resonant::experiments
