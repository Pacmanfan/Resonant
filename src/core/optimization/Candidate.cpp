#include "optimization/Candidate.h"

#include "optimization/Problem.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <sstream>

namespace resonant::optimization {

namespace {

nlohmann::json number(double v) { return std::isfinite(v) ? nlohmann::json(v) : nlohmann::json(nullptr); }
double numberOr(const nlohmann::json& j, double fallback) { return j.is_number() ? j.get<double>() : fallback; }

std::vector<double> numbers(const nlohmann::json& j)
{
    std::vector<double> out;
    if (j.is_array())
        for (const auto& v : j)
            out.push_back(numberOr(v, std::numeric_limits<double>::quiet_NaN()));
    return out;
}

nlohmann::json array(const std::vector<double>& values)
{
    nlohmann::json out = nlohmann::json::array();
    for (double v : values)
        out.push_back(number(v));
    return out;
}

std::string format(double v)
{
    std::ostringstream s;
    s.precision(6);
    s << v;
    return s.str();
}

}  // namespace

std::vector<std::string> Candidate::failedConstraints(const OptimizationProblem& problem) const
{
    std::vector<std::string> out;
    for (const ConstraintResult& r : constraints) {
        if (r.satisfied)
            continue;
        std::string label = r.id, unit;
        for (const DesignConstraint& c : problem.constraints)
            if (c.id == r.id) {
                label = c.label.empty() ? c.id : c.label;
                unit = c.unit;
            }
        std::string line = label + (r.hard ? " (hard)" : " (soft)") + ": ";
        if (!r.evaluated)
            line += "not evaluated - " + r.message;
        else
            line += format(r.value) + " vs limit " + format(r.limit) + ", misses by " + format(r.violation) + (unit.empty() ? "" : " " + unit);
        out.push_back(line);
    }
    return out;
}

nlohmann::json toJson(const Candidate& c)
{
    nlohmann::json constraints = nlohmann::json::array();
    for (const ConstraintResult& r : c.constraints)
        constraints.push_back(toJson(r));
    nlohmann::json j{{"id", c.id},
                     {"parents", c.parents},
                     {"generation", c.generation},
                     {"origin", c.origin},
                     {"point", toJson(c.point)},
                     {"evaluation", toJson(c.evaluation)},
                     {"objectiveValues", array(c.objectiveValues)},
                     {"scores", array(c.scores)},
                     {"constraints", constraints},
                     {"feasible", c.feasible},
                     {"hardViolation", number(c.hardViolation)},
                     {"softPenalty", number(c.softPenalty)},
                     {"weightedScore", number(c.weightedScore)},
                     {"fromCache", c.fromCache},
                     {"replayed", c.replayed},
                     {"paretoRank", c.paretoRank},
                     {"seconds", c.seconds}};
    if (c.robust) {
        const RobustStatistics& r = *c.robust;
        j["robust"] = {{"samples", r.samples},       {"mean", array(r.mean)},       {"stddev", array(r.stddev)},
                       {"minimum", array(r.minimum)}, {"maximum", array(r.maximum)}, {"worstScore", array(r.worstScore)},
                       {"constraintProbability", array(r.constraintProbability)}, {"measure", r.measure}};
    }
    return j;
}

Candidate candidateFromJson(const nlohmann::json& j)
{
    Candidate c;
    c.id = j.value("id", std::string());
    c.parents = j.value("parents", std::vector<std::string>());
    c.generation = j.value("generation", 0);
    c.origin = j.value("origin", std::string());
    if (auto p = designPointFromJson(j.value("point", nlohmann::json::object())))
        c.point = p.take();
    c.evaluation = evaluationFromJson(j.value("evaluation", nlohmann::json::object()));
    c.objectiveValues = numbers(j.value("objectiveValues", nlohmann::json::array()));
    c.scores = numbers(j.value("scores", nlohmann::json::array()));
    for (const auto& r : j.value("constraints", nlohmann::json::array()))
        c.constraints.push_back(constraintResultFromJson(r));
    c.feasible = j.value("feasible", false);
    c.hardViolation = numberOr(j.value("hardViolation", nlohmann::json()), std::numeric_limits<double>::infinity());
    c.softPenalty = numberOr(j.value("softPenalty", nlohmann::json()), 0.0);
    c.weightedScore = numberOr(j.value("weightedScore", nlohmann::json()), std::numeric_limits<double>::infinity());
    c.fromCache = j.value("fromCache", false);
    c.replayed = j.value("replayed", false);
    c.paretoRank = j.value("paretoRank", -1);
    c.seconds = j.value("seconds", 0.0);
    if (j.contains("robust")) {
        const auto& r = j["robust"];
        RobustStatistics s;
        s.samples = r.value("samples", 0);
        s.mean = numbers(r.value("mean", nlohmann::json::array()));
        s.stddev = numbers(r.value("stddev", nlohmann::json::array()));
        s.minimum = numbers(r.value("minimum", nlohmann::json::array()));
        s.maximum = numbers(r.value("maximum", nlohmann::json::array()));
        s.worstScore = numbers(r.value("worstScore", nlohmann::json::array()));
        s.constraintProbability = numbers(r.value("constraintProbability", nlohmann::json::array()));
        s.measure = r.value("measure", std::string());
        c.robust = std::move(s);
    }
    return c;
}

bool betterScalar(const Candidate& a, const Candidate& b)
{
    if (a.evaluated() != b.evaluated())
        return a.evaluated();
    if (!a.evaluated())
        return false;
    if (a.feasible != b.feasible)
        return a.feasible;
    if (!a.feasible)
        return a.hardViolation < b.hardViolation;
    return a.weightedScore < b.weightedScore;
}

bool dominates(const Candidate& a, const Candidate& b)
{
    if (a.evaluated() != b.evaluated())
        return a.evaluated();
    if (!a.evaluated())
        return false;
    if (a.feasible != b.feasible)
        return a.feasible;
    if (!a.feasible)
        return a.hardViolation < b.hardViolation;
    bool strictly = false;
    for (size_t i = 0; i < a.scores.size() && i < b.scores.size(); ++i) {
        const double x = a.scores[i] + 0.0, y = b.scores[i] + 0.0;
        if (!(x <= y))
            return false;
        if (x < y)
            strictly = true;
    }
    return strictly;
}

std::vector<int> paretoRanks(const std::vector<Candidate>& candidates, bool feasibleOnly)
{
    const size_t n = candidates.size();
    std::vector<int> rank(n, -1);
    std::vector<size_t> pool;
    for (size_t i = 0; i < n; ++i)
        if (candidates[i].evaluated() && (!feasibleOnly || candidates[i].feasible))
            pool.push_back(i);
    std::vector<int> dominatedBy(n, 0);
    std::vector<std::vector<size_t>> dominatesList(n);
    for (size_t a : pool)
        for (size_t b : pool) {
            if (a == b)
                continue;
            if (dominates(candidates[a], candidates[b]))
                dominatesList[a].push_back(b);
            else if (dominates(candidates[b], candidates[a]))
                ++dominatedBy[a];
        }
    std::vector<size_t> front;
    for (size_t a : pool)
        if (dominatedBy[a] == 0)
            front.push_back(a);
    int level = 0;
    while (!front.empty()) {
        std::vector<size_t> next;
        for (size_t a : front) {
            rank[a] = level;
            for (size_t b : dominatesList[a])
                if (--dominatedBy[b] == 0)
                    next.push_back(b);
        }
        front = std::move(next);
        ++level;
    }
    return rank;
}

std::vector<size_t> paretoFront(const std::vector<Candidate>& candidates, const std::string& modelId)
{
    std::vector<Candidate> subset;
    std::vector<size_t> map;
    for (size_t i = 0; i < candidates.size(); ++i)
        if (modelId.empty() || candidates[i].evaluation.model.modelId == modelId) {
            subset.push_back(candidates[i]);
            map.push_back(i);
        }
    const std::vector<int> rank = paretoRanks(subset, true);
    std::vector<size_t> out;
    // One entry per distinct design (a duplicate point is not a separate trade-off).
    std::set<std::string> seen;
    for (size_t i = 0; i < subset.size(); ++i)
        if (rank[i] == 0 && seen.insert(DesignSpace::canonical(subset[i].point)).second)
            out.push_back(map[i]);
    return out;
}

std::vector<size_t> rankByPriorities(const std::vector<Candidate>& candidates, const std::vector<double>& weights)
{
    std::vector<size_t> order(candidates.size());
    std::iota(order.begin(), order.end(), size_t(0));
    const auto value = [&](const Candidate& c) {
        double s = c.softPenalty;
        for (size_t i = 0; i < c.scores.size(); ++i)
            s += (i < weights.size() ? weights[i] : 1.0) * c.scores[i];
        return std::isfinite(s) ? s : std::numeric_limits<double>::infinity();
    };
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const Candidate& x = candidates[a];
        const Candidate& y = candidates[b];
        if (x.evaluated() != y.evaluated())
            return x.evaluated();
        if (x.feasible != y.feasible)
            return x.feasible;
        if (!x.feasible)
            return x.hardViolation < y.hardViolation;
        return value(x) < value(y);
    });
    return order;
}

