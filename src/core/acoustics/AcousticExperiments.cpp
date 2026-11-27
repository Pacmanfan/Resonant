#include "acoustics/AcousticExperiments.h"

#include "acoustics/Conventions.h"
#include "foundation/Units.h"

#include <algorithm>
#include <cmath>

namespace resonant::acoustics {

using namespace experiments;

double soundPressureLevelDb(double rmsPa) { return 20.0 * std::log10(rmsPa / kReferencePressureAir); }

Result<CalibrationRecord> calibratorSensitivity(const std::vector<double>& recording, double fs, double levelDb, double frequencyHz,
                                                double levelUncertaintyDb, Quantity rawQuantity)
{
    const double rms = kReferencePressureAir * std::pow(10.0, levelDb / 20.0);
    auto r = sensitivityFromReference(recording, fs, rms, frequencyHz, Quantity::Pressure, rawQuantity,
                                      std::pow(10.0, levelUncertaintyDb / 20.0) - 1.0);
    if (!r)
        return r;
    CalibrationRecord c = r.take();
    c.referenceInstrument = "acoustic calibrator, " + std::to_string(levelDb) + " dB re 20 uPa at " + std::to_string(frequencyHz) + " Hz";
    c.procedure = "acoustic calibrator: " + c.procedure;
    return c;
}

FieldSampler fieldSampler(std::shared_ptr<const FieldEvaluator> evaluator)
{
    return [evaluator](double x, double y, double z) -> Result<std::complex<double>> {
        if (!evaluator)
            return Result<std::complex<double>>::failure("no acoustic field model");
        return evaluator->pressure(Vec3(x, y, z));
    };
}

FrequencySeries freeFieldResponse(const Vec3& source, const Vec3& receiver, double paAt1mPerUnit, Quantity commandQuantity,
                                  const std::vector<double>& frequencies, const Medium& medium)
{
    FrequencySeries s;
    s.quantity = {Quantity::Pressure, commandQuantity, false};
    s.category = DataCategory::Simulated;
    s.label = "free-field point source";
    auto model = std::make_shared<const FreeFieldModel>();
    s.model = model->name() + " (" + model->id() + "); an ideal source of flat response: no loudspeaker high-pass, no reflections";
    for (double f : frequencies) {
        if (!(f > 0.0))
            continue;
        PointSource p;
        p.position = source;
        p.forward = (receiver - source).normalized();
        p.frequencyHz = f;
        p.amplitude = Complex(paAt1mPerUnit, 0.0);
        p.amplitudePa = paAt1mPerUnit;
        FieldEvaluator evaluator({p}, f, medium, model);
        s.frequencyHz.push_back(f);
        s.values.push_back(evaluator.pressure(receiver));
    }
    return s;
}

namespace {

double distanceAlong(const SpatialSample& a, const SpatialSample& b)
{
    return std::sqrt((a.pose.x - b.pose.x) * (a.pose.x - b.pose.x) + (a.pose.y - b.pose.y) * (a.pose.y - b.pose.y) +
                     (a.pose.z - b.pose.z) * (a.pose.z - b.pose.z));
}

FocusMetrics metricsOf(const std::vector<SpatialSample>& samples, const std::vector<double>& magnitude)
{
    FocusMetrics m;
    m.points = samples.size();
    const size_t peak = static_cast<size_t>(std::max_element(magnitude.begin(), magnitude.end()) - magnitude.begin());
    m.peakMagnitude = magnitude[peak];
    m.peakPosition = Vec3(samples[peak].pose.x, samples[peak].pose.y, samples[peak].pose.z);
    // Positions along the line (cumulative distance in scan order).
    std::vector<double> s(samples.size(), 0.0);
    for (size_t i = 1; i < samples.size(); ++i)
        s[i] = s[i - 1] + distanceAlong(samples[i], samples[i - 1]);
    const double half = m.peakMagnitude / 2.0;  // -6 dB
    double left = s.front(), right = s.back();
    bool leftFound = false, rightFound = false;
    size_t lobeLeft = 0, lobeRight = samples.size() - 1;
    for (size_t i = peak; i > 0; --i)
        if (magnitude[i - 1] < half) {
            const double t = (half - magnitude[i - 1]) / (magnitude[i] - magnitude[i - 1]);
            left = s[i - 1] + t * (s[i] - s[i - 1]);
            leftFound = true;
            lobeLeft = i - 1;
            break;
        }
    for (size_t i = peak; i + 1 < samples.size(); ++i)
        if (magnitude[i + 1] < half) {
            const double t = (magnitude[i] - half) / (magnitude[i] - magnitude[i + 1]);
            right = s[i] + t * (s[i + 1] - s[i]);
            rightFound = true;
            lobeRight = i + 1;
            break;
        }
    m.widthMinus6dB = right - left;
    if (!leftFound || !rightFound)
        m.note = "the main lobe reaches the end of the scan: the -6 dB width is a lower bound";
    // Walk down the main lobe to its first minima, then take the largest local maximum outside.
    while (lobeLeft > 0 && magnitude[lobeLeft - 1] < magnitude[lobeLeft])
        --lobeLeft;
    while (lobeRight + 1 < samples.size() && magnitude[lobeRight + 1] < magnitude[lobeRight])
        ++lobeRight;
    double sidelobe = 0.0;
    for (size_t i = 1; i + 1 < samples.size(); ++i)
        if ((i < lobeLeft || i > lobeRight) && magnitude[i] >= magnitude[i - 1] && magnitude[i] >= magnitude[i + 1])
            sidelobe = std::max(sidelobe, magnitude[i]);
    m.peakSidelobeDb = sidelobe > 0.0 ? 20.0 * std::log10(sidelobe / m.peakMagnitude) : 0.0;
    return m;
}

}  // namespace

Result<FocusMetrics> focusMetrics(const SpatialDataset& line)
{
    if (line.samples.size() < 3)
        return Result<FocusMetrics>::failure("focus metrics need at least three positions along a line");
    std::vector<double> magnitude;
    for (const auto& s : line.samples)
        magnitude.push_back(std::abs(s.value));
    FocusMetrics m = metricsOf(line.samples, magnitude);
    m.note += (m.note.empty() ? "" : "; ") + std::string("from ") + std::to_string(m.points) +
              " measured positions; values between them are interpolated linearly for the width only";
    return m;
}

Result<FocusComparison> compareFocus(const SpatialDataset& measured, const FieldSampler& simulated)
{
    auto m = focusMetrics(measured);
    if (!m)
        return Result<FocusComparison>::failure(m.message());
    std::vector<double> magnitude;
    for (const auto& s : measured.samples) {
        auto v = simulated(s.pose.x, s.pose.y, s.pose.z);
        if (!v)
            return Result<FocusComparison>::failure(v.message());
        magnitude.push_back(std::abs(v.value()));
    }
    FocusComparison c;
    c.measured = m.value();
    c.simulated = metricsOf(measured.samples, magnitude);
    c.simulated.note = "the simulated field sampled at the measured positions";
    c.locationErrorM = (c.measured.peakPosition - c.simulated.peakPosition).norm();
    c.widthErrorM = c.measured.widthMinus6dB - c.simulated.widthMinus6dB;
    c.sidelobeDifferenceDb = c.measured.peakSidelobeDb - c.simulated.peakSidelobeDb;
    return c;
}

Result<std::vector<CalibrationRecord>> elementCalibration(const std::vector<std::complex<double>>& responses,
                                                          const std::vector<std::string>& actuatorIds,
                                                          const std::vector<double>& pathPhaseRad, size_t referenceIndex,
                                                          double frequencyHz)
{
    using R = Result<std::vector<CalibrationRecord>>;
    if (responses.size() != actuatorIds.size() || responses.size() != pathPhaseRad.size() || referenceIndex >= responses.size())
        return R::failure("element calibration needs one response and one path phase per element and a valid reference");
    const std::complex<double> reference = responses[referenceIndex] * std::polar(1.0, -pathPhaseRad[referenceIndex]);
    if (std::abs(reference) == 0.0)
        return R::failure("the reference element produced no signal");
    std::vector<CalibrationRecord> out;
    for (size_t i = 0; i < responses.size(); ++i) {
        const std::complex<double> own = responses[i] * std::polar(1.0, -pathPhaseRad[i]);
        const std::complex<double> ratio = own / reference;
        CalibrationRecord r;
        r.type = CalibrationType::ChannelToChannel;
        r.actuatorId = actuatorIds[i];
        r.scalar = std::abs(ratio);
        r.unit = "1";
        r.phaseRad = std::arg(ratio);
        r.referenceFrequencyHz = frequencyHz;
        r.minFrequencyHz = r.maxFrequencyHz = frequencyHz;
        r.procedure = "each element driven alone; complex tone at one microphone, the propagation phase from the geometry "
                      "removed; amplitude and phase relative to element " + actuatorIds[referenceIndex];
        r.source = "derived by Resonant";
        out.push_back(r);
    }
    return out;
}

// --- templates -------------------------------------------------------------------------------

namespace {

Experiment base(const std::string& id, const std::string& name, const std::string& description, double fs)
{
    Experiment e;
    e.id = id;
    e.name = name;
    e.description = description;
    e.domain = PhysicsDomain::Acoustic;
    e.acquisition.sampleRateHz = fs;
    e.acquisition.format = SampleFormat::Float32;
    e.acquisition.blockFrames = 2048;
    DeviceConfiguration c;
    c.sampleRateHz = fs;
    c.format = SampleFormat::Float32;
    e.devices["sim:audio-8x2"] = c;
    e.safety.amplitudeUnit = "FS";
    e.safety.maxOutputAmplitude = 0.5;
    e.safety.maxDurationSeconds = 30.0;
    e.environment.temperatureC = 20.0;
    e.environment.notes = "assumed laboratory conditions";
    return e;
}

Sensor microphone(const std::string& id, const std::string& name, double x, double y, double z)
{
    Sensor s;
    s.id = id;
    s.name = name;
    s.category = SensorCategory::Microphone;
    s.domain = PhysicsDomain::Acoustic;
    s.pose.x = x;
    s.pose.y = y;
    s.pose.z = z;
    s.pose.dx = 0.0;
    s.pose.dy = -1.0;
    s.pose.dz = 0.0;
    s.measures = Quantity::Pressure;
    return s;
}

Actuator speaker(const std::string& id, const std::string& name, double x, double y, double z, ActuatorCategory category)
{
    Actuator a;
    a.id = id;
    a.name = name;
    a.category = category;
    a.domain = PhysicsDomain::Acoustic;
    a.pose.x = x;
    a.pose.y = y;
    a.pose.z = z;
    a.pose.dx = 0.0;
    a.pose.dy = 1.0;
    a.pose.dz = 0.0;
    a.commandQuantity = Quantity::DigitalFullScale;
    a.outputQuantity = Quantity::Pressure;
    a.minFrequencyHz = category == ActuatorCategory::UltrasonicTransducer ? 35000.0 : 50.0;
    a.maxFrequencyHz = category == ActuatorCategory::UltrasonicTransducer ? 45000.0 : 20000.0;
    a.maxCommandAmplitude = 0.8;
    return a;
}

ChannelBinding input(const std::string& id, const std::string& channel, const std::string& sensor, bool reference = false)
{
    ChannelBinding b;
    b.id = id;
    b.deviceId = "sim:audio-8x2";
    b.channelId = channel;
    b.direction = ChannelDirection::Input;
    b.sensorId = sensor;
    b.reference = reference;
    return b;
}

ChannelBinding output(const std::string& id, const std::string& channel, const std::string& actuator)
{
    ChannelBinding b;
    b.id = id;
    b.deviceId = "sim:audio-8x2";
    b.channelId = channel;
    b.direction = ChannelDirection::Output;
    b.actuatorId = actuator;
    return b;
}

}  // namespace

Experiment speakerCharacterizationExperiment(const std::string& id)
{
    Experiment e = base(id, "Speaker characterization",
                        "Sweep the loudspeaker, record a microphone 1 m in front of it and the electrical loopback of the drive, and "
                        "estimate the frequency response (H1, microphone over drive).",
                        48000.0);
    e.templateId = "acoustic.speaker-characterization";
    e.sensors.push_back(microphone("mic-1", "Measurement microphone", 0.0, 1.0, 0.0));
    Sensor loop;
    loop.id = "drive-ref";
    loop.name = "Drive loopback (electrical)";
    loop.category = SensorCategory::GenericVoltage;
    loop.domain = PhysicsDomain::Acoustic;
    loop.measures = Quantity::DigitalFullScale;
    e.sensors.push_back(loop);
    e.actuators.push_back(speaker("spk-1", "Loudspeaker", 0.0, 0.0, 0.0, ActuatorCategory::Speaker));
    e.channels.push_back(input("ch-mic", "in0", "mic-1"));
    e.channels.push_back(input("ch-ref", "in7", "drive-ref", true));
    e.channels.push_back(output("ch-spk", "out0", "spk-1"));
    e.stimulus.kind = StimulusKind::LogSweep;
    e.stimulus.amplitude = 0.25;
    e.stimulus.startHz = 50.0;
    e.stimulus.endHz = 15000.0;
    e.stimulus.durationSeconds = 2.0;
    e.stimulus.outputBindings = {"ch-spk"};
    e.safety.allowedOutputChannels = {"ch-spk"};
    e.safety.minFrequencyHz = 20.0;
    e.safety.maxFrequencyHz = 20000.0;
    e.acquisition.durationSeconds = 2.5;
    // The simulated interface's first microphone where the sensor is.
    e.devices["sim:audio-8x2"].options = {{"micPositions", {{0.0, 1.0, 0.0}}}};
    // Segments long compared with the 2.9 ms acoustic delay (a delay of tau
    // within segments of length T biases H1 low by about 1 - tau / T).
    ProcessingStep h;
    h.operation = "transfer-function";
    h.parameters = {{"reference", "ch-ref"}, {"segmentLength", 16384}, {"overlap", 0.5}, {"window", "hann"}, {"bindings", {"ch-mic"}}};
    e.processing.push_back(h);
    return e;
}

Experiment fieldMappingExperiment(const std::string& id, double frequencyHz)
{
    Experiment e = base(id, "Acoustic field mapping",
                        "Drive the loudspeaker with a tone and record seven microphones on a line 1 m in front of it; the tone's "
                        "complex amplitude at each microphone is a measured point of the field.",
                        48000.0);
    e.templateId = "acoustic.field-mapping";
    for (int i = 0; i < 7; ++i) {
        e.sensors.push_back(microphone("mic-" + std::to_string(i + 1), "Microphone " + std::to_string(i + 1), -0.3 + 0.1 * i, 1.0, 0.0));
        e.channels.push_back(input("ch-mic-" + std::to_string(i + 1), "in" + std::to_string(i), "mic-" + std::to_string(i + 1)));
    }
    e.actuators.push_back(speaker("spk-1", "Loudspeaker", 0.0, 0.0, 0.0, ActuatorCategory::Speaker));
    e.channels.push_back(output("ch-spk", "out0", "spk-1"));
    e.stimulus.kind = StimulusKind::Sine;
    e.stimulus.amplitude = 0.2;
    e.stimulus.startHz = e.stimulus.endHz = frequencyHz;
    e.stimulus.durationSeconds = 1.0;
    e.stimulus.outputBindings = {"ch-spk"};
    e.safety.allowedOutputChannels = {"ch-spk"};
    e.safety.minFrequencyHz = 20.0;
    e.safety.maxFrequencyHz = 20000.0;
    e.acquisition.durationSeconds = 1.0;
    e.synchronization.phaseSensitive = true;
    e.synchronization.strategy = "single-device";
    ProcessingStep seg;
    seg.operation = "segment";
    seg.parameters = {{"startSeconds", 0.25}, {"durationSeconds", 0.5}};
    e.processing.push_back(seg);
    ProcessingStep tone;
    tone.operation = "tone";
    tone.parameters = {{"frequencyHz", frequencyHz}, {"window", "hann"}};
    e.processing.push_back(tone);
    return e;
}

Experiment ultrasonicElementExperiment(const std::string& id, const std::string& elementBinding)
{
    Experiment e = base(id, "Ultrasonic element characterization",
                        "Drive one 40 kHz transducer of an array alone with a tone burst and record its amplitude and phase at a "
                        "reference microphone. One run per element; elementCalibration() combines the runs.",
                        192000.0);
    e.templateId = "acoustic.ultrasonic-element";
    e.sensors.push_back(microphone("mic-1", "Ultrasonic receiver", 0.0, 0.3, 0.0));
    e.sensors.back().category = SensorCategory::UltrasonicReceiver;
    e.actuators.push_back(speaker("tx-a", "Transducer A", 0.0, 0.0, 0.0, ActuatorCategory::UltrasonicTransducer));
    e.actuators.push_back(speaker("tx-b", "Transducer B", 0.5, 0.0, 0.0, ActuatorCategory::UltrasonicTransducer));
    e.channels.push_back(input("ch-mic", "in0", "mic-1"));
    e.channels.push_back(output("out-a", "out0", "tx-a"));
    e.channels.push_back(output("out-b", "out1", "tx-b"));
    e.devices["sim:audio-8x2"].options = {{"speakerHighPassHz", 1000.0}};
    e.stimulus.kind = StimulusKind::Sine;
    e.stimulus.amplitude = 0.3;
    e.stimulus.startHz = e.stimulus.endHz = 40000.0;
    e.stimulus.durationSeconds = 0.05;
    e.stimulus.fadeSeconds = 0.002;
    e.stimulus.outputBindings = {elementBinding};
    e.safety.allowedOutputChannels = {"out-a", "out-b"};
    e.safety.minFrequencyHz = 35000.0;
    e.safety.maxFrequencyHz = 45000.0;
    e.acquisition.durationSeconds = 0.06;
    ProcessingStep seg;
    seg.operation = "segment";
    seg.parameters = {{"startSeconds", 0.01}, {"durationSeconds", 0.03}};
    e.processing.push_back(seg);
    ProcessingStep tone;
    tone.operation = "tone";
    tone.parameters = {{"frequencyHz", 40000.0}, {"window", "hann"}};
    e.processing.push_back(tone);
    return e;
}

Experiment focusingValidationExperiment(const std::string& id, const Vec3& focus)
{
    Experiment e = fieldMappingExperiment(id, 4000.0);
    e.name = "Acoustic focusing validation";
    e.description = "Two loudspeakers driven with delays that align their arrivals at a target point; the microphone line "
                    "measures the pressure at and around the target for comparison with the predicted focus.";
    e.templateId = "acoustic.focusing-validation";
    e.actuators.push_back(speaker("spk-2", "Loudspeaker 2", 0.5, 0.0, 0.0, ActuatorCategory::Speaker));
    e.channels.push_back(output("ch-spk-2", "out1", "spk-2"));
    e.stimulus.outputBindings = {"ch-spk", "ch-spk-2"};
    e.safety.allowedOutputChannels = {"ch-spk", "ch-spk-2"};
    // Delays that make both arrivals coincide at the focus (the farther source fires first).
    const double c = 343.0;
    const double r1 = (focus - Vec3(0.0, 0.0, 0.0)).norm(), r2 = (focus - Vec3(0.5, 0.0, 0.0)).norm();
    const double longest = std::max(r1, r2);
    e.stimulus.channelDelaySeconds = {{"ch-spk", (longest - r1) / c}, {"ch-spk-2", (longest - r2) / c}};
    e.devices["sim:audio-8x2"].options = {{"speakerPositions", {{0.0, 0.0, 0.0}, {0.5, 0.0, 0.0}}}};
    return e;
}

}  // namespace resonant::acoustics
