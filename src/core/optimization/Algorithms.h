#pragma once

// Optimization methods (RES-OPT-007, RES-OPT-008, RES-OPT-017).
//
// Every method works through an EvaluationService in the unit cube of the
// free design variables, so none depends on a physics domain or on a
// third-party library. Implemented, chosen for expensive engineering
// simulations:
//
//   sweep                  full-factorial grid (one- and multi-variable sweeps)
//   random                 uniform random sampling
//   latin-hypercube        stratified sampling (design of experiments)
//   nelder-mead            derivative-free simplex (adaptive coefficients), local
//   lbfgs                  bound-constrained limited-memory BFGS with central
//                          finite-difference gradients, local; the gradients are
//                          approximations and are checked for reliability
//   differential-evolution population-based, global, handles discrete variables
//                          by quantization, constraint domination
//   nsga2                  Pareto (multi-objective) genetic algorithm
//   bayesian               Gaussian-process surrogate with expected improvement,
//                          for very expensive evaluations with few variables
//
// Not implemented (documented in docs/optimization/OPTIMIZATION_ALGORITHMS.md):
// SQP, CMA-ES, Powell, adjoint gradients.
#include "optimization/Engine.h"

#include <string>
#include <vector>

namespace resonant::optimization {

struct AlgorithmInfo {
    std::string id;
    std::string name;
    std::string family;       // "sampling", "derivative-free", "gradient", "population", "surrogate"
    bool multiObjective = false;
    bool handlesDiscrete = true;
    bool finiteDifferenceGradients = false;
    bool global = false;      // explores the whole space (not a local method)
    std::string description;
    std::string convergence;  // the stopping rule
};

class Algorithm {
public:
    virtual ~Algorithm() = default;
    virtual AlgorithmInfo info() const = 0;
    virtual Termination run(EvaluationService& service) const = 0;
};

const std::vector<const Algorithm*>& algorithms();
const Algorithm* findAlgorithm(std::string_view id);

struct ProblemTraits {
    size_t variables = 0;
    bool discrete = false;
    size_t objectives = 1;
    size_t constraints = 0;
    double secondsPerEvaluation = 0.0;
};
ProblemTraits traitsOf(const OptimizationProblem& problem, const Capabilities& capabilities);

struct Recommendation {
    std::string algorithm;
    std::string reason;
};
// Methods suited to the problem, best first, each with the reason.
std::vector<Recommendation> recommendAlgorithms(const ProblemTraits& traits);

}  // namespace resonant::optimization
