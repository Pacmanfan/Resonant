#pragma once

// A periodic two-layer electromagnetic unit cell at normal incidence (a 1D
// photonic crystal, RES-FUNC-010): the exact Bloch dispersion of the infinite
// lattice, its band gaps, the attenuation inside a gap, the finite stack as a
// layered-media problem, and the long-wavelength (homogenized) description
// with its validity. Lossless, non-magnetic, isotropic layers.
//   cos(K a) = cos(k1 d1) cos(k2 d2) - 1/2 (n1/n2 + n2/n1) sin(k1 d1) sin(k2 d2)

#include "electromagnetics/LayeredMedia.h"
#include "foundation/Result.h"
#include "foundation/Types.h"

#include <string>
#include <vector>

namespace resonant::em {

inline constexpr const char* kPeriodicLayersModelId = "em.periodic.layers.bloch";
inline constexpr const char* kPeriodicLayersModelVersion = "1";

struct PeriodicCell {
    double thickness1 = 0.0, permittivity1 = 1.0;  // layer 1: m, relative permittivity
    double thickness2 = 0.0, permittivity2 = 1.0;  // layer 2
    double period() const { return thickness1 + thickness2; }
    Status validate() const;
};

struct BlochPoint {
    double frequencyHz = 0.0;
    double dispersion = 0.0;  // the right-hand side: cos(K a) when |.| <= 1
    Complex wavenumber;       // K: real part in [0, pi/a]; imaginary part >= 0 inside a gap (Np/m)
    bool inGap = false;
};
BlochPoint blochPoint(const PeriodicCell& cell, double frequencyHz);

struct BandGap {
    double lowerHz = 0.0, upperHz = 0.0;
    double centerHz = 0.0;
    double relativeWidth = 0.0;     // (upper - lower) / center
    double peakAttenuation = 0.0;   // largest Im(K) a in the gap, Np per cell
    bool atZoneEdge = true;         // K = pi / a (or K = 0: at the zone centre)
};
// The gaps below fmax, edges refined by bisection to a relative 1e-10.
std::vector<BandGap> bandGaps(const PeriodicCell& cell, double fmaxHz, int samples = 4000);

struct EffectiveMedium {
    double index = 0.0;                    // the long-wavelength limit of K c / omega
    double permittivityParallel = 0.0;     // quasi-static, E parallel to the layers: (e1 d1 + e2 d2) / a
    double permittivityPerpendicular = 0.0;  // quasi-static, E across the layers: a / (d1/e1 + d2/e2)
    double validBelowHz = 0.0;             // a / lambda_eff < 0.1
    std::string model;
};
// Homogenized values: derived from the cell, never entered; valid only below validBelowHz.
EffectiveMedium effectiveMedium(const PeriodicCell& cell);
// The Bloch effective index K c / omega at a frequency in the first band (fails in a gap or above it).
Result<double> blochEffectiveIndex(const PeriodicCell& cell, double frequencyHz);

// The finite stack of `periods` cells between vacuum half-spaces.
LayeredProblem periodicStack(const PeriodicCell& cell, int periods, double frequencyHz);

}  // namespace resonant::em
