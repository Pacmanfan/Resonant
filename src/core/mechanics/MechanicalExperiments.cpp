#include "mechanics/MechanicalExperiments.h"

#include "mechanics/StructuralModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace resonant::mechanics {

using namespace experiments;

namespace {

std::complex<double> valueAt(const MeasurementData& d, double f)
{
    auto upper = std::lower_bound(d.axis.begin(), d.axis.end(), f);
    if (upper == d.axis.begin())
        return d.complex.front();
    if (upper == d.axis.end())
        return d.complex.back();
    const size_t i = static_cast<size_t>(upper - d.axis.begin());
    const double t = (f - d.axis[i - 1]) / (d.axis[i] - d.axis[i - 1]);
    return d.complex[i - 1] + t * (d.complex[i] - d.complex[i - 1]);
}

}  // namespace

Result<std::vector<IdentifiedMode>> identifyModes(const ProcessingItem& tf, double minRelative, double minCoherence, double minHz)
{
    using R = Result<std::vector<IdentifiedMode>>;
    if (tf.record.kind != MeasurementKind::TransferFunction)
        return R::failure("modes are identified from a transfer function (FRF)");
    const auto& f = tf.data.axis;
    const auto& h = tf.data.complex;
    if (f.size() < 5 || h.size() != f.size())
        return R::failure("the transfer function is too short");
    std::vector<double> m(h.size());
    for (size_t k = 0; k < h.size(); ++k)
        m[k] = std::abs(h[k]);
    const bool hasCoherence = tf.data.auxiliary.size() == h.size();
    double largest = 0.0;
    for (size_t k = 0; k < m.size(); ++k)
        if (f[k] >= minHz && (!hasCoherence || tf.data.auxiliary[k] >= minCoherence))
            largest = std::max(largest, m[k]);
    if (largest == 0.0)
        return R::failure("no part of the FRF has enough coherence");
    const double df = f[1] - f[0];
    std::vector<IdentifiedMode> modes;
    for (size_t k = 1; k + 1 < m.size(); ++k) {
        if (f[k] < minHz || m[k] < minRelative * largest || m[k] < m[k - 1] || m[k] < m[k + 1])
            continue;
        if (hasCoherence && tf.data.auxiliary[k] < minCoherence)
            continue;
        IdentifiedMode mode;
        // Parabolic refinement of the peak.
        const double a = m[k - 1], b = m[k], c = m[k + 1];
        const double denominator = a - 2.0 * b + c;
        const double shift = denominator < 0.0 ? 0.5 * (a - c) / denominator : 0.0;
        mode.frequencyHz = f[k] + shift * df;
        mode.peakMagnitude = b - 0.25 * (a - c) * shift;
        mode.coherence = hasCoherence ? tf.data.auxiliary[k] : 1.0;
        mode.frequencyUncertaintyHz = df / 2.0;
        // Half-power points by linear interpolation.
        const double half = mode.peakMagnitude / std::sqrt(2.0);
        double f1 = 0.0, f2 = 0.0;
        for (size_t i = k; i > 0; --i)
            if (m[i - 1] < half) {
                f1 = f[i - 1] + (half - m[i - 1]) / (m[i] - m[i - 1]) * df;
                break;
            }
        for (size_t i = k; i + 1 < m.size(); ++i)
            if (m[i + 1] < half) {
                f2 = f[i] + (m[i] - half) / (m[i] - m[i + 1]) * df;
                break;
            }
        if (f1 > 0.0 && f2 > f1) {
            mode.dampingRatio = (f2 - f1) / (2.0 * mode.frequencyHz);
            mode.qualityFactor = 1.0 / (2.0 * mode.dampingRatio);
            mode.bandwidthResolved = (f2 - f1) > 2.0 * df;
        } else {
            mode.bandwidthResolved = false;
        }
        mode.note = "peak of the measured FRF with its half-power bandwidth (assumes light damping and separated modes); an "
                    "estimate of a resonance at the measured points, not a computed eigenmode";
        if (!mode.bandwidthResolved)
            mode.note += "; the bandwidth is not resolved by the spectral resolution: the damping is an upper bound or unknown";
        modes.push_back(mode);
    }
    std::sort(modes.begin(), modes.end(), [](const IdentifiedMode& x, const IdentifiedMode& y) { return x.peakMagnitude > y.peakMagnitude; });
    return modes;
}

double modalAssuranceCriterion(const std::vector<std::complex<double>>& a, const std::vector<std::complex<double>>& b)
{
    if (a.size() != b.size() || a.empty())
        return 0.0;
    std::complex<double> ab(0.0, 0.0);
    double aa = 0.0, bb = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        ab += std::conj(a[i]) * b[i];
        aa += std::norm(a[i]);
        bb += std::norm(b[i]);
    }
    return (aa > 0.0 && bb > 0.0) ? std::norm(ab) / (aa * bb) : 0.0;
}

