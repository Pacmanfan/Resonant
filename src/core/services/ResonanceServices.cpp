// Resonance spectroscopy as a service (REQ-CHAT-20261126-018, RES-RSN-013).
#include "services/Services.h"

#include "dsp/Resonance.h"

#include <cmath>

namespace resonant::services {

using nlohmann::json;

namespace {

json number(double v)
{
    if (std::isnan(v))
        return nullptr;
    if (std::isinf(v))
        return v > 0 ? "inf" : "-inf";
    return v;
}

constexpr size_t kMaxSamples = 200000;

}  // namespace

Outcome<json> ResonanceService::identify(const CallContext& c, const json& request)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    try {
        if (request.contains("samples")) {
            const auto samples = request.at("samples").get<std::vector<double>>();
            if (samples.size() > kMaxSamples)
                return invalid("at most 200 000 samples per request", "samples");
            dsp::PencilOptions o;
            o.minHz = request.value("minHz", 0.0);
            o.maxHz = request.value("maxHz", std::numeric_limits<double>::infinity());
            o.maxModes = std::clamp(request.value("maxModes", 12), 1, 60);
            o.skipSamples = request.value("skipSamples", static_cast<size_t>(0));
            auto id = dsp::identifyByMatrixPencil(samples, request.value("sampleInterval", 0.0), o);
            if (!id)
                return invalid(id.message(), "samples");
            json list = json::array();
            for (const auto& r : id.value().resonances)
                list.push_back({{"frequencyHz", r.frequencyHz},
                                {"decayRate", r.decayRate},
                                {"quality", number(r.quality)},
                                {"bandwidthHz", r.bandwidthHz},
                                {"amplitude", r.amplitude},
                                {"phase", r.phase},
                                {"frequencyUncertaintyHz", r.frequencyUncertainty},
                                {"qualityUncertainty", number(r.qualityUncertainty)},
                                {"overlapping", r.overlapping},
                                {"asymmetric", r.asymmetric},
                                {"fitQuality", r.fitQuality}});
            return json{{"method", "matrix-pencil"}, {"resonances", list}, {"relativeResidual", id.value().relativeResidual},
                        {"durationSeconds", id.value().durationSeconds}, {"warnings", id.value().warnings}};
        }
        if (request.contains("frequenciesHz")) {
            const auto f = request.at("frequenciesHz").get<std::vector<double>>();
            const auto y = request.at("values").get<std::vector<double>>();
            if (f.size() != y.size())
                return invalid("as many values as frequencies", "values");
            if (f.size() > kMaxSamples)
                return invalid("at most 200 000 points per request", "frequenciesHz");
            std::vector<std::string> warnings;
            const auto fits = dsp::fitResonancePeaks(f, y, std::clamp(request.value("count", 4), 1, 40), &warnings);
            json list = json::array();
            for (const auto& r : fits)
                list.push_back({{"frequencyHz", r.centerHz},
                                {"quality", r.quality},
                                {"bandwidthHz", 2.0 * r.halfWidthHz},
                                {"decayRate", 3.14159265358979323846 * 2.0 * r.halfWidthHz},
                                {"amplitude", r.amplitude},
                                {"baseline", r.baseline},
                                {"frequencyUncertaintyHz", r.centerUncertainty},
                                {"qualityUncertainty", r.qualityUncertainty},
                                {"overlapping", r.overlapping},
                                {"asymmetric", r.asymmetric},
                                {"fitQuality", 1.0 - r.relativeResidual * r.relativeResidual},
                                {"pointsInLinewidth", r.pointsInLinewidth}});
            if (fits.empty() && !warnings.empty())
                return invalid(warnings.front(), "frequenciesHz");
            return json{{"method", "lorentzian-fit"}, {"resonances", list}, {"warnings", warnings}};
        }
    } catch (const std::exception& e) {
        return invalid(std::string("invalid request: ") + e.what(), "request");
    }
    return invalid("give either samples and sampleInterval, or frequenciesHz and values", "request");
}

}  // namespace resonant::services
