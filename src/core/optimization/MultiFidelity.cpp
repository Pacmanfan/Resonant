#include "optimization/MultiFidelity.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>

namespace resonant::optimization {

namespace {
std::vector<double> ranksOf(const std::vector<double>& v)
{
    std::vector<size_t> order(v.size());
    std::iota(order.begin(), order.end(), size_t(0));
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return v[a] < v[b]; });
    std::vector<double> r(v.size());
    for (size_t i = 0; i < order.size(); ++i)
        r[order[i]] = static_cast<double>(i);
    return r;
}
}  // namespace

std::vector<Candidate> selectForValidation(const OptimizationProblem& problem, const std::vector<Candidate>& candidates, size_t count)
{
    std::vector<Candidate> pool;
    for (const Candidate& c : candidates)
        if (c.feasible && c.evaluated() && c.evaluation.model.modelId == problem.modelId)
            pool.push_back(c);
    std::vector<Candidate> out;
    std::set<std::string> seen;
    const auto take = [&](const Candidate& c) {
        if (out.size() < count && seen.insert(DesignSpace::canonical(c.point)).second)
            out.push_back(c);
    };
    if (problem.objectives.size() > 1)
        for (size_t i : paretoFront(pool))
            take(pool[i]);
    std::stable_sort(pool.begin(), pool.end(), [](const Candidate& a, const Candidate& b) { return a.weightedScore < b.weightedScore; });
    for (const Candidate& c : pool)
        take(c);
    return out;
}

MultiFidelityReport reevaluate(EvaluationService& service, const std::vector<Candidate>& candidates, const std::string& modelId)
{
    MultiFidelityReport report;
    const Capabilities& caps = service.evaluator().capabilities();
    report.toModel = modelId;
    if (const ModelInfo* m = caps.model(modelId))
        report.toFidelity = m->fidelity;
    if (!candidates.empty()) {
        report.fromModel = candidates.front().evaluation.model.modelId;
        report.fromFidelity = candidates.front().evaluation.model.fidelity;
    }
    std::vector<DesignPoint> points;
    std::vector<std::vector<std::string>> parents;
    for (const Candidate& c : candidates) {
        points.push_back(c.point);
        parents.push_back({c.id});
    }
    report.validated = service.evaluatePoints(points, "re-evaluation with " + modelId, 0, parents, modelId, true);
    std::vector<double> lo, hi;
    for (size_t i = 0; i < report.validated.size(); ++i) {
        const Candidate& a = candidates[i];
        const Candidate& b = report.validated[i];
        FidelityComparisonRow row;
        row.screenedId = a.id;
        row.validatedId = b.id;
        row.screened = a.objectiveValues;
        row.validated = b.objectiveValues;
        for (size_t k = 0; k < a.objectiveValues.size() && k < b.objectiveValues.size(); ++k)
            row.difference.push_back(b.objectiveValues[k] - a.objectiveValues[k]);
        row.validatedFeasible = b.feasible;
        if (!b.evaluated())
            report.notes.push_back(a.id + ": the higher-fidelity model could not evaluate it: " + b.evaluation.message);
        else if (!a.scores.empty() && !b.scores.empty()) {
            lo.push_back(a.scores[0]);
            hi.push_back(b.scores[0]);
        }
        if (a.feasible && b.evaluated() && !b.feasible)
            report.notes.push_back(a.id + " was feasible with the screening model but is not with '" + modelId + "'");
        report.rows.push_back(std::move(row));
    }
    report.rankAgreement = NAN;
    if (lo.size() >= 3) {
        const auto r1 = ranksOf(lo), r2 = ranksOf(hi);
        const double n = static_cast<double>(r1.size());
        double d2 = 0.0;
        for (size_t i = 0; i < r1.size(); ++i)
            d2 += (r1[i] - r2[i]) * (r1[i] - r2[i]);
        report.rankAgreement = 1.0 - 6.0 * d2 / (n * (n * n - 1.0));
    }
    if (static_cast<int>(report.toFidelity) <= static_cast<int>(report.fromFidelity))
        report.notes.push_back("the re-evaluation model is not of a higher fidelity than the screening model");
    report.notes.push_back("values of the two models are shown side by side and never combined; the models make different assumptions");
    return report;
}

bool isValidated(const OptimizationProblem& problem, const Candidate& c)
{
    if (!c.evaluated() || !c.feasible)
        return false;
    if (!problem.validationModelId.empty() && c.evaluation.model.modelId == problem.validationModelId)
        return true;
    return static_cast<int>(c.evaluation.model.fidelity) >= static_cast<int>(Fidelity::HighFidelity);
}

}  // namespace resonant::optimization
