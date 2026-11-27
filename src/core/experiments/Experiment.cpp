#include "experiments/Experiment.h"

#include "foundation/Units.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>

namespace resonant::experiments {

namespace {

template <typename E, size_t N>
using Table = std::array<std::pair<E, std::string_view>, N>;

template <typename E, size_t N>
std::string_view nameIn(const Table<E, N>& table, E value)
{
    for (const auto& [v, name] : table)
        if (v == value)
            return name;
    return table.front().second;
}

template <typename E, size_t N>
Result<E> valueIn(const Table<E, N>& table, std::string_view text, const char* what)
{
    for (const auto& [v, name] : table)
        if (name == text)
            return v;
    std::string names;
    for (const auto& entry : table)
        names += (names.empty() ? "" : ", ") + std::string(entry.second);
    return Result<E>::failure(std::string("unknown ") + what + " '" + std::string(text) + "' (" + names + ")");
}

constexpr Table<ExperimentState, 9> kStates{{{ExperimentState::Draft, "draft"},
                                             {ExperimentState::Validating, "validating"},
                                             {ExperimentState::Ready, "ready"},
                                             {ExperimentState::Armed, "armed"},
                                             {ExperimentState::Running, "running"},
                                             {ExperimentState::Stopping, "stopping"},
                                             {ExperimentState::Completed, "completed"},
                                             {ExperimentState::Failed, "failed"},
                                             {ExperimentState::Cancelled, "cancelled"}}};

constexpr Table<SensorCategory, 15> kSensorCategories{{{SensorCategory::Microphone, "microphone"},
                                                       {SensorCategory::UltrasonicReceiver, "ultrasonic-receiver"},
                                                       {SensorCategory::PressureProbe, "pressure-probe"},
                                                       {SensorCategory::Accelerometer, "accelerometer"},
                                                       {SensorCategory::VelocitySensor, "velocity-sensor"},
                                                       {SensorCategory::DisplacementSensor, "displacement-sensor"},
                                                       {SensorCategory::StrainGauge, "strain-gauge"},
                                                       {SensorCategory::LoadCell, "load-cell"},
                                                       {SensorCategory::LaserVibrometer, "laser-vibrometer"},
                                                       {SensorCategory::RfReceiverChannel, "rf-receiver-channel"},
                                                       {SensorCategory::Antenna, "antenna"},
                                                       {SensorCategory::NearFieldProbe, "near-field-probe"},
                                                       {SensorCategory::FieldStrengthSensor, "field-strength-sensor"},
                                                       {SensorCategory::RfPowerSensor, "rf-power-sensor"},
                                                       {SensorCategory::GenericVoltage, "generic-voltage"}}};

constexpr Table<ActuatorCategory, 7> kActuatorCategories{{{ActuatorCategory::Speaker, "speaker"},
                                                          {ActuatorCategory::UltrasonicTransducer, "ultrasonic-transducer"},
                                                          {ActuatorCategory::Shaker, "shaker"},
                                                          {ActuatorCategory::PiezoActuator, "piezo-actuator"},
                                                          {ActuatorCategory::RfTransmitter, "rf-transmitter"},
                                                          {ActuatorCategory::AntennaElement, "antenna-element"},
                                                          {ActuatorCategory::GenericSource, "generic-source"}}};

constexpr Table<StimulusKind, 7> kStimuli{{{StimulusKind::None, "none"},
                                           {StimulusKind::Sine, "sine"},
                                           {StimulusKind::LogSweep, "log-sweep"},
                                           {StimulusKind::LinearChirp, "linear-chirp"},
                                           {StimulusKind::WhiteNoise, "white-noise"},
                                           {StimulusKind::Pulse, "pulse"},
                                           {StimulusKind::RfCarrier, "rf-carrier"}}};

constexpr Table<AcquisitionMode, 4> kModes{{{AcquisitionMode::Finite, "finite"},
                                            {AcquisitionMode::Continuous, "continuous"},
                                            {AcquisitionMode::Triggered, "triggered"},
                                            {AcquisitionMode::Repeated, "repeated"}}};

constexpr Table<TriggerKind, 4> kTriggers{{{TriggerKind::Immediate, "immediate"},
                                           {TriggerKind::Software, "software"},
                                           {TriggerKind::External, "external"},
                                           {TriggerKind::Level, "level"}}};

constexpr Table<ChannelDirection, 2> kDirections{{{ChannelDirection::Input, "input"}, {ChannelDirection::Output, "output"}}};

Result<PhysicsDomain> domainFrom(const nlohmann::json& v)
{
    const std::string text = v.value("domain", std::string("acoustic"));
    if (auto d = physicsDomainFromString(text))
        return *d;
    return Result<PhysicsDomain>::failure("unknown physics domain '" + text + "'");
}

// SplitMix64: a portable generator, so a noise stimulus is the same on every platform.
struct SplitMix {
    uint64_t state;
    double uniform()
    {
        uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        z ^= z >> 31;
        return static_cast<double>(z >> 11) * (1.0 / 9007199254740992.0);  // [0, 1)
    }
};

nlohmann::json sensorJson(const Sensor& s)
{
    return {{"id", s.id},
            {"name", s.name},
            {"category", nameIn(kSensorCategories, s.category)},
            {"domain", toString(s.domain)},
            {"pose", toJson(s.pose)},
            {"measures", toString(s.measures)},
            {"unit", siUnit(s.measures)},
            {"nominalSensitivity", s.nominalSensitivity},
            {"dynamicRange", {s.dynamicRangeMin, s.dynamicRangeMax}},
            {"noiseFloor", s.noiseFloor},
            {"frequencyResponse", s.frequencyResponse},
            {"calibrationId", s.calibrationId},
            {"uncertainty", toJson(s.uncertainty)},
            {"status", s.status}};
}

Result<Sensor> sensorFrom(const nlohmann::json& v)
{
    Sensor s;
    s.id = v.value("id", std::string());
    s.name = v.value("name", s.id);
    auto category = valueIn(kSensorCategories, v.value("category", std::string("microphone")), "sensor category");
    auto domain = domainFrom(v);
    auto pose = poseFromJson(v.value("pose", nlohmann::json::object()));
    auto measures = quantityFromString(v.value("measures", std::string("pressure")));
    auto uncertainty = uncertaintyBudgetFromJson(v.value("uncertainty", nlohmann::json()));
    for (const std::string* problem : {category ? nullptr : &category.message(), domain ? nullptr : &domain.message(),
                                       pose ? nullptr : &pose.message(), measures ? nullptr : &measures.message(),
                                       uncertainty ? nullptr : &uncertainty.message()})
        if (problem)
            return Result<Sensor>::failure("sensor '" + s.id + "': " + *problem);
    s.category = category.value();
    s.domain = domain.value();
    s.pose = pose.value();
    s.measures = measures.value();
    s.uncertainty = uncertainty.value();
    s.nominalSensitivity = v.value("nominalSensitivity", 0.0);
    const auto range = v.value("dynamicRange", nlohmann::json::array({0.0, 0.0}));
    if (range.is_array() && range.size() == 2) {
        s.dynamicRangeMin = range[0].get<double>();
        s.dynamicRangeMax = range[1].get<double>();
    }
    s.noiseFloor = v.value("noiseFloor", 0.0);
    s.frequencyResponse = v.value("frequencyResponse", std::string());
    s.calibrationId = v.value("calibrationId", std::string());
    s.status = v.value("status", std::string("ok"));
    return s;
}

nlohmann::json actuatorJson(const Actuator& a)
{
    return {{"id", a.id},
            {"name", a.name},
            {"category", nameIn(kActuatorCategories, a.category)},
            {"domain", toString(a.domain)},
            {"pose", toJson(a.pose)},
            {"commandQuantity", toString(a.commandQuantity)},
            {"outputQuantity", toString(a.outputQuantity)},
            {"ratedBandHz", {a.minFrequencyHz, a.maxFrequencyHz}},
            {"maxCommandAmplitude", a.maxCommandAmplitude},
            {"delaySeconds", a.delaySeconds},
            {"calibrationId", a.calibrationId},
            {"status", a.status}};
}

Result<Actuator> actuatorFrom(const nlohmann::json& v)
{
    Actuator a;
    a.id = v.value("id", std::string());
    a.name = v.value("name", a.id);
    auto category = valueIn(kActuatorCategories, v.value("category", std::string("speaker")), "actuator category");
    auto domain = domainFrom(v);
    auto pose = poseFromJson(v.value("pose", nlohmann::json::object()));
    auto command = quantityFromString(v.value("commandQuantity", std::string("voltage")));
    auto output = quantityFromString(v.value("outputQuantity", std::string("pressure")));
    for (const std::string* problem : {category ? nullptr : &category.message(), domain ? nullptr : &domain.message(),
                                       pose ? nullptr : &pose.message(), command ? nullptr : &command.message(),
                                       output ? nullptr : &output.message()})
        if (problem)
            return Result<Actuator>::failure("actuator '" + a.id + "': " + *problem);
    a.category = category.value();
    a.domain = domain.value();
    a.pose = pose.value();
    a.commandQuantity = command.value();
    a.outputQuantity = output.value();
    const auto band = v.value("ratedBandHz", nlohmann::json::array({0.0, 0.0}));
    if (band.is_array() && band.size() == 2) {
        a.minFrequencyHz = band[0].get<double>();
        a.maxFrequencyHz = band[1].get<double>();
    }
    a.maxCommandAmplitude = v.value("maxCommandAmplitude", 0.0);
    a.delaySeconds = v.value("delaySeconds", 0.0);
    a.calibrationId = v.value("calibrationId", std::string());
    a.status = v.value("status", std::string("ok"));
    return a;
}

}  // namespace

std::string_view toString(ExperimentState state) { return nameIn(kStates, state); }
Result<ExperimentState> experimentStateFromString(std::string_view text) { return valueIn(kStates, text, "experiment state"); }
std::string_view toString(SensorCategory category) { return nameIn(kSensorCategories, category); }
Result<SensorCategory> sensorCategoryFromString(std::string_view text) { return valueIn(kSensorCategories, text, "sensor category"); }
std::string_view toString(ActuatorCategory category) { return nameIn(kActuatorCategories, category); }
Result<ActuatorCategory> actuatorCategoryFromString(std::string_view text)
{
    return valueIn(kActuatorCategories, text, "actuator category");
}
std::string_view toString(StimulusKind kind) { return nameIn(kStimuli, kind); }
Result<StimulusKind> stimulusKindFromString(std::string_view text) { return valueIn(kStimuli, text, "stimulus"); }
std::string_view toString(AcquisitionMode mode) { return nameIn(kModes, mode); }
std::string_view toString(TriggerKind kind) { return nameIn(kTriggers, kind); }

std::vector<Quantity> measurableQuantities(SensorCategory category)
{
    switch (category) {
    case SensorCategory::Microphone:
    case SensorCategory::UltrasonicReceiver:
    case SensorCategory::PressureProbe: return {Quantity::Pressure};
    case SensorCategory::Accelerometer: return {Quantity::Acceleration};
    case SensorCategory::VelocitySensor: return {Quantity::Velocity};
    case SensorCategory::DisplacementSensor: return {Quantity::Displacement};
    case SensorCategory::StrainGauge: return {Quantity::Strain};
    case SensorCategory::LoadCell: return {Quantity::Force};
    case SensorCategory::LaserVibrometer: return {Quantity::Velocity, Quantity::Displacement};
    case SensorCategory::RfReceiverChannel: return {Quantity::DigitalFullScale, Quantity::Voltage};
    case SensorCategory::Antenna: return {Quantity::Voltage, Quantity::Power, Quantity::DigitalFullScale};
    case SensorCategory::NearFieldProbe: return {Quantity::Voltage, Quantity::ElectricField, Quantity::MagneticField};
    case SensorCategory::FieldStrengthSensor: return {Quantity::ElectricField};
    case SensorCategory::RfPowerSensor: return {Quantity::Power};
    case SensorCategory::GenericVoltage: return {Quantity::Voltage, Quantity::DigitalFullScale};
    }
    return {};
}

Result<std::vector<double>> generateStimulus(const Stimulus& s, double fs, double delaySeconds, double gain)
{
    if (!(fs > 0.0))
        return Result<std::vector<double>>::failure("the sample rate must be positive");
    if (s.kind == StimulusKind::None)
        return std::vector<double>{};
    if (s.kind == StimulusKind::RfCarrier)
        return Result<std::vector<double>>::failure("an RF carrier is set on the source, it has no command samples");
    if (!(s.durationSeconds > 0.0) || !(s.amplitude >= 0.0))
        return Result<std::vector<double>>::failure("a stimulus needs a positive duration and a non-negative amplitude");
    if (!(s.dutyCycle > 0.0 && s.dutyCycle <= 1.0))
        return Result<std::vector<double>>::failure("the duty cycle must lie in (0, 1]");
    const size_t total = static_cast<size_t>(std::llround(s.durationSeconds * fs));
    const size_t active = static_cast<size_t>(std::llround(s.durationSeconds * s.dutyCycle * fs));
    std::vector<double> x(total, 0.0);
    const double T = static_cast<double>(active) / fs;
    const double f1 = s.startHz, f2 = s.endHz;
    if ((s.kind == StimulusKind::LogSweep || s.kind == StimulusKind::LinearChirp) && !(f1 > 0.0 && f2 > f1))
        return Result<std::vector<double>>::failure("a sweep needs 0 < start frequency < end frequency");
    if ((s.kind == StimulusKind::Sine || s.kind == StimulusKind::Pulse) && !(f1 > 0.0))
        return Result<std::vector<double>>::failure("the stimulus frequency must be positive");
    if (!(delaySeconds >= 0.0))
        return Result<std::vector<double>>::failure("a stimulus delay must not be negative");
    SplitMix random{s.seed};
    const double twoPi = 2.0 * units::pi;
    const size_t offset = static_cast<size_t>(std::ceil(delaySeconds * fs));
    if (offset >= total)
        return std::vector<double>(total, 0.0);
    x.assign(total, 0.0);
    std::vector<double> shaped(active, 0.0);
    for (size_t i = 0; i < active; ++i) {
        const double t = static_cast<double>(i) / fs;
        double v = 0.0;
        switch (s.kind) {
        case StimulusKind::Sine: v = std::sin(twoPi * f1 * t); break;
        case StimulusKind::LogSweep: {
            // Exponential sweep (Farina): instantaneous frequency f1 (f2/f1)^(t/T).
            const double r = std::log(f2 / f1);
            v = std::sin(twoPi * f1 * T / r * (std::exp(t / T * r) - 1.0));
            break;
        }
        case StimulusKind::LinearChirp: v = std::sin(twoPi * (f1 * t + 0.5 * (f2 - f1) / T * t * t)); break;
        case StimulusKind::WhiteNoise: v = 2.0 * random.uniform() - 1.0; break;
        case StimulusKind::Pulse: {
            const double width = 1.0 / f1;  // one Hann pulse of length 1 / f
            v = t < width ? 0.5 * (1.0 - std::cos(twoPi * t / width)) : 0.0;
            break;
        }
        default: break;
        }
        shaped[i] = s.amplitude * gain * v;
    }
    // Raised-cosine fades, so the switching transients do not dominate the spectrum.
    if (s.kind != StimulusKind::Pulse && s.fadeSeconds > 0.0) {
        const size_t fade = std::min(active / 2, static_cast<size_t>(std::llround(s.fadeSeconds * fs)));
        for (size_t i = 0; i < fade; ++i) {
            const double g = 0.5 * (1.0 - std::cos(units::pi * static_cast<double>(i) / static_cast<double>(fade)));
            shaped[i] *= g;
            shaped[active - 1 - i] *= g;
        }
    }
    // The delay: integer part by shifting, fractional part by linear interpolation
    // between neighbouring samples (adequate for signals well below fs / 4).
    const double fraction = static_cast<double>(offset) - delaySeconds * fs;  // in [0, 1)
    for (size_t i = offset; i < total; ++i) {
        const double position = static_cast<double>(i - offset) + fraction;
        const size_t k = static_cast<size_t>(position);
        const double w = position - static_cast<double>(k);
        const double a = k < active ? shaped[k] : 0.0, b = k + 1 < active ? shaped[k + 1] : 0.0;
        x[i] = (1.0 - w) * a + w * b;
    }
    return x;
}

const Sensor* Experiment::sensor(const std::string& sid) const
{
    for (const auto& s : sensors)
        if (s.id == sid)
            return &s;
    return nullptr;
}

const Actuator* Experiment::actuator(const std::string& aid) const
{
    for (const auto& a : actuators)
        if (a.id == aid)
            return &a;
    return nullptr;
}

const ChannelBinding* Experiment::binding(const std::string& bid) const
{
    for (const auto& b : channels)
        if (b.id == bid)
            return &b;
    return nullptr;
}

std::vector<const ChannelBinding*> Experiment::inputBindings() const
{
    std::vector<const ChannelBinding*> out;
    for (const auto& b : channels)
        if (b.direction == ChannelDirection::Input)
            out.push_back(&b);
    return out;
}

std::vector<const ChannelBinding*> Experiment::outputBindings() const
{
    std::vector<const ChannelBinding*> out;
    for (const auto& b : channels)
        if (b.direction == ChannelDirection::Output)
            out.push_back(&b);
    return out;
}

bool Experiment::hasPhysicalOutput() const
{
    return (stimulus.kind != StimulusKind::None && !stimulus.outputBindings.empty()) || !sweepDeviceId.empty();
}

nlohmann::json toJson(const Experiment& e)
{
    nlohmann::json devices = nlohmann::json::object();
    for (const auto& [id, config] : e.devices)
        devices[id] = toJson(config);
    nlohmann::json sensors = nlohmann::json::array(), actuators = nlohmann::json::array(), channels = nlohmann::json::array();
    for (const auto& s : e.sensors)
        sensors.push_back(sensorJson(s));
    for (const auto& a : e.actuators)
        actuators.push_back(actuatorJson(a));
    for (const auto& b : e.channels)
        channels.push_back({{"id", b.id},
                            {"deviceId", b.deviceId},
                            {"channelId", b.channelId},
                            {"direction", nameIn(kDirections, b.direction)},
                            {"sensorId", b.sensorId},
                            {"actuatorId", b.actuatorId},
                            {"externalGain", b.externalGain},
                            {"calibrationId", b.calibrationId},
                            {"reference", b.reference}});
    const auto& a = e.acquisition;
    const auto& st = e.stimulus;
    const auto& t = e.trigger;
    nlohmann::json domains = nlohmann::json::array();
    for (const auto& d : e.synchronization.clockDomains)
        domains.push_back({{"id", d.id},
                           {"deviceId", d.deviceId},
                           {"source", d.source},
                           {"nominalHz", d.nominalHz},
                           {"referenceHz", d.referenceHz},
                           {"offsetSeconds", d.offsetSeconds},
                           {"driftPpm", d.driftPpm},
                           {"uncertaintySeconds", d.uncertaintySeconds}});
    nlohmann::json processing = nlohmann::json::array();
    for (const auto& p : e.processing)
        processing.push_back(toJson(p));
    nlohmann::json history = nlohmann::json::array();
    for (const auto& h : e.history) {
        nlohmann::json steps = nlohmann::json::array();
        for (const auto& s : h.steps)
            steps.push_back({{"step", s.step}, {"name", s.name}, {"status", s.status}, {"message", s.message}, {"seconds", s.seconds}});
        history.push_back({{"runId", h.runId},
                           {"startedUtc", h.startedUtc},
                           {"endedUtc", h.endedUtc},
                           {"finalState", toString(h.finalState)},
                           {"steps", steps},
                           {"measurementIds", h.measurementIds},
                           {"message", h.message},
                           {"operator", h.operatorName},
                           {"outputAuthorized", h.outputAuthorized},
                           {"reportPath", h.reportPath}});
    }
    nlohmann::json annotations = nlohmann::json::array();
    for (const auto& n : e.annotations)
        annotations.push_back({{"timeUtc", n.timeUtc}, {"author", n.author}, {"text", n.text}});
    // A run's transient states are not persisted: a saved experiment is a draft,
    // ready, or the outcome of its last run.
    ExperimentState persisted = e.state;
    if (persisted == ExperimentState::Validating || persisted == ExperimentState::Armed || persisted == ExperimentState::Running ||
        persisted == ExperimentState::Stopping)
        persisted = ExperimentState::Draft;
    return {{"id", e.id},
            {"name", e.name},
            {"description", e.description},
            {"domain", toString(e.domain)},
            {"templateId", e.templateId},
            {"projectId", e.projectId},
            {"revision", e.revision},
            {"devices", devices},
            {"sensors", sensors},
            {"actuators", actuators},
            {"channels", channels},
            {"acquisition",
             {{"mode", toString(a.mode)},
              {"sampleRateHz", a.sampleRateHz},
              {"durationSeconds", a.durationSeconds},
              {"repetitions", a.repetitions},
              {"blockFrames", a.blockFrames},
              {"queueBlocks", a.queueBlocks},
              {"maxBackpressureSeconds", a.maxBackpressureSeconds},
              {"format", toString(a.format)},
              {"clipFraction", a.clipFraction},
              {"recordToDisk", a.recordToDisk},
              {"centerFrequencyHz", a.centerFrequencyHz},
              {"gainDb", a.gainDb}}},
            {"stimulus",
             {{"kind", toString(st.kind)},
              {"amplitude", st.amplitude},
              {"startHz", st.startHz},
              {"endHz", st.endHz},
              {"durationSeconds", st.durationSeconds},
              {"dutyCycle", st.dutyCycle},
              {"fadeSeconds", st.fadeSeconds},
              {"powerDbm", st.powerDbm},
              {"seed", st.seed},
              {"outputBindings", st.outputBindings},
              {"channelDelaySeconds", st.channelDelaySeconds},
              {"channelGain", st.channelGain}}},
            {"trigger",
             {{"kind", toString(t.kind)},
              {"source", t.source},
              {"level", t.level},
              {"risingEdge", t.risingEdge},
              {"timeoutSeconds", t.timeoutSeconds},
              {"preTriggerSeconds", t.preTriggerSeconds}}},
            {"synchronization",
             {{"phaseSensitive", e.synchronization.phaseSensitive},
              {"strategy", e.synchronization.strategy},
              {"clockDomains", domains},
              {"timingCalibrationIds", e.synchronization.timingCalibrationIds},
              {"channelDelayCorrection", e.synchronization.channelDelayCorrection}}},
            {"positions", toJson(e.positions)},
            {"positionerDeviceId", e.positionerDeviceId},
            {"probeBindingId", e.probeBindingId},
            {"sweepDeviceId", e.sweepDeviceId},
            {"sweep",
             {{"startHz", e.sweep.startHz},
              {"stopHz", e.sweep.stopHz},
              {"points", e.sweep.points},
              {"powerDbm", e.sweep.powerDbm},
              {"ifBandwidthHz", e.sweep.ifBandwidthHz},
              {"ports", e.sweep.ports}}},
            {"environment",
             {{"temperatureC", e.environment.temperatureC},
              {"relativeHumidity", e.environment.relativeHumidity},
              {"pressurePa", e.environment.pressurePa},
              {"recorded", e.environment.recorded},
              {"notes", e.environment.notes}}},
            {"safety", toJson(e.safety)},
            {"requireCalibration", e.requireCalibration},
            {"processing", processing},
            {"history", history},
            {"resultIds", e.resultIds},
            {"notes", e.notes},
            {"annotations", annotations},
            {"state", toString(persisted)}};
}

Result<Experiment> experimentFromJson(const nlohmann::json& v)
{
    using R = Result<Experiment>;
    if (!v.is_object())
        return R::failure("an experiment must be an object");
    Experiment e;
    e.id = v.value("id", std::string());
    if (e.id.empty())
        return R::failure("an experiment needs an id");
    auto fail = [&](const std::string& message) { return R::failure("experiment '" + e.id + "': " + message); };
    e.name = v.value("name", e.id);
    e.description = v.value("description", std::string());
    auto domain = domainFrom(v);
    if (!domain)
        return fail(domain.message());
    e.domain = domain.value();
    e.templateId = v.value("templateId", std::string());
    e.projectId = v.value("projectId", std::string());
    e.revision = v.value("revision", 1);
    const nlohmann::json devices = v.value("devices", nlohmann::json::object());  // items() of a temporary would dangle
    for (const auto& [id, config] : devices.items()) {
        auto c = deviceConfigurationFromJson(config);
        if (!c)
            return fail("device '" + id + "': " + c.message());
        e.devices[id] = c.value();
    }
    for (const auto& s : v.value("sensors", nlohmann::json::array())) {
        auto sensor = sensorFrom(s);
        if (!sensor)
            return fail(sensor.message());
        e.sensors.push_back(sensor.value());
    }
    for (const auto& a : v.value("actuators", nlohmann::json::array())) {
        auto actuator = actuatorFrom(a);
        if (!actuator)
            return fail(actuator.message());
        e.actuators.push_back(actuator.value());
    }
    for (const auto& c : v.value("channels", nlohmann::json::array())) {
        ChannelBinding b;
        b.id = c.value("id", std::string());
        b.deviceId = c.value("deviceId", std::string());
        b.channelId = c.value("channelId", std::string());
        auto direction = valueIn(kDirections, c.value("direction", std::string("input")), "channel direction");
        if (!direction)
            return fail(direction.message());
        b.direction = direction.value();
        b.sensorId = c.value("sensorId", std::string());
        b.actuatorId = c.value("actuatorId", std::string());
        b.externalGain = c.value("externalGain", 1.0);
        b.calibrationId = c.value("calibrationId", std::string());
        b.reference = c.value("reference", false);
        e.channels.push_back(b);
    }
    const auto a = v.value("acquisition", nlohmann::json::object());
    auto mode = valueIn(kModes, a.value("mode", std::string("finite")), "acquisition mode");
    auto format = sampleFormatFromString(a.value("format", std::string("float32")));
    if (!mode)
        return fail(mode.message());
    if (!format)
        return fail(format.message());
    e.acquisition.mode = mode.value();
    e.acquisition.format = format.value();
    e.acquisition.sampleRateHz = a.value("sampleRateHz", 48000.0);
    e.acquisition.durationSeconds = a.value("durationSeconds", 1.0);
    e.acquisition.repetitions = a.value("repetitions", 1);
    e.acquisition.blockFrames = a.value("blockFrames", static_cast<size_t>(1024));
    e.acquisition.queueBlocks = a.value("queueBlocks", static_cast<size_t>(64));
    e.acquisition.maxBackpressureSeconds = a.value("maxBackpressureSeconds", 2.0);
    e.acquisition.clipFraction = a.value("clipFraction", 0.999);
    e.acquisition.recordToDisk = a.value("recordToDisk", true);
    e.acquisition.centerFrequencyHz = a.value("centerFrequencyHz", 0.0);
    e.acquisition.gainDb = a.value("gainDb", 0.0);
    const auto s = v.value("stimulus", nlohmann::json::object());
    auto kind = stimulusKindFromString(s.value("kind", std::string("none")));
    if (!kind)
        return fail(kind.message());
    e.stimulus.kind = kind.value();
    e.stimulus.amplitude = s.value("amplitude", 0.0);
    e.stimulus.startHz = s.value("startHz", 1000.0);
    e.stimulus.endHz = s.value("endHz", e.stimulus.startHz);
    e.stimulus.durationSeconds = s.value("durationSeconds", 1.0);
    e.stimulus.dutyCycle = s.value("dutyCycle", 1.0);
    e.stimulus.fadeSeconds = s.value("fadeSeconds", 0.01);
    e.stimulus.powerDbm = s.value("powerDbm", -1000.0);
    e.stimulus.seed = s.value("seed", 1u);
    e.stimulus.outputBindings = s.value("outputBindings", std::vector<std::string>{});
    e.stimulus.channelDelaySeconds = s.value("channelDelaySeconds", std::map<std::string, double>{});
    e.stimulus.channelGain = s.value("channelGain", std::map<std::string, double>{});
    const auto t = v.value("trigger", nlohmann::json::object());
    auto trigger = valueIn(kTriggers, t.value("kind", std::string("immediate")), "trigger");
    if (!trigger)
        return fail(trigger.message());
    e.trigger.kind = trigger.value();
    e.trigger.source = t.value("source", std::string());
    e.trigger.level = t.value("level", 0.0);
    e.trigger.risingEdge = t.value("risingEdge", true);
    e.trigger.timeoutSeconds = t.value("timeoutSeconds", 10.0);
    e.trigger.preTriggerSeconds = t.value("preTriggerSeconds", 0.0);
    const auto y = v.value("synchronization", nlohmann::json::object());
    e.synchronization.phaseSensitive = y.value("phaseSensitive", false);
    e.synchronization.strategy = y.value("strategy", std::string("none"));
    for (const auto& d : y.value("clockDomains", nlohmann::json::array()))
        e.synchronization.clockDomains.push_back({d.value("id", std::string()), d.value("deviceId", std::string()),
                                                  d.value("source", std::string("internal")), d.value("nominalHz", 0.0),
                                                  d.value("referenceHz", 0.0), d.value("offsetSeconds", 0.0),
                                                  d.value("driftPpm", 0.0), d.value("uncertaintySeconds", 0.0)});
    e.synchronization.timingCalibrationIds = y.value("timingCalibrationIds", std::vector<std::string>{});
    e.synchronization.channelDelayCorrection = y.value("channelDelayCorrection", std::map<std::string, double>{});
    auto positions = scanDefinitionFromJson(v.value("positions", nlohmann::json()));
    if (!positions)
        return fail(positions.message());
    e.positions = positions.value();
    e.positionerDeviceId = v.value("positionerDeviceId", std::string());
    e.probeBindingId = v.value("probeBindingId", std::string());
    e.sweepDeviceId = v.value("sweepDeviceId", std::string());
    const auto sw = v.value("sweep", nlohmann::json::object());
    e.sweep.startHz = sw.value("startHz", 0.0);
    e.sweep.stopHz = sw.value("stopHz", 0.0);
    e.sweep.points = sw.value("points", 201);
    e.sweep.powerDbm = sw.value("powerDbm", -10.0);
    e.sweep.ifBandwidthHz = sw.value("ifBandwidthHz", 1000.0);
    e.sweep.ports = sw.value("ports", std::vector<int>{1});
    const auto env = v.value("environment", nlohmann::json::object());
    e.environment.temperatureC = env.value("temperatureC", 20.0);
    e.environment.relativeHumidity = env.value("relativeHumidity", 0.0);
    e.environment.pressurePa = env.value("pressurePa", 0.0);
    e.environment.recorded = env.value("recorded", false);
    e.environment.notes = env.value("notes", std::string());
    auto safety = safetyLimitsFromJson(v.value("safety", nlohmann::json()));
    if (!safety)
        return fail(safety.message());
    e.safety = safety.value();
    e.requireCalibration = v.value("requireCalibration", false);
    for (const auto& p : v.value("processing", nlohmann::json::array())) {
        auto step = processingStepFromJson(p);
        if (!step)
            return fail(step.message());
        e.processing.push_back(step.value());
    }
    for (const auto& h : v.value("history", nlohmann::json::array())) {
        ExecutionRecord r;
        r.runId = h.value("runId", std::string());
        r.startedUtc = h.value("startedUtc", std::string());
        r.endedUtc = h.value("endedUtc", std::string());
        auto final = experimentStateFromString(h.value("finalState", std::string("draft")));
        if (!final)
            return fail(final.message());
        r.finalState = final.value();
        for (const auto& st : h.value("steps", nlohmann::json::array()))
            r.steps.push_back({st.value("step", 0), st.value("name", std::string()), st.value("status", std::string()),
                               st.value("message", std::string()), st.value("seconds", 0.0)});
        r.measurementIds = h.value("measurementIds", std::vector<std::string>{});
        r.message = h.value("message", std::string());
        r.operatorName = h.value("operator", std::string());
        r.outputAuthorized = h.value("outputAuthorized", false);
        r.reportPath = h.value("reportPath", std::string());
        e.history.push_back(r);
    }
    e.resultIds = v.value("resultIds", std::vector<std::string>{});
    e.notes = v.value("notes", std::string());
    for (const auto& n : v.value("annotations", nlohmann::json::array()))
        e.annotations.push_back({n.value("timeUtc", std::string()), n.value("author", std::string()), n.value("text", std::string())});
    auto state = experimentStateFromString(v.value("state", std::string("draft")));
    if (!state)
        return fail(state.message());
    e.state = state.value();
    return e;
}

ValidationReport validateExperiment(const Experiment& e)
{
    ValidationReport r;
    auto error = [&](std::string text) { r.errors.push_back(std::move(text)); };
    auto warning = [&](std::string text) { r.warnings.push_back(std::move(text)); };
    if (e.id.empty())
        error("the experiment has no id");
    if (e.name.empty())
        error("the experiment has no name");
    std::set<std::string> ids;
    auto unique = [&](const std::string& id, const char* what) {
        if (id.empty())
            error(std::string("a ") + what + " has no id");
        else if (!ids.insert(id).second)
            error(std::string("the id '") + id + "' is used twice");
    };
    for (const auto& s : e.sensors) {
        unique(s.id, "sensor");
        const auto allowed = measurableQuantities(s.category);
        if (std::find(allowed.begin(), allowed.end(), s.measures) == allowed.end())
            error("sensor '" + s.id + "': a " + std::string(toString(s.category)) + " does not measure " +
                  std::string(toString(s.measures)) +
                  (s.measures == Quantity::Stress ? " (stress is inferred from measured strain with a material model, never "
                                                    "measured directly)"
                                                  : ""));
        if (s.domain != e.domain)
            warning("sensor '" + s.id + "' belongs to the " + std::string(toString(s.domain)) + " domain, the experiment to the " +
                    std::string(toString(e.domain)) + " domain");
        if (s.calibrationId.empty()) {
            if (e.requireCalibration)
                error("sensor '" + s.id + "' has no calibration selected and the experiment requires calibrated sensors");
            else
                warning("sensor '" + s.id + "' has no calibration selected: its data stays in instrument units" +
                        std::string(s.nominalSensitivity > 0.0 ? " (a nominal data-sheet sensitivity is available and will be "
                                                                 "reported as nominal)"
                                                               : ""));
        }
    }
    for (const auto& a : e.actuators)
        unique(a.id, "actuator");
    std::set<std::string> devicesUsed;
    for (const auto& b : e.channels) {
        unique(b.id, "channel binding");
        if (b.deviceId.empty() || b.channelId.empty())
            error("channel binding '" + b.id + "' names no device channel");
        devicesUsed.insert(b.deviceId);
        if (!e.devices.count(b.deviceId))
            warning("channel binding '" + b.id + "': device '" + b.deviceId + "' has no stored configuration; defaults are used");
        if (b.direction == ChannelDirection::Input) {
            if (!b.sensorId.empty() && !e.sensor(b.sensorId))
                error("channel binding '" + b.id + "' refers to the unknown sensor '" + b.sensorId + "'");
            if (!b.actuatorId.empty())
                error("input channel binding '" + b.id + "' names an actuator");
        } else {
            if (b.actuatorId.empty() || !e.actuator(b.actuatorId))
                error("output channel binding '" + b.id + "' needs a known actuator");
            if (!b.sensorId.empty())
                error("output channel binding '" + b.id + "' names a sensor");
        }
        if (!(b.externalGain > 0.0))
            error("channel binding '" + b.id + "': the external gain must be positive");
    }
    const auto& a = e.acquisition;
    if (!(a.sampleRateHz > 0.0) || !(a.durationSeconds > 0.0) || a.blockFrames == 0)
        error("the acquisition needs a positive sample rate, duration and block length");
    if (a.queueBlocks < 2)
        error("the acquisition buffer needs at least 2 blocks");
    if (a.repetitions < 1)
        error("the acquisition needs at least one repetition");
    if (e.inputBindings().empty() && e.positionerDeviceId.empty() && e.domain != PhysicsDomain::Electromagnetic)
        warning("the experiment records no input channel");
    if (e.stimulus.kind != StimulusKind::None) {
        const auto& st = e.stimulus;
        if (st.outputBindings.empty())
            error("the stimulus names no output channel");
        for (const auto& id : st.outputBindings) {
            const ChannelBinding* b = e.binding(id);
            if (!b || b->direction != ChannelDirection::Output) {
                error("the stimulus output '" + id + "' is not an output channel binding");
                continue;
            }
            const Actuator* act = e.actuator(b->actuatorId);
            if (!act)
                continue;
            if (act->maxCommandAmplitude > 0.0 && st.amplitude > act->maxCommandAmplitude)
                error("the stimulus amplitude " + std::to_string(st.amplitude) + " exceeds the rating of actuator '" + act->id +
                      "' (" + std::to_string(act->maxCommandAmplitude) + ")");
            if (act->maxFrequencyHz > 0.0 && (st.startHz < act->minFrequencyHz || st.endHz > act->maxFrequencyHz))
                warning("the stimulus band reaches outside the rated band of actuator '" + act->id + "'");
        }
        if (st.kind != StimulusKind::RfCarrier) {
            const double highest = std::max(st.startHz, st.endHz);
            if (st.kind != StimulusKind::WhiteNoise && highest >= a.sampleRateHz / 2.0)
                error("the stimulus reaches " + std::to_string(highest) + " Hz, at or above the Nyquist frequency " +
                      std::to_string(a.sampleRateHz / 2.0) + " Hz of the acquisition");
            if (st.durationSeconds > a.durationSeconds)
                warning("the stimulus is longer than the acquisition; its end is not recorded");
        }
        if (e.safety.maxOutputAmplitude <= 0.0 && st.kind != StimulusKind::RfCarrier)
            warning("the safety limits permit no output; authorization will be refused until they are set");
    }
    if (e.synchronization.phaseSensitive) {
        std::set<std::string> inputDevices;
        for (const auto* b : e.inputBindings())
            inputDevices.insert(b->deviceId);
        const std::string& strategy = e.synchronization.strategy;
        if (inputDevices.size() > 1) {
            if (strategy != "shared-clock" && strategy != "timing-calibration")
                error("phase-sensitive measurement across " + std::to_string(inputDevices.size()) +
                      " devices needs a shared sample clock or a timing calibration; equal nominal sample rates do not make "
                      "acquisition coherent");
            if (strategy == "timing-calibration" && e.synchronization.timingCalibrationIds.empty())
                error("the timing-calibration strategy names no timing calibration");
        } else if (strategy == "none") {
            warning("phase-sensitive measurement on one device: relies on the device's channels sharing one sample clock "
                    "(checked against its capabilities when it is connected)");
        }
    }
    if (!e.positions.empty()) {
        auto positions = scanPositions(e.positions);
        if (!positions)
            error("positions: " + positions.message());
    }
    if (!e.positionerDeviceId.empty() && e.positions.empty())
        error("a positioner is named but no positions are defined");
    if (!e.positionerDeviceId.empty()) {
        const ChannelBinding* probe = e.binding(e.probeBindingId);
        if (!probe || probe->direction != ChannelDirection::Input)
            error("a positioner moves a probe: name its input binding (probeBindingId)");
    }
    if (!e.sweepDeviceId.empty()) {
        if (!(e.sweep.startHz > 0.0 && e.sweep.stopHz > e.sweep.startHz) || e.sweep.points < 2)
            error("the network-analyser sweep needs 0 < start < stop and at least two points");
        if (e.sweep.ports.empty() || e.sweep.ports.size() > 4)
            error("the sweep measures 1 to 4 ports");
        if (e.safety.maxTransmitPowerDbm <= -1000.0)
            warning("the safety limits permit no RF output; the sweep will be refused until a test-port power limit is set");
    }
    return r;
}

}  // namespace resonant::experiments
