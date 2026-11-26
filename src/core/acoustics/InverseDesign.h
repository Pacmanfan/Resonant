// Inverse design for point targets: find complex source strengths w such
// that the field at M target points approximates desired complex values b.
//
//   H w ~ b,   H(m, i) = pressure at target m per unit strength of source i
//
// solved as the regularized, weighted least-squares problem
//
//   minimize  sum_m q_m |(H w - b)_m|^2  +  lambda * sigma_max^2 * ||w||^2
//
// through the singular value decomposition of the row-weighted matrix
// (no matrix is inverted). `lambda` is relative to the largest singular
// value squared, so it is dimensionless and comparable between problems.
#pragma once

#include "acoustics/Propagation.h"

#include <Eigen/Dense>

namespace resonant::acoustics {

struct TargetPoint {
    Id objectId;
    std::string label;
    Vec3 position = Vec3::Zero();  // world
    Complex desired = Complex(1.0, 0.0);  // desired complex peak pressure, Pa
    double weight = 1.0;
};

struct TargetCollection {
    std::vector<TargetPoint> points;
    // Names of target objects of a kind no solver handles yet (line, plane,
    // surface, volume). They are reported, never silently treated as points.
    std::vector<std::string> unsupported;
};

// The point targets of a scene (or of the listed objects only). A target
// marked "suppress" asks for zero pressure.
TargetCollection collectTargets(const Scene& scene, const std::vector<Id>& only = {});

struct InverseOptions {
    double regularization = 1e-6;  // lambda, relative, >= 0
    // Largest allowed source strength |w_i| in Pa at 1 m; infinity = no limit.
    double maxAmplitudePa = std::numeric_limits<double>::infinity();
    // The solution counts as accurate when the relative residual is below this.
    double residualTolerance = 0.05;
};

struct InverseSolution {
    bool solved = false;
    std::string message;                 // why it could not be solved, when solved is false
    Eigen::VectorXcd weights;            // one per source: complex strength, Pa at 1 m
    Eigen::VectorXcd achieved;           // H w at the targets, Pa
    double residualNorm = 0.0;           // ||H w - b||, Pa (unweighted)
    double relativeResidual = 0.0;       // residualNorm / ||b||  (residualNorm itself when b = 0)
    double conditionNumber = 0.0;        // sigma_max / sigma_min of the weighted matrix
    int rank = 0;
    bool amplitudeLimited = false;       // some strengths were clipped to maxAmplitudePa
    bool accurate = false;               // relativeResidual <= residualTolerance
    std::vector<std::string> notes;      // what the user should know about this solution
};

// The propagation matrix for unit-strength sources (the sources' own
// amplitudes are ignored; their positions, axes and directivities are used).
Eigen::MatrixXcd propagationMatrix(const std::vector<PointSource>& sources, const std::vector<TargetPoint>& targets,
                                   const Medium& medium, const PropagationModel& model);

InverseSolution solveInverse(const std::vector<PointSource>& sources, const std::vector<TargetPoint>& targets,
                             const Medium& medium, const PropagationModel& model, const InverseOptions& options = {});

// Array settings with the solved strengths applied as per-element amplitude
// and phase overrides (delay overrides are cleared). `sources` must be the
// sources the solution was computed for; those belonging to `arrayId` are
// applied.
ArraySettings applyWeights(const ArraySettings& settings, const Id& arrayId, const std::vector<PointSource>& sources,
                           const Eigen::VectorXcd& weights);

}  // namespace resonant::acoustics
