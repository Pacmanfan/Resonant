#pragma once

// Surrogate models and response surfaces (RES-OPT-015).
//
// A surrogate approximates a quantity from evaluated designs so that fewer
// expensive simulations are needed. Its prediction is NEVER a simulation
// result: it is reported as a prediction with the model that made it, its
// training data and errors, and it never becomes a candidate's evaluation.
//
//   quadratic          least-squares polynomial of degree 2 (cross terms
//                      when there are enough points), no uncertainty
//   rbf                cubic radial-basis interpolation with a linear tail,
//                      no uncertainty
//   gaussian-process   zero-mean-after-centring process with a squared-
//                      exponential kernel, an isotropic length scale and a
//                      noise level chosen by the largest marginal likelihood
//                      on a grid; it predicts a mean and a standard deviation
//
// Inputs are points of the unit cube of the free design variables. The
// validation error is the leave-one-out root-mean-square error.
#include "foundation/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace resonant::optimization {

struct Candidate;
struct OptimizationProblem;

struct SurrogateInfo {
    std::string kind;
    std::string version = "1";
    size_t trainingPoints = 0;
    std::vector<std::string> inputs;   // variable ids
    std::string output;                // what is predicted
    double trainingRmse = 0.0;
    double validationRmse = 0.0;       // leave-one-out
    std::vector<std::string> notes;
};

struct Prediction {
    double mean = 0.0;
    double stddev = 0.0;
    bool hasUncertainty = false;
};

class SurrogateModel {
public:
    virtual ~SurrogateModel() = default;
    virtual std::string kind() const = 0;
    virtual Status fit(const std::vector<std::vector<double>>& x, const std::vector<double>& y) = 0;
    virtual Prediction predict(const std::vector<double>& x) const = 0;
};

std::unique_ptr<SurrogateModel> makeSurrogate(const std::string& kind);  // null for an unknown kind
std::vector<std::string> surrogateKinds();

// Expected improvement over `best` (minimization) for a prediction with
// uncertainty: E[max(0, best - Y)], Y ~ N(mean, stddev^2).
double expectedImprovement(const Prediction& prediction, double best);

struct SurrogateStudy {
    SurrogateInfo info;
    std::unique_ptr<SurrogateModel> model;
    std::vector<std::vector<double>> trainingInputs;
    std::vector<double> trainingOutputs;
};

// Fits a surrogate of one objective's VALUE (objectiveIndex) over the
// evaluated candidates of one model (feasible or not; failed ones are left
// out) and computes its training and leave-one-out errors.
Result<SurrogateStudy> fitSurrogate(const OptimizationProblem& problem, const std::vector<Candidate>& candidates, size_t objectiveIndex,
                                    const std::string& kind, const std::string& modelId = {});
// Active learning: of the pool (unit-cube points), the indices of the k
// points the surrogate is least certain about (largest predicted standard
// deviation), or - for a surrogate without uncertainty - the points
// farthest from every training point.
std::vector<size_t> mostInformative(const SurrogateStudy& study, const std::vector<std::vector<double>>& pool, size_t k);

}  // namespace resonant::optimization
