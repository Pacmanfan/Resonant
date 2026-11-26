#include "acoustics/InverseDesign.h"

#include "foundation/Units.h"

#include <cmath>

namespace resonant::acoustics {

TargetCollection collectTargets(const Scene& scene, const std::vector<Id>& only)
{
    TargetCollection collection;
    scene.forEach([&](const SceneObject& object) {
        const auto* target = componentOf<TargetComponent>(object);
        if (!target)
            return;
        if (!only.empty() && std::find(only.begin(), only.end(), object.id()) == only.end())
            return;
        if (target->settings.kind != TargetKind::Point) {
            collection.unsupported.push_back(object.state.name);
            return;
        }
        TargetPoint point;
        point.objectId = object.id();
        point.label = object.state.name;
        point.position = transformPoint(scene.worldMatrix(object), Vec3::Zero());
        point.desired = target->settings.suppress
                            ? Complex(0.0, 0.0)
                            : std::polar(target->settings.desiredAmplitudePa, target->settings.desiredPhaseRad);
        point.weight = target->settings.weight;
        collection.points.push_back(std::move(point));
    });
    return collection;
}

Eigen::MatrixXcd propagationMatrix(const std::vector<PointSource>& sources, const std::vector<TargetPoint>& targets,
                                   const Medium& medium, const PropagationModel& model)
{
    Eigen::MatrixXcd matrix(static_cast<Eigen::Index>(targets.size()), static_cast<Eigen::Index>(sources.size()));
    for (size_t m = 0; m < targets.size(); ++m)
        for (size_t i = 0; i < sources.size(); ++i)
            matrix(static_cast<Eigen::Index>(m), static_cast<Eigen::Index>(i)) =
                model.transfer(sources[i], targets[m].position, medium).value;
    return matrix;
}

InverseSolution solveInverse(const std::vector<PointSource>& sources, const std::vector<TargetPoint>& targets,
                             const Medium& medium, const PropagationModel& model, const InverseOptions& options)
{
    InverseSolution solution;
    if (sources.empty()) {
        solution.message = "there are no active sources to solve for";
        return solution;
    }
    if (targets.empty()) {
        solution.message = "there are no point targets";
        return solution;
    }
    if (!(options.regularization >= 0.0) || !(options.maxAmplitudePa > 0.0)) {
        solution.message = "the regularization must not be negative and the amplitude limit must be positive";
        return solution;
    }
    const double frequency = sources.front().frequencyHz;
    for (const PointSource& source : sources) {
        if (std::abs(source.frequencyHz - frequency) > 1e-9 * frequency) {
            solution.message = "the sources have different frequencies; a single-frequency solution needs one";
            return solution;
        }
    }

    const Eigen::MatrixXcd H = propagationMatrix(sources, targets, medium, model);
    Eigen::VectorXcd b(static_cast<Eigen::Index>(targets.size()));
    Eigen::VectorXd rootWeight(static_cast<Eigen::Index>(targets.size()));
    for (size_t m = 0; m < targets.size(); ++m) {
        b(static_cast<Eigen::Index>(m)) = targets[m].desired;
        rootWeight(static_cast<Eigen::Index>(m)) = std::sqrt(targets[m].weight);
    }
    if (!H.allFinite()) {
        solution.message = "the propagation matrix is not finite; check the source and target positions";
        return solution;
    }

    const Eigen::MatrixXcd weighted = rootWeight.asDiagonal() * H;
    const Eigen::VectorXcd rhs = rootWeight.asDiagonal() * b;
    const Eigen::BDCSVD<Eigen::MatrixXcd> svd(weighted, Eigen::ComputeThinU | Eigen::ComputeThinV);
    const Eigen::VectorXd sigma = svd.singularValues();
    const double sigmaMax = sigma.size() > 0 ? sigma(0) : 0.0;
    if (!(sigmaMax > 0.0)) {
        solution.message = "no source reaches any target (the propagation matrix is zero)";
        return solution;
    }
    const double lambda = options.regularization * sigmaMax * sigmaMax;
    // Singular values below this are numerically zero.
    const double floor = sigmaMax * 1e-13 * static_cast<double>(std::max(H.rows(), H.cols()));

    Eigen::VectorXcd coefficients = svd.matrixU().adjoint() * rhs;
    int rank = 0;
    for (Eigen::Index i = 0; i < sigma.size(); ++i) {
        if (sigma(i) > floor)
            ++rank;
        // Tikhonov filter factor s / (s^2 + lambda); with lambda = 0 this is
        // the pseudo-inverse, and null directions are dropped.
        const double factor = sigma(i) > floor || lambda > 0.0 ? sigma(i) / (sigma(i) * sigma(i) + lambda) : 0.0;
        coefficients(i) *= factor;
    }
    solution.weights = svd.matrixV() * coefficients;
    solution.rank = rank;
    solution.conditionNumber = sigma(sigma.size() - 1) > floor ? sigmaMax / sigma(sigma.size() - 1)
                                                               : std::numeric_limits<double>::infinity();

    if (std::isfinite(options.maxAmplitudePa)) {
        for (Eigen::Index i = 0; i < solution.weights.size(); ++i) {
            if (std::abs(solution.weights(i)) > options.maxAmplitudePa) {
                solution.weights(i) = std::polar(options.maxAmplitudePa, std::arg(solution.weights(i)));
                solution.amplitudeLimited = true;
            }
        }
    }

    solution.achieved = H * solution.weights;
    solution.residualNorm = (solution.achieved - b).norm();
    const double targetNorm = b.norm();
    solution.relativeResidual = targetNorm > 0.0 ? solution.residualNorm / targetNorm : solution.residualNorm;
    solution.accurate = solution.relativeResidual <= options.residualTolerance;
    solution.solved = true;

    if (solution.amplitudeLimited)
        solution.notes.push_back("Some source strengths exceeded the amplitude limit and were clipped to it; the "
                                 "achieved field was recalculated with the clipped strengths.");
    if (rank < static_cast<int>(std::min(H.rows(), H.cols())))
        solution.notes.push_back("The targets are not independent for this arrangement of sources (rank "
                                 + std::to_string(rank) + " of " + std::to_string(std::min(H.rows(), H.cols()))
                                 + "); the minimum-norm solution was taken.");
    if (targets.size() > sources.size())
        solution.notes.push_back("There are more targets than sources, so the targets can only be met in the "
                                 "least-squares sense.");
    if (!solution.accurate)
        solution.notes.push_back("The requested target values are NOT achieved accurately: the residual is "
                                 + std::to_string(100.0 * solution.relativeResidual) + " % of the requested field.");
    return solution;
}

ArraySettings applyWeights(const ArraySettings& settings, const Id& arrayId, const std::vector<PointSource>& sources,
                           const Eigen::VectorXcd& weights)
{
    ArraySettings result = settings;
    for (size_t i = 0; i < sources.size() && static_cast<Eigen::Index>(i) < weights.size(); ++i) {
        if (sources[i].objectId != arrayId || sources[i].elementIndex < 0)
            continue;
        const Complex w = weights(static_cast<Eigen::Index>(i));
        ElementOverride& o = result.overrides[sources[i].elementIndex];
        o.amplitudePa = std::abs(w);
        double phase = std::arg(w);
        if (phase < 0.0)
            phase += 2.0 * units::pi;
        o.phaseRad = phase;
        o.delaySec.reset();
    }
    return result;
}

}  // namespace resonant::acoustics
