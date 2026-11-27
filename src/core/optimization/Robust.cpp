#include "optimization/Robust.h"

#include <algorithm>
#include <thread>

namespace resonant::optimization {

Result<RobustnessReport> assessRobustness(const OptimizationProblem& problem, std::shared_ptr<const DesignEvaluator> evaluator,
                                          const DesignPoint& point, std::shared_ptr<EvaluationCache> cache)
{
    using R = Result<RobustnessReport>;
    if (!problem.robust.enabled)
        return R::failure("the problem defines no uncertain parameters");
    if (Status s = problem.validateAgainst(evaluator->capabilities()); !s)
        return R::failure(s.message());
    OptimizationProblem nominalProblem = problem;
    nominalProblem.robust.enabled = false;
    nominalProblem.fillUnits(evaluator->capabilities());
    OptimizationProblem robustProblem = problem;
    robustProblem.fillUnits(evaluator->capabilities());
    ServiceOptions options;
    options.runId = "ROBUST";
    options.cache = cache ? cache : std::make_shared<EvaluationCache>();
    options.parallel = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);
    EvaluationService nominal(nominalProblem, evaluator, options);
    EvaluationService robust(robustProblem, evaluator, options);
    auto a = nominal.evaluatePoints({point}, "nominal", 0, {}, {}, true);
    auto b = robust.evaluatePoints({point}, "robust assessment", 0, {}, {}, true);
    if (a.empty() || b.empty())
        return R::failure("the design could not be evaluated");
    RobustnessReport report;
    report.nominal = a.front();
    report.robust = b.front();
    report.notes.push_back("robust statistics over " + std::to_string(problem.robust.samples) +
                           " perturbed samples (Latin-hypercube stratified, the same for every design)");
    for (const UncertainParameter& u : problem.robust.parameters)
        report.notes.push_back(u.label.empty() ? u.binding : u.label + ": " + std::string(toString(u.distribution)) +
                                                                (u.distribution == Distribution::Uniform ? " +/- " : " sigma ") +
                                                                std::to_string(u.width) + (u.relative ? " (relative)" : "") + " - source: " + u.source);
    return report;
}

}  // namespace resonant::optimization
