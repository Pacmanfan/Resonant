#pragma once

// A cantilever whose elements accumulate damage (RES-DMS-006, RES-DMS-011):
// Euler-Bernoulli Hermite elements, clamped at x = 0, the tip deflection prescribed in
// steps (displacement control, so softening is traversed). Each element's equivalent
// strain is its largest extreme-fibre strain |w''| t / 2; its damage (Damage.h) reduces
// its bending stiffness E (1 - D) I, uniformly over the section. At each step the damage
// and the deflected shape are iterated to a fixed point; the step is committed only when
// it converged. The modes of the damaged beam (stiffness with the committed damage,
// consistent mass) come from the generalized eigenproblem - no damage-to-frequency law.
// Energies: external work, stored (1/2 u^T K(D) u), dissipated (sum over elements of the
// undamaged element energy times dD, averaged over the step like the work).

#include "foundation/Result.h"
#include "mechanics/Damage.h"

#include <vector>

namespace resonant::mechanics {

struct DamagedBeamSpec {
    double length = 0.0, width = 0.0, thickness = 0.0;  // m
    double density = 0.0;                              // kg/m^3
    DamageParameters damage;                           // E, kappa0, kappaF
    int elements = 40;
    std::vector<double> tipDeflections;                // the path, m (each a step)
    int modes = 3;
    double tolerance = 1e-10;                          // on the largest damage change of a fixed-point iteration
    Status validate() const;
};

struct BeamStep {
    double deflection = 0.0, load = 0.0;  // m, N (the tip reaction)
    double maxDamage = 0.0, rootDamage = 0.0;
    double externalWork = 0.0, storedEnergy = 0.0, dissipated = 0.0;  // J
    int iterations = 0;
    double residual = 0.0;
    bool converged = true;
};

struct DamagedBeamResult {
    std::vector<BeamStep> steps;
    std::vector<double> damage;          // per element, after the last committed step
    std::vector<double> elementCentres;  // m
    std::vector<double> frequencies;     // Hz, damaged (after the path)
    std::vector<double> undamagedFrequencies;
    double undamagedTipStiffness = 0.0, damagedTipStiffness = 0.0;  // N/m
};

Result<DamagedBeamResult> damagedCantilever(const DamagedBeamSpec& spec);

}  // namespace resonant::mechanics
