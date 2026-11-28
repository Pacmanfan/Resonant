#include "functional/Analysis.h"

#include <cmath>

namespace resonant::functional {

using nlohmann::json;

std::string_view toString(Category c)
{
    switch (c) {
    case Category::Propagation: return "propagation";
    case Category::Reflection: return "reflection";
    case Category::Focusing: return "focusing";
    case Category::Confinement: return "confinement";
    case Category::Dissipation: return "dissipation";
    case Category::Force: return "force";
    case Category::Coupled: return "coupled";
    }
    return "confinement";
}

const MetricInfo* AnalysisKind::metric(std::string_view key) const
{
    for (const MetricInfo& m : metrics)
        if (m.key == key)
            return &m;
    return nullptr;
}

const ModelInfo* AnalysisKind::model(std::string_view modelId) const
{
    if (modelId.empty())
        return models.empty() ? nullptr : &models.front();
    for (const ModelInfo& m : models)
        if (m.id == modelId)
            return &m;
    return nullptr;
}

namespace {
json number(double v)
{
    if (std::isnan(v))
        return nullptr;
    if (std::isinf(v))
        return v > 0 ? "inf" : "-inf";
    return v;
}
}  // namespace

json toJson(const AnalysisKind& k)
{
    json models = json::array(), metrics = json::array(), roles = json::array();
    for (const ModelInfo& m : k.models)
        models.push_back({{"id", m.id}, {"name", m.name}, {"version", m.version}, {"fidelity", m.fidelity}, {"assumptions", m.assumptions},
                          {"validity", m.validity}, {"typicalSeconds", m.typicalSeconds}});
    for (const MetricInfo& m : k.metrics)
        metrics.push_back({{"key", m.key}, {"label", m.label}, {"unit", m.unit}, {"definition", m.definition}});
    for (const RoleInfo& r : k.roles)
        roles.push_back({{"role", r.role}, {"kind", r.kind}, {"description", r.description}, {"required", r.required}});
    return {{"id", k.id},       {"title", k.title},       {"description", k.description}, {"domains", k.domains},
            {"category", toString(k.category)}, {"mechanism", k.mechanism}, {"models", models}, {"metrics", metrics}, {"roles", roles}};
}

json toJson(const AnalysisOutcome& o)
{
    json metrics = json::object();
    for (const auto& [key, value] : o.metrics)
        metrics[key] = number(value);
    json modes = json::array();
    for (const Mode& m : o.modes) {
        json losses = json::object();
        for (const auto& [mechanism, q] : m.lossQ)
            losses[mechanism] = number(q);
        modes.push_back({{"label", m.label},         {"frequencyHz", m.frequencyHz}, {"quality", number(m.quality)}, {"details", m.details},
                         {"nature", m.nature},       {"decayRate", m.decayRate},     {"bandwidthHz", m.bandwidthHz}, {"lossQ", losses}});
    }
    json curves = json::array();
    for (const Curve& c : o.curves)
        curves.push_back({{"id", c.id}, {"title", c.title}, {"xLabel", c.xLabel}, {"xUnit", c.xUnit}, {"yLabel", c.yLabel}, {"yUnit", c.yUnit},
                          {"x", c.x}, {"y", c.y}});
    return {{"metrics", metrics},       {"modes", modes},           {"curves", curves},       {"assumptions", o.assumptions},
            {"warnings", o.warnings},   {"model", {{"id", o.modelId}, {"name", o.modelName}, {"version", o.modelVersion}}},
            {"solver", o.solver},       {"behaviour", o.behaviour}, {"details", o.details},   {"discretization", o.discretization}};
}

void AnalysisRegistry::add(AnalysisKind kind)
{
    for (AnalysisKind& k : kinds_)
        if (k.id == kind.id) {
            k = std::move(kind);
            return;
        }
    kinds_.push_back(std::move(kind));
}

const AnalysisKind* AnalysisRegistry::find(std::string_view id) const
{
    for (const AnalysisKind& k : kinds_)
        if (k.id == id)
            return &k;
    return nullptr;
}

std::vector<Issue> validateForAnalysis(const FunctionalStructure& s, const AnalysisRegistry& registry)
{
    std::vector<Issue> issues;
    const AnalysisKind* kind = registry.find(s.analysis.kind);
    if (!kind) {
        issues.push_back({"analysis.kind", "no analysis '" + s.analysis.kind + "' (see ListAnalysisKinds)"});
        return issues;
    }
    if (!kind->model(s.analysis.model))
        issues.push_back({"analysis.model", "the analysis '" + kind->id + "' has no model '" + s.analysis.model + "'"});
    for (const RoleInfo& r : kind->roles) {
        if (!r.required)
            continue;
        const bool present = r.kind == "body" ? s.body(r.role) != nullptr : r.kind == "region" ? s.region(r.role) != nullptr : s.boundary(r.role) != nullptr;
        if (!present)
            issues.push_back({r.kind == "body" ? "bodies" : r.kind == "region" ? "regions" : "boundaries",
                              "the analysis needs a " + r.kind + " with the role '" + r.role + "' (" + r.description + ")"});
    }
    for (const Objective& o : s.objectives)
        if (!kind->metric(o.metric))
            issues.push_back({"objectives." + o.id + ".metric", "the analysis '" + kind->id + "' computes no metric '" + o.metric + "'"});
    return issues;
}

}  // namespace resonant::functional