Result<std::vector<std::complex<double>>> operatingShape(const std::vector<ProcessingItem>& tfs, double frequencyHz)
{
    std::vector<std::complex<double>> shape;
    for (const auto& t : tfs) {
        if (t.record.kind != MeasurementKind::TransferFunction || t.data.axis.empty())
            return Result<std::vector<std::complex<double>>>::failure("an operating shape needs transfer functions");
        if (frequencyHz < t.data.axis.front() || frequencyHz > t.data.axis.back())
            return Result<std::vector<std::complex<double>>>::failure("the frequency lies outside the measured range");
        shape.push_back(valueAt(t.data, frequencyHz));
    }
    return shape;
}

std::vector<ModePair> pairModes(const std::vector<IdentifiedMode>& measured, const std::vector<std::vector<std::complex<double>>>& measuredShapes,
                                const std::vector<double>& simulatedHz, const std::vector<std::vector<std::complex<double>>>& simulatedShapes)
{
    std::vector<ModePair> pairs;
    const bool shapes = measuredShapes.size() == measured.size() && simulatedShapes.size() == simulatedHz.size() && !measured.empty();
    std::vector<bool> used(simulatedHz.size(), false);
    for (size_t i = 0; i < measured.size(); ++i) {
        size_t best = simulatedHz.size();
        double score = -std::numeric_limits<double>::infinity(), bestMac = -1.0;
        for (size_t j = 0; j < simulatedHz.size(); ++j) {
            if (used[j])
                continue;
            double s;
            double mac = -1.0;
            if (shapes) {
                mac = modalAssuranceCriterion(measuredShapes[i], simulatedShapes[j]);
                s = mac;
            } else {
                s = -std::abs(simulatedHz[j] - measured[i].frequencyHz);
            }
            if (s > score) {
                score = s;
                best = j;
                bestMac = mac;
            }
        }
        if (best == simulatedHz.size())
            continue;
        used[best] = true;
        ModePair p;
        p.measured = i;
        p.simulated = best;
        p.measuredHz = measured[i].frequencyHz;
        p.simulatedHz = simulatedHz[best];
        p.frequencyErrorPercent = 100.0 * (p.simulatedHz - p.measuredHz) / p.measuredHz;
        p.mac = bestMac;
        pairs.push_back(p);
    }
    return pairs;
}

std::vector<std::complex<double>> simulatedShapeAt(const StructuralModel& model, const Mode& mode, const std::vector<Vec3>& points,
                                                   const std::vector<Vec3>& directions)
{
    std::vector<std::complex<double>> out;
    for (size_t i = 0; i < points.size(); ++i) {
        const size_t node = nearestNode(model, points[i]);
        const Vec3 d = i < directions.size() ? directions[i].normalized() : Vec3::UnitZ();
        const double v = mode.shape.size() == static_cast<long>(model.dofCount()) ? model.translation(mode.shape, node).dot(d) : 0.0;
        out.emplace_back(v, 0.0);
    }
    return out;
}

ProcessingStep inferStressStep(double youngsModulusPa, const std::string& materialName)
{
    ProcessingStep s;
    s.operation = "scale";
    s.parameters = {{"factor", youngsModulusPa},
                    {"quantity", "stress"},
                    {"reason", "uniaxial Hooke's law sigma = E epsilon with E = " + std::to_string(youngsModulusPa) + " Pa (" +
                                   materialName + "): linear elastic, isotropic, uniaxial stress along the gauge"}};
    return s;
}

// --- templates ------------------------------------------------------------------------------

namespace {

Experiment base(const std::string& id, const std::string& name, const std::string& description)
{
    Experiment e;
    e.id = id;
    e.name = name;
    e.description = description;
    e.domain = PhysicsDomain::Mechanical;
    e.acquisition.sampleRateHz = 8192.0;
    e.acquisition.format = SampleFormat::Float32;
    e.acquisition.blockFrames = 1024;
    DeviceConfiguration c;
    c.sampleRateHz = 8192.0;
    c.format = SampleFormat::Float32;
    e.devices["sim:daq-4x1"] = c;
    e.safety.amplitudeUnit = "V";
    e.safety.maxOutputAmplitude = 2.0;
    e.safety.maxDurationSeconds = 60.0;
    e.safety.minFrequencyHz = 5.0;
    e.safety.maxFrequencyHz = 2000.0;
    e.safety.allowedOutputChannels = {"ch-shaker"};
    Sensor force;
    force.id = "load-cell";
    force.name = "Force transducer (stinger)";
    force.category = SensorCategory::LoadCell;
    force.domain = PhysicsDomain::Mechanical;
    force.measures = Quantity::Force;
    force.nominalSensitivity = 0.0225;
    e.sensors.push_back(force);
    const double xs[3] = {0.1, 0.25, 0.4};
    for (int i = 0; i < 3; ++i) {
        Sensor a;
        a.id = "acc-" + std::to_string(i + 1);
        a.name = "Accelerometer " + std::to_string(i + 1);
        a.category = SensorCategory::Accelerometer;
        a.domain = PhysicsDomain::Mechanical;
        a.measures = Quantity::Acceleration;
        a.nominalSensitivity = 0.0102;
        a.pose.x = xs[i];
        e.sensors.push_back(a);
    }
    Actuator shaker;
    shaker.id = "shaker";
    shaker.name = "Electrodynamic shaker";
    shaker.category = ActuatorCategory::Shaker;
    shaker.domain = PhysicsDomain::Mechanical;
    shaker.commandQuantity = Quantity::Voltage;
    shaker.outputQuantity = Quantity::Force;
    shaker.minFrequencyHz = 5.0;
    shaker.maxFrequencyHz = 3000.0;
    shaker.maxCommandAmplitude = 5.0;
    e.actuators.push_back(shaker);
    auto bind = [&](const std::string& bid, const std::string& ch, ChannelDirection dir, const std::string& who) {
        ChannelBinding b;
        b.id = bid;
        b.deviceId = "sim:daq-4x1";
        b.channelId = ch;
        b.direction = dir;
        (dir == ChannelDirection::Input ? b.sensorId : b.actuatorId) = who;
        e.channels.push_back(b);
    };
    bind("ch-force", "ai0", ChannelDirection::Input, "load-cell");
    bind("ch-acc-1", "ai1", ChannelDirection::Input, "acc-1");
    bind("ch-acc-2", "ai2", ChannelDirection::Input, "acc-2");
    bind("ch-acc-3", "ai3", ChannelDirection::Input, "acc-3");
    bind("ch-shaker", "ao0", ChannelDirection::Output, "shaker");
    e.channels[0].reference = true;
    e.synchronization.phaseSensitive = true;
    e.synchronization.strategy = "single-device";
    return e;
}

}  // namespace

