#pragma once

// Sensitivity analysis (RES-OPT-008).
//
// Local sensitivities at a design are central finite differences of every
// objective quantity with respect to every free variable,
//
//   dq/dx ~ (q(x + h) - q(x - h)) / (2 h),   h = step x (upper - lower)
//
// reported with the one-sided differences and a reliability verdict: when
// the forward and backward differences disagree by more than half the
// central value (or the quantity is not available on both sides) the
// estimate is marked unreliable - a discontinuity, solver noise or a mesh
// change. They are approximations, never exact derivatives.
//
// The normalized sensitivity is dq/dx x (upper - lower) / objective scale: the
// change of the objective's score across the variable's whole range in a
// linear model. Variables whose normalized sensitivity is below 1 % for every
// objective are flagged as having negligible effect.
//
// From sampled candidates (a sweep or a Latin hypercube) the analysis gives
// Pearson and Spearman (rank) correlation between each variable and each
// objective value, and one-variable response curves.
#include "optimization/Engine.h"

#include <string>
#include <vector>

namespace resonant::optimization {

struct LocalSensitivity {
    std::string variable, objective, unit;  // unit of dq/dx: objective unit per variable unit
    double derivative = 0.0;
    double forward = 0.0, backward = 0.0;
    double normalized = 0.0;
    bool reliable = true;
    std::string note;
};

struct SensitivityReport {
    std::vector<LocalSensitivity> entries;           // variable-major
    std::vector<std::string> variables, objectives;
    std::vector<std::vector<double>> heatmap;        // [variable][objective], |normalized|
    std::vector<std::string> ranking;                // variables by their largest |normalized|, most influential first
    std::vector<std::string> negligible;
    std::vector<std::string> warnings;
    int evaluations = 0;
};

// Local finite-difference sensitivities at a design point (complete), each
// evaluation through the service (cached, recorded as candidates with the
// origin "sensitivity").
SensitivityReport localSensitivity(EvaluationService& service, const DesignPoint& at, double step = 0.01);

struct Correlation {
    std::string variable, objective;
    double pearson = 0.0, spearman = 0.0;
    size_t samples = 0;
};
// Correlations over evaluated candidates of one model.
std::vector<Correlation> correlations(const OptimizationProblem& problem, const std::vector<Candidate>& candidates, const std::string& modelId = {});

struct ResponseCurve {
    std::string variable, objective;
    std::vector<double> x, y;  // variable values (SI) and objective values
};
// One-at-a-time response curves through a design: `points` values of each
// free variable across its range, the others held.
std::vector<ResponseCurve> responseCurves(EvaluationService& service, const DesignPoint& at, int points = 9);

}  // namespace resonant::optimization
