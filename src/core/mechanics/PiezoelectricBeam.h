#pragma once

// A piezoelectric cantilever: a unimorph or bimorph beam with a resistive load
// (REQ-CHAT-20261127-001, RES-FAM-012). A lumped single-mode model of the first
// bending mode, Rayleigh's method with the static tip-load deflection shape
// phi(x) = (3 x^2 L - x^3) / (2 L^3):
//   m x'' + c x' + k x + theta V = -m_f a_base      (the tip deflection x)
//   C V' + V / R = theta x'                          (the electrode circuit)
// with k = 3 EI / L^3, m = 33/140 m' L + M_tip, m_f = 3/8 m' L + M_tip,
// theta_i = e31_i p_i b (z_i - z_n) [phi'(L_e1) - phi'(L_e0)] (e31 = d31 E_p, the poling
// sign p_i, the layer's distance from the neutral axis, the electrode span), and
// C_i = eps33^S b (L_e1 - L_e0) / h_i (eps33^S = eps33^T - d31^2 E_p). Two layers are
// connected in series or in parallel. The beam stiffness and mass include every
// layer (electrodes, adhesion, encapsulation): the coating is never neglected.
//
// The single-mode Rayleigh estimate of the short-circuit frequency is 1.5 % above the
// exact Euler-Bernoulli value of a uniform beam without a tip mass. Assumptions: the
// first mode only, Euler-Bernoulli bending, linear piezoelectricity, perfect bonding,
// electrodes that are equipotential, a purely resistive load, viscous modal damping.

#include "foundation/Result.h"
#include "mechanics/Laminate.h"

#include <complex>
#include <string>
#include <vector>

namespace resonant::mechanics {

struct PiezoLayer {
    size_t ply = 0;
    double d31 = 0.0;       // C/N, along the beam axis (in-plane orientation applied by the caller)
    double eps33T = 0.0;    // relative permittivity at constant stress
    int poling = +1;        // +1 along the stack axis, -1 against it
    double start = 0.0, end = 1.0;  // the electrode's span along the length, normalized from the root
};

struct HarvesterSpec {
    std::vector<Ply> plies;  // from the bottom; the beam axis is the laminate x-axis
    double width = 0.0, length = 0.0;  // m
    double tipMass = 0.0;    // kg
    std::vector<PiezoLayer> piezo;
    std::string connection = "single";  // single, series, parallel
    double dampingRatio = -1.0;         // < 0: from the plies' loss factors (zeta = eta / 2)
};

struct HarvesterModel {
    double mass = 0.0, forcingMass = 0.0, stiffness = 0.0, damping = 0.0, dampingRatio = 0.0;
    double theta = 0.0;        // N/V
    double capacitance = 0.0;  // F
    double shortCircuitHz = 0.0, openCircuitHz = 0.0;
    double couplingSquared = 0.0;  // theta^2 / (k C)
    double neutralAxis = 0.0;      // from the bottom face, m
    double eulerBernoulliHz = 0.0; // the exact first frequency of the same beam without a tip mass, for comparison
    std::vector<double> layerTheta, layerCapacitance;
    std::string dampingSource;
    std::vector<std::string> assumptions;
};

Result<HarvesterModel> harvesterModel(const HarvesterSpec& spec);

struct HarvesterPoint {
    double frequency = 0.0;  // Hz
    double load = 0.0;       // ohm
    std::complex<double> displacement;  // tip deflection relative to the base, m (amplitude phasor)
    std::complex<double> voltage;       // V
    double electricalPower = 0.0;       // W, time-averaged
    double inputPower = 0.0;            // W delivered by the base excitation
    double dissipatedPower = 0.0;       // W in the mechanical damping
    double efficiency = 0.0;            // electrical / input
};

// The steady harmonic response to a base acceleration of amplitude `acceleration` (m/s^2).
HarvesterPoint harvesterResponse(const HarvesterModel& model, double frequencyHz, double loadOhm, double acceleration);
// The load that maximizes the electrical power at a frequency.
double optimalLoad(const HarvesterModel& model, double frequencyHz);
// The converse effect: the tip deflection under a drive voltage amplitude (no base motion).
std::complex<double> actuatorDisplacement(const HarvesterModel& model, double frequencyHz, double voltage);

}  // namespace resonant::mechanics
