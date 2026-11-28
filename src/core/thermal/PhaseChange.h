#pragma once

// Melting and solidification in a slab (RES-DMS-007): transient conduction by the
// enthalpy method on a finite-volume grid. The enthalpy per volume, from the solid at
// the melting point, is H = rho c_s (T - Tm) below Tm, any value in [0, rho L] at Tm
// (liquid fraction f = H / (rho L)), and rho L + rho c_l (T - Tm) above; each cell's
// conductivity is (1 - f) k_s + f k_l. Explicit in time - conservative to rounding - with
// the step refused above its stability limit rho c_min dx^2 / (4 k_max).
// Reference: the one-phase Stefan problem (solid at Tm, a wall at Tw): the front
// s(t) = 2 lambda sqrt(alpha_l t) with lambda exp(lambda^2) erf(lambda) = St / sqrt(pi),
// St = c_l (Tw - Tm) / L.

#include "foundation/Result.h"

#include <functional>
#include <string>
#include <vector>

namespace resonant::thermal {

struct PhaseMaterial {
    double density = 0.0;
    double solidSpecificHeat = 0.0, liquidSpecificHeat = 0.0;     // J/(kg K)
    double solidConductivity = 0.0, liquidConductivity = 0.0;     // W/(m K)
    double meltingTemperature = 0.0;                              // K
    double latentHeat = 0.0;                                      // J/kg
    Status validate() const;
};

struct SlabFace {
    enum class Kind { Adiabatic, Temperature, Flux } kind = Kind::Adiabatic;
    double value = 0.0;  // K, or W/m^2 into the slab
};

struct PhaseChangeProblem {
    double length = 0.0;
    int cells = 100;
    PhaseMaterial material;
    double initialTemperature = 0.0;
    SlabFace left, right;
    Status validate() const;
};

struct PhaseChangeSettings {
    double duration = 0.0;
    double timeStep = 0.0;  // 0: 0.9 of the stability limit
    int snapshots = 10;
};

struct PhaseChangeResult {
    std::vector<double> centres;                          // m
    std::vector<double> times, front, leftFlux;           // s, m (melted thickness), W/m^2 into the slab at x = 0
    std::vector<std::vector<double>> temperature, liquidFraction;  // per snapshot, per cell
    double heatIn = 0.0;          // J/m^2 through the faces
    double sensibleChange = 0.0, latentChange = 0.0;  // J/m^2
    double balanceResidual = 0.0; // heatIn - (sensible + latent)
    double timeStep = 0.0, stabilityLimit = 0.0;
    int steps = 0;
    // Entropy (REQ-CHAT-20261126-022, RES-ACC-013), J/(m^2 K): the slab's change from its
    // states (exactly: rho c ln(T/Tm) sensible, rho L f / Tm latent), the inflow with the heat
    // through the faces (q / T at the face: a fixed-temperature face's value, a flux face's
    // cell), and the production of the conduction between cell centres and from a
    // fixed-temperature face to its cell (q (1/T_cold - 1/T_hot), never negative). The change
    // minus inflow and production is the time discretization's error.
    double entropyChange = 0.0, entropyIn = 0.0, entropyProduction = 0.0;
};

Result<PhaseChangeResult> solvePhaseChange(const PhaseChangeProblem& problem, const PhaseChangeSettings& settings,
                                           const std::function<bool()>& cancelled = {});

// lambda of the one-phase Stefan problem for a Stefan number St.
double stefanLambda(double stefanNumber);

}  // namespace resonant::thermal
