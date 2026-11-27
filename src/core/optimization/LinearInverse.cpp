#include "optimization/LinearInverse.h"

#include "foundation/Types.h"

#include <Eigen/SVD>

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace resonant::optimization {

namespace {

struct Solved {
    Eigen::VectorXcd w;
    double condition = 0.0;
    double sigmaMax = 0.0;
    int rank = 0;
};

Solved solveDense(const Eigen::MatrixXcd& a, const Eigen::VectorXcd& b, const LinearInverseOptions& o)
{
    Solved s;
    Eigen::BDCSVD<Eigen::MatrixXcd> svd(a, Eigen::ComputeThinU | Eigen::ComputeThinV);
    const Eigen::VectorXd& sigma = svd.singularValues();
    s.sigmaMax = sigma.size() ? sigma(0) : 0.0;
    const double sigmaMin = sigma.size() ? sigma(sigma.size() - 1) : 0.0;
    s.condition = sigmaMin > 0.0 ? s.sigmaMax / sigmaMin : std::numeric_limits<double>::infinity();
    const double lambda = (o.regularization + o.powerPenalty) * s.sigmaMax * s.sigmaMax;
    const Eigen::VectorXcd ub = svd.matrixU().adjoint() * b;
    Eigen::VectorXcd coefficients(sigma.size());
    for (Eigen::Index i = 0; i < sigma.size(); ++i) {
        const double si = sigma(i);
        if (si > o.rcond * s.sigmaMax)
            ++s.rank;
        if (o.method == LinearInverseOptions::Method::MinimumNorm)
            coefficients(i) = si > o.rcond * s.sigmaMax ? ub(i) / si : Complex(0.0, 0.0);
        else
            coefficients(i) = si > 0.0 ? ub(i) * (si / (si * si + lambda)) : Complex(0.0, 0.0);
    }
    s.w = svd.matrixV() * coefficients;
    return s;
}

std::string number(double v)
{
    std::ostringstream s;
    s.precision(4);
    s << v;
    return s.str();
}

}  // namespace

Result<LinearInverseResult> solveLinearInverse(const LinearSystem& system, const LinearInverseOptions& o)
{
    using R = Result<LinearInverseResult>;
    const Eigen::Index m = system.response.rows(), n = system.response.cols();
    if (m == 0 || n == 0)
        return R::failure("the linear model has no targets or no sources");
    if (system.desired.size() != m)
        return R::failure("one desired value per target is needed");
    if (o.regularization < 0.0 || o.powerPenalty < 0.0)
        return R::failure("regularization and power penalty must not be negative");
    if (o.maxMagnitude && !(*o.maxMagnitude > 0.0))
        return R::failure("the weight bound must be positive");
    Eigen::VectorXd q = system.rowWeights.size() == m ? system.rowWeights : Eigen::VectorXd::Ones(m);
    if ((q.array() < 0.0).any())
        return R::failure("target weights must not be negative");
    const Eigen::VectorXd sq = q.cwiseSqrt();
    const Eigen::MatrixXcd a = sq.asDiagonal() * system.response;
    const Eigen::VectorXcd b = sq.cast<Complex>().cwiseProduct(system.desired);

    LinearInverseResult result;
    std::vector<int> active;
    if (o.maxActive > 0 && o.maxActive < n) {
        // Orthogonal matching pursuit.
        Eigen::VectorXcd residual = b;
        std::set<int> chosen;
        for (int k = 0; k < o.maxActive; ++k) {
            int best = -1;
            double bestValue = -1.0;
            for (Eigen::Index j = 0; j < n; ++j) {
                if (chosen.contains(static_cast<int>(j)))
                    continue;
                const double norm = a.col(j).norm();
                if (norm <= 0.0)
                    continue;
                const double value = std::abs(a.col(j).dot(residual)) / norm;
                if (value > bestValue) {
                    bestValue = value;
                    best = static_cast<int>(j);
                }
            }
            if (best < 0)
                break;
            chosen.insert(best);
            Eigen::MatrixXcd sub(m, static_cast<Eigen::Index>(chosen.size()));
            Eigen::Index c = 0;
            for (int j : chosen)
                sub.col(c++) = a.col(j);
            const Solved s = solveDense(sub, b, o);
            residual = b - sub * s.w;
        }
        active.assign(chosen.begin(), chosen.end());
        result.notes.push_back("sparse excitation: " + std::to_string(active.size()) + " of " + std::to_string(n) +
                               " sources chosen by orthogonal matching pursuit (a greedy selection, not a guaranteed optimum)");
    } else {
        for (Eigen::Index j = 0; j < n; ++j)
            active.push_back(static_cast<int>(j));
    }
    Eigen::MatrixXcd sub(m, static_cast<Eigen::Index>(active.size()));
    for (size_t c = 0; c < active.size(); ++c)
        sub.col(static_cast<Eigen::Index>(c)) = a.col(active[c]);
    const Solved s = solveDense(sub, b, o);
    Eigen::VectorXcd w = s.w;
    result.conditionNumber = s.condition;
    result.rank = s.rank;

    if (o.maxMagnitude && (w.cwiseAbs().array() > *o.maxMagnitude * (1.0 + 1e-12)).any()) {
        // Accelerated projected gradient (FISTA) on the convex bounded problem.
        const double bound = *o.maxMagnitude;
        const double lambda = (o.regularization + o.powerPenalty) * s.sigmaMax * s.sigmaMax;
        const double lipschitz = 2.0 * (s.sigmaMax * s.sigmaMax + lambda);
        const auto project = [bound](Eigen::VectorXcd v) {
            for (Eigen::Index i = 0; i < v.size(); ++i)
                if (std::abs(v(i)) > bound)
                    v(i) *= bound / std::abs(v(i));
            return v;
        };
        Eigen::VectorXcd x = project(w), y = x;
        double t = 1.0;
        int it = 0;
        for (; it < o.maxIterations; ++it) {
            const Eigen::VectorXcd gradient = 2.0 * (sub.adjoint() * (sub * y - b)) + 2.0 * lambda * y;
            const Eigen::VectorXcd next = project(y - gradient / lipschitz);
            const double change = (next - x).norm();
            const double tn = 0.5 * (1.0 + std::sqrt(1.0 + 4.0 * t * t));
            y = next + ((t - 1.0) / tn) * (next - x);
            x = next;
            t = tn;
            if (change <= 1e-12 * std::max(1.0, x.norm()))
                break;
        }
        w = x;
        result.iterations = it + 1;
        result.notes.push_back("weight bound |w| <= " + number(bound) + " " + system.weightUnit +
                               ": solved as a convex bounded problem (accelerated projected gradient, " + std::to_string(result.iterations) +
                               " iterations)");
    }
    result.weights = Eigen::VectorXcd::Zero(n);
    for (size_t c = 0; c < active.size(); ++c)
        result.weights(active[c]) = w(static_cast<Eigen::Index>(c));
    result.activeSources = active;
    if (o.maxMagnitude)
        for (Eigen::Index i = 0; i < n; ++i)
            if (std::abs(result.weights(i)) >= *o.maxMagnitude * (1.0 - 1e-6))
                result.boundLimited = true;

    result.achieved = system.response * result.weights;
    const double bnorm = system.desired.norm();
    result.residualNorm = (result.achieved - system.desired).norm();
    result.relativeResidual = bnorm > 0.0 ? result.residualNorm / bnorm : result.residualNorm;
    result.exact = result.relativeResidual <= 1e-9 || (bnorm == 0.0 && result.residualNorm <= 1e-15);
    result.accurate = result.relativeResidual <= o.residualTolerance;
    const bool underdetermined = static_cast<Eigen::Index>(active.size()) > m && result.rank == m;
    if (result.exact && o.method == LinearInverseOptions::Method::MinimumNorm && underdetermined)
        result.classification = "exact solution of smallest norm (the system is underdetermined: other exact solutions exist)";
    else if (result.exact)
        result.classification = "exact solution (the residual is at numerical precision)";
    else if (result.accurate)
        result.classification = "approximate solution (least squares; relative residual " + number(result.relativeResidual) + ")";
    else
        result.classification = "approximate solution that misses the targets (relative residual " + number(result.relativeResidual) +
                                "): the wanted field is not reachable with these sources" + (result.boundLimited ? " and bounds" : "");
    if (o.method == LinearInverseOptions::Method::Regularized && (o.regularization + o.powerPenalty) > 0.0)
        result.notes.push_back("regularized (lambda + mu = " + number(o.regularization + o.powerPenalty) +
                               " x sigma_max^2): a deliberate bias towards smaller weights");
    if (result.conditionNumber > 1e8)
        result.notes.push_back("the response matrix is ill-conditioned (condition number " + number(result.conditionNumber) +
                               "): small changes of the targets change the weights strongly");
    if (result.rank < std::min<Eigen::Index>(m, static_cast<Eigen::Index>(active.size())))
        result.notes.push_back("the response matrix is rank deficient (rank " + std::to_string(result.rank) + ")");
    result.notes.push_back("the solution is exact for the model '" + system.model + "' only; it is a design to be checked, not a measured field");
    return result;
}

std::map<std::string, double, std::less<>> weightBindings(const LinearSystem& system, const Eigen::VectorXcd& weights)
{
    std::map<std::string, double, std::less<>> out;
    for (const std::string& b : system.zeroBindings)
        out[b] = 0.0;
    for (Eigen::Index i = 0; i < weights.size(); ++i) {
        const size_t k = static_cast<size_t>(i);
        if (k < system.amplitudeBindings.size() && !system.amplitudeBindings[k].empty())
            out[system.amplitudeBindings[k]] = std::abs(weights(i));
        if (k < system.phaseBindings.size() && !system.phaseBindings[k].empty())
            out[system.phaseBindings[k]] = std::abs(weights(i)) > 0.0 ? std::arg(weights(i)) : 0.0;
    }
    return out;
}

}  // namespace resonant::optimization
