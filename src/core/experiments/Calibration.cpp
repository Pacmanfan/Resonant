#include "experiments/Calibration.h"

#include "dsp/CrossCorrelation.h"
#include "dsp/SpectralEstimation.h"
#include "foundation/Id.h"
#include "foundation/Time.h"
#include "foundation/Units.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

namespace resonant::experiments {

namespace {

constexpr std::array<std::pair<CalibrationType, std::string_view>, 9> kTypes{{
    {CalibrationType::Amplitude, "amplitude"},
    {CalibrationType::Phase, "phase"},
    {CalibrationType::Timing, "timing"},
    {CalibrationType::Frequency, "frequency"},
    {CalibrationType::Position, "position"},
    {CalibrationType::Orientation, "orientation"},
    {CalibrationType::SensorSensitivity, "sensor-sensitivity"},
    {CalibrationType::ActuatorTransferFunction, "actuator-transfer-function"},
    {CalibrationType::ChannelToChannel, "channel-to-channel"},
}};

std::string format(double v)
{
    std::ostringstream out;
    out.precision(6);
    out << v;
    return out.str();
}

Status sortResponse(std::vector<ResponsePoint>& response)
{
    std::sort(response.begin(), response.end(), [](const ResponsePoint& a, const ResponsePoint& b) { return a.frequencyHz < b.frequencyHz; });
    for (size_t i = 0; i < response.size(); ++i) {
        if (!(response[i].frequencyHz >= 0.0) || !std::isfinite(response[i].value.real()) || !std::isfinite(response[i].value.imag()))
            return Status::failure("a calibration response point is not finite");
        if (i > 0 && response[i].frequencyHz == response[i - 1].frequencyHz)
            return Status::failure("the calibration response has two values at " + format(response[i].frequencyHz) + " Hz");
    }
    return Status::success();
}

// Bins of a transfer-function estimate inside the band with enough coherence.
struct Selected {
    std::vector<ResponsePoint> points;
    size_t rejected = 0;
};

Selected selectBins(const dsp::TransferFunctionEstimate& t, double minHz, double maxHz, double minCoherence, double scale)
{
    Selected s;
    for (size_t k = 0; k < t.h1.size(); ++k) {
        const double f = t.grid.frequencyHz[k];
        if (f < minHz || f > maxHz)
            continue;
        if (t.coherence[k] < minCoherence) {
            ++s.rejected;
            continue;
        }
        s.points.push_back({f, t.h1[k] * scale, t.magnitudeRandomError[k]});
    }
    return s;
}

}  // namespace

std::string_view toString(CalibrationType type)
{
    for (const auto& [t, name] : kTypes)
        if (t == type)
            return name;
    return "amplitude";
}

Result<CalibrationType> calibrationTypeFromString(std::string_view text)
{
    for (const auto& [t, name] : kTypes)
        if (name == text)
            return t;
    return Result<CalibrationType>::failure("unknown calibration type '" + std::string(text) + "'");
}

Result<std::complex<double>> CalibrationRecord::valueAt(double f) const
{
    using R = Result<std::complex<double>>;
    if (response.empty()) {
        if (!(scalar > 0.0))
            return R::failure("calibration " + id + " has neither a scalar value nor a response table");
        return std::complex<double>(scalar, 0.0) * std::polar(1.0, phaseRad);
    }
    if (f < response.front().frequencyHz || f > response.back().frequencyHz)
        return R::failure("calibration " + id + " covers " + format(response.front().frequencyHz) + " ... " +
                          format(response.back().frequencyHz) + " Hz; " + format(f) + " Hz lies outside (no extrapolation)");
    auto upper = std::lower_bound(response.begin(), response.end(), f,
                                  [](const ResponsePoint& p, double value) { return p.frequencyHz < value; });
    if (upper->frequencyHz == f)
        return upper->value;
    const auto lower = upper - 1;
    const double t = (f - lower->frequencyHz) / (upper->frequencyHz - lower->frequencyHz);
    const double magnitude = std::abs(lower->value) + t * (std::abs(upper->value) - std::abs(lower->value));
    double p0 = std::arg(lower->value), p1 = std::arg(upper->value);
    while (p1 - p0 > units::pi)
        p1 -= 2.0 * units::pi;
    while (p1 - p0 < -units::pi)
        p1 += 2.0 * units::pi;
    return std::polar(magnitude, p0 + t * (p1 - p0));
}

bool CalibrationRecord::covers(double fromHz, double toHz) const
{
    if (!response.empty())
        return fromHz >= response.front().frequencyHz && toHz <= response.back().frequencyHz;
    if (minFrequencyHz == 0.0 && maxFrequencyHz == 0.0)
        return true;
    return fromHz >= minFrequencyHz && toHz <= maxFrequencyHz;
}

bool CalibrationRecord::expiredAt(const std::string& nowUtc) const { return !validUntilUtc.empty() && nowUtc > validUntilUtc; }

nlohmann::json toJson(const CalibrationRecord& r)
{
    nlohmann::json response = nlohmann::json::array();
    for (const auto& p : r.response)
        response.push_back({p.frequencyHz, p.value.real(), p.value.imag(), p.relativeUncertainty});
    return {{"id", r.id},
            {"supersedes", r.supersedes},
            {"version", r.version},
            {"type", toString(r.type)},
            {"deviceId", r.deviceId},
            {"channelId", r.channelId},
            {"sensorId", r.sensorId},
            {"actuatorId", r.actuatorId},
            {"dateUtc", r.dateUtc},
            {"procedure", r.procedure},
            {"referenceInstrument", r.referenceInstrument},
            {"operator", r.operatorName},
            {"source", r.source},
            {"scalar", r.scalar},
            {"unit", r.unit},
            {"response", response},
            {"responseColumns", {"frequencyHz", "re", "im", "relativeUncertainty"}},
            {"scalarJustification", r.scalarJustification},
            {"delaySeconds", r.delaySeconds},
            {"phaseRad", r.phaseRad},
            {"referenceFrequencyHz", r.referenceFrequencyHz},
            {"positionOffset", {r.positionOffset[0], r.positionOffset[1], r.positionOffset[2]}},
            {"frequencyErrorPpm", r.frequencyErrorPpm},
            {"frequencyRangeHz", {r.minFrequencyHz, r.maxFrequencyHz}},
            {"amplitudeRange", {r.minAmplitude, r.maxAmplitude}},
            {"temperatureC", r.temperatureC},
            {"temperatureRecorded", r.temperatureRecorded},
            {"validUntilUtc", r.validUntilUtc},
            {"files", r.files},
            {"uncertainty", toJson(r.uncertainty)},
            {"fromSimulatedDevice", r.fromSimulatedDevice}};
}

Result<CalibrationRecord> calibrationRecordFromJson(const nlohmann::json& v)
{
    using R = Result<CalibrationRecord>;
    if (!v.is_object())
        return R::failure("a calibration record must be an object");
    CalibrationRecord r;
    r.id = v.value("id", std::string());
    auto type = calibrationTypeFromString(v.value("type", std::string("sensor-sensitivity")));
    if (!type)
        return R::failure("calibration " + r.id + ": " + type.message());
    r.type = type.value();
    r.supersedes = v.value("supersedes", std::string());
    r.version = v.value("version", 1);
    r.deviceId = v.value("deviceId", std::string());
    r.channelId = v.value("channelId", std::string());
    r.sensorId = v.value("sensorId", std::string());
    r.actuatorId = v.value("actuatorId", std::string());
    r.dateUtc = v.value("dateUtc", std::string());
    r.procedure = v.value("procedure", std::string());
    r.referenceInstrument = v.value("referenceInstrument", std::string());
    r.operatorName = v.value("operator", std::string());
    r.source = v.value("source", std::string());
    r.scalar = v.value("scalar", 0.0);
    r.unit = v.value("unit", std::string());
    for (const auto& p : v.value("response", nlohmann::json::array())) {
        if (!p.is_array() || p.size() < 3)
            return R::failure("calibration " + r.id + ": a response point is [frequencyHz, re, im, relativeUncertainty]");
        r.response.push_back({p[0].get<double>(), {p[1].get<double>(), p[2].get<double>()}, p.size() > 3 ? p[3].get<double>() : 0.0});
    }
    if (Status ok = sortResponse(r.response); !ok)
        return R::failure("calibration " + r.id + ": " + ok.message());
    r.scalarJustification = v.value("scalarJustification", std::string());
    r.delaySeconds = v.value("delaySeconds", 0.0);
    r.phaseRad = v.value("phaseRad", 0.0);
    r.referenceFrequencyHz = v.value("referenceFrequencyHz", 0.0);
    const auto offset = v.value("positionOffset", nlohmann::json::array({0.0, 0.0, 0.0}));
    for (size_t i = 0; i < 3 && i < offset.size(); ++i)
        r.positionOffset[i] = offset[i].get<double>();
    r.frequencyErrorPpm = v.value("frequencyErrorPpm", 0.0);
    const auto band = v.value("frequencyRangeHz", nlohmann::json::array({0.0, 0.0}));
    if (band.size() == 2) {
        r.minFrequencyHz = band[0].get<double>();
        r.maxFrequencyHz = band[1].get<double>();
    }
    const auto amplitude = v.value("amplitudeRange", nlohmann::json::array({0.0, 0.0}));
    if (amplitude.size() == 2) {
        r.minAmplitude = amplitude[0].get<double>();
        r.maxAmplitude = amplitude[1].get<double>();
    }
    r.temperatureC = v.value("temperatureC", 0.0);
    r.temperatureRecorded = v.value("temperatureRecorded", false);
    r.validUntilUtc = v.value("validUntilUtc", std::string());
    r.files = v.value("files", std::vector<std::string>{});
    auto uncertainty = uncertaintyBudgetFromJson(v.value("uncertainty", nlohmann::json()));
    if (!uncertainty)
        return R::failure("calibration " + r.id + ": " + uncertainty.message());
    r.uncertainty = uncertainty.value();
    r.fromSimulatedDevice = v.value("fromSimulatedDevice", false);
    return r;
}

Result<std::string> CalibrationLibrary::add(CalibrationRecord record)
{
    using R = Result<std::string>;
    if (record.id.empty())
        record.id = "cal-" + Id::generate().str();
    if (find(record.id))
        return R::failure("calibration " + record.id + " exists already; a new calibration is a new record (calibration "
                          "history is never overwritten)");
    if (!record.supersedes.empty()) {
        const CalibrationRecord* previous = find(record.supersedes);
        if (!previous)
            return R::failure("calibration " + record.id + " supersedes the unknown record " + record.supersedes);
        for (const auto& r : records_)
            if (r.supersedes == record.supersedes)
                return R::failure("calibration " + record.supersedes + " is already superseded by " + r.id);
        record.version = previous->version + 1;
    }
    if (Status ok = sortResponse(record.response); !ok)
        return R::failure(ok.message());
    if (record.dateUtc.empty())
        record.dateUtc = utcNowIso8601();
    records_.push_back(record);
    return record.id;
}

const CalibrationRecord* CalibrationLibrary::find(const std::string& id) const
{
    for (const auto& r : records_)
        if (r.id == id)
            return &r;
    return nullptr;
}

std::vector<const CalibrationRecord*> CalibrationLibrary::history(const std::string& id) const
{
    std::vector<const CalibrationRecord*> chain;
    for (const CalibrationRecord* r = find(id); r && chain.size() < records_.size(); r = r->supersedes.empty() ? nullptr : find(r->supersedes))
        chain.push_back(r);
    std::reverse(chain.begin(), chain.end());
    return chain;
}

const CalibrationRecord* CalibrationLibrary::latest(CalibrationType type, const std::string& sensorId, const std::string& deviceId,
                                                    const std::string& channelId) const
{
    const CalibrationRecord* best = nullptr;
    for (const auto& r : records_) {
        if (r.type != type || r.sensorId != sensorId)
            continue;
        if (!deviceId.empty() && r.deviceId != deviceId)
            continue;
        if (!channelId.empty() && r.channelId != channelId)
            continue;
        const bool superseded =
            std::any_of(records_.begin(), records_.end(), [&](const CalibrationRecord& other) { return other.supersedes == r.id; });
        if (!superseded)
            best = &r;
    }
    return best;
}

nlohmann::json CalibrationLibrary::toJson() const
{
    nlohmann::json list = nlohmann::json::array();
    for (const auto& r : records_)
        list.push_back(experiments::toJson(r));
    return list;
}

Result<CalibrationLibrary> CalibrationLibrary::fromJson(const nlohmann::json& value)
{
    CalibrationLibrary library;
    if (value.is_null())
        return library;
    if (!value.is_array())
        return Result<CalibrationLibrary>::failure("the calibration library must be a list");
    for (const auto& v : value) {
        auto r = calibrationRecordFromJson(v);
        if (!r)
            return Result<CalibrationLibrary>::failure(r.message());
        if (library.find(r.value().id))
            return Result<CalibrationLibrary>::failure("calibration " + r.value().id + " appears twice");
        library.records_.push_back(r.value());
    }
    return library;
}

CalibrationStatus checkCalibration(const CalibrationRecord& r, double fromHz, double toHz, const std::string& nowUtc)
{
    CalibrationStatus s;
    if (r.response.empty() && !(r.scalar > 0.0) && r.type != CalibrationType::Timing && r.type != CalibrationType::Position)
        s.problems.push_back("calibration " + r.id + " holds no value");
    if (r.expiredAt(nowUtc))
        s.problems.push_back("calibration " + r.id + " expired on " + r.validUntilUtc);
    if (!r.covers(fromHz, toHz)) {
        if (r.response.empty())
            s.warnings.push_back("calibration " + r.id + " was made at " + format(r.minFrequencyHz) + " ... " +
                                 format(r.maxFrequencyHz) + " Hz; applying it across " + format(fromHz) + " ... " + format(toHz) +
                                 " Hz assumes a flat response there");
        else
            s.problems.push_back("calibration " + r.id + " does not cover " + format(fromHz) + " ... " + format(toHz) + " Hz");
    }
    if (r.unit.empty() && r.type != CalibrationType::Timing && r.type != CalibrationType::Position)
        s.warnings.push_back("calibration " + r.id + " states no unit");
    if (r.uncertainty.empty())
        s.warnings.push_back("calibration " + r.id + " states no uncertainty");
    if (r.fromSimulatedDevice)
        s.warnings.push_back("calibration " + r.id + " was derived from a simulated device: it tests the procedure and calibrates "
                             "nothing physical");
    s.usable = s.problems.empty();
    return s;
}

Result<std::vector<double>> applySensitivity(const std::vector<double>& raw, const CalibrationRecord& r)
{
    if (r.frequencyDependent() && r.scalarJustification.empty())
        return Result<std::vector<double>>::failure("calibration " + r.id + " is frequency dependent; apply it to a spectrum, "
                                                    "not as one scalar (or state why a scalar is justified)");
    if (!(r.scalar > 0.0))
        return Result<std::vector<double>>::failure("calibration " + r.id + " has no scalar sensitivity");
    std::vector<double> out(raw.size());
    for (size_t i = 0; i < raw.size(); ++i)
        out[i] = raw[i] / r.scalar;
    return out;
}

Result<std::vector<std::complex<double>>> applyResponse(const std::vector<double>& frequencyHz,
                                                        const std::vector<std::complex<double>>& values, const CalibrationRecord& r)
{
    using R = Result<std::vector<std::complex<double>>>;
    if (frequencyHz.size() != values.size())
        return R::failure("frequency and value counts differ");
    std::vector<std::complex<double>> out(values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        auto s = r.valueAt(frequencyHz[i]);
        if (!s)
            return R::failure(s.message());
        if (std::abs(s.value()) == 0.0)
            return R::failure("calibration " + r.id + " is zero at " + format(frequencyHz[i]) + " Hz");
        out[i] = values[i] / s.value();
    }
    return out;
}

Result<CalibrationRecord> sensitivityFromReference(const std::vector<double>& recording, double fs, double referenceRms,
                                                   double referenceFrequencyHz, Quantity physical, Quantity raw,
                                                   double referenceRelativeUncertainty)
{
    using R = Result<CalibrationRecord>;
    if (recording.size() < 1024)
        return R::failure("the calibration recording is too short (at least 1024 samples)");
    if (!(referenceRms > 0.0) || !(referenceFrequencyHz > 0.0) || referenceFrequencyHz >= fs / 2.0)
        return R::failure("the reference needs a positive level and a frequency below the Nyquist frequency");
    // Find the tone within +-2 % of the nominal frequency (reference sources are not exact).
    double bestF = referenceFrequencyHz, bestA = 0.0;
    const double span = 0.02 * referenceFrequencyHz;
    for (int i = -200; i <= 200; ++i) {
        const double f = referenceFrequencyHz + span * i / 200.0;
        const double a = std::abs(dsp::toneAmplitude(recording, fs, f, dsp::WindowKind::Hann));
        if (a > bestA) {
            bestA = a;
            bestF = f;
        }
    }
    // Refine with a parabola through the neighbours.
    const double step = span / 200.0;
    const double am = std::abs(dsp::toneAmplitude(recording, fs, bestF - step, dsp::WindowKind::Hann));
    const double ap = std::abs(dsp::toneAmplitude(recording, fs, bestF + step, dsp::WindowKind::Hann));
    const double denominator = am - 2.0 * bestA + ap;
    if (denominator < 0.0)
        bestF += 0.5 * step * (am - ap) / denominator;
    const double amplitude = std::abs(dsp::toneAmplitude(recording, fs, bestF, dsp::WindowKind::FlatTop));
    const double recordedRms = amplitude / std::sqrt(2.0);
    // Everything that is not the tone, as a noise estimate.
    const double totalRms = dsp::rms(recording);
    const double residual = std::sqrt(std::max(0.0, totalRms * totalRms - recordedRms * recordedRms));
    if (!(recordedRms > 0.0) || recordedRms < 3.0 * residual)
        return R::failure("the recording does not show a clear tone near " + format(referenceFrequencyHz) + " Hz (tone " +
                          format(recordedRms) + " RMS against " + format(residual) + " RMS of everything else)");
    CalibrationRecord r;
    r.type = CalibrationType::SensorSensitivity;
    r.scalar = recordedRms / referenceRms;
    r.unit = std::string(siUnit(raw)) + "/" + std::string(siUnit(physical));
    r.referenceFrequencyHz = bestF;
    r.minFrequencyHz = r.maxFrequencyHz = bestF;
    r.procedure = "reference source of " + format(referenceRms) + " " + std::string(siUnit(physical)) + " RMS at " +
                  format(referenceFrequencyHz) + " Hz (tone found at " + format(bestF) +
                  " Hz); flat-top single-frequency amplitude of the recording divided by the reference level";
    r.source = "derived by Resonant";
    UncertaintyComponent ref;
    ref.source = UncertaintySource::SensorCalibration;
    ref.type = UncertaintyType::Calibration;
    ref.relative = referenceRelativeUncertainty;
    ref.description = "level of the reference source (as stated by its calibration)";
    r.uncertainty.add(ref);
    UncertaintyComponent noise;
    noise.source = UncertaintySource::Noise;
    noise.type = UncertaintyType::Statistical;
    noise.relative = residual / recordedRms / std::sqrt(static_cast<double>(recording.size()) / 2.0);
    noise.description = "noise and distortion in the recording (residual RMS over the tone RMS, per sqrt(N/2))";
    r.uncertainty.add(noise);
    return r;
}

Result<CalibrationRecord> responseByComparison(const std::vector<double>& reference, const std::vector<double>& dut, double fs,
                                               double referenceSensitivity, const std::string& unit, double minHz, double maxHz,
                                               size_t segmentLength, double minCoherence)
{
    using R = Result<CalibrationRecord>;
    if (!(referenceSensitivity > 0.0))
        return R::failure("the reference sensor's sensitivity must be positive");
    dsp::WelchSettings settings;
    settings.segmentLength = segmentLength;
    auto t = dsp::estimateTransferFunction(reference, dut, fs, settings);
    if (!t)
        return R::failure(t.message());
    Selected s = selectBins(t.value(), minHz, maxHz, minCoherence, referenceSensitivity);
    if (s.points.empty())
        return R::failure("no frequency in " + format(minHz) + " ... " + format(maxHz) + " Hz has a coherence of at least " +
                          format(minCoherence));
    CalibrationRecord r;
    r.type = CalibrationType::SensorSensitivity;
    r.unit = unit;
    r.response = std::move(s.points);
    r.procedure = "comparison with a reference sensor of sensitivity " + format(referenceSensitivity) + " " + unit +
                  " under the same excitation; H1 transfer function (" + settings.describe() + ", " +
                  std::to_string(t.value().grid.averages) + " averages); " + std::to_string(s.rejected) +
                  " bin(s) below coherence " + format(minCoherence) + " left out";
    r.source = "derived by Resonant";
    UncertaintyComponent random;
    random.source = UncertaintySource::Noise;
    random.type = UncertaintyType::Statistical;
    double worst = 0.0;
    for (const auto& p : r.response)
        worst = std::max(worst, p.relativeUncertainty);
    random.relative = worst;
    random.description = "largest random error of |H1| over the band (per-frequency values in the response table)";
    r.uncertainty.add(random);
    return r;
}

Result<std::vector<CalibrationRecord>> channelToChannel(const std::vector<ChannelRecording>& channels, size_t referenceIndex,
                                                        double fs, double frequencyHz, long maxLagSamples)
{
    using R = Result<std::vector<CalibrationRecord>>;
    if (channels.size() < 2 || referenceIndex >= channels.size())
        return R::failure("channel-to-channel calibration needs at least two channels and a valid reference");
    const auto& ref = channels[referenceIndex];
    const std::complex<double> refTone = dsp::toneAmplitude(ref.samples, fs, frequencyHz, dsp::WindowKind::Hann);
    if (std::abs(refTone) == 0.0)
        return R::failure("the reference channel shows no signal at " + format(frequencyHz) + " Hz");
    std::vector<CalibrationRecord> out;
    for (size_t i = 0; i < channels.size(); ++i) {
        if (i == referenceIndex)
            continue;
        const auto& c = channels[i];
        if (c.samples.size() != ref.samples.size())
            return R::failure("channel " + c.channelId + " has a different length than the reference");
        const std::complex<double> tone = dsp::toneAmplitude(c.samples, fs, frequencyHz, dsp::WindowKind::Hann);
        auto delay = dsp::estimateDelay(ref.samples, c.samples, fs, maxLagSamples);
        if (!delay)
            return R::failure("channel " + c.channelId + ": " + delay.message());
        const std::complex<double> ratio = tone / refTone;
        CalibrationRecord r;
        r.type = CalibrationType::ChannelToChannel;
        r.deviceId = c.deviceId;
        r.channelId = c.channelId;
        r.scalar = std::abs(ratio);
        r.unit = "1";
        r.phaseRad = std::arg(ratio);
        r.referenceFrequencyHz = frequencyHz;
        r.minFrequencyHz = r.maxFrequencyHz = frequencyHz;
        r.delaySeconds = delay.value().delaySeconds;
        r.procedure = "common signal recorded on all channels; gain ratio and phase at " + format(frequencyHz) +
                      " Hz relative to channel " + ref.deviceId + "/" + ref.channelId + "; delay: " + delay.value().method;
        r.source = "derived by Resonant";
        UncertaintyComponent clock;
        clock.source = UncertaintySource::Clock;
        clock.type = UncertaintyType::Statistical;
        clock.absolute = delay.value().standardUncertaintySeconds;
        clock.description = "delay estimate (s)";
        r.uncertainty.add(clock);
        out.push_back(r);
    }
    return out;
}

Result<CalibrationRecord> actuatorTransferFunction(const std::vector<double>& command, const std::vector<double>& output, double fs,
                                                   const std::string& unit, double minHz, double maxHz, size_t segmentLength,
                                                   double minCoherence)
{
    using R = Result<CalibrationRecord>;
    dsp::WelchSettings settings;
    settings.segmentLength = segmentLength;
    auto t = dsp::estimateTransferFunction(command, output, fs, settings);
    if (!t)
        return R::failure(t.message());
    Selected s = selectBins(t.value(), minHz, maxHz, minCoherence, 1.0);
    if (s.points.empty())
        return R::failure("no frequency in " + format(minHz) + " ... " + format(maxHz) + " Hz has a coherence of at least " +
                          format(minCoherence));
    CalibrationRecord r;
    r.type = CalibrationType::ActuatorTransferFunction;
    r.unit = unit;
    r.response = std::move(s.points);
    if (auto delay = dsp::estimateDelay(command, output, fs, static_cast<long>(fs * 0.05)))
        r.delaySeconds = delay.value().delaySeconds;
    r.procedure = "commanded signal against the physical output measured with a calibrated sensor; H1 (" + settings.describe() +
                  ", " + std::to_string(t.value().grid.averages) + " averages); " + std::to_string(s.rejected) +
                  " bin(s) below coherence " + format(minCoherence) + " left out; the delay is the bulk delay from "
                  "cross-correlation";
    r.source = "derived by Resonant";
    UncertaintyComponent random;
    random.source = UncertaintySource::Noise;
    random.type = UncertaintyType::Statistical;
    for (const auto& p : r.response)
        random.relative = std::max(random.relative, p.relativeUncertainty);
    random.description = "largest random error of |H1| over the band";
    r.uncertainty.add(random);
    return r;
}

}  // namespace resonant::experiments
