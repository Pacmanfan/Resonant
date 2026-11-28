#pragma once

// A standard linear solid (Zener) (RES-DMS-008): a spring E_inf in parallel with a
// Maxwell branch (E1, eta = E1 tau). sigma = E_inf eps + q, dq/dt = -q / tau + E1 deps/dt,
// integrated exactly for a strain linear within each step:
//   q_{n+1} = exp(-dt / tau) q_n + E1 tau (deps / dt) (1 - exp(-dt / tau)).
// Stored energy 1/2 E_inf eps^2 + 1/2 q^2 / E1; dissipation rate q^2 / (E1 tau).
// Relaxation: sigma(t) = eps0 (E_inf + E1 exp(-t / tau)); loss modulus
// E'' = E1 w tau / (1 + w^2 tau^2), so a harmonic cycle dissipates pi eps0^2 E''.

#include "foundation/Result.h"

#include <vector>

namespace resonant::mechanics {

struct ZenerParameters {
    double relaxedModulus = 0.0;  // E_inf, Pa
    double branchModulus = 0.0;   // E1, Pa
    double relaxationTime = 0.0;  // tau, s
    Status validate() const;
    double lossModulus(double angularFrequency) const;
    double storageModulus(double angularFrequency) const;
};

struct ZenerPoint {
    double time = 0.0, strain = 0.0, stress = 0.0;
    double branchStress = 0.0;  // q, the internal (history) variable
    double storedEnergy = 0.0, dissipated = 0.0;  // J/m^3
};

// The response to a strain history given at times (ascending, starting at the first entry).
Result<std::vector<ZenerPoint>> zenerResponse(const ZenerParameters& p, const std::vector<double>& times, const std::vector<double>& strains);

}  // namespace resonant::mechanics