Experiment modalTestExperiment(const std::string& id)
{
    Experiment e = base(id, "Structural modal test",
                        "Random excitation through a shaker; force at the drive point and acceleration at three points; "
                        "accelerance FRFs (H1) from which the resonances are identified.");
    e.templateId = "mechanical.modal-test";
    e.stimulus.kind = StimulusKind::WhiteNoise;
    e.stimulus.amplitude = 1.0;
    e.stimulus.startHz = 5.0;
    e.stimulus.endHz = 2000.0;
    e.stimulus.durationSeconds = 16.0;
    e.stimulus.fadeSeconds = 0.05;
    e.stimulus.outputBindings = {"ch-shaker"};
    e.safety.minFrequencyHz = 0.0;
    e.safety.maxFrequencyHz = 4096.0;
    e.acquisition.durationSeconds = 16.0;
    ProcessingStep cal;
    cal.operation = "calibrate";
    cal.parameters = {{"bindings", {"ch-force", "ch-acc-1", "ch-acc-2", "ch-acc-3"}}};
    e.processing.push_back(cal);
    // 0.5 Hz bins: the half-power bandwidth of the lightest-damped mode spans several bins.
    ProcessingStep h;
    h.operation = "transfer-function";
    h.parameters = {{"reference", "ch-force"}, {"segmentLength", 16384}, {"overlap", 0.5}, {"window", "hann"},
                    {"bindings", {"ch-acc-1", "ch-acc-2", "ch-acc-3"}}};
    e.processing.push_back(h);
    return e;
}

Experiment vibrationCharacterizationExperiment(const std::string& id, double frequencyHz)
{
    Experiment e = base(id, "Vibration characterization",
                        "A controlled sinusoidal excitation; the acceleration amplitude and phase at three points relative to the "
                        "measured force.");
    e.templateId = "mechanical.vibration-characterization";
    e.stimulus.kind = StimulusKind::Sine;
    e.stimulus.amplitude = 0.5;
    e.stimulus.startHz = e.stimulus.endHz = frequencyHz;
    e.stimulus.durationSeconds = 4.0;
    e.stimulus.outputBindings = {"ch-shaker"};
    e.acquisition.durationSeconds = 4.0;
    ProcessingStep seg;
    seg.operation = "segment";
    seg.parameters = {{"startSeconds", 1.0}, {"durationSeconds", 2.0}};
    e.processing.push_back(seg);
    ProcessingStep tone;
    tone.operation = "tone";
    tone.parameters = {{"frequencyHz", frequencyHz}, {"window", "hann"}};
    e.processing.push_back(tone);
    return e;
}

Experiment shakerCalibrationExperiment(const std::string& id)
{
    Experiment e = base(id, "Shaker calibration",
                        "The shaker's transfer function from drive voltage to force (measured with the load cell): commanded "
                        "output against physical output.");
    e.templateId = "mechanical.actuator-calibration";
    e.stimulus.kind = StimulusKind::LogSweep;
    e.stimulus.amplitude = 1.0;
    e.stimulus.startHz = 10.0;
    e.stimulus.endHz = 2000.0;
    e.stimulus.durationSeconds = 8.0;
    e.stimulus.outputBindings = {"ch-shaker"};
    e.acquisition.durationSeconds = 8.0;
    ProcessingStep cal;
    cal.operation = "calibrate";
    cal.parameters = {{"bindings", {"ch-force"}}};
    e.processing.push_back(cal);
    ProcessingStep h;
    h.operation = "transfer-function";
    h.parameters = {{"reference", "ch-shaker"}, {"segmentLength", 4096}, {"overlap", 0.5}, {"window", "hann"}, {"bindings", {"ch-force"}}};
    e.processing.push_back(h);
    return e;
}

}  // namespace resonant::mechanics
