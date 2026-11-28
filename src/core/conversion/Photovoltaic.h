#pragma once

// Photovoltaic conversion by an equivalent circuit (REQ-CHAT-20261128-005, RES-CONV-005,
// ADR-000233).
//
// The single-diode model
//     I = I_ph - I_0 (exp((V + I R_s) / a) - 1) - (V + I R_s) / R_sh,   a = n k T / q,
// solved by Newton's method and, independently, in closed form with the Lambert W
// function. The photocurrent comes from the spectrum:
//     I_ph = q A integral( EQE(lambda) Phi(lambda) dlambda ),  Phi = E lambda / (h c),
//     EQE = (1 - R) (1 - exp(-alpha w)) eta_collection  below the gap wavelength, 0 above,
// with the band gap's Varshni temperature dependence, and I_0(T) scaled as T^3
// exp(-E_g / n k T). The incident light is accounted for term by term:
// - optics: reflected, transmitted (below-gap light included);
// - absorption: thermalization above the gap; uncollected carriers;
// - the circuit: the voltage deficit I_ph (E_g / q - V_j), the diode, shunt and
//   series losses, and the electrical output.
// Each term is computed independently, so the stage's residual checks the optics
// and the circuit.
//
// What it is not: no semiconductor transport (drift, diffusion, recombination
// kinetics) is solved. The equivalent-circuit parameters are inputs, never derived
// from carrier lifetimes.

#include "conversion/ConversionStage.h"
#include "foundation/Result.h"

#include <array>
#include <limits>
#include <string>
#include <vector>

namespace resonant::conversion {

struct SpectralPoint {
    double wavelength = 0.0;  // m
    double irradiance = 0.0;  // W / (m^2 m), spectral
};

struct Spectrum {
    std::vector<SpectralPoint> points;  // increasing wavelength
    std::string description;
    double total() const;  // W/m^2, trapezoidal
    Status validate() const;
};
// A black body of the given temperature, scaled to `total` W/m^2 over [min, max]:
// a stand-in for a standard solar spectrum, labelled as such.
Spectrum blackBodySpectrum(double temperature, double total, double minWavelength = 280e-9, double maxWavelength = 4000e-9, int points = 2000);

struct PhotovoltaicCell {
    std::string name;
    double area = 0.0;             // m^2
    double bandGap = 0.0;          // eV at the reference temperature
    double varshniAlpha = 4.73e-4; // eV/K (silicon)
    double varshniBeta = 636.0;    // K (silicon)
    std::vector<std::array<double, 2>> absorption;  // (wavelength m, alpha 1/m), increasing; empty: absorptionAboveGap
    double absorptionAboveGap = 1e5;  // 1/m
    double thickness = 0.0;        // m, the absorber
    double reflectance = 0.0;      // front reflectance, spectrally flat
    double collection = 1.0;       // fraction of the generated carriers collected
    double saturationCurrent = 0.0;  // A, I_0 at the reference temperature
    double idealityFactor = 1.0;
    double seriesResistance = 0.0;   // ohm
    double shuntResistance = std::numeric_limits<double>::infinity();  // ohm
    double referenceTemperature = 298.15;  // K
    std::string provenance;
    Status validate() const;
};

struct PhotovoltaicState {
    double temperature = 0.0;        // K
    double bandGap = 0.0;            // eV
    double photocurrent = 0.0;       // A
    double saturationCurrent = 0.0;  // A
    double modifiedThermalVoltage = 0.0;  // a = n k T / q, V
};
PhotovoltaicState cellState(const PhotovoltaicCell& cell, const Spectrum& spectrum, double temperature);

// The current at a terminal voltage: Newton's method, and the closed form.
double cellCurrent(const PhotovoltaicCell& cell, const PhotovoltaicState& state, double voltage);
double cellCurrentLambertW(const PhotovoltaicCell& cell, const PhotovoltaicState& state, double voltage);
// The principal branch W0(x), x >= -1/e; and W0(exp(x)) for large x without overflow.
double lambertW0(double x);
double lambertW0OfExp(double x);

struct OperatingPoint {
    double voltage = 0.0, current = 0.0, power = 0.0;
};

struct PhotovoltaicResult {
    PhotovoltaicState state;
    double shortCircuitCurrent = 0.0, openCircuitVoltage = 0.0;
    OperatingPoint maximumPower;
    double fillFactor = 0.0;
    double efficiency = 0.0;  // the maximum power over the incident power
    std::vector<OperatingPoint> curve;
    // The incident light, term by term (W).
    double incident = 0.0, reflected = 0.0, transmitted = 0.0, absorbed = 0.0;
    double thermalization = 0.0, uncollected = 0.0, voltageDeficit = 0.0;
    double diodeLoss = 0.0, shuntLoss = 0.0, seriesLoss = 0.0;
    double circuitResidual = 0.0;  // I_ph V_j - (diode + shunt + series + output): the circuit's own check
    ConversionStage stage;         // optical to electrical, at the maximum power point
    std::vector<std::string> assumptions;
};
Result<PhotovoltaicResult> solvePhotovoltaic(const PhotovoltaicCell& cell, const Spectrum& spectrum, double temperature, int curvePoints = 101);

}  // namespace resonant::conversion
