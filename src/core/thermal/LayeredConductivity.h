#pragma once

// The effective conductivity of a layered medium (RES-META-009): layers of
// thickness d_i and conductivity k_i stacked along z.
//   k_parallel      = sum(d_i k_i) / sum(d_i)        (heat along the layers: parallel paths)
//   k_perpendicular = sum(d_i) / sum(d_i / k_i)      (heat across them: series resistances)
// exact for steady conduction with a uniform gradient (no interface
// resistance); checked against the finite-volume solver on the explicit layers.

#include "foundation/Result.h"

#include <vector>

namespace resonant::thermal {

struct ConductiveLayer {
    double thickness = 0.0;     // m
    double conductivity = 0.0;  // W/(m K)
};

struct EffectiveConductivity {
    double parallel = 0.0, perpendicular = 0.0;  // W/(m K)
    double anisotropy = 0.0;                      // parallel / perpendicular
    double period = 0.0;                          // m
};
Result<EffectiveConductivity> layeredConductivity(const std::vector<ConductiveLayer>& cell);

// The explicit check: `periods` repetitions of the cell resolved by `cellsPerLayer`
// finite volumes per layer, a temperature difference across (z) and along (x) the
// stack in turn, adiabatic elsewhere; the conductivities that the computed heat flow implies.
struct ExplicitConductivity {
    double parallel = 0.0, perpendicular = 0.0;
    double balanceResidual = 0.0;  // W, the larger of the two solves
};
Result<ExplicitConductivity> explicitLayeredConductivity(const std::vector<ConductiveLayer>& cell, int periods, int cellsPerLayer = 2);

}  // namespace resonant::thermal
