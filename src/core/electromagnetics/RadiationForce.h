#pragma once

// The radiation force on a layered body (REQ-CHAT-20261126-022, RES-ACC-009, ADR-000226):
// a plane wave at normal incidence on layers between two vacuum half-spaces. From the
// solution's own fields, two ways:
//   surfaces  the time-averaged vacuum Maxwell stress (momentum flux of +z momentum along +z)
//             T(z) = (eps0 |E|^2 + mu0 |H|^2) / 4 at z = 0- and z = d+, F = T(0-) - T(d+);
//   volume    the time-averaged Lorentz force on the induced (polarization and conduction)
//             currents, f = (1/2) Re(J conj(mu0 H)), J = j w eps0 (eps_r - 1) E, integrated over
//             the layers by Simpson's rule;
// against (1 + R - T) I / c from the fluxes. With vacuum on both sides the total force is the
// same for every momentum formulation inside matter (Abraham, Minkowski); other half-spaces,
// and magnetic layers (whose magnetization current the volume method leaves out), are
// refused with that reason. Also the absorbed power, locally (the integral of the absorbed
// power density) and globally (the Poynting fluxes at the faces).

#include "electromagnetics/LayeredMedia.h"
#include "foundation/Result.h"

#include <string>
#include <vector>

namespace resonant::em {

struct RadiationForce {
    double intensity = 0.0;  // W/m^2
    double reflectance = 0.0, transmittance = 0.0, absorptance = 0.0;
    double fluxIncidentSide = 0.0, fluxExitSide = 0.0;  // momentum flux T(0-), T(d+), N/m^2
    double pressureSurfaces = 0.0;   // N/m^2, along +z
    double pressureVolume = 0.0;     // N/m^2
    double pressureFluxes = 0.0;     // (1 + R - T) I / c
    double absorbedLocal = 0.0, absorbedGlobal = 0.0;  // W/m^2
    int points = 0;                  // integration points over the layers
    std::vector<double> z, forceDensity, absorbedDensity;  // N/m^3, W/m^3
    std::string formulation = "vacuum Maxwell stress at the control surfaces; Lorentz force on the induced currents";
};

// `points`: the integration points over the whole stack (odd per layer, at least 9 each).
Result<RadiationForce> radiationForce(const LayeredSolution& solution, int points = 2001);

}  // namespace resonant::em
