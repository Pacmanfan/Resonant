#pragma once

// Split-ring resonator arrays as a quasi-static circuit (RES-META-008): an
// approximation, not a full-wave calculation (no periodic plane-wave port
// exists for the Yee solver).
//   L = mu0 r (ln(8 r / r_w) - 2),  r_w = (w + t) / 4   (a circular loop of an equivalent wire)
//   C = eps0 eps_eff (w t / g + (w + t))               (the gap and its fringing; eps_eff = (1 + eps_s) / 2)
//   R = 2 pi r / (sigma delta 2 (w + t))                (skin effect on the strip's perimeter)
//   w0 = 1 / sqrt(L C),  F = pi r^2 / a^2  (Pendry et al., IEEE MTT 47, 2075, 1999)
//   1/Q = R / (w0 L) + tan(delta_s) eps_s / (1 + eps_s)  (conductor; the substrate's share of the gap field)
//   Gamma = w0 / Q
//   mu_eff(w) = 1 - F w^2 / (w^2 - w0^2 - j w Gamma)   (e^{+jwt}: Im mu <= 0)
// Re mu < 0 between w0 and w0 / sqrt(1 - F) when the loss is small. Valid for
// rings much smaller than the wavelength (a / lambda < 0.1 at w0).

#include "foundation/Result.h"
#include "foundation/Types.h"

#include <vector>

namespace resonant::em {

inline constexpr const char* kSplitRingModelId = "em.split-ring.circuit";

struct SplitRingSpec {
    double radius = 0.0;           // mean ring radius, m
    double width = 0.0;            // strip width, m
    double gap = 0.0;              // m
    double metalThickness = 0.0;   // m
    double latticeSpacing = 0.0;   // a, m (square array, rings stacked at the same period)
    double substratePermittivity = 1.0;
    double substrateLossTangent = 0.0;
    double conductivity = 5.8e7;   // S/m
    Status validate() const;
};

struct SplitRingModel {
    double inductance = 0.0, capacitance = 0.0, resistance = 0.0;  // at the resonance (skin effect)
    double resonanceHz = 0.0;
    double fillingFactor = 0.0;
    double dampingRate = 0.0;          // Gamma, rad/s
    double quality = 0.0;              // 1/Q = 1/Q_conductor + 1/Q_dielectric
    double qConductor = 0.0, qDielectric = 0.0;  // +inf without that loss
    double magneticPlasmaHz = 0.0;     // f0 / sqrt(1 - F): Re mu = 0 again (lossless)
    double cellOverWavelength = 0.0;   // a / lambda0 at f0
};
Result<SplitRingModel> splitRingModel(const SplitRingSpec& spec);
Complex splitRingPermeability(const SplitRingModel& model, double frequencyHz);
// Causality: Re mu at each frequency from Im mu by the Kramers-Kronig relation
//   Re mu(w) - mu_inf = (2 / pi) P int_0^inf w' chi''(w') / (w'^2 - w^2) dw',  chi'' = -Im mu,  mu_inf = 1 - F
// (numerical quadrature with the singularity subtracted); the largest |difference| from the model.
double splitRingKramersKronigResidual(const SplitRingModel& model, const std::vector<double>& frequenciesHz);

}  // namespace resonant::em
