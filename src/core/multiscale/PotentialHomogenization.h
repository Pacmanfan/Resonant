#pragma once

// Computational homogenization of a steady potential problem (RES-MSC-006):
//   div(K grad u) = 0  in a voxel RVE, K a 3x3 tensor per voxel
// - heat conduction (K: thermal conductivity), electric conduction (electrical
// conductivity) and electrostatics (permittivity) alike. Trilinear hexahedral finite
// elements on the voxel grid (2x2x2 Gauss points), conjugate gradients with an
// incomplete-Cholesky preconditioner.
//   periodic: u = G.x + phi, phi periodic          (the RVE must be periodic)
//   linear:   u = G.x on the whole boundary         (kinematic uniform; an upper estimate)
//   flux:     the normal flux Q.n prescribed on the whole boundary (static uniform; the lower estimate):
//             K_eff = (the matrix of <grad u> over Q = e1, e2, e3)^-1
//   mixed:    u = G.x on the two faces normal to G, no flux through the others
// For any RVE: flux <= mixed <= linear and flux <= periodic <= linear (apparent values).
// The effective tensor satisfies K_eff G = <K grad u>; the Hill-Mandel residual
// |<grad u . K grad u> - G . K_eff G| / (G . K_eff G) checks the energy consistency.

#include "foundation/Result.h"

#include <Eigen/Dense>

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace resonant::multiscale {

enum class Boundary { Periodic, Linear, Mixed, Flux };
std::string toString(Boundary boundary);

struct PotentialProblem {
    std::array<int, 3> n{1, 1, 1};
    std::array<double, 3> size{1.0, 1.0, 1.0};
    std::vector<Eigen::Matrix3d> K;  // per voxel, x fastest; symmetric positive definite
    bool periodic = true;            // the structure itself is periodic
};

struct PotentialResult {
    Eigen::Matrix3d effective = Eigen::Matrix3d::Zero();
    double hillMandel = 0.0;     // the largest relative energy residual of the three load cases
    double residual = 0.0;       // the largest relative CG residual
    int iterations = 0;          // the largest CG iteration count
    int unknowns = 0;
    Boundary boundary = Boundary::Periodic;
};

// `cancelled` is polled between the load cases (may be empty).
Result<PotentialResult> homogenize(const PotentialProblem& problem, Boundary boundary, double tolerance = 1e-10,
                                   const std::function<bool()>& cancelled = {});

// Bounds of a two-phase isotropic composite (volume fraction f of phase 2).
struct TwoPhaseBounds {
    double voigt = 0.0, reuss = 0.0;               // arithmetic and harmonic means
    double hashinLower = 0.0, hashinUpper = 0.0;   // Hashin-Shtrikman (3D)
    double maxwellGarnett = 0.0;                    // dilute spheres of phase 2 in phase 1 (equals one HS bound)
};
TwoPhaseBounds twoPhaseBounds(double k1, double k2, double f2);

// The self-consistent conductivity of an untextured polycrystal of uniaxial grains
// (principal values ka, ka, kc): sum over the principal directions of (k_i - k*) / (k_i + 2 k*) = 0.
double selfConsistentUniaxial(double ka, double kc);

}  // namespace resonant::multiscale
