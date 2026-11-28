#pragma once

// Mechanical resonators as lumped oscillators (RES-RSN-005, RES-RSN-008): a
// cantilever's bending mode referred to its tip (Euler-Bernoulli), and two
// cantilevers coupled by a spring between their tips as a network of the
// oscillators module. Structural damping by a loss factor eta is represented
// by the equivalent viscous damper at the mode's frequency (c = eta k / w).

#include "foundation/Result.h"
#include "oscillators/LumpedNetwork.h"

namespace resonant::mechanics {

struct CantileverSpec {
    double length = 0.0, width = 0.0, thickness = 0.0;  // m; bending across the thickness
    double youngsModulus = 0.0, density = 0.0;            // Pa, kg/m^3
    double lossFactor = 0.0;                              // structural, eta
    Status validate() const;
};

struct CantileverMode {
    int mode = 1;
    double frequencyHz = 0.0;
    double tipMass = 0.0;       // kg: the modal mass for a unit tip displacement (mode 1: 0.25 of the beam)
    double tipStiffness = 0.0;  // N/m: w^2 times the tip mass
    double quality = 0.0;       // 1 / eta
    double kineticEnergyPerTip2 = 0.0;  // J per m^2 of tip amplitude at the maximum velocity
};
// Euler-Bernoulli clamped-free bending modes (lambda_n L = 1.8751, 4.6941, 7.8548, ...).
Result<CantileverMode> cantileverMode(const CantileverSpec& spec, int mode = 1);

// Two cantilevers whose tips are joined by a spring of stiffness kc (N/m).
Result<oscillators::Network> coupledCantilevers(const CantileverSpec& first, const CantileverSpec& second, double couplingStiffness);

}  // namespace resonant::mechanics
