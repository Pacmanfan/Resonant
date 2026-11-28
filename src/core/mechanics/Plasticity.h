#pragma once

// Rate-independent J2 plasticity, small strain (RES-DMS-005): von Mises yield with
// linear isotropic (H) and linear kinematic (Prager, Hk) hardening, backward-Euler
// radial return and its consistent tangent.
//   f = |dev(sigma) - alpha| - sqrt(2/3) (sigma_y + H e_p)
//   d eps_p = dgamma n,  d alpha = (2/3) Hk dgamma n,  d e_p = sqrt(2/3) dgamma
// Voigt vectors at the interface: strain [exx, eyy, ezz, gyz, gxz, gxy] (engineering
// shear), stress [sxx, syy, szz, tyz, txz, txy]; Mandel notation inside.
// Energies per volume: elastic 1/2 sigma:eps_e, hardening 1/2 H e_p^2 + 3/4 alpha:alpha / Hk,
// dissipated = the plastic work minus the stored hardening energy = sigma_y e_p (linear hardening).
// Also: a uniaxial-stress driver (the other stresses held at zero by Newton) and a bar
// fixed at both ends with a load at an interior point (statically indeterminate).

#include "foundation/Result.h"

#include <Eigen/Dense>

#include <vector>

namespace resonant::mechanics {

using Vec6 = Eigen::Matrix<double, 6, 1>;
using Mat6 = Eigen::Matrix<double, 6, 6>;

struct J2Parameters {
    double youngsModulus = 0.0, poissonsRatio = 0.3;
    double yieldStress = 0.0;          // initial, Pa
    double isotropicHardening = 0.0;   // H, Pa
    double kinematicHardening = 0.0;   // Hk, Pa
    Status validate() const;
};

struct J2State {
    Vec6 plasticStrain = Vec6::Zero();  // Voigt (engineering shear)
    Vec6 backStress = Vec6::Zero();     // Voigt stress
    double equivalentPlasticStrain = 0.0;
    double dissipated = 0.0;            // J/m^3, accumulated
};

struct J2Response {
    Vec6 stress = Vec6::Zero();
    Mat6 tangent = Mat6::Zero();  // d sigma / d eps (Voigt)
    J2State state;                // the trial state
    bool plastic = false;
    double elasticEnergy = 0.0, hardeningEnergy = 0.0;  // J/m^3
};

// From the committed state and the total strain of the step.
J2Response j2Update(const J2Parameters& p, const J2State& committed, const Vec6& strain);

// Uniaxial stress: the axial strain given, the five other stresses zero.
struct UniaxialPoint {
    double stress = 0.0, tangent = 0.0;  // d sigma_11 / d eps_11
    Vec6 strain = Vec6::Zero();          // the full strain found
    J2Response response;
    int iterations = 0;
};
Result<UniaxialPoint> j2Uniaxial(const J2Parameters& p, const J2State& committed, double axialStrain, const Vec6& guess = Vec6::Zero());

// A bar of length L and section A fixed at both ends, loaded by a force P at x = a:
// the segment [0, a] in tension, [a, L] in compression, one uniaxial material point each.
struct BarStep {
    double load = 0.0, displacement = 0.0;     // N, m at the load point
    double force1 = 0.0, force2 = 0.0;         // N: axial forces of the two segments
    double plasticStrain1 = 0.0, plasticStrain2 = 0.0;  // axial plastic strains
    double externalWork = 0.0, elasticEnergy = 0.0, hardeningEnergy = 0.0, dissipated = 0.0;  // J
    int iterations = 0;
    double residual = 0.0;
    bool converged = true;
};
struct BarSpec {
    J2Parameters material;
    double length = 1.0, loadPoint = 0.3, area = 1e-4;
    std::vector<double> loads;  // the load path, N (each a step)
    double tolerance = 1e-10;
    Status validate() const;
};
// Stops at the first step that does not converge (it is reported, not committed).
Result<std::vector<BarStep>> j2Bar(const BarSpec& spec);

}  // namespace resonant::mechanics
