#pragma once

// Robustness of single designs (RES-OPT-016).
//
// Robust optimization itself happens in the evaluation service: a problem
// with robust settings evaluates every design nominally and at the common,
// Latin-hypercube stratified samples of its uncertain parameters, and scores
// it by the chosen robust measure (Engine.h). This file scores ONE design
// both ways, for comparing a nominal optimum with a robust one.
#include "optimization/Engine.h"

#include <memory>
#include <string>
#include <vector>

namespace resonant::optimization {

// Robustness of one design under the problem's uncertain parameters: the
// nominal evaluation and the statistics over the perturbed samples.
struct RobustnessReport {
    Candidate nominal;   // evaluated without perturbation
    Candidate robust;    // the same design scored over the samples
    std::vector<std::string> notes;
};
Result<RobustnessReport> assessRobustness(const OptimizationProblem& problem, std::shared_ptr<const DesignEvaluator> evaluator,
                                          const DesignPoint& point, std::shared_ptr<EvaluationCache> cache = {});

}  // namespace resonant::optimization
