#pragma once

// The experiment: a serializable, reproducible definition of a measurement,
// separate from its execution (ExperimentRunner.h). It can be created,
// configured, saved, validated and inspected without any hardware.
//
// A sensor is a physical transducer with a pose; the instrument channel that
// digitizes its output is a separate thing, and a ChannelBinding connects
// them. The same holds for actuators and output channels. Commanded output
// (a voltage on a channel) is not the physical output (a pressure, a force,
// a field): the actuator's calibration relates the two.
#include "experiments/ExperimentTypes.h"
#include "experiments/Hardware.h"
#include "experiments/Safety.h"
#include "experiments/Spatial.h"
#include "experiments/Uncertainty.h"
#include "foundation/Result.h"
#include "foundation/Types.h"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace resonant::experiments {

enum class ExperimentState { Draft, Validating, Ready, Armed, Running, Stopping, Completed, Failed, Cancelled };
std::string_view toString(ExperimentState state);
Result<ExperimentState> experimentStateFromString(std::string_view text);

enum class SensorCategory {
    Microphone, UltrasonicReceiver, PressureProbe,                                     // acoustic
    Accelerometer, VelocitySensor, DisplacementSensor, StrainGauge, LoadCell, LaserVibrometer,  // mechanical
    RfReceiverChannel, Antenna, NearFieldProbe, FieldStrengthSensor, RfPowerSensor,  // electromagnetic
    GenericVoltage
};
std::string_view toString(SensorCategory category);
Result<SensorCategory> sensorCategoryFromString(std::string_view text);
// The physical quantities a sensor of this category measures directly.
std::vector<Quantity> measurableQuantities(SensorCategory category);

enum class ActuatorCategory { Speaker, UltrasonicTransducer, Shaker, PiezoActuator, RfTransmitter, AntennaElement, GenericSource };
std::string_view toString(ActuatorCategory category);
Result<ActuatorCategory> actuatorCategoryFromString(std::string_view text);

struct Sensor {
    std::string id;
    std::string name;
    SensorCategory category = SensorCategory::Microphone;
    PhysicsDomain domain = PhysicsDomain::Acoustic;
    Pose pose;
    Quantity measures = Quantity::Pressure;
    // Nominal (data-sheet) sensitivity: raw-reading unit per measured unit,
    // e.g. V/Pa. Used only when no calibration is selected, and reported as such.
    double nominalSensitivity = 0.0;
    double dynamicRangeMin = 0.0, dynamicRangeMax = 0.0;  // measured unit
    double noiseFloor = 0.0;                              // measured unit, RMS
    std::string frequencyResponse;                        // a description, or a calibration id
    std::string calibrationId;                            // the selected calibration
    UncertaintyBudget uncertainty;
    std::string status = "ok";
};

struct Actuator {
    std::string id;
    std::string name;
    ActuatorCategory category = ActuatorCategory::Speaker;
    PhysicsDomain domain = PhysicsDomain::Acoustic;
    Pose pose;
    Quantity commandQuantity = Quantity::Voltage;  // what the channel delivers
    Quantity outputQuantity = Quantity::Pressure;  // what the actuator produces
    double minFrequencyHz = 0.0, maxFrequencyHz = 0.0;  // rated band
    double maxCommandAmplitude = 0.0;              // rated, command unit, peak
    double delaySeconds = 0.0;                     // command-to-output delay (nominal; a calibration supersedes)
    std::string calibrationId;                     // command -> output transfer function
    std::string status = "ok";
};

struct ChannelBinding {
    std::string id;
    std::string deviceId;
    std::string channelId;                 // the device's channel id
    ChannelDirection direction = ChannelDirection::Input;
    std::string sensorId;                  // inputs
    std::string actuatorId;                // outputs
    double externalGain = 1.0;             // conditioning between transducer and channel (amplifier), V/V
    std::string calibrationId;             // channel (instrument) calibration
    bool reference = false;                // the reference channel of a transfer-function measurement
};

enum class StimulusKind { None, Sine, LogSweep, LinearChirp, WhiteNoise, Pulse, RfCarrier };
std::string_view toString(StimulusKind kind);
Result<StimulusKind> stimulusKindFromString(std::string_view text);

struct Stimulus {
    StimulusKind kind = StimulusKind::None;
    double amplitude = 0.0;        // peak, command unit (V); RF: unused
    double startHz = 1000.0, endHz = 1000.0;
    double durationSeconds = 1.0;
    double dutyCycle = 1.0;
    double fadeSeconds = 0.01;     // raised-cosine fade in and out
    double powerDbm = -1000.0;     // RfCarrier
    unsigned seed = 1;             // WhiteNoise
    std::vector<std::string> outputBindings;  // channel binding ids
    // Per output binding: a delay (s, a true time shift) and a gain applied to
    // the common stimulus (array steering and focusing). Absent = 0 s, 1.
    std::map<std::string, double> channelDelaySeconds;
    std::map<std::string, double> channelGain;
};

