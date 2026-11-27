#include "electromagnetics/RfExperiments.h"

#include "dsp/Windows.h"
#include "foundation/Id.h"
#include "foundation/Time.h"

#include <algorithm>
#include <cmath>

namespace resonant::em {

using namespace experiments;

namespace {

double wrapDeg(double d)
{
    while (d > 180.0)
        d -= 360.0;
    while (d <= -180.0)
        d += 360.0;
    return d;
}

double halfPowerWidth(const std::vector<double>& angle, const std::vector<double>& db, size_t peak, bool periodic)
{
    const size_t n = angle.size();
    auto at = [&](long i) { return static_cast<size_t>(((i % static_cast<long>(n)) + static_cast<long>(n)) % static_cast<long>(n)); };
    double left = 0.0, right = 0.0;
    bool foundLeft = false, foundRight = false;
    for (long i = static_cast<long>(peak); i > static_cast<long>(peak) - static_cast<long>(n) + 1; --i) {
        if (!periodic && i <= 0)
            break;
        const size_t a = at(i - 1), b = at(i);
        if (db[a] < -3.0) {
            double span = angle[b] - angle[a];
            if (span < 0.0)
                span += 360.0;
            const double t = (-3.0 - db[a]) / (db[b] - db[a]);
            left = angle[b] - (1.0 - t) * span;
            foundLeft = true;
            break;
        }
    }
    for (long i = static_cast<long>(peak); i < static_cast<long>(peak) + static_cast<long>(n) - 1; ++i) {
        if (!periodic && i + 1 >= static_cast<long>(n))
            break;
        const size_t a = at(i), b = at(i + 1);
        if (db[b] < -3.0) {
            double span = angle[b] - angle[a];
            if (span < 0.0)
                span += 360.0;
            const double t = (db[a] + 3.0) / (db[a] - db[b]);
            right = angle[a] + t * span;
            foundRight = true;
            break;
        }
    }
    if (!foundLeft || !foundRight)
        return 0.0;
    double width = right - left;
    while (width < 0.0)
        width += 360.0;
    return width;
}

}  // namespace

Result<ProcessingItem> importTouchstoneMeasurement(const std::string& text, const std::string& fileName, const std::string& experimentId)
{
    auto parsed = parseTouchstone(text, portsFromFileName(fileName));
    if (!parsed)
        return Result<ProcessingItem>::failure(fileName + ": " + parsed.message());
    const NetworkParameters& n = parsed.value();
    ProcessingItem item;
    MeasurementRecord& r = item.record;
    r.id = "m-" + Id::generate().str();
    r.experimentId = experimentId;
    r.channelId = "s-parameters";
    r.domain = PhysicsDomain::Electromagnetic;
    r.kind = MeasurementKind::NetworkParameters;
    r.category = DataCategory::Raw;
    r.quantity = {Quantity::ScatteringParameter, std::nullopt, false};
    r.shape = {n.frequencyHz.size(), static_cast<size_t>(n.ports), static_cast<size_t>(n.ports)};
    r.acquisition = {{"source", fileName},
                     {"touchstoneVersion", n.version},
                     {"format", n.sourceFormat},
                     {"frequencyUnit", n.sourceFrequencyUnit},
                     {"referenceImpedance", n.referenceImpedance},
                     {"comments", n.comments}};
    r.note = "imported from " + fileName + " (not acquired by Resonant; the instrument and its calibration are as the file states)";
    r.createdUtc = utcNowIso8601();
    item.data.axis = n.frequencyHz;
    for (const auto& m : n.s)
        item.data.complex.insert(item.data.complex.end(), m.begin(), m.end());
    return item;
}

Result<NetworkParameters> networkOf(const ProcessingItem& item)
{
    using R = Result<NetworkParameters>;
    if (item.record.kind != MeasurementKind::NetworkParameters)
        return R::failure("'" + item.record.id + "' holds no network parameters");
    NetworkParameters n;
    const size_t frequencies = item.data.axis.size();
    if (frequencies == 0 || item.data.complex.size() % frequencies != 0)
        return R::failure("'" + item.record.id + "': the data does not match its frequencies");
    const size_t perPoint = item.data.complex.size() / frequencies;
    n.ports = static_cast<int>(std::lround(std::sqrt(static_cast<double>(perPoint))));
    if (static_cast<size_t>(n.ports * n.ports) != perPoint)
        return R::failure("'" + item.record.id + "': not a square S matrix per frequency");
    n.frequencyHz = item.data.axis;
    for (size_t f = 0; f < frequencies; ++f)
        n.s.emplace_back(item.data.complex.begin() + static_cast<long>(f * perPoint),
                         item.data.complex.begin() + static_cast<long>((f + 1) * perPoint));
    const auto& z = item.record.acquisition.value("referenceImpedance", nlohmann::json(50.0));
    if (z.is_array())
        n.referenceImpedance = z.get<std::vector<double>>();
    else
        n.referenceImpedance.assign(static_cast<size_t>(n.ports), z.is_number() ? z.get<double>() : 50.0);
    return n;
}

ReflectionMetrics reflectionMetrics(const std::vector<double>& f, const std::vector<Complex>& s11, double z0)
{
    ReflectionMetrics m;
    m.frequencyHz = f;
    size_t best = 0;
    for (size_t i = 0; i < s11.size(); ++i) {
        const double g = std::min(std::abs(s11[i]), 1.0 - 1e-12);
        m.impedance.push_back(z0 * (1.0 + s11[i]) / (1.0 - s11[i]));
        m.returnLossDb.push_back(-20.0 * std::log10(std::max(g, 1e-15)));
        m.vswr.push_back((1.0 + g) / (1.0 - g));
        if (m.returnLossDb.back() > m.returnLossDb[best])
            best = i;
    }
    if (s11.empty())
        return m;
    m.bestFrequencyHz = f[best];
    m.bestReturnLossDb = m.returnLossDb[best];
    if (m.bestReturnLossDb >= 10.0) {
        size_t lo = best, hi = best;
        while (lo > 0 && m.returnLossDb[lo - 1] >= 10.0)
            --lo;
        while (hi + 1 < f.size() && m.returnLossDb[hi + 1] >= 10.0)
            ++hi;
        m.bandwidth10dBHz = f[hi] - f[lo];
    }
    return m;
}

Result<ComparisonMetrics> compareReflection(const NetworkParameters& measured, const std::vector<std::pair<double, AntennaImpedance>>& simulated,
                                            const std::string& model, int port)
{
    if (port < 1 || port > measured.ports)
        return Result<ComparisonMetrics>::failure("no port " + std::to_string(port));
    const double z0 = measured.referenceImpedance[static_cast<size_t>(port - 1)];
    FrequencySeries m, s;
    m.quantity = s.quantity = {Quantity::ScatteringParameter, std::nullopt, false};
    m.category = DataCategory::Raw;
    s.category = DataCategory::Simulated;
    s.model = model;
    for (size_t f = 0; f < measured.frequencyHz.size(); ++f) {
        m.frequencyHz.push_back(measured.frequencyHz[f]);
        m.values.push_back(measured.at(f, port, port));
    }
    for (const auto& [f, z] : simulated) {
        if (!z.valid)
            continue;
        s.frequencyHz.push_back(f);
        s.values.push_back((z.input - z0) / (z.input + z0));
    }
    return compareFrequencySeries(m, s);
}

Result<std::vector<Complex>> channelPhasors(const std::vector<std::vector<Complex>>& iq, double fs, double offsetHz)
{
    if (iq.empty() || !(fs > 0.0) || !(std::abs(offsetHz) < fs / 2.0))
        return Result<std::vector<Complex>>::failure("channel phasors need I/Q records and an offset within +-fs/2");
    std::vector<Complex> out;
    for (const auto& x : iq) {
        if (x.size() < 16)
            return Result<std::vector<Complex>>::failure("an I/Q record is too short");
        const auto w = dsp::windowCoefficients(dsp::WindowKind::Hann, x.size());
        const double cg = dsp::windowProperties(w).coherentGain;
        Complex sum(0.0, 0.0);
        for (size_t n = 0; n < x.size(); ++n)
            sum += x[n] * w[n] * std::polar(1.0, -2.0 * kPi * offsetHz * static_cast<double>(n) / fs);
        out.push_back(sum / (static_cast<double>(x.size()) * cg));
    }
    return out;
}

Result<std::vector<CalibrationRecord>> arrayChannelCalibration(const std::vector<Complex>& phasors, const std::vector<std::string>& channelIds,
                                                               const std::string& deviceId, size_t referenceIndex,
                                                               const std::vector<double>& expectedPhaseRad, double frequencyHz)
{
    using R = Result<std::vector<CalibrationRecord>>;
    if (phasors.size() != channelIds.size() || referenceIndex >= phasors.size() ||
        (!expectedPhaseRad.empty() && expectedPhaseRad.size() != phasors.size()))
        return R::failure("array calibration needs one phasor (and expected phase) per channel and a valid reference");
    auto own = [&](size_t i) { return phasors[i] * std::polar(1.0, expectedPhaseRad.empty() ? 0.0 : -expectedPhaseRad[i]); };
    const Complex reference = own(referenceIndex);
    if (std::abs(reference) == 0.0)
        return R::failure("the reference channel received nothing");
    std::vector<CalibrationRecord> out;
    for (size_t i = 0; i < phasors.size(); ++i) {
        const Complex ratio = own(i) / reference;
        CalibrationRecord r;
        r.type = CalibrationType::ChannelToChannel;
        r.deviceId = deviceId;
        r.channelId = channelIds[i];
        r.scalar = std::abs(ratio);
        r.unit = "1";
        r.phaseRad = std::arg(ratio);
        r.referenceFrequencyHz = frequencyHz;
        r.minFrequencyHz = r.maxFrequencyHz = frequencyHz;
        r.procedure = "a common far-field source received on every channel; complex amplitude at " + std::to_string(frequencyHz) +
                      " Hz relative to channel " + channelIds[referenceIndex] +
                      (expectedPhaseRad.empty() ? std::string(" (source at broadside: no geometric phase)") : std::string(", the "
                                                                                                                "geometric phase removed"));
        r.source = "derived by Resonant";
        out.push_back(r);
    }
    return out;
}

Result<EmArraySettings> applyArrayCalibration(const EmArraySettings& settings, const std::vector<const CalibrationRecord*>& perElement)
{
    if (static_cast<int>(perElement.size()) != settings.elementCount())
        return Result<EmArraySettings>::failure("the array has " + std::to_string(settings.elementCount()) + " elements but " +
                                                std::to_string(perElement.size()) + " calibrations were given");
    EmArraySettings out = settings;
    for (int i = 0; i < settings.elementCount(); ++i) {
        const CalibrationRecord* c = perElement[static_cast<size_t>(i)];
        if (!c)
            continue;
        if (!(c->scalar > 0.0))
            return Result<EmArraySettings>::failure("calibration " + c->id + " has no gain");
        ElementExcitation& e = out.overrides[i];
        const double amplitude = e.amplitude.value_or(settings.amplitude);
        const double phase = e.phase.value_or(0.0);
        e.amplitude = amplitude / c->scalar;
        e.phase = phase - c->phaseRad;
    }
    return out;
}

Result<MeasuredPattern> measuredPattern(const SpatialDataset& angular)
{
    if (angular.samples.size() < 3)
        return Result<MeasuredPattern>::failure("a pattern needs at least three angles");
    std::vector<std::pair<double, double>> points;
    double largest = 0.0;
    for (const auto& s : angular.samples) {
        points.emplace_back(std::fmod(s.angleDeg + 360.0, 360.0), std::abs(s.value));
        largest = std::max(largest, std::abs(s.value));
    }
    if (largest == 0.0)
        return Result<MeasuredPattern>::failure("nothing was received at any angle");
    std::sort(points.begin(), points.end());
    MeasuredPattern p;
    for (const auto& [a, v] : points) {
        p.angleDeg.push_back(a);
        p.relativeDb.push_back(20.0 * std::log10(std::max(v, 1e-12 * largest) / largest));
    }
    const size_t peak = static_cast<size_t>(std::max_element(p.relativeDb.begin(), p.relativeDb.end()) - p.relativeDb.begin());
    p.peakAngleDeg = p.angleDeg[peak];
    const bool periodic = p.angleDeg.back() - p.angleDeg.front() > 300.0;
    p.halfPowerBeamwidthDeg = halfPowerWidth(p.angleDeg, p.relativeDb, peak, periodic);
    p.note = "relative pattern (normalized to the strongest angle) from " + std::to_string(points.size()) +
             " angles; absolute gain needs a calibrated measurement chain";
    return p;
}

MeasuredPattern patternCut(const PatternGrid& grid, double thetaDeg)
{
    MeasuredPattern p;
    if (grid.thetaCount == 0 || grid.phiCount == 0 || !(grid.stepRad > 0.0))
        return p;
    const int i = std::clamp(static_cast<int>(std::lround(thetaDeg * kPi / 180.0 / grid.stepRad)), 0, grid.thetaCount - 1);
    double largest = 0.0;
    for (int j = 0; j < grid.phiCount; ++j)
        largest = std::max(largest, grid.value(i, j));
    for (int j = 0; j < grid.phiCount; ++j) {
        p.angleDeg.push_back(j * grid.stepRad * 180.0 / kPi);
        p.relativeDb.push_back(20.0 * std::log10(std::max(grid.value(i, j), 1e-12) / std::max(largest, 1e-300)));
    }
    const size_t peak = static_cast<size_t>(std::max_element(p.relativeDb.begin(), p.relativeDb.end()) - p.relativeDb.begin());
    p.peakAngleDeg = p.angleDeg[peak];
    p.halfPowerBeamwidthDeg = halfPowerWidth(p.angleDeg, p.relativeDb, peak, true);
    p.note = "cut at theta = " + std::to_string(thetaDeg) + " deg of the simulated pattern (" + grid.model + ")";
    return p;
}

Result<PatternComparison> comparePattern(const MeasuredPattern& measured, const MeasuredPattern& simulated)
{
    if (measured.angleDeg.empty() || simulated.angleDeg.size() < 2)
        return Result<PatternComparison>::failure("nothing to compare");
    auto sim = [&](double a) {
        a = std::fmod(a + 360.0, 360.0);
        auto upper = std::lower_bound(simulated.angleDeg.begin(), simulated.angleDeg.end(), a);
        size_t j = static_cast<size_t>(upper - simulated.angleDeg.begin());
        const size_t n = simulated.angleDeg.size();
        const size_t i = (j + n - 1) % n;
        j %= n;
        double span = simulated.angleDeg[j] - simulated.angleDeg[i];
        if (span <= 0.0)
            span += 360.0;
        double offset = a - simulated.angleDeg[i];
        if (offset < 0.0)
            offset += 360.0;
        const double t = std::clamp(offset / span, 0.0, 1.0);
        return simulated.relativeDb[i] + t * (simulated.relativeDb[j] - simulated.relativeDb[i]);
    };
    PatternComparison c;
    double sum = 0.0;
    for (size_t k = 0; k < measured.angleDeg.size(); ++k) {
        const double s = sim(measured.angleDeg[k]);
        if (measured.relativeDb[k] < -30.0 || s < -30.0)
            continue;
        const double d = measured.relativeDb[k] - s;
        sum += d * d;
        ++c.points;
    }
    c.rmsDifferenceDb = c.points ? std::sqrt(sum / static_cast<double>(c.points)) : 0.0;
    c.beamPointingErrorDeg = wrapDeg(measured.peakAngleDeg - simulated.peakAngleDeg);
    c.measuredBeamwidthDeg = measured.halfPowerBeamwidthDeg;
    c.simulatedBeamwidthDeg = simulated.halfPowerBeamwidthDeg;
    c.note = "relative patterns (both normalized to their maximum); the difference is taken where both lie above -30 dB";
    return c;
}

Result<double> receivedPowerDbm(double readingDbfs, const RfMeasurementChain& chain)
{
    if (!chain.receiverDbmAtFullScale)
        return Result<double>::failure("the receiver is not calibrated (no input power for full scale): only relative levels "
                                       "(dBFS) are known");
    if (!chain.cableLossDb)
        return Result<double>::failure("the cable loss is not stated: the power at the antenna terminals is unknown");
    return readingDbfs + *chain.receiverDbmAtFullScale + *chain.cableLossDb;
}

Result<double> fieldStrengthDbuvPerM(double readingDbfs, const RfMeasurementChain& chain)
{
    auto power = receivedPowerDbm(readingDbfs, chain);
    if (!power)
        return power;
    if (!chain.antennaFactorDbPerM)
        return Result<double>::failure("the antenna factor is not stated: the field strength is unknown");
    // P (dBm) -> V (dBuV) across the reference impedance: V = 10 log10(P_mW * 1e-3 * R) * 2 + 120.
    const double voltageDbuv = power.value() - 30.0 + 10.0 * std::log10(chain.referenceImpedance) + 120.0;
    return voltageDbuv + *chain.antennaFactorDbPerM;
}

// --- templates ------------------------------------------------------------------------------

Experiment antennaImpedanceExperiment(const std::string& id, double centreHz)
{
    Experiment e;
    e.id = id;
    e.name = "Antenna impedance measurement";
    e.description = "S11 of an antenna with a vector network analyser; return loss, VSWR and impedance, for comparison with the "
                    "simulated antenna impedance.";
    e.domain = PhysicsDomain::Electromagnetic;
    e.templateId = "rf.antenna-impedance";
    e.sweepDeviceId = "sim:vna-2";
    e.sweep.startHz = 0.8 * centreHz;
    e.sweep.stopHz = 1.2 * centreHz;
    e.sweep.points = 201;
    e.sweep.powerDbm = -10.0;
    e.sweep.ports = {1};
    DeviceConfiguration c;
    c.options = {{"dutResonanceHz", centreHz}};
    e.devices["sim:vna-2"] = c;
    e.safety.maxTransmitPowerDbm = 0.0;
    e.safety.allowedOutputChannels = {"sim:vna-2"};
    e.safety.minFrequencyHz = 0.5 * centreHz;
    e.safety.maxFrequencyHz = 1.5 * centreHz;
    e.safety.regulatoryNote = "test-port power into the antenna under test in a shielded or licensed test environment";
    return e;
}

Experiment arrayCalibrationExperiment(const std::string& id, double frequencyHz)
{
    Experiment e;
    e.id = id;
    e.name = "Antenna array channel calibration";
    e.description = "A far-field source at broadside received by the four channels of a coherent receiver; the complex amplitude of "
                    "the tone on each channel gives its gain and phase relative to channel 1.";
    e.domain = PhysicsDomain::Electromagnetic;
    e.templateId = "rf.array-calibration";
    e.acquisition.sampleRateHz = 1e6;
    e.acquisition.durationSeconds = 0.02;
    e.acquisition.blockFrames = 4096;
    e.acquisition.format = SampleFormat::ComplexFloat32;
    e.acquisition.centerFrequencyHz = frequencyHz - 100e3;
    DeviceConfiguration rx;
    rx.format = SampleFormat::ComplexFloat32;
    rx.options = {{"coherent", true}};
    e.devices["sim:rf-rx-4"] = rx;
    DeviceConfiguration sg;
    sg.format = SampleFormat::ComplexFloat32;
    e.devices["sim:siggen"] = sg;
    for (int i = 0; i < 4; ++i) {
        Sensor s;
        s.id = "element-" + std::to_string(i + 1);
        s.name = "Array element " + std::to_string(i + 1);
        s.category = SensorCategory::RfReceiverChannel;
        s.domain = PhysicsDomain::Electromagnetic;
        s.measures = Quantity::DigitalFullScale;
        s.pose.x = 0.15 * i;
        e.sensors.push_back(s);
        ChannelBinding b;
        b.id = "ch-rx-" + std::to_string(i + 1);
        b.deviceId = "sim:rf-rx-4";
        b.channelId = "rx" + std::to_string(i);
        b.direction = ChannelDirection::Input;
        b.sensorId = s.id;
        b.reference = i == 0;
        e.channels.push_back(b);
    }
    Actuator tx;
    tx.id = "source";
    tx.name = "Far-field calibration source";
    tx.category = ActuatorCategory::RfTransmitter;
    tx.domain = PhysicsDomain::Electromagnetic;
    tx.commandQuantity = Quantity::Power;
    tx.outputQuantity = Quantity::ElectricField;
    e.actuators.push_back(tx);
    ChannelBinding out;
    out.id = "ch-tx";
    out.deviceId = "sim:siggen";
    out.channelId = "rf0";
    out.direction = ChannelDirection::Output;
    out.actuatorId = "source";
    e.channels.push_back(out);
    e.stimulus.kind = StimulusKind::RfCarrier;
    e.stimulus.startHz = e.stimulus.endHz = frequencyHz;
    e.stimulus.powerDbm = -20.0;
    e.stimulus.outputBindings = {"ch-tx"};
    e.safety.maxTransmitPowerDbm = -10.0;
    e.safety.allowedOutputChannels = {"ch-tx"};
    e.safety.minFrequencyHz = 0.9 * frequencyHz;
    e.safety.maxFrequencyHz = 1.1 * frequencyHz;
    e.safety.regulatoryNote = "low-power test transmission in a shielded enclosure or under the applicable licence";
    e.synchronization.phaseSensitive = true;
    e.synchronization.strategy = "single-device";
    ProcessingStep tone;
    tone.operation = "tone";
    tone.parameters = {{"frequencyHz", 100e3}, {"window", "hann"}};
    e.processing.push_back(tone);
    return e;
}

Experiment radiationPatternExperiment(const std::string& id, double frequencyHz)
{
    Experiment e = arrayCalibrationExperiment(id, frequencyHz);
    e.name = "Antenna radiation pattern";
    e.description = "A probe antenna moved around the antenna under test (which transmits) in the azimuth plane; the received "
                    "tone amplitude against angle is the measured relative pattern.";
    e.templateId = "rf.radiation-pattern";
    e.sensors.resize(1);
    e.sensors[0].id = "probe";
    e.sensors[0].name = "Probe antenna";
    e.sensors[0].category = SensorCategory::Antenna;
    e.channels.erase(std::remove_if(e.channels.begin(), e.channels.end(),
                                    [](const ChannelBinding& b) { return b.direction == ChannelDirection::Input && b.channelId != "rx0"; }),
                     e.channels.end());
    e.channels[0].id = "ch-probe";
    e.channels[0].sensorId = "probe";
    e.devices["sim:rf-rx-4"].options = {{"coherent", true}, {"autPattern", true}};
    e.devices["sim:positioner"] = DeviceConfiguration{};
    e.positionerDeviceId = "sim:positioner";
    e.probeBindingId = "ch-probe";
    e.positions.kind = ScanKind::Angular;
    e.positions.radius = 3.0;
    e.positions.startDeg = -180.0;
    e.positions.stopDeg = 175.0;
    e.positions.stepDeg = 5.0;
    e.positions.plane = "azimuth";
    e.acquisition.durationSeconds = 0.005;
    e.synchronization.phaseSensitive = false;
    e.synchronization.strategy = "none";
    return e;
}

}  // namespace resonant::em
