#pragma once

// A plane wave at normal incidence on a stack of homogeneous layers
// (REQ-CHAT-20261126-016, RES-CEME-011, docs/electromagnetics/LAYERED_MEDIA.md).
//
// Exact transfer-matrix solution of Maxwell's equations for a stratified
// medium: in each layer E(z) = A e^{-jkz} + B e^{+jkz}, with tangential E and
// H continuous at every interface. Time dependence e^{+j omega t}, peak
// amplitudes; a lossy medium has eps_r = eps' - j eps''_eff with
// eps''_eff = eps' tan(delta) + sigma / (omega eps0). The wave arrives from
// the half-space before the first layer, travelling along +z, and leaves
// into the half-space after the last.
//
// Outputs: the reflection and transmission coefficients, the reflected,
// transmitted and absorbed fractions of the incident power, the field and
// the time-averaged Poynting flux at any depth, and the power absorbed in
// any interval [z1, z2] as S(z1) - S(z2) (exact), which equals the integral
// of the absorbed power density 1/2 omega eps0 eps''_eff |E|^2.
#include "foundation/Result.h"

#include <complex>
#include <string>
#include <vector>

namespace resonant::em {

using Complex = std::complex<double>;

struct MediumLayer {
    std::string name;
    double thickness = 0.0;           // m (ignored for the two half-spaces)
    double permittivityReal = 1.0;    // eps'
    double lossTangent = 0.0;         // tan(delta)
    double conductivity = 0.0;        // S/m
    double permeability = 1.0;        // mu_r (real)

    Complex relativePermittivity(double omega) const;  // eps' - j eps''_eff
    double lossFactor(double omega) const;              // eps''_eff
};

struct LayeredProblem {
    double frequencyHz = 1e9;
    double incidentIntensity = 1.0;   // W/m^2, time-averaged, of the incident wave
    MediumLayer incidence;            // the half-space z < 0 (lossless)
    std::vector<MediumLayer> layers;  // from z = 0 along +z
    MediumLayer exit;                 // the half-space after the last layer (lossless)

    Status validate() const;
    double totalThickness() const;
};

class LayeredSolution {
public:
    static Result<LayeredSolution> solve(const LayeredProblem& problem);

    const LayeredProblem& problem() const { return problem_; }
    Complex reflection() const { return r_; }     // E_reflected / E_incident at z = 0
    Complex transmission() const { return t_; }   // E_transmitted at the exit interface / E_incident
    double reflectance() const;                   // |r|^2
    double transmittance() const;                 // transmitted / incident power
    double absorptance() const;                   // 1 - R - T (exact, from the fluxes)

    // Tangential fields (V/m, A/m, peak) and the time-averaged Poynting flux
    // along +z (W/m^2) at depth z (0 <= z <= total thickness; outside the stack
    // the incident-side or exit-side fields).
    Complex electricField(double z) const;
    Complex magneticField(double z) const;
    double poyntingFlux(double z) const;
    // Power absorbed per unit area between z1 and z2 (W/m^2).
    double absorbedPower(double z1, double z2) const { return poyntingFlux(z1) - poyntingFlux(z2); }
    // The absorbed power density at z (W/m^3).
    double absorbedPowerDensity(double z) const;
    // The layer containing z (-1 before the stack, layers.size() after).
    int layerAt(double z) const;

private:
    LayeredProblem problem_;
    double omega_ = 0.0;
    Complex r_, t_;
    Complex incidentAmplitude_;
    std::vector<double> starts_;              // z of each layer's first face
    std::vector<Complex> k_, eta_;            // per layer
    std::vector<Complex> endE_, endH_;        // fields at each layer's last face
    Complex etaIn_, etaOut_, kIn_, kOut_;
};

inline constexpr const char* kLayeredSolverId = "em.layered.transfer-matrix";
inline constexpr const char* kLayeredSolverVersion = "1.0";

}  // namespace resonant::em
