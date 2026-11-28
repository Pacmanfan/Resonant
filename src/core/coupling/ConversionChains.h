#pragma once

// A cross-domain conversion chain (REQ-CHAT-20261128-005, RES-CONV-006, ADR-000233):
// acoustic excitation -> mechanical strain -> piezoelectric conversion -> electrical
// output -> electrical load, every stage with an independently verifiable balance.
//
// The configuration: a plane wave in a duct, normally incident on a piezoelectric
// plate that closes the duct (mechanics/PiezoelectricSolid). The coupled plate is
// solved once for a unit uniform pressure. That gives its surface's volume
// velocity U and the specific impedance Z_s = A / U. The duct's plane-wave relation
// at the plate then gives the surface pressure p = 2 p_i Z_s / (Z_s + rho c) and the
// reflected wave p_r = p - p_i, and the solution scales with p. The stages:
// - acoustic: the incident power p_i^2 A / 2 rho c against the reflected power
//   (from p_r) and the work the pressure does on the plate (from the structural
//   solution);
// - electromechanical: that work against the structural damping and the power at
//   the terminals 1/2 Re(V I*);
// - delivery: the terminals' power against the load's 1/2 |V|^2 / R.
//
// Assumptions: a one-dimensional plane wave (below the duct's first cross mode),
// a rigid duct; the plate's back face radiates nothing (no back cavity or fluid
// beyond it); fluid loading of the plate through the plane-wave impedance only.

#include "conversion/ConversionStage.h"
#include "mechanics/PiezoelectricSolid.h"

#include <complex>

namespace resonant::coupling {

struct DuctPiezoelectricHarvester {
    mechanics::PiezoelectricBody body;       // the plate; its outer face meets the incident wave
    mechanics::LayeredSupports supports;
    std::string loadTerminal = "load";       // the terminal whose resistive load receives the output
    double fluidDensity = 1.204;             // kg/m^3 (air at 20 C)
    double soundSpeed = 343.2;               // m/s
    double incidentPressure = 0.0;           // Pa, amplitude
    double frequency = 0.0;                  // Hz
    double lossFactor = 0.01;                // structural loss factor
};

struct DuctHarvesterResult {
    double area = 0.0;  // m^2, the plate's face (the duct's cross-section)
    std::complex<double> surfaceImpedance;       // Pa s/m
    std::complex<double> surfacePressure;        // Pa
    std::complex<double> reflectionCoefficient;
    double incidentPower = 0.0, reflectedPower = 0.0;
    double absorbedPower = 0.0;                  // the pressure's work on the plate (structural solution)
    double dampingPower = 0.0, terminalPower = 0.0, loadPower = 0.0;
    std::complex<double> voltage;                // at the load terminal
    conversion::ConversionChain chain;
    conversion::ChainReport report;
    std::vector<std::string> assumptions;
};

Result<DuctHarvesterResult> ductPiezoelectricHarvester(const DuctPiezoelectricHarvester& spec);

}  // namespace resonant::coupling
