#pragma once

// Helmholtz resonators (RES-RSN-006): a cylindrical cavity of volume V with a
// circular neck of radius a and length L.
//
// Lumped model (valid while ka, kL and the cavity's size times k are small):
//   inertance M = rho L_eff / S, compliance C = V / (rho c^2), f0 = 1 / (2 pi sqrt(M C))
//   L_eff = L + outer end correction (flanged 8a/3pi = 0.8488 a; unflanged 0.6133 a)
//              + inner end correction 0.8488 a (1 - 1.25 a / R) (Ingard)
//   losses, each its own mechanism:
//     radiation R_rad = rho c k^2 / (2 pi) (flanged; unflanged: / (4 pi))
//     viscous boundary layer of the neck R_visc = (2 L_eff / (pi a^3)) sqrt(w rho mu / 2)
//   Q_i = w0 M / R_i, 1/Q = sum 1/Q_i
// Plane-wave stepped-duct model (the check): the exact transfer matrices of the
// neck and the cavity as uniform ducts, rigid back, loaded at the mouth by the
// baffled-piston radiation impedance (whose reactance is the outer end
// correction); the inner end correction is not part of a plane-wave model.

#include "foundation/Result.h"
#include "foundation/Types.h"
#include "oscillators/LumpedNetwork.h"

#include <string>
#include <vector>

namespace resonant {
class Geometry;
}

namespace resonant::manipulators {

inline constexpr const char* kHelmholtzModelId = "acoustic.helmholtz.lumped";
inline constexpr const char* kHelmholtzDuctModelId = "acoustic.helmholtz.stepped-duct";
inline constexpr const char* kHelmholtzModelVersion = "1";

struct HelmholtzSpec {
    double cavityRadius = 0.0, cavityLength = 0.0;  // m
    double neckRadius = 0.0, neckLength = 0.0;      // m
    bool flanged = true;            // the outer end in a baffle
    bool innerCorrection = true;    // include the neck-to-cavity end correction
    bool radiationLoss = true, viscousLoss = true;
    double density = 1.204, soundSpeed = 343.0, viscosity = 1.81e-5;  // air at 20 degC
    Status validate() const;
    double volume() const;
    double neckArea() const;
};

HelmholtzSpec helmholtzSpecOf(const Geometry& geometry);

struct HelmholtzResult {
    double frequencyHz = 0.0;
    double volume = 0.0, neckArea = 0.0;
    double outerCorrection = 0.0, innerCorrection = 0.0, effectiveLength = 0.0;
    double inertance = 0.0, compliance = 0.0;
    double radiationResistance = 0.0, viscousResistance = 0.0;  // at f0, Pa s/m^3
    double qRadiation = 0.0, qViscous = 0.0, quality = 0.0;
    double bandwidthHz = 0.0;
    double neckKa = 0.0, neckKL = 0.0, cavityKSize = 0.0;
    double pressureGainAtResonance = 0.0;  // |p_cavity / p_incident|, flanged mouth (blocked pressure 2 p_i)
    double storedEnergyPerPa2 = 0.0;       // J per (incident Pa)^2 at resonance, time-averaged
    std::vector<std::string> warnings;
};
Result<HelmholtzResult> analyzeHelmholtz(const HelmholtzSpec& spec);

// The lumped series impedance seen by the outside at the mouth (Pa s/m^3).
Complex helmholtzImpedance(const HelmholtzSpec& spec, double frequencyHz);
// |p_cavity / p_incident| of the lumped model.
double helmholtzPressureGain(const HelmholtzSpec& spec, double frequencyHz);

// The plane-wave stepped-duct model: the input impedance at the mouth looking into the resonator.
Complex steppedDuctImpedance(const HelmholtzSpec& spec, double frequencyHz);
// Its series resonance (Im(Z_in + Z_rad) = 0) near the lumped one, by bisection, and the Q of its
// radiation-loaded volume-velocity response (half-power width).
struct DuctResonance {
    double frequencyHz = 0.0;
    double quality = 0.0;
};
Result<DuctResonance> steppedDuctResonance(const HelmholtzSpec& spec);

// Two coupled Helmholtz resonators: resonator 1 opens to the outside through its neck (radiation and
// viscous loss), and a connecting neck (radius, length) joins its cavity to the closed resonator 2.
// Coordinates: q1, the volume displaced out through neck 1; qc, through the connector into cavity 2.
//   M = diag(rho L1_eff / S1, rho Lc_eff / Sc),  potential (q1 + qc)^2 / 2 C1 + qc^2 / 2 C2
// The connector's end corrections are the inner one at both ends. Resistances are evaluated at
// resonator 1's lumped frequency (their frequency dependence is not part of the network).
Result<oscillators::Network> coupledHelmholtz(const HelmholtzSpec& open, const HelmholtzSpec& closed, double connectorRadius, double connectorLength);

}  // namespace resonant::manipulators
