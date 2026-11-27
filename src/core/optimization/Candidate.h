#pragma once

// Candidate designs and their comparison (RES-OPT-005, RES-OPT-019).
//
// Every evaluated design is kept as a Candidate: its id, its parents (the
// lineage), the algorithm step that proposed it, its complete design point,
// the evaluation (quantities, the exact model record, notes), the objective
// values and scores, every constraint result with the amount it misses by,
// its feasibility, and - for robust optimization - the statistics over the
// perturbed samples. Candidates are never modified after they are stored.
#include "optimization/Design.h"
#include "optimization/Evaluator.h"
#include "optimization/Objectives.h"

#include <nlohmann/json.hpp>

#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace resonant::optimization {

struct OptimizationProblem;

struct RobustStatistics {
    int samples = 0;
    std::vector<double> mean, stddev, minimum, maximum;  // per objective, of the objective value
    std::vector<double> worstScore;                      // per objective, the worst (largest) score
    std::vector<double> constraintProbability;           // per constraint: fraction of samples meeting it
    std::string measure;                                 // what the candidate's objective values are
};

struct Candidate {
    std::string id;
    std::vector<std::string> parents;
    int generation = 0;
    std::string origin;            // "initial", "sweep", "nelder-mead", "re-evaluation", ...
    DesignPoint point;
    Evaluation evaluation;         // nominal evaluation (robust: the unperturbed design)
    std::vector<double> objectiveValues;  // per problem objective (robust: the robust measure)
    std::vector<double> scores;           // per objective, to minimize
    std::vector<ConstraintResult> constraints;
    bool feasible = false;
    double hardViolation = 0.0;    // sum of hard violations / scale
    double softPenalty = 0.0;
    double weightedScore = std::numeric_limits<double>::infinity();  // sum weight * score + soft penalty
    std::optional<RobustStatistics> robust;
    bool fromCache = false;        // the evaluation was reused (identical dependency state)
    bool replayed = false;         // restored from a checkpoint
    int paretoRank = -1;           // 0 = non-dominated among feasible candidates; -1 = not ranked
    double seconds = 0.0;

    bool evaluated() const { return evaluation.ok(); }
    Fidelity fidelity() const { return evaluation.model.fidelity; }
    // Hard constraints that failed, as "label: value vs limit (misses by x unit)".
    std::vector<std::string> failedConstraints(const OptimizationProblem& problem) const;
};

nlohmann::json toJson(const Candidate& candidate);
Candidate candidateFromJson(const nlohmann::json& value);

// Constraint domination (Deb): a feasible candidate beats an infeasible one;
// two infeasible ones compare by hard violation; two feasible ones by score.
bool betterScalar(const Candidate& a, const Candidate& b);
// Pareto dominance on the scores (all <=, one <), after feasibility.
bool dominates(const Candidate& a, const Candidate& b);
// Non-dominated sorting: rank per candidate (0 = first front); candidates
// not evaluated get -1.
std::vector<int> paretoRanks(const std::vector<Candidate>& candidates, bool feasibleOnly = true);
// Indices of the first front (feasible, evaluated, by the given model only
// when modelId is not empty: candidates of different models are not ranked
// together).
std::vector<size_t> paretoFront(const std::vector<Candidate>& candidates, const std::string& modelId = {});
// Indices ordered by a weighted sum of scores chosen AFTER the run (user
// priorities); feasible first.
std::vector<size_t> rankByPriorities(const std::vector<Candidate>& candidates, const std::vector<double>& weights);

// A side-by-side comparison of two candidates.
struct DesignComparison {
    struct Row {
        std::string what, unit;
        double a = 0.0, b = 0.0, difference = 0.0;
        bool differs = false;
    };
    std::vector<Row> variables, objectives, constraints;
    bool sameModel = true;
    std::vector<std::string> notes;  // model differences and what they mean
};
DesignComparison compare(const OptimizationProblem& problem, const Candidate& a, const Candidate& b);

}  // namespace resonant::optimization
