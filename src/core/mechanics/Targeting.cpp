#include "mechanics/Targeting.h"

#include <Eigen/SVD>
#include <Eigen/SparseLU>

#include <cmath>

namespace resonant::mechanics {

Result<TargetingSolution> solveTargets(const StructuralModel& model, const SystemMatrices& system, const std::vector<ForceInput>& actuators,
                                       const std::vector<TargetRequest>& targets, double frequencyHz, const Damping& damping,
                                       double regularization)
{
    using Failure = Result<TargetingSolution>;
    (void)model;
    if (actuators.empty())
        return Failure::failure("there is no active actuator on the structure");
    if (targets.empty())
        return Failure::failure("there is no vibration target on the structure");
    if (!(frequencyHz > 0.0))
        return Failure::failure("the frequency must be positive");
    if (damping.kind == DampingKind::ModalRatio)
        return Failure::failure("targeting solves the dynamic stiffness directly; use Rayleigh damping, a loss factor or none");
    const double omega = 2.0 * 3.14159265358979323846 * frequencyHz;
    const Eigen::SparseMatrix<Complex> d = dynamicStiffness(system, omega, damping);
    Eigen::SparseLU<Eigen::SparseMatrix<Complex>> lu(d);
    if (lu.info() != Eigen::Success)
        return Failure::failure("the dynamic stiffness is singular at this frequency (an undamped natural frequency); add damping");
    const auto rows = static_cast<Eigen::Index>(targets.size()), cols = static_cast<Eigen::Index>(actuators.size());
    TargetingSolution out;
    out.frequencyHz = frequencyHz;
    out.transfer.resize(rows, cols);
    for (Eigen::Index j = 0; j < cols; ++j) {
        const Eigen::VectorXcd p = system.map.reduce(actuators[static_cast<size_t>(j)].pattern).cast<Complex>();
        const Eigen::VectorXcd u = lu.solve(p);
        for (Eigen::Index i = 0; i < rows; ++i)
            out.transfer(i, j) = system.map.reduce(targets[static_cast<size_t>(i)].point.selector).cast<Complex>().dot(u);
    }
    Eigen::VectorXd w(rows);
    Eigen::VectorXcd t(rows);
    for (Eigen::Index i = 0; i < rows; ++i) {
        w(i) = std::sqrt(targets[static_cast<size_t>(i)].weight);
        t(i) = targets[static_cast<size_t>(i)].desired;
        out.desired.push_back(t(i));
    }
    const Eigen::MatrixXcd a = w.asDiagonal() * out.transfer;
    const Eigen::VectorXcd b = w.asDiagonal() * t;
    Eigen::BDCSVD<Eigen::MatrixXcd> svd(a, Eigen::ComputeThinU | Eigen::ComputeThinV);
    const Eigen::VectorXd sigma = svd.singularValues();
    const double lambda = regularization * (sigma.size() ? sigma(0) : 0.0);
    out.regularization = lambda;
    Eigen::VectorXcd f = Eigen::VectorXcd::Zero(cols);
    for (Eigen::Index r = 0; r < sigma.size(); ++r) {
        const double s = sigma(r);
        if (s <= 0.0)
            continue;
        const double filter = s / (s * s + lambda * lambda);
        f += (filter * svd.matrixU().col(r).dot(b)) * svd.matrixV().col(r);
    }
    for (Eigen::Index r = 0; r < sigma.size(); ++r)
        out.singularValues.push_back(sigma(r));
    const Eigen::VectorXcd achieved = out.transfer * f;
    for (Eigen::Index j = 0; j < cols; ++j)
        out.forces.push_back(f(j));
    for (Eigen::Index i = 0; i < rows; ++i)
        out.achieved.push_back(achieved(i));
    const double wanted = b.norm();
    out.relativeResidual = wanted > 0.0 ? (a * f - b).norm() / wanted : 0.0;
    out.notes = "Weighted least squares with Tikhonov regularization lambda = " + std::to_string(regularization) + " x the largest singular value.";
    if (rows > cols)
        out.notes += " More targets than actuators: an exact match is generally impossible.";
    if (sigma.size() && sigma(sigma.size() - 1) < 1e-8 * sigma(0))
        out.notes += " The transfer matrix is nearly singular: some target combinations cannot be controlled independently.";
    return out;
}

}  // namespace resonant::mechanics
