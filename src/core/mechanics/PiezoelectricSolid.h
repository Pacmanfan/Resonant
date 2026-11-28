#pragma once

// Piezoelectric continua: coupled electromechanical 20-node elements on layered
// bodies (REQ-CHAT-20261128-005, RES-CONV-002, ADR-000233).
//
// The stress-charge constitutive relations
//     T = c^E S - e^T E,    D = e S + eps^S E,    E = -grad phi,
// with anisotropic c^E, e and eps^S in the material axes (3 the poling axis, along
// the layer normal or against it), rotated into world axes element by element.
// Each node of a piezoelectric layer carries the electric potential besides its
// three displacements. The coupled equations
//     [K_uu   K_uphi ] [u  ]   [F]
//     [K_phiu -K_phiphi] [phi] = [Q]
// with K_uphi = int B^T e^T grad N, K_phiphi = int grad N^T eps^S grad N, and Q the
// nodal charges (zero inside the dielectric).
//
// Electrodes are equipotential faces (or segments of faces) of piezoelectric
// layers, each wired to a terminal:
// - ground (0 V);
// - driven (a given voltage);
// - floating (its total charge zero: open circuit);
// - a resistive load to ground. In the harmonic analysis I = j omega Q = V / R; in
//   a static analysis a load discharges and is grounded.
//
// Analyses:
// - static: the converse and direct effects;
// - modal, short or open circuit: the interior and floating potentials condensed
//   (K_uu + K_uphi K_phiphi^-1 K_phiu), so the existing eigensolver applies;
// - harmonic: base excitation and applied loads, a structural loss factor, loads
//   on the terminals, with an energy balance from independent terms: the base
//   work against the damping and electrical powers.
//
// The lumped single-mode cantilever (mechanics/PiezoelectricBeam) is kept as the
// reference model. Not modelled: dielectric and piezoelectric losses, electrode
// thickness and stiffness (electrodes are surfaces), nonlinear and hysteretic
// (ferroelectric) behaviour, depolarization.

#include "mechanics/Analyses.h"
#include "mechanics/LayeredSolid.h"

#include <Eigen/Core>

#include <complex>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace resonant::mechanics {

struct PiezoelectricConstants {
    std::string name, provenance;
    Eigen::Matrix<double, 6, 6> cE = Eigen::Matrix<double, 6, 6>::Zero();  // Pa, Voigt xx yy zz xy yz zx (engineering shear)
    Eigen::Matrix<double, 3, 6> e = Eigen::Matrix<double, 3, 6>::Zero();   // C/m^2
    Eigen::Matrix3d epsS = Eigen::Matrix3d::Zero();                         // F/m
    double density = 0.0;  // kg/m^3
    Status validate() const;
    // The strain-charge form: d = e s^E, eps^T = eps^S + d e^T.
    Eigen::Matrix<double, 3, 6> d() const;
    Eigen::Matrix3d epsT() const;
    // From the strain-charge data: c^E = (s^E)^-1, e = d c^E, eps^S = eps^T - d c^E d^T.
    static Result<PiezoelectricConstants> fromStrainCharge(const Eigen::Matrix<double, 6, 6>& sE, const Eigen::Matrix<double, 3, 6>& d,
                                                           const Eigen::Matrix3d& epsT, double density, std::string name, std::string provenance);
    // The commonly tabulated PZT-5A stress-charge constants (Berlincourt): batches differ by 10-20 %.
    static PiezoelectricConstants pzt5a();
};

struct PiezoLayer3D {
    int layer = 0;
    PiezoelectricConstants constants;
    int poling = +1;  // +1 along the surface normal (outwards), -1 against it
};

struct Electrode {
    std::string name;
    int layer = 0;
    bool outerFace = true;               // the layer's outer face, else its inner face
    std::optional<ParameterBox> region;  // a segment of the face ((u, v) box); default the whole face
    std::string terminal = "ground";
};

struct Terminal {
    enum class Kind { Driven, Floating, Load };
    std::string name;
    Kind kind = Kind::Floating;
    double voltage = 0.0;     // Driven: V (static value, or harmonic amplitude)
    double resistance = 0.0;  // Load: ohm
};

struct PiezoelectricBody {
    LayeredBody body;                // geometry, elastic layers, interfaces
    std::vector<PiezoLayer3D> piezo; // which layers are piezoelectric
    std::vector<Electrode> electrodes;
    std::vector<Terminal> terminals; // "ground" is implicit
    Status validate() const;
};

struct PiezoTerminalResult {
    std::complex<double> voltage, charge, current;  // static: real; harmonic: amplitudes (current = j omega charge)
};

struct PiezoStaticResult {
    LayeredMesh mesh;
    Eigen::VectorXd displacement;   // full mechanical dofs
    std::vector<double> potential;  // per node (0 for nodes outside the piezoelectric layers)
    std::map<std::string, PiezoTerminalResult> terminals;
    double maxDisplacement = 0.0;
    double storedEnergy = 0.0;      // J: the electric enthalpy's elastic and dielectric parts, 1/2 (u^T K u + phi^T K_phiphi phi)
    std::string fidelity;
    std::vector<std::string> assumptions;
};
Result<PiezoStaticResult> solvePiezoStatic(const PiezoelectricBody& body, const LayeredSupports& supports, const LayeredLoads& loads);

struct PiezoModalResult {
    LayeredMesh mesh;
    std::vector<double> frequencies;  // Hz
    std::string condition;            // short circuit / open circuit
    std::string fidelity;
    std::vector<std::string> assumptions;
};
// Load terminals are shorted (short circuit) or left open (open circuit); driven
// terminals are held at 0 V; floating terminals stay floating.
Result<PiezoModalResult> solvePiezoModal(const PiezoelectricBody& body, const LayeredSupports& supports, int count, bool openCircuit);

struct PiezoHarmonicSettings {
    double frequency = 0.0;                    // Hz
    Vec3 baseAcceleration = Vec3::Zero();      // m/s^2 amplitude of the supports' motion (relative coordinates)
    LayeredLoads loads;                        // further applied loads (amplitudes, in phase)
    double lossFactor = 0.0;                   // structural loss factor of every layer (added to the materials' own)
};

struct PiezoHarmonicResult {
    LayeredMesh mesh;
    Eigen::VectorXcd displacement;  // full mechanical dofs, amplitudes relative to the base
    std::map<std::string, PiezoTerminalResult> terminals;
    double inputPower = 0.0;        // W: the base excitation's and applied loads' work, and the driven terminals' supply
    double dampingPower = 0.0;      // W in the structural loss
    double loadPower = 0.0;         // W in the terminals' resistive loads
    double residual() const { return inputPower - dampingPower - loadPower; }
    std::string fidelity;
    std::vector<std::string> assumptions;
};
Result<PiezoHarmonicResult> solvePiezoHarmonic(const PiezoelectricBody& body, const LayeredSupports& supports, const PiezoHarmonicSettings& settings);

}  // namespace resonant::mechanics