// The command samples of a stimulus at fs (one channel), delayed by `delay`
// seconds (a shift of the whole signal) and scaled by `gain`.
Result<std::vector<double>> generateStimulus(const Stimulus& stimulus, double sampleRateHz, double delaySeconds = 0.0,
                                             double gain = 1.0);

enum class AcquisitionMode { Finite, Continuous, Triggered, Repeated };
std::string_view toString(AcquisitionMode mode);

struct AcquisitionSettings {
    AcquisitionMode mode = AcquisitionMode::Finite;
    double sampleRateHz = 48000.0;
    double durationSeconds = 1.0;
    int repetitions = 1;
    size_t blockFrames = 1024;
    size_t queueBlocks = 64;           // bounded buffer between device and recorder
    double maxBackpressureSeconds = 2.0;
    SampleFormat format = SampleFormat::Float32;
    double clipFraction = 0.999;       // |reading| >= clipFraction * range counts as clipped
    bool recordToDisk = true;
    double centerFrequencyHz = 0.0;    // RF receivers
    double gainDb = 0.0;
};

enum class TriggerKind { Immediate, Software, External, Level };
std::string_view toString(TriggerKind kind);

struct TriggerConfig {
    TriggerKind kind = TriggerKind::Immediate;
    std::string source;
    double level = 0.0;
    bool risingEdge = true;
    double timeoutSeconds = 10.0;
    double preTriggerSeconds = 0.0;
};

struct ClockDomain {
    std::string id;
    std::string deviceId;
    std::string source = "internal";   // internal, external-10MHz, pps, shared:<domain>
    double nominalHz = 0.0;
    double referenceHz = 0.0;
    double offsetSeconds = 0.0;
    double driftPpm = 0.0;
    double uncertaintySeconds = 0.0;
};

struct SynchronizationConfig {
    // A phase-sensitive experiment compares phases or delays between
    // channels; it needs a common clock or a timing calibration.
    bool phaseSensitive = false;
    std::string strategy = "none";     // none, single-device, shared-clock, shared-trigger, timing-calibration
    std::vector<ClockDomain> clockDomains;
    std::vector<std::string> timingCalibrationIds;
    std::map<std::string, double> channelDelayCorrection;  // binding id -> s, subtracted
};

struct EnvironmentConditions {
    double temperatureC = 20.0;
    double relativeHumidity = 0.0;     // %; 0 = not recorded
    double pressurePa = 0.0;           // 0 = not recorded
    bool recorded = false;             // the values were measured, not assumed
    std::string notes;
};

struct StepLog {
    int step = 0;
    std::string name;
    std::string status;   // ok, warning, failed, skipped
    std::string message;
    double seconds = 0.0;
};

struct ExecutionRecord {
    std::string runId;
    std::string startedUtc, endedUtc;
    ExperimentState finalState = ExperimentState::Draft;
    std::vector<StepLog> steps;
    std::vector<std::string> measurementIds;
    std::string message;
    std::string operatorName;
    bool outputAuthorized = false;
    std::string reportPath;
};

struct Annotation {
    std::string timeUtc;
    std::string author;
    std::string text;
};

struct Experiment {
    std::string id;
    std::string name;
    std::string description;
    PhysicsDomain domain = PhysicsDomain::Acoustic;
    std::string templateId;
    std::string projectId;
    int revision = 1;
    std::map<std::string, DeviceConfiguration> devices;  // device id -> configuration used
    std::vector<Sensor> sensors;
    std::vector<Actuator> actuators;
    std::vector<ChannelBinding> channels;
    AcquisitionSettings acquisition;
    Stimulus stimulus;
    TriggerConfig trigger;
    SynchronizationConfig synchronization;
    ScanDefinition positions;
    std::string positionerDeviceId;   // moves the probe through `positions`; empty = fixed sensors
    std::string probeBindingId;       // the input binding whose sensor the positioner moves
    std::string sweepDeviceId;        // a network analyser measuring `sweep` (a physical output: needs authorization)
    SweepSettings sweep;
    EnvironmentConditions environment;
    SafetyLimits safety;
    bool requireCalibration = false;  // refuse to run with an uncalibrated sensor
    std::vector<ProcessingStep> processing;
    std::vector<ExecutionRecord> history;
    std::vector<std::string> resultIds;
    std::string notes;
    std::vector<Annotation> annotations;
    ExperimentState state = ExperimentState::Draft;

    const Sensor* sensor(const std::string& id) const;
    const Actuator* actuator(const std::string& id) const;
    const ChannelBinding* binding(const std::string& id) const;
    std::vector<const ChannelBinding*> inputBindings() const;
    std::vector<const ChannelBinding*> outputBindings() const;
    bool hasPhysicalOutput() const;
};

nlohmann::json toJson(const Experiment& experiment);
Result<Experiment> experimentFromJson(const nlohmann::json& value);

struct ValidationReport {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    bool ok() const { return errors.empty(); }
};

// Checks the definition alone (no devices): unique ids, references, sensor /
// quantity compatibility, sample rate against the stimulus, stimulus against
// the actuators' ratings and the safety limits, synchronization for
// phase-sensitive experiments, scan positions.
ValidationReport validateExperiment(const Experiment& experiment);

}  // namespace resonant::experiments
