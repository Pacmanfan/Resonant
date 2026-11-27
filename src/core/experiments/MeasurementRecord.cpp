#include "experiments/MeasurementRecord.h"

#include <array>
#include <sstream>

namespace resonant::experiments {

namespace {

constexpr std::array<std::pair<MeasurementKind, std::string_view>, 7> kKinds{{
    {MeasurementKind::TimeSeries, "time-series"},
    {MeasurementKind::Spectrum, "spectrum"},
    {MeasurementKind::PowerSpectrum, "power-spectrum"},
    {MeasurementKind::TransferFunction, "transfer-function"},
    {MeasurementKind::NetworkParameters, "network-parameters"},
    {MeasurementKind::Scalar, "scalar"},
    {MeasurementKind::Vector, "vector"},
}};

}  // namespace

std::string_view toString(MeasurementKind kind)
{
    for (const auto& [k, name] : kKinds)
        if (k == kind)
            return name;
    return "time-series";
}

Result<MeasurementKind> measurementKindFromString(std::string_view text)
{
    for (const auto& [k, name] : kKinds)
        if (name == text)
            return k;
    return Result<MeasurementKind>::failure("unknown measurement kind '" + std::string(text) + "'");
}

bool DataQuality::clean() const
{
    return complete && droppedFrames == 0 && gaps == 0 && clippedSamples == 0 && !saturated && timestampDiscontinuities == 0 &&
           invalidTimestamps == 0 && deviceErrors.empty();
}

std::string DataQuality::summary() const
{
    if (clean())
        return "complete; no dropped samples, gaps, clipping or timestamp problems";
    std::ostringstream out;
    out << (complete ? "complete" : "INCOMPLETE");
    if (droppedFrames)
        out << "; " << droppedFrames << " dropped frame(s) in " << gaps << " gap(s)";
    if (clippedSamples)
        out << "; " << clippedSamples << " clipped sample(s)" << (saturated ? " (saturated)" : "");
    if (timestampDiscontinuities)
        out << "; " << timestampDiscontinuities << " timestamp discontinuit" << (timestampDiscontinuities == 1 ? "y" : "ies");
    if (invalidTimestamps)
        out << "; " << invalidTimestamps << " invalid timestamp(s)";
    for (const auto& e : deviceErrors)
        out << "; device error: " << e;
    return out.str();
}

nlohmann::json toJson(const DataQuality& q)
{
    return {{"expectedFrames", q.expectedFrames},
            {"recordedFrames", q.recordedFrames},
            {"droppedFrames", q.droppedFrames},
            {"gaps", q.gaps},
            {"clippedSamples", q.clippedSamples},
            {"saturated", q.saturated},
            {"timestampDiscontinuities", q.timestampDiscontinuities},
            {"invalidTimestamps", q.invalidTimestamps},
            {"deviceErrors", q.deviceErrors},
            {"complete", q.complete},
            {"peakAbsolute", q.peakAbsolute},
            {"notes", q.notes},
            {"summary", q.summary()}};
}

DataQuality dataQualityFromJson(const nlohmann::json& v)
{
    DataQuality q;
    if (!v.is_object())
        return q;
    q.expectedFrames = v.value("expectedFrames", uint64_t{0});
    q.recordedFrames = v.value("recordedFrames", uint64_t{0});
    q.droppedFrames = v.value("droppedFrames", uint64_t{0});
    q.gaps = v.value("gaps", uint64_t{0});
    q.clippedSamples = v.value("clippedSamples", uint64_t{0});
    q.saturated = v.value("saturated", false);
    q.timestampDiscontinuities = v.value("timestampDiscontinuities", uint64_t{0});
    q.invalidTimestamps = v.value("invalidTimestamps", uint64_t{0});
    q.deviceErrors = v.value("deviceErrors", std::vector<std::string>{});
    q.complete = v.value("complete", true);
    q.peakAbsolute = v.value("peakAbsolute", 0.0);
    q.notes = v.value("notes", std::vector<std::string>{});
    return q;
}

nlohmann::json toJson(const MeasurementRecord& r)
{
    nlohmann::json history = nlohmann::json::array();
    for (const auto& s : r.history)
        history.push_back(toJson(s));
    nlohmann::json v = {{"id", r.id},
                        {"experimentId", r.experimentId},
                        {"runId", r.runId},
                        {"bindingId", r.bindingId},
                        {"sensorId", r.sensorId},
                        {"deviceId", r.deviceId},
                        {"channelId", r.channelId},
                        {"domain", toString(r.domain)},
                        {"kind", toString(r.kind)},
                        {"category", toString(r.category)},
                        {"quantity", toJson(r.quantity)},
                        {"timeBase",
                         {{"sampleRateHz", r.timeBase.sampleRateHz},
                          {"startUtc", r.timeBase.startUtc},
                          {"startSeconds", r.timeBase.startSeconds},
                          {"clockDomain", r.timeBase.clockDomain},
                          {"hardwareTimestamps", r.timeBase.hardwareTimestamps},
                          {"timingCorrectionSeconds", r.timeBase.timingCorrectionSeconds}}},
                        {"dataRef", r.dataRef},
                        {"shape", r.shape},
                        {"nativeFormat", toString(r.nativeFormat)},
                        {"simulatedDevice", r.simulatedDevice},
                        {"calibrationId", r.calibrationId},
                        {"sourceIds", r.sourceIds},
                        {"history", history},
                        {"uncertainty", toJson(r.uncertainty)},
                        {"quality", toJson(r.quality)},
                        {"acquisition", r.acquisition},
                        {"createdUtc", r.createdUtc},
                        {"note", r.note}};
    if (r.pose)
        v["pose"] = toJson(*r.pose);
    return v;
}

Result<MeasurementRecord> measurementRecordFromJson(const nlohmann::json& v)
{
    using R = Result<MeasurementRecord>;
    if (!v.is_object())
        return R::failure("a measurement record must be an object");
    MeasurementRecord r;
    r.id = v.value("id", std::string());
    if (r.id.empty())
        return R::failure("a measurement record needs an id");
    r.experimentId = v.value("experimentId", std::string());
    r.runId = v.value("runId", std::string());
    r.bindingId = v.value("bindingId", std::string());
    r.sensorId = v.value("sensorId", std::string());
    r.deviceId = v.value("deviceId", std::string());
    r.channelId = v.value("channelId", std::string());
    const std::string domain = v.value("domain", std::string("acoustic"));
    if (auto d = physicsDomainFromString(domain))
        r.domain = *d;
    else
        return R::failure("measurement '" + r.id + "': unknown domain '" + domain + "'");
    auto kind = measurementKindFromString(v.value("kind", std::string("time-series")));
    auto category = dataCategoryFromString(v.value("category", std::string("raw")));
    auto quantity = quantitySpecFromJson(v.value("quantity", nlohmann::json::object()));
    auto format = sampleFormatFromString(v.value("nativeFormat", std::string("float64")));
    auto uncertainty = uncertaintyBudgetFromJson(v.value("uncertainty", nlohmann::json()));
    for (const std::string* problem : {kind ? nullptr : &kind.message(), category ? nullptr : &category.message(),
                                       quantity ? nullptr : &quantity.message(), format ? nullptr : &format.message(),
                                       uncertainty ? nullptr : &uncertainty.message()})
        if (problem)
            return R::failure("measurement '" + r.id + "': " + *problem);
    r.kind = kind.value();
    r.category = category.value();
    r.quantity = quantity.value();
    r.nativeFormat = format.value();
    r.uncertainty = uncertainty.value();
    const auto t = v.value("timeBase", nlohmann::json::object());
    r.timeBase.sampleRateHz = t.value("sampleRateHz", 0.0);
    r.timeBase.startUtc = t.value("startUtc", std::string());
    r.timeBase.startSeconds = t.value("startSeconds", 0.0);
    r.timeBase.clockDomain = t.value("clockDomain", std::string());
    r.timeBase.hardwareTimestamps = t.value("hardwareTimestamps", false);
    r.timeBase.timingCorrectionSeconds = t.value("timingCorrectionSeconds", 0.0);
    if (v.contains("pose")) {
        auto pose = poseFromJson(v["pose"]);
        if (!pose)
            return R::failure("measurement '" + r.id + "': " + pose.message());
        r.pose = pose.value();
    }
    r.dataRef = v.value("dataRef", std::string());
    r.shape = v.value("shape", std::vector<size_t>{});
    r.simulatedDevice = v.value("simulatedDevice", false);
    r.calibrationId = v.value("calibrationId", std::string());
    r.sourceIds = v.value("sourceIds", std::vector<std::string>{});
    for (const auto& s : v.value("history", nlohmann::json::array())) {
        auto step = processingStepFromJson(s);
        if (!step)
            return R::failure("measurement '" + r.id + "': " + step.message());
        r.history.push_back(step.value());
    }
    r.quality = dataQualityFromJson(v.value("quality", nlohmann::json::object()));
    r.acquisition = v.value("acquisition", nlohmann::json::object());
    r.createdUtc = v.value("createdUtc", std::string());
    r.note = v.value("note", std::string());
    if (r.category == DataCategory::Raw && !r.history.empty())
        return R::failure("measurement '" + r.id + "' is marked raw but has a processing history");
    if (r.category != DataCategory::Raw && r.category != DataCategory::Simulated && r.category != DataCategory::Command &&
        r.sourceIds.empty())
        return R::failure("derived measurement '" + r.id + "' names no source data");
    return r;
}

}  // namespace resonant::experiments
