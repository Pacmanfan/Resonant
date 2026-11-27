#pragma once

// Multi-fidelity screening and re-evaluation (RES-OPT-014).
//
// Candidates found with an inexpensive model are re-evaluated with a more
// complete one. A re-evaluation is a NEW candidate (parent: the screened
// one, origin "re-evaluation") with its own model record; values from
// different models are never merged or ranked together. The report states,
// per objective, both values and their difference, and how well the cheap
// model ranked the candidates (Spearman correlation of the two rankings).
//
// A design counts as validated only when it was evaluated with the
// problem's validation model, or with a model of high fidelity or above.
#include "optimization/Engine.h"

#include <string>
#include <vector>

namespace resonant::optimization {

struct FidelityComparisonRow {
    std::string screenedId, validatedId;
    std::vector<double> screened, validated, difference;  // per objective, objective values
    bool validatedFeasible = false;
};

struct MultiFidelityReport {
    std::string fromModel, toModel;
    Fidelity fromFidelity = Fidelity::Analytical, toFidelity = Fidelity::Analytical;
    std::vector<FidelityComparisonRow> rows;
    std::vector<Candidate> validated;   // the new candidates
    double rankAgreement = 0.0;         // Spearman correlation of the first objective's scores, NaN below 3 rows
    std::vector<std::string> notes;
};

// The best `count` feasible candidates of the problem's model (by weighted
// score; for several objectives the Pareto front first).
std::vector<Candidate> selectForValidation(const OptimizationProblem& problem, const std::vector<Candidate>& candidates, size_t count);

// Re-evaluates the candidates with `modelId` (outside the run's budget).
MultiFidelityReport reevaluate(EvaluationService& service, const std::vector<Candidate>& candidates, const std::string& modelId);

bool isValidated(const OptimizationProblem& problem, const Candidate& candidate);

}  // namespace resonant::optimization
