#pragma once

// Estimation of a model's physical parameters from observations (REQ-CHAT-20261126-023,
// RES-LAB-008, -009): weighted nonlinear least squares by Levenberg-Marquardt, with bounds,
// optional Gaussian priors (then a maximum a posteriori estimate), and several observations
// (datasets) fitted jointly. Residuals are (observed - predicted) / standard uncertainty;
// without stated uncertainties the fit is unweighted and the covariance is scaled by the
// residual variance (and says so).
//
// Identifiability: the parameters are scaled by their initial values, so the Jacobian's
// singular values and vectors compare like with like. A parameter is
//   identified           relative standard error <= 10 % (a strong correlation is reported in the notes)
//   poorly identified    relative standard error <= 100 %, or a correlation above 0.99 with more than 1 %
//   not identifiable     it loads the weakest direction of a near-singular Jacobian
//                        (singular value ratio < 1e-6), or its relative error exceeds 100 %
// and the combinations the data do determine are named.

#include "foundation/Result.h"

#include <Eigen/Dense>
#include <nlohmann/json.hpp>

#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace resonant {
class JobContext;
}

namespace resonant::laboratory {

struct EstimatedParameter {
    std::string id, label, unit, meaning;
    double initial = 0.0;
    double lower = -std::numeric_limits<double>::infinity(), upper = std::numeric_limits<double>::infinity();
    std::optional<double> priorMean, priorStandard;
};

struct Observation {
    std::string dataset, label, unit;
    std::vector<double> axis, values;
    std::vector<double> standard;  // per value; empty: not stated (unweighted)
};

// The model's prediction of observation `index` for the parameter values (in order).
using Predictor = std::function<Result<std::vector<double>>(const std::vector<double>& parameters, size_t index)>;

struct EstimationOptions {
    int maxIterations = 100;
    double tolerance = 1e-10;       // relative change of the cost
    double relativeStep = 1e-6;     // finite differences (of the scaled parameters)
    double coverage = 0.95;
};

struct ParameterEstimate {
    std::string id, unit;
    double initial = 0.0, estimate = 0.0, standardError = 0.0, lower = 0.0, upper = 0.0, relativeError = 0.0;
    std::string identifiability;
    bool atBound = false;
};

struct EstimationResult {
    std::string method;
    bool weighted = true, converged = false;
    int iterations = 0, evaluations = 0, degreesOfFreedom = 0;
    double chiSquare = 0.0, reducedChiSquare = 0.0, conditionNumber = 0.0;
    std::vector<ParameterEstimate> parameters;
    Eigen::MatrixXd covariance, correlation;
    std::vector<double> singularValues;
    std::vector<std::vector<double>> fitted, residuals;  // per observation (residuals normalized)
    std::vector<std::string> notes;
    bool identified() const;
    nlohmann::json toJson() const;
};

Result<EstimationResult> estimateParameters(const std::vector<EstimatedParameter>& parameters, const std::vector<Observation>& observations,
                                            const Predictor& predict, const EstimationOptions& options = {}, JobContext* context = nullptr);

}  // namespace resonant::laboratory
