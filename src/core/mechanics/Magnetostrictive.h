#pragma once

// Magnetostrictive conversion: a rod in a coil, as an actuator and as a harvester
// (REQ-CHAT-20261128-005, RES-CONV-004, ADR-000233).
//
// LINEAR (piezomagnetic) constitutive relations about a magnetic bias point H0:
//     S = s^H T + d33 H,    B = d33 T + mu^T H,
// k33^2 = d33^2 / (s^H mu^T), mu^S = mu^T (1 - k^2), s^B = s^H (1 - k^2). This is a
// small-signal approximation valid for dynamic fields well below the bias. Not
// modelled: hysteresis, saturation, the Delta-E effect beyond the stated
// compliance, eddy currents, temperature dependence. A nonlinear constitutive
// model is not implemented, and nothing here claims to be one.
//
// The rod (length L, area A) in a long coil (N turns over L, H = N i / L), with a
// tip mass, as one degree of freedom (the tip elongation x):
//     m x'' + c x' + k^H x - Theta i = F,          k^H = A / (s^H L), Theta = A d33 N / (s^H L)
//     -Theta x' - L_c i' = (R_c + R_load) i,        L_c = mu^S N^2 A / L
// with m = M + rho A L / 3 (Rayleigh) and the base excitation's forcing mass
// M + rho A L / 2.

#include "foundation/Result.h"

#include <complex>
#include <string>
#include <vector>

namespace resonant::mechanics {

struct PiezomagneticConstants {
    std::string name, provenance;
    double complianceH = 0.0;   // s^H_33, 1/Pa
    double d33 = 0.0;           // m/A
    double permeabilityT = 0.0; // mu^T_33, H/m
    double density = 0.0;       // kg/m^3
    double biasField = 0.0;     // A/m, the bias the constants hold about
    double linearFieldLimit = 0.0;  // A/m, the dynamic field amplitude beyond which the linear model is not claimed
    double couplingSquared() const;      // k33^2
    double permeabilityS() const;        // mu^S
    double complianceB() const;          // s^B
    Status validate() const;
    // Illustrative Terfenol-D values (literature order of magnitude, not a datasheet).
    static PiezomagneticConstants terfenolD();
};

struct MagnetostrictiveRod {
    PiezomagneticConstants material;
    double length = 0.0, area = 0.0;  // m, m^2
    int turns = 0;
    double coilResistance = 0.0;      // ohm
    double tipMass = 0.0;             // kg
    double dampingRatio = 0.01;       // viscous, of the open-coil mode
    Status validate() const;
};

struct MagnetostrictiveModel {
    double mass = 0.0, forcingMass = 0.0, stiffnessH = 0.0, damping = 0.0;
    double theta = 0.0;          // N/A
    double inductance = 0.0;     // H
    double openCoilHz = 0.0;     // no current (H = 0)
    double shortedIdealHz = 0.0; // an ideal shorted coil (R = 0: B constant)
    double couplingSquared = 0.0;
    std::vector<std::string> assumptions;
};
Result<MagnetostrictiveModel> magnetostrictiveModel(const MagnetostrictiveRod& rod);

// The actuator: a DC current's free elongation and blocked force.
struct ActuatorResponse {
    double field = 0.0;          // A/m
    double freeElongation = 0.0; // m
    double blockedForce = 0.0;   // N
    bool withinLinearRange = true;
};
ActuatorResponse magnetostrictiveActuator(const MagnetostrictiveRod& rod, double current);

// The harvester: steady response to a base acceleration amplitude.
struct MagnetostrictiveHarvesterPoint {
    double frequency = 0.0, load = 0.0;
    std::complex<double> elongation, current;
    double loadPower = 0.0, coilLoss = 0.0, dampingLoss = 0.0, inputPower = 0.0;
    double dynamicField = 0.0;  // A/m, amplitude
    bool withinLinearRange = true;
    double residual() const { return inputPower - loadPower - coilLoss - dampingLoss; }
};
MagnetostrictiveHarvesterPoint magnetostrictiveHarvester(const MagnetostrictiveRod& rod, const MagnetostrictiveModel& model, double frequencyHz,
                                                         double loadOhm, double acceleration);

}  // namespace resonant::mechanics
