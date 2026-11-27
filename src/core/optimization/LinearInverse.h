#pragma once

// Inverse design of source configurations by linear least squares
// (RES-OPT-009), independent of the physics that produced the response.
//
// Given the complex response A (targets x sources), the wanted values b and
// row weights q, find source weights w with
//
//   minimize  sum_m q_m |(A w - b)_m|^2 + (lambda + mu) sigma_max^2 ||w||^2
//   subject to |w_i| <= W            (weight bounds, when given)
//              w_i = 0, i not in S    (sparse excitation, when asked)
//
// lambda (regularization) and mu (power penalty) are relative to the
// largest singular value squared of the weighted matrix, so they are
// dimensionless and comparable between problems; ||w||^2 is proportional to
// the drive power of identical sources.
//
//   Regularized    Tikhonov solution by the singular value decomposition
//   MinimumNorm    the pseudo-inverse (singular values below rcond sigma_max
//                  dropped): the exact solution of smallest norm when the
//                  system is consistent, the least-squares one otherwise
//   bounds         the bounded problem is convex and is solved by an
//                  accelerated projected gradient method to a stated tolerance
//   sparse         orthogonal matching pursuit chooses at most k sources
//                  (greedy: a good, not a guaranteed optimal, selection)
//
// The result says whether it is an exact mathematical solution (residual at
// numerical precision), an exact minimum-norm solution of an underdetermined
// system, or an approximate least-squares solution, and why.
#include "foundation/Result.h"
#include "optimization/Evaluator.h"

#include <Eigen/Core>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace resonant::optimization {

struct LinearInverseOptions {
    enum class Method { Regularized, MinimumNorm };
    Method method = Method::Regularized;
    double regularization = 1e-6;     // lambda, relative
    double powerPenalty = 0.0;        // mu, relative
    std::optional<double> maxMagnitude;  // W, in the weight unit
    int maxActive = 0;                // > 0: at most this many active sources
    double rcond = 1e-10;             // MinimumNorm cut-off
    double residualTolerance = 0.05;  // "accurate" when the relative residual is below this
    int maxIterations = 20000;        // bounded problem
};

struct LinearInverseResult {
    Eigen::VectorXcd weights;
    Eigen::VectorXcd achieved;        // A w
    double residualNorm = 0.0;        // ||A w - b|| (unweighted), target unit
    double relativeResidual = 0.0;    // residualNorm / ||b|| (residualNorm when b = 0)
    double conditionNumber = 0.0;     // of the weighted matrix (active columns)
    int rank = 0;
    std::vector<int> activeSources;   // all sources unless sparse
    bool boundLimited = false;        // some |w_i| is at the bound
    bool exact = false;               // residual at numerical precision
    bool accurate = false;            // relativeResidual <= tolerance
    int iterations = 0;               // bounded problem
    std::string classification;
    std::vector<std::string> notes;
};

Result<LinearInverseResult> solveLinearInverse(const LinearSystem& system, const LinearInverseOptions& options = {});

// The design values (by binding) that realize the weights: amplitude |w_i|
// and phase arg w_i in (-pi, pi] for each source.
std::map<std::string, double, std::less<>> weightBindings(const LinearSystem& system, const Eigen::VectorXcd& weights);

}  // namespace resonant::optimization
