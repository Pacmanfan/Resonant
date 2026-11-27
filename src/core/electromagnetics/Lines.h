#pragma once

// Transmission lines and waveguides: closed-form models under stated
// assumptions (TEM or quasi-TEM lines, ideal uniform guides with perfectly
// conducting walls unless a conductor loss is asked for).
#include "electromagnetics/Conventions.h"

#include <string>
#include <vector>

namespace resonant::em {

// --- transmission lines ---------------------------------------------------------------

struct LineParameters {
    double characteristicImpedance = 0.0;  // ohm
    double effectivePermittivity = 1.0;
    double phaseVelocity = 0.0;            // m/s
    std::string model;                     // the formula used and its range
};

// Coaxial line (TEM): Z0 = eta0 / (2 pi sqrt(er)) ln(b / a). Exact.
LineParameters coaxialLine(double innerRadius, double outerRadius, double relativePermittivity);
// Microstrip (quasi-TEM, zero strip thickness, no dispersion): Hammerstad
// and Jensen (1980), stated accuracy about 0.2 % for 0.01 <= w/h <= 100, er <= 128.
LineParameters microstripLine(double width, double height, double relativePermittivity);
// Symmetric stripline, zero thickness, centred: exact conformal mapping
// Z0 = eta0 / (4 sqrt(er)) K(k) / K(k'), k = sech(pi w / (2 b)).
LineParameters striplineLine(double width, double groundSpacing, double relativePermittivity);
// Coplanar waveguide on an infinitely thick substrate, zero thickness:
// exact conformal mapping, er_eff = (er + 1) / 2, Z0 = eta0 / (4 sqrt(er_eff)) K(k') / K(k), k = s / (s + 2 g).
LineParameters coplanarLine(double stripWidth, double gap, double relativePermittivity);

// Complete elliptic integral of the first kind K(k) (arithmetic-geometric mean).
double ellipticK(double k);

// Ideal line: input impedance of a load seen through length l at wavenumber beta (lossless or with attenuation alpha).
Complex inputImpedance(Complex load, double characteristicImpedance, double beta, double length, double alpha = 0.0);
// Quarter-wave transformer impedance sqrt(Z0 RL) for a real load.
double quarterWaveTransformer(double z0, double loadResistance);

// Lumped L-network matching a load to a real source impedance at one
// frequency: series and shunt reactances of both solutions (when they exist).
struct LMatch {
    double seriesReactance = 0.0;  // ohm, positive inductive
    double shuntSusceptance = 0.0; // S, positive capacitive
    bool shuntAtLoad = true;       // the shunt element sits across the load
};
std::vector<LMatch> lNetworkMatches(Complex load, double sourceResistance);

// Conductor and dielectric attenuation of a coaxial line, Np/m.
double coaxialConductorLoss(double innerRadius, double outerRadius, double relativePermittivity, double conductivity, double frequencyHz);
double dielectricLoss(double relativePermittivity, double lossTangent, double frequencyHz);  // TEM: k tan(delta) / 2

// --- waveguides ------------------------------------------------------------------------

enum class ModeKind { TE, TM, TEM };

struct GuideMode {
    ModeKind kind = ModeKind::TE;
    int m = 0, n = 0;
    double cutoffHz = 0.0;
    std::string label() const;
};

// Rectangular guide a x b (a >= b), filled with er, mu_r: f_c = c/(2 sqrt(er mur)) sqrt((m/a)^2 + (n/b)^2).
// TE: m, n >= 0 not both zero; TM: m, n >= 1. The lowest `count`, ascending.
std::vector<GuideMode> rectangularGuideModes(double a, double b, int count, double relativePermittivity = 1.0, double relativePermeability = 1.0);
// Circular guide of radius a: TE_nm from zeros of J_n', TM_nm from zeros of J_n.
std::vector<GuideMode> circularGuideModes(double radius, int count, double relativePermittivity = 1.0);
// Parallel-plate guide of spacing d: TEM, TE_n and TM_n at n c / (2 d sqrt(er)).
std::vector<GuideMode> parallelPlateModes(double spacing, int count, double relativePermittivity = 1.0);
// The m-th positive zero of J_n (derivative = false) or J_n' (true).
double besselZero(int n, int m, bool derivative);

// Propagation of a mode at a frequency above cutoff.
struct ModePropagation {
    bool propagating = false;
    double beta = 0.0;              // rad/m (0 below cutoff)
    double attenuation = 0.0;       // Np/m below cutoff (evanescent decay)
    double guideWavelength = 0.0;   // m
    double waveImpedance = 0.0;     // ohm: eta / sqrt(1 - (fc/f)^2) for TE, eta sqrt(1 - (fc/f)^2) for TM
    double phaseVelocity = 0.0, groupVelocity = 0.0;
};
ModePropagation propagate(const GuideMode& mode, double frequencyHz, double relativePermittivity = 1.0, double relativePermeability = 1.0);

// Rectangular cavity a x b x d: resonances TE_mnp and TM_mnp, ascending.
struct CavityMode {
    ModeKind kind = ModeKind::TE;
    int m = 0, n = 0, p = 0;
    double frequencyHz = 0.0;
};
std::vector<CavityMode> rectangularCavityModes(double a, double b, double d, int count, double relativePermittivity = 1.0);

// Fields of the TE10 mode of a rectangular guide at (x, z), x across the wide side from 0 to a, peak E0:
// Ey = E0 sin(pi x / a) e^{-j beta z}, Hx = -(beta / omega mu) Ey, Hz = (j pi / (omega mu a)) E0 cos(pi x / a) e^{-j beta z}.
struct TE10Field {
    Complex ey, hx, hz;
};
TE10Field te10Field(double a, double x, double z, double frequencyHz, Complex e0 = Complex(1.0, 0.0));
// Power carried by TE10: |E0|^2 a b beta / (4 omega mu).
double te10Power(double a, double b, double frequencyHz, double e0);

}  // namespace resonant::em
