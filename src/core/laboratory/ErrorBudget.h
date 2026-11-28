#pragma once

// Error budgets and uncertainty propagation (REQ-CHAT-20261126-023, RES-LAB-007), after
// the GUM (JCGM 100) and its Supplement 1 (JCGM 101, Monte Carlo):
//   u_c^2 = sum_i sum_j c_i c_j u_i u_j r_ij        (r_ij = 1 on the diagonal, stated otherwise, else 0)
//   nu_eff = u_c^4 / sum_i (c_i u_i)^4 / nu_i        (Welch-Satterthwaite; without correlations)
//   U = k u_c, k the Student t quantile for the coverage probability at nu_eff
// A component states its source - instrument, sensor, calibration, resolution,
// repeatability, reproducibility, sampling, timing, position, environment, manufacturing,
// material, numerical, model-form - and how its standard uncertainty is obtained from what
// is known: a normal value (with the coverage factor it was stated at), or the half-width of
// a rectangular, triangular or U-shaped distribution. Resolution (a rectangular half-step),
// accuracy (a specification), repeatability (a statistic of repeated runs) and
// reproducibility are different components and are never merged. Correlations are only
// what the user states, with a justification; otherwise the components are uncorrelated
// and the result says so.

#include "foundation/Result.h"

#include <Eigen/Dense>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace resonant::laboratory {

struct BudgetComponent {
    std::string id, source, description, unit, method;
    std::string distribution = "normal";  // normal, rectangular, triangular, u-shaped
    double value = 0.0;        // normal: the stated uncertainty (at `statedCoverage`); others: the half-width
    double statedCoverage = 1.0;  // the coverage factor a normal value was stated at (2 for an expanded U)
    double sensitivity = 1.0;  // c_i
    double degreesOfFreedom = 0.0;  // 0: infinite (a specification, a well-known value)
    double standard() const;
};

struct BudgetCorrelation {
    std::string a, b;
    double r = 0.0;
    std::string justification;
};

struct BudgetLine {
    std::string id, source;
    double standard = 0.0, contribution = 0.0;  // c u, and its share of u_c^2 (with its correlation terms)
};

struct BudgetResult {
    double value = 0.0, combined = 0.0, expanded = 0.0, coverageFactor = 2.0, effectiveDegreesOfFreedom = 0.0, coverageProbability = 0.95;
    std::string unit, correlation;
    std::vector<BudgetLine> lines;
    std::vector<std::string> notes;
    nlohmann::json toJson() const;
};

struct ErrorBudget {
    std::string quantity, unit;
    double value = 0.0;
    double coverageProbability = 0.95;
    std::vector<BudgetComponent> components;
    std::vector<BudgetCorrelation> correlations;
    Result<BudgetResult> evaluate() const;
    nlohmann::json toJson() const;
    static Result<ErrorBudget> fromJson(const nlohmann::json& j);
};

// The Student t quantile t_{(1+p)/2, nu} (nu <= 0: the normal quantile).
double coverageFactor(double degreesOfFreedom, double probability = 0.95);

struct InputQuantity {
    std::string id;
    double mean = 0.0, standard = 0.0;
    std::string distribution = "normal";  // normal, rectangular (mean +- sqrt(3) standard)
};

struct Propagation {
    std::string method;
    double mean = 0.0, standard = 0.0, lower = 0.0, upper = 0.0, probability = 0.95;
    int samples = 0;
    std::vector<double> sensitivities;  // first order: df/dx_i
    std::vector<std::string> notes;
    nlohmann::json toJson() const;
};

using Model = std::function<Result<double>(const std::vector<double>&)>;
// First order (the law of propagation of uncertainty) with central-difference sensitivities.
Result<Propagation> propagateLinear(const Model& f, const std::vector<InputQuantity>& inputs, const Eigen::MatrixXd& correlation = {});
// Monte Carlo (JCGM 101) with correlated inputs (a Gaussian copula); deterministic for a seed.
Result<Propagation> propagateMonteCarlo(const Model& f, const std::vector<InputQuantity>& inputs, const Eigen::MatrixXd& correlation = {},
                                        int samples = 20000, uint64_t seed = 1);

}  // namespace resonant::laboratory
