#pragma once

// Pair-potential lattice statics (RES-MSC-009): the energy of a periodic structure at
// 0 K under a homogeneous deformation, its equilibrium lattice parameter, cohesive
// energy, elastic constants (Voigt, by central differences of the energy density),
// Born stability and isotropic (Voigt-Reuss-Hill) moduli. Classical and static: no
// zero-point motion, no temperature.
//   E = 1/2 sum_i sum_j phi(r_ij) over images within the cutoff
//       + the tail of a uniform density beyond it: 2 pi rho 4 eps (sigma^12 / (9 rc^9) - sigma^6 / (3 rc^3)) per atom

#include "atomistic/ForceField.h"

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace resonant::atomistic {

struct StaticsSettings {
    double cutoffSigma = 0.0;    // 0: the force field's
    bool tailCorrection = true;
};

// J per atom; `deformation` maps the reference lattice and positions (F = I + strain).
Result<double> energyPerAtom(const Crystal& crystal, const ForceField& field, const Eigen::Matrix3d& deformation = Eigen::Matrix3d::Identity(),
                             const StaticsSettings& settings = {});
// The largest force on an atom (N) in the deformed structure: nonzero means the basis would relax.
Result<double> maxForce(const Crystal& crystal, const ForceField& field, const Eigen::Matrix3d& deformation, const StaticsSettings& settings = {});

struct Relaxed {
    Crystal crystal;
    double scale = 1.0;            // applied to the lattice
    double energyPerAtom = 0.0;    // J (the cohesive energy is its negative)
    double pressure = 0.0;         // Pa, at the minimum (should be ~0)
    int evaluations = 0;
};
// The isotropic scale of the lattice that minimizes the energy (relative 1e-10).
Result<Relaxed> relaxIsotropic(const Crystal& crystal, const ForceField& field, const StaticsSettings& settings = {});

struct ElasticResult {
    Eigen::Matrix<double, 6, 6> C = Eigen::Matrix<double, 6, 6>::Zero();  // Pa, Voigt (xx, yy, zz, yz, xz, xy)
    bool stable = false;           // Born: C positive definite
    double cauchyDeviation = 0.0;  // |C12 - C44| / C44 (0 for central forces at centrosymmetric sites)
    double bulkVoigt = 0.0, bulkReuss = 0.0, shearVoigt = 0.0, shearReuss = 0.0;
    double bulk = 0.0, shear = 0.0, young = 0.0, poisson = 0.0;  // Hill averages
    double density = 0.0;          // kg/m^3
    double longitudinalSpeed = 0.0, transverseSpeed = 0.0;  // m/s, isotropic
    double maxInternalForce = 0.0; // N, under the applied shear strain
    std::vector<std::string> warnings;
};
// Elastic constants of a relaxed structure (strain amplitude `strain`).
Result<ElasticResult> elasticConstants(const Crystal& relaxed, const ForceField& field, const StaticsSettings& settings = {}, double strain = 1e-4);

}  // namespace resonant::atomistic
