#pragma once

// Actuator inputs for wanted vibration at target points (and none at
// suppression points), at one frequency.
//
// The structure is linear: at angular frequency omega, the target
// displacements are t = H f, H[i][j] = s_i^T D(omega)^-1 p_j, with D the
// dynamic stiffness, p_j the load pattern of actuator j and s_i the target
// selector. H is calculated with one factorization of D. The complex forces
// f minimize ||W (H f - t_wanted)||^2 + (lambda sigma_max)^2 ||f||^2
// (weighted, Tikhonov-regularized least squares by SVD). Nothing guarantees
// that the wanted distribution is reachable: the residual and the
// achieved values are always reported.
#include "mechanics/Analyses.h"

namespace resonant::mechanics {

struct TargetRequest {
    std::string label;
    ResponsePoint point;
    Complex desired = Complex(0.0, 0.0);
    double weight = 1.0;
};

struct TargetingSolution {
    double frequencyHz = 0.0;
    std::vector<Complex> forces;      // per actuator, N (complex: amplitude and phase)
    std::vector<Complex> achieved;    // per target, m
    std::vector<Complex> desired;
    Eigen::MatrixXcd transfer;        // targets x actuators, m/N
    std::vector<double> singularValues;
    double relativeResidual = 0.0;    // ||W (H f - t)|| / ||W t||, or ||W H f|| / ||H|| when every target is a suppression
    double regularization = 0.0;
    std::string notes;
};

Result<TargetingSolution> solveTargets(const StructuralModel& model, const SystemMatrices& system, const std::vector<ForceInput>& actuators,
                                       const std::vector<TargetRequest>& targets, double frequencyHz, const Damping& damping,
                                       double regularization);

}  // namespace resonant::mechanics