DesignComparison compare(const OptimizationProblem& problem, const Candidate& a, const Candidate& b)
{
    DesignComparison out;
    const auto row = [](std::string what, std::string unit, double x, double y) {
        DesignComparison::Row r;
        r.what = std::move(what);
        r.unit = std::move(unit);
        r.a = x;
        r.b = y;
        r.difference = y - x;
        r.differs = std::abs(y - x) > 1e-12 * std::max({1.0, std::abs(x), std::abs(y)}) || std::isnan(x) != std::isnan(y);
        return r;
    };
    for (const DesignVariable& v : problem.space.variables) {
        if (!v.active)
            continue;
        const auto ia = a.point.values.find(v.id), ib = b.point.values.find(v.id);
        out.variables.push_back(row(v.label.empty() ? v.id : v.label, v.unit, ia == a.point.values.end() ? NAN : ia->second,
                                    ib == b.point.values.end() ? NAN : ib->second));
    }
    for (size_t i = 0; i < problem.objectives.size(); ++i) {
        const Objective& o = problem.objectives[i];
        out.objectives.push_back(row(o.label.empty() ? o.id : o.label, o.unit, i < a.objectiveValues.size() ? a.objectiveValues[i] : NAN,
                                     i < b.objectiveValues.size() ? b.objectiveValues[i] : NAN));
    }
    for (size_t i = 0; i < problem.constraints.size(); ++i) {
        const DesignConstraint& c = problem.constraints[i];
        out.constraints.push_back(row(c.label.empty() ? c.id : c.label, c.unit, i < a.constraints.size() ? a.constraints[i].value : NAN,
                                      i < b.constraints.size() ? b.constraints[i].value : NAN));
    }
    const ModelRecord& ma = a.evaluation.model;
    const ModelRecord& mb = b.evaluation.model;
    out.sameModel = ma.modelId == mb.modelId && ma.version == mb.version && ma.snapshotKey == mb.snapshotKey;
    if (ma.modelId != mb.modelId)
        out.notes.push_back("evaluated with different models: '" + ma.modelName + "' (" + std::string(toString(ma.fidelity)) + ") and '" +
                            mb.modelName + "' (" + std::string(toString(mb.fidelity)) +
                            "); the numbers are not directly comparable - each model makes different assumptions");
    else if (ma.version != mb.version)
        out.notes.push_back("evaluated with different versions of '" + ma.modelName + "'");
    if (ma.snapshotKey != mb.snapshotKey)
        out.notes.push_back("evaluated on different design snapshots (the project changed between them)");
    if (a.robust.has_value() != b.robust.has_value())
        out.notes.push_back("one candidate was evaluated robustly (statistics over perturbed samples), the other nominally");
    return out;
}

}  // namespace resonant::optimization
