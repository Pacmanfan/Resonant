#include "functional/FunctionalStudy.h"

#include "foundation/Version.h"
#include "optimization/Expression.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "simulation/Job.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <limits>

namespace resonant::functional {

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

std::string nowUtc()
{
    const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
    gmtime_r(&t, &tm);
    char text[32];
    std::strftime(text, sizeof text, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return text;
}

json modelJson(const ModelInfo& m)
{
    return {{"id", m.id}, {"name", m.name}, {"version", m.version}, {"fidelity", m.fidelity}, {"assumptions", m.assumptions}, {"validity", m.validity}};
}

json parametersJson(const std::vector<ParameterValue>& parameters)
{
    json a = json::array();
    for (const ParameterValue& p : parameters)
        a.push_back({{"id", p.id}, {"name", p.name}, {"kind", toString(p.kind)}, {"value", p.value}, {"unit", p.unit}});
    return a;
}

json objectivesJson(const std::vector<ObjectiveResult>& list)
{
    json a = json::array();
    for (const ObjectiveResult& o : list)
        a.push_back({{"id", o.id}, {"metric", o.metric}, {"sense", o.sense}, {"value", o.available ? number(o.value) : json(nullptr)},
                     {"available", o.available}, {"met", o.met}, {"deviation", number(o.deviation)}, {"weight", o.weight}});
    return a;
}

json constraintsJson(const std::vector<ConstraintResult>& list)
{
    json a = json::array();
    for (const ConstraintResult& c : list)
        a.push_back({{"id", c.id},           {"expression", c.expression}, {"relation", c.relation}, {"value", c.evaluated ? number(c.value) : json(nullptr)},
                     {"limit", c.limit},      {"satisfied", c.satisfied},   {"hard", c.hard},         {"category", c.category},
                     {"process", c.process},  {"message", c.message}});
    return a;
}

bool feasible(const std::vector<ConstraintResult>& list)
{
    return std::all_of(list.begin(), list.end(), [](const ConstraintResult& c) { return !c.hard || c.satisfied; });
}

json metricsJson(const AnalysisKind& kind, const Values& metrics)
{
    json a = json::array();
    for (const auto& [key, value] : metrics) {
        const MetricInfo* m = kind.metric(key);
        a.push_back({{"key", key}, {"label", m ? m->label : key}, {"unit", m ? m->unit : ""}, {"definition", m ? m->definition : ""}, {"value", number(value)}});
    }
    return a;
}

json discretizationSettings(const FunctionalStructure& s, const ModelInfo& model)
{
    return {{"kind", s.analysis.kind}, {"model", model.id}, {"version", model.version}, {"settings", s.analysis.settings}};
}

struct Prepared {
    const AnalysisKind* kind = nullptr;
    const ModelInfo* model = nullptr;
};

Result<Prepared> prepare(const FunctionalStructure& s, const Project& project, const AnalysisRegistry& registry, const std::string& modelId)
{
    using R = Result<Prepared>;
    std::vector<Issue> issues = validateStructure(s, project);
    for (Issue& i : validateForAnalysis(s, registry))
        issues.push_back(std::move(i));
    if (!issues.empty()) {
        std::string text = "the structure is not valid: ";
        for (size_t i = 0; i < issues.size() && i < 4; ++i)
            text += (i ? "; " : "") + issues[i].field + ": " + issues[i].message;
        return R::failure(text);
    }
    Prepared p;
    p.kind = registry.find(s.analysis.kind);
    p.model = p.kind->model(modelId.empty() ? s.analysis.model : modelId);
    if (!p.model)
        return R::failure("the analysis '" + p.kind->id + "' has no model '" + modelId + "'");
    return p;
}

}  // namespace

Result<std::pair<std::unique_ptr<Project>, FunctionalStructure>> withParameters(const FunctionalStructure& s, const Project& project, const Values& values)
{
    using R = Result<std::pair<std::unique_ptr<Project>, FunctionalStructure>>;
    auto copy = deserializeProject(serializeProject(project));
    if (!copy)
        return R::failure(copy.message());
    std::unique_ptr<Project> p = std::move(copy.value().project);
    FunctionalStructure structure = s;
    if (Status st = applyParameters(structure, *p, values); !st)
        return R::failure(st.message());
    return std::make_pair(std::move(p), std::move(structure));
}

Result<AnalysisOutcome> evaluateStructure(const FunctionalStructure& s, const Project& project, const AnalysisRegistry& registry,
                                          const std::string& modelId, JobContext* context)
{
    auto prepared = prepare(s, project, registry, modelId);
    if (!prepared)
        return Result<AnalysisOutcome>::failure(prepared.message());
    auto parameters = resolveParameters(s, project);
    if (!parameters)
        return Result<AnalysisOutcome>::failure(parameters.message());
    auto outcome = prepared.value().kind->evaluate(s, project, valuesOf(parameters.value()), *prepared.value().model, context);
    if (!outcome)
        return outcome;
    AnalysisOutcome& o = outcome.value();
    if (o.modelId.empty()) {
        o.modelId = prepared.value().model->id;
        o.modelName = prepared.value().model->name;
        o.modelVersion = prepared.value().model->version;
    }
    for (const auto& [key, value] : o.metrics)
        if (std::isnan(value))
            return Result<AnalysisOutcome>::failure("the model returned an invalid value for '" + key + "'");
    return outcome;
}

std::vector<ObjectiveResult> evaluateObjectives(const FunctionalStructure& s, const Values& metrics)
{
    std::vector<ObjectiveResult> out;
    for (const Objective& o : s.objectives) {
        ObjectiveResult r;
        r.id = o.id;
        r.metric = o.metric;
        r.sense = o.sense;
        r.weight = o.weight;
        const auto it = metrics.find(o.metric);
        if (it != metrics.end() && std::isfinite(it->second)) {
            r.available = true;
            r.value = it->second;
            if (o.sense == "target") {
                r.deviation = std::abs(r.value - o.target) / std::max(std::abs(o.target), 1e-300);
                r.met = r.deviation <= 0.01;
            } else if (o.sense == "within") {
                const double outside = r.value < o.low ? o.low - r.value : r.value > o.high ? r.value - o.high : 0.0;
                r.deviation = outside / (o.high - o.low);
                r.met = outside == 0.0;
            } else {
                r.deviation = r.value;
            }
        }
        out.push_back(r);
    }
    return out;
}

std::vector<ConstraintResult> evaluateConstraints(const FunctionalStructure& s, const Values& parameters, const Values& metrics)
{
    std::vector<ConstraintResult> out;
    Values names = parameters;
    for (const auto& [k, v] : metrics)
        names[k] = v;
    for (const Constraint& c : s.constraints) {
        ConstraintResult r;
        r.id = c.id;
        r.expression = c.expression;
        r.relation = c.relation;
        r.category = c.category;
        r.process = c.process;
        r.limit = c.limit;
        r.hard = c.hard;
        auto e = optimization::Expression::parse(c.expression);
        if (!e) {
            r.message = e.message();
            out.push_back(r);
            continue;
        }
        auto v = e.value().evaluate(names);
        if (!v) {
            r.message = v.message();
            out.push_back(r);
            continue;
        }
        r.evaluated = true;
        r.value = v.value();
        r.satisfied = c.relation == "<=" ? r.value <= c.limit : r.value >= c.limit;
        if (!r.satisfied)
            r.message = c.expression + " = " + std::to_string(r.value) + " violates " + c.relation + " " + std::to_string(c.limit) +
                        (c.unit.empty() ? "" : " " + c.unit) + (c.process.empty() ? "" : " (" + c.process + ")");
        out.push_back(r);
    }
    return out;
}

Result<json> analyzeDocument(const FunctionalStructure& s, const Project& project, const AnalysisRegistry& registry, const std::string& modelId,
                             JobContext* context)
{
    using R = Result<json>;
    auto prepared = prepare(s, project, registry, modelId);
    if (!prepared)
        return R::failure(prepared.message());
    const AnalysisKind& kind = *prepared.value().kind;
    const ModelInfo& model = *prepared.value().model;
    auto parameters = resolveParameters(s, project);
    if (!parameters)
        return R::failure(parameters.message());
    auto outcome = evaluateStructure(s, project, registry, model.id, context);
    if (!outcome)
        return R::failure(outcome.message());
    const Values values = valuesOf(parameters.value());
    const auto objectives = evaluateObjectives(s, outcome.value().metrics);
    const auto constraints = evaluateConstraints(s, values, outcome.value().metrics);
    json d = toJson(outcome.value());
    d["format"] = kResultFormat;
    d["version"] = 1;
    d["kind"] = "analysis";
    d["structure"] = {{"id", s.id}, {"name", s.name}, {"document", s.toJson()}};
    d["analysis"] = {{"kind", kind.id}, {"title", kind.title}, {"category", toString(kind.category)}, {"domains", kind.domains},
                     {"mechanism", kind.mechanism}, {"model", modelJson(model)}};
    d["parameters"] = parametersJson(parameters.value());
    d["metrics"] = metricsJson(kind, outcome.value().metrics);
    d["objectives"] = objectivesJson(objectives);
    d["constraints"] = constraintsJson(constraints);
    d["feasible"] = feasible(constraints);
    d["representations"] = {{"engineering", s.bodies.size()}, {"regions", s.regions.size()}, {"boundaries", s.boundaries.size()},
                            {"simulation", outcome.value().discretization}};
    d["dependencies"] = dependenciesOf(s, project, discretizationSettings(s, model)).toJson();
    d["provenance"] = {{"application", std::string("Resonant ") + kApplicationVersion}, {"created", nowUtc()}, {"model", model.id},
                       {"modelVersion", model.version}, {"solver", outcome.value().solver}};
    return d;
}

Result<json> sweepDocument(const FunctionalStructure& s, const Project& project, const AnalysisRegistry& registry, const std::vector<SweepAxis>& axes,
                           const std::string& modelId, JobContext* context)
{
    using R = Result<json>;
    auto prepared = prepare(s, project, registry, modelId);
    if (!prepared)
        return R::failure(prepared.message());
    const AnalysisKind& kind = *prepared.value().kind;
    const ModelInfo& model = *prepared.value().model;
    auto base = resolveParameters(s, project);
    if (!base)
        return R::failure(base.message());
    if (axes.empty())
        return R::failure("a sweep needs at least one parameter");
    size_t total = 1;
    for (const SweepAxis& a : axes) {
        const auto it = std::find_if(base.value().begin(), base.value().end(), [&](const ParameterValue& p) { return p.id == a.parameter; });
        if (it == base.value().end())
            return R::failure("the structure has no parameter '" + a.parameter + "'");
        if (!it->editable)
            return R::failure("'" + a.parameter + "' is derived and cannot be swept");
        if (a.values.empty())
            return R::failure("the sweep of '" + a.parameter + "' has no values");
        for (double v : a.values)
            if (!std::isfinite(v) || v < it->minimum || v > it->maximum)
                return R::failure("a value of '" + a.parameter + "' lies outside its range");
        total *= a.values.size();
    }
    if (total > 400)
        return R::failure("a sweep has at most 400 points (" + std::to_string(total) + " requested)");
    const Values baseline = valuesOf(base.value());
    auto baseOutcome = evaluateStructure(s, project, registry, model.id, nullptr);
    json points = json::array();
    std::vector<std::vector<Mode>> modes;
    std::vector<Values> pointMetrics, pointParameters;
    std::vector<bool> pointOk;
    std::vector<size_t> index(axes.size(), 0);
    for (size_t n = 0; n < total; ++n) {
        if (context && context->cancelRequested())
            return R::failure("cancelled");
        if (context)
            context->reportProgress(static_cast<double>(n) / static_cast<double>(total), "point " + std::to_string(n + 1) + " of " + std::to_string(total));
        Values values;
        json at = json::object();
        for (size_t a = 0; a < axes.size(); ++a) {
            values[axes[a].parameter] = axes[a].values[index[a]];
            at[axes[a].parameter] = axes[a].values[index[a]];
        }
        json point{{"index", n}, {"values", at}};
        auto built = withParameters(s, project, values);
        Result<AnalysisOutcome> outcome = built ? evaluateStructure(built.value().second, *built.value().first, registry, model.id, nullptr)
                                                : Result<AnalysisOutcome>::failure(built.message());
        if (outcome) {
            auto resolved = resolveParameters(built.value().second, *built.value().first);
            const Values all = resolved ? valuesOf(resolved.value()) : values;
            const auto objectives = evaluateObjectives(s, outcome.value().metrics);
            const auto constraints = evaluateConstraints(s, all, outcome.value().metrics);
            json metrics = json::object();
            for (const auto& [k, v] : outcome.value().metrics)
                metrics[k] = number(v);
            json pm = json::array();
            for (const Mode& m : outcome.value().modes)
                pm.push_back({{"label", m.label}, {"frequencyHz", m.frequencyHz}, {"quality", number(m.quality)}});
            point["status"] = "ok";
            point["metrics"] = metrics;
            point["modes"] = pm;
            point["objectives"] = objectivesJson(objectives);
            point["constraints"] = constraintsJson(constraints);
            point["feasible"] = feasible(constraints);
            point["warnings"] = outcome.value().warnings;
            modes.push_back(outcome.value().modes);
            pointMetrics.push_back(outcome.value().metrics);
            pointParameters.push_back(all);
            pointOk.push_back(feasible(constraints));
        } else {
            point["status"] = "failed";
            point["message"] = outcome.message();
            modes.emplace_back();
            pointMetrics.emplace_back();
            pointParameters.push_back(values);
            pointOk.push_back(false);
        }
        points.push_back(std::move(point));
        for (size_t a = axes.size(); a-- > 0;) {
            if (++index[a] < axes[a].values.size())
                break;
            index[a] = 0;
        }
    }
    // Rank feasible points by the weighted objectives: target and range deviations, and
    // minimize / maximize scaled over the swept points.
    json best = nullptr;
    if (!s.objectives.empty()) {
        double bestScore = std::numeric_limits<double>::infinity();
        size_t bestIndex = 0;
        for (size_t n = 0; n < total; ++n) {
            if (!pointOk[n])
                continue;
            double score = 0.0;
            bool complete = true;
            for (const ObjectiveResult& o : evaluateObjectives(s, pointMetrics[n])) {
                if (!o.available) {
                    complete = false;
                    break;
                }
                double term = o.deviation;
                if (o.sense == "minimize" || o.sense == "maximize") {
                    double lo = std::numeric_limits<double>::infinity(), hi = -lo;
                    for (size_t m = 0; m < total; ++m)
                        if (pointOk[m] && pointMetrics[m].count(o.metric)) {
                            lo = std::min(lo, pointMetrics[m].at(o.metric));
                            hi = std::max(hi, pointMetrics[m].at(o.metric));
                        }
                    const double span = hi > lo ? hi - lo : 1.0;
                    term = o.sense == "minimize" ? (o.value - lo) / span : (hi - o.value) / span;
                }
                score += o.weight * term;
            }
            if (complete && score < bestScore) {
                bestScore = score;
                bestIndex = n;
            }
        }
        if (std::isfinite(bestScore)) {
            best = {{"index", bestIndex}, {"score", bestScore}};
            if (baseOutcome)
                best["explanation"] = explainCandidate(s, project, kind, baseline, baseOutcome.value().metrics, pointParameters[bestIndex],
                                                       pointMetrics[bestIndex], nullptr);
        }
    }
    json d;
    d["format"] = kResultFormat;
    d["version"] = 1;
    d["kind"] = "sweep";
    d["structure"] = {{"id", s.id}, {"name", s.name}, {"document", s.toJson()}};
    d["analysis"] = {{"kind", kind.id}, {"title", kind.title}, {"model", modelJson(model)}};
    json ax = json::array();
    for (const SweepAxis& a : axes)
        ax.push_back({{"parameter", a.parameter}, {"values", a.values}});
    d["axes"] = ax;
    d["baseline"] = parametersJson(base.value());
    d["points"] = std::move(points);
    d["tracks"] = trackModes(modes);
    d["best"] = best;
    d["metricInfo"] = metricsJson(kind, {});
    for (const MetricInfo& m : kind.metrics)
        d["metricInfo"].push_back({{"key", m.key}, {"label", m.label}, {"unit", m.unit}});
    d["dependencies"] = dependenciesOf(s, project, discretizationSettings(s, model)).toJson();
    d["provenance"] = {{"application", std::string("Resonant ") + kApplicationVersion}, {"created", nowUtc()}, {"model", model.id},
                       {"modelVersion", model.version}, {"evaluations", total}};
    return d;
}

Result<json> sensitivityDocument(const FunctionalStructure& s, const Project& project, const AnalysisRegistry& registry,
                                 const std::vector<std::string>& requested, double relativeStep, const std::string& modelId, JobContext* context)
{
    using R = Result<json>;
    auto prepared = prepare(s, project, registry, modelId);
    if (!prepared)
        return R::failure(prepared.message());
    const AnalysisKind& kind = *prepared.value().kind;
    const ModelInfo& model = *prepared.value().model;
    if (!(relativeStep > 0.0 && relativeStep < 0.5))
        return R::failure("the relative step must lie in (0, 0.5)");
    auto base = resolveParameters(s, project);
    if (!base)
        return R::failure(base.message());
    auto center = evaluateStructure(s, project, registry, model.id, context);
    if (!center)
        return R::failure(center.message());
    std::vector<const ParameterValue*> chosen;
    for (const ParameterValue& p : base.value()) {
        const bool wanted = requested.empty() ? p.editable : std::find(requested.begin(), requested.end(), p.id) != requested.end();
        if (!wanted)
            continue;
        if (!p.editable)
            return R::failure("'" + p.id + "' is derived: its sensitivity follows from the parameters it depends on");
        chosen.push_back(&p);
    }
    for (const std::string& id : requested)
        if (std::none_of(base.value().begin(), base.value().end(), [&](const ParameterValue& p) { return p.id == id; }))
            return R::failure("the structure has no parameter '" + id + "'");
    json entries = json::array();
    size_t done = 0;
    for (const ParameterValue* p : chosen) {
        if (context && context->cancelRequested())
            return R::failure("cancelled");
        if (context)
            context->reportProgress(static_cast<double>(done++) / static_cast<double>(chosen.size()), p->name);
        const double scale = std::abs(p->value) > 0.0 ? std::abs(p->value)
                             : std::isfinite(p->maximum - p->minimum) ? p->maximum - p->minimum : 1.0;
        const double h = relativeStep * scale;
        double up = p->value + h, down = p->value - h;
        if (up > p->maximum)
            up = p->value;
        if (down < p->minimum)
            down = p->value;
        if (up == down)
            return R::failure("'" + p->id + "' has no room for a step inside its range");
        const auto at = [&](double v) -> Result<AnalysisOutcome> {
            if (v == p->value)
                return center.value();
            auto built = withParameters(s, project, {{p->id, v}});
            if (!built)
                return Result<AnalysisOutcome>::failure(built.message());
            return evaluateStructure(built.value().second, *built.value().first, registry, model.id, nullptr);
        };
        auto hi = at(up), lo = at(down);
        if (!hi || !lo)
            return R::failure("'" + p->id + "': " + (hi ? lo.message() : hi.message()));
        for (const auto& [key, value] : center.value().metrics) {
            if (!hi.value().metrics.count(key) || !lo.value().metrics.count(key) || !std::isfinite(value))
                continue;
            const double derivative = (hi.value().metrics.at(key) - lo.value().metrics.at(key)) / (up - down);
            const MetricInfo* m = kind.metric(key);
            entries.push_back({{"parameter", p->id},
                               {"metric", key},
                               {"derivative", number(derivative)},
                               {"normalized", value != 0.0 ? number(derivative * p->value / value) : json(nullptr)},
                               {"unit", (m ? m->unit : std::string()) + "/" + (p->unit.empty() ? "1" : p->unit)},
                               {"step", up - down},
                               {"scheme", up != p->value && down != p->value ? "central" : "one-sided"}});
        }
    }
    json d;
    d["format"] = kResultFormat;
    d["version"] = 1;
    d["kind"] = "sensitivity";
    d["structure"] = {{"id", s.id}, {"name", s.name}, {"document", s.toJson()}};
    d["analysis"] = {{"kind", kind.id}, {"title", kind.title}, {"model", modelJson(model)}};
    d["parameters"] = parametersJson(base.value());
    d["metrics"] = metricsJson(kind, center.value().metrics);
    d["entries"] = std::move(entries);
    d["relativeStep"] = relativeStep;
    d["dependencies"] = dependenciesOf(s, project, discretizationSettings(s, model)).toJson();
    d["provenance"] = {{"application", std::string("Resonant ") + kApplicationVersion}, {"created", nowUtc()}, {"model", model.id},
                       {"modelVersion", model.version}};
    return d;
}

std::vector<StaleReason> resultStaleness(const json& result, const Project& project, const AnalysisRegistry& registry)
{
    const std::string id = result.value("structure", json::object()).value("id", "");
    auto list = structuresOf(project);
    const FunctionalStructure* current = nullptr;
    if (list)
        for (const auto& s : list.value())
            if (s.id == id)
                current = &s;
    if (!current)
        return {{"definition", "the structure '" + id + "' is no longer in the project"}};
    const AnalysisKind* kind = registry.find(current->analysis.kind);
    const std::string modelId = result.value("analysis", json::object()).value("model", json::object()).value("id", "");
    const ModelInfo* model = kind ? kind->model(modelId) : nullptr;
    if (!model)
        return {{"definition", "the analysis or model of the result no longer exists"}};
    const DependencyRecord recorded = DependencyRecord::fromJson(result.value("dependencies", json::object()));
    return staleness(recorded, dependenciesOf(*current, project, discretizationSettings(*current, *model)), project);
}

json trackModes(const std::vector<std::vector<Mode>>& points, double tolerance)
{
    json tracks = json::array();
    if (points.empty())
        return tracks;
    struct Track {
        std::string label;
        std::vector<double> frequency;
        std::vector<int> ambiguousAt;
        double last = 0.0;
    };
    std::vector<Track> list;
    for (const Mode& m : points[0])
        list.push_back({m.label, {m.frequencyHz}, {}, m.frequencyHz});
    const auto labelled = [](const std::vector<Mode>& modes) {
        std::set<std::string> seen;
        for (const Mode& m : modes)
            if (m.label.empty() || !seen.insert(m.label).second)
                return false;
        return true;
    };
    json crossings = json::array();
    for (size_t p = 1; p < points.size(); ++p) {
        const std::vector<Mode>& modes = points[p];
        std::vector<bool> used(modes.size(), false);
        const bool byLabel = labelled(modes) && labelled(points[p - 1]);
        for (Track& t : list) {
            int chosen = -1;
            if (byLabel) {
                for (size_t i = 0; i < modes.size(); ++i)
                    if (modes[i].label == t.label)
                        chosen = static_cast<int>(i);
            } else {
                double bestDistance = std::numeric_limits<double>::infinity(), second = bestDistance;
                for (size_t i = 0; i < modes.size(); ++i) {
                    if (used[i])
                        continue;
                    const double distance = std::abs(modes[i].frequencyHz - t.last) / std::max(t.last, 1e-300);
                    if (distance < bestDistance) {
                        second = bestDistance;
                        bestDistance = distance;
                        chosen = static_cast<int>(i);
                    } else if (distance < second) {
                        second = distance;
                    }
                }
                if (chosen >= 0 && second - bestDistance < tolerance)
                    t.ambiguousAt.push_back(static_cast<int>(p));
            }
            if (chosen >= 0) {
                used[static_cast<size_t>(chosen)] = true;
                t.frequency.push_back(modes[static_cast<size_t>(chosen)].frequencyHz);
                t.last = modes[static_cast<size_t>(chosen)].frequencyHz;
            } else {
                t.frequency.push_back(std::numeric_limits<double>::quiet_NaN());
            }
        }
        // Labelled modes that exchange their order are reported (a crossing, not an ambiguity).
        if (byLabel)
            for (size_t a = 0; a < list.size(); ++a)
                for (size_t b = a + 1; b < list.size(); ++b) {
                    const double a0 = list[a].frequency[p - 1], a1 = list[a].frequency[p], b0 = list[b].frequency[p - 1], b1 = list[b].frequency[p];
                    if (std::isfinite(a0 + a1 + b0 + b1) && (a0 - b0) * (a1 - b1) < 0.0)
                        crossings.push_back({{"between", {p - 1, p}}, {"modes", {list[a].label, list[b].label}}});
                }
    }
    for (const Track& t : list) {
        json f = json::array();
        for (double v : t.frequency)
            f.push_back(number(v));
        tracks.push_back({{"label", t.label}, {"frequencyHz", f}, {"ambiguousAt", t.ambiguousAt}, {"matchedBy", labelled(points[0]) ? "label" : "continuity"}});
    }
    // Avoided crossings: two tracks whose gap has an interior minimum well below its ends (coupled modes
    // repelling instead of crossing); the minimum gap is the coupling splitting.
    json avoided = json::array();
    for (size_t a = 0; a < list.size(); ++a)
        for (size_t b = a + 1; b < list.size(); ++b) {
            std::vector<double> gap;
            for (size_t p = 0; p < points.size(); ++p)
                gap.push_back(std::abs(list[b].frequency[p] - list[a].frequency[p]));
            if (gap.size() < 3 || std::any_of(gap.begin(), gap.end(), [](double g) { return !std::isfinite(g); }))
                continue;
            const size_t k = static_cast<size_t>(std::min_element(gap.begin(), gap.end()) - gap.begin());
            if (k == 0 || k + 1 == gap.size())
                continue;
            if (gap.front() > 1.5 * gap[k] && gap.back() > 1.5 * gap[k])
                avoided.push_back({{"modes", {list[a].label, list[b].label}}, {"at", k}, {"minimumGapHz", gap[k]}});
        }
    return {{"tracks", tracks}, {"crossings", crossings}, {"avoidedCrossings", avoided}};
}

json explainCandidate(const FunctionalStructure& s, const Project& project, const AnalysisKind& kind, const Values& baseP, const Values& baseM,
                      const Values& candP, const Values& candM, const AnalysisOutcome* outcome)
{
    json changed = json::array();
    for (const auto& [id, value] : candP) {
        const auto it = baseP.find(id);
        if (it == baseP.end() || it->second == value)
            continue;
        const Parameter* p = s.parameter(id);
        changed.push_back({{"id", id}, {"name", p ? p->name : id}, {"from", it->second}, {"to", value}, {"unit", p ? p->unit : ""},
                           {"relativeChange", it->second != 0.0 ? json((value - it->second) / std::abs(it->second)) : json(nullptr)}});
    }
    json materials = json::array();
    for (const Region& r : s.regions) {
        const Material* m = project.materials.find(Id(r.materialId));
        materials.push_back({{"region", r.role}, {"material", m ? m->name() : r.materialId}, {"materialId", r.materialId}});
    }
    json improved = json::array(), worsened = json::array();
    const auto before = evaluateObjectives(s, baseM), after = evaluateObjectives(s, candM);
    for (size_t i = 0; i < before.size() && i < after.size(); ++i) {
        if (!before[i].available || !after[i].available)
            continue;
        const ObjectiveResult& b = before[i];
        const ObjectiveResult& a = after[i];
        bool better = false, worse = false;
        if (b.sense == "maximize") {
            better = a.value > b.value;
            worse = a.value < b.value;
        } else if (b.sense == "minimize") {
            better = a.value < b.value;
            worse = a.value > b.value;
        } else {
            better = a.deviation < b.deviation;
            worse = a.deviation > b.deviation;
        }
        const json entry{{"id", a.id}, {"metric", a.metric}, {"from", b.value}, {"to", a.value}, {"sense", a.sense}};
        if (better)
            improved.push_back(entry);
        if (worse)
            worsened.push_back(entry);
    }
    // Metrics that are no objective but changed by more than 5 % are listed as side effects.
    json side = json::array();
    for (const auto& [key, value] : candM) {
        const auto it = baseM.find(key);
        if (it == baseM.end() || !std::isfinite(value) || !std::isfinite(it->second) || it->second == 0.0)
            continue;
        if (std::any_of(s.objectives.begin(), s.objectives.end(), [&](const Objective& o) { return o.metric == key; }))
            continue;
        if (std::abs(value - it->second) > 0.05 * std::abs(it->second)) {
            const MetricInfo* m = kind.metric(key);
            side.push_back({{"metric", key}, {"label", m ? m->label : key}, {"from", it->second}, {"to", value}, {"unit", m ? m->unit : ""}});
        }
    }
    json active = json::array();
    for (const ConstraintResult& c : evaluateConstraints(s, candP, candM)) {
        if (!c.evaluated)
            continue;
        const double margin = std::abs(c.value - c.limit) / std::max(std::abs(c.limit), 1e-300);
        if (!c.satisfied || margin <= 0.05)
            active.push_back({{"id", c.id}, {"value", c.value}, {"limit", c.limit}, {"relation", c.relation}, {"satisfied", c.satisfied},
                              {"hard", c.hard}, {"process", c.process}, {"margin", margin}});
    }
    const ModelInfo* model = kind.model(s.analysis.model);
    json limits = json::array();
    if (model) {
        if (!model->validity.empty())
            limits.push_back(model->validity);
        for (const std::string& a : model->assumptions)
            limits.push_back(a);
    }
    if (outcome)
        for (const std::string& w : outcome->warnings)
            limits.push_back(w);
    return {{"parametersChanged", changed},
            {"materials", materials},
            {"operatingConditions", {{"frequencyRange", s.frequencyHigh > 0 ? json({s.frequencyLow, s.frequencyHigh}) : json(nullptr)},
                                     {"excitation", s.excitation},
                                     {"environment", s.environment}}},
            {"model", model ? modelJson(*model) : json(nullptr)},
            {"objectivesImproved", improved},
            {"objectivesWorsened", worsened},
            {"sideEffects", side},
            {"activeConstraints", active},
            {"mechanism", kind.mechanism},
            {"limitations", limits},
            {"verification", "every value was computed by the model named above for this candidate's own geometry; it is not an extrapolation"}};
}

}  // namespace resonant::functional
