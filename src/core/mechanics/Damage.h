#pragma once

// Scalar isotropic damage (RES-DMS-006): sigma = (1 - D) E eps with D a function of the
// largest equivalent strain reached, kappa (a history variable), D = 0 up to kappa0, then
//   exponential: D = 1 - (kappa0 / kappa) exp(-(kappa - kappa0) / (kappaF - kappa0))  (softening at once:
//                the stress peaks at kappa0; in a structure the damage localizes and depends on the mesh)
//   power:       D = 1 - (kappa0 / kappa)^n, 0 < n < 1  (stiffness loss without softening: the stress
//                still rises as kappa^(1 - n), so a structure stays stable and mesh-objective)
// Unloading is secant (towards the origin, D fixed). The energy released when D grows,
// Y = 1/2 E eps^2 per volume, is dissipated: d(dissipated) = Y dD. Damage never heals.

#include "foundation/Result.h"

#include <string>

namespace resonant::mechanics {

struct DamageParameters {
    double youngsModulus = 0.0;
    double thresholdStrain = 0.0;  // kappa0
    double softeningStrain = 0.0;  // kappaF > kappa0 (exponential law)
    std::string law = "exponential";  // or "power"
    double exponent = 0.5;            // n (power law)
    Status validate() const;
    double damage(double kappa) const;
    double damageSlope(double kappa) const;  // dD / dkappa
};

struct DamageState {
    double kappa = 0.0;  // the largest equivalent strain reached (set to kappa0 initially by the caller or 0)
    double damage = 0.0;
    double dissipated = 0.0;  // J/m^3
};

struct DamageResponse {
    double stress = 0.0, tangent = 0.0;  // tangent: on the loading branch the softening slope, else secant
    DamageState state;
    bool loading = false;
    double storedEnergy = 0.0;           // J/m^3
};

// Uniaxial, equivalent strain |eps|.
DamageResponse damageUpdate(const DamageParameters& p, const DamageState& committed, double strain);

}  // namespace resonant::mechanics
