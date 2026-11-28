#pragma once

// Two-mirror optical resonators in the paraxial approximation (RES-FUNC-007):
// Hermite-Gaussian modes of spherical mirrors (Siegman, Lasers, ch. 19-21;
// Kogelnik and Li, Appl. Opt. 5, 1550, 1966). Scalar, paraxial (waist much
// larger than the wavelength), mirrors aligned on one axis and thin; a lossless
// medium of refractive index n between them. This is not a full
// electromagnetic solution: an unstable resonator has no Gaussian mode and is
// reported as such, and the stability limits g1 g2 = 0 and 1 are marginal.

#include "foundation/Result.h"
#include "foundation/Types.h"

#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace resonant::em {

inline constexpr const char* kOpticalResonatorModelId = "optics.resonator.paraxial";
inline constexpr const char* kOpticalResonatorModelVersion = "1";

struct OpticalResonatorSpec {
    double length = 0.0;               // mirror separation, m
    // Radii of curvature, m, positive for a mirror concave toward the other; infinity = plane.
    double radius1 = std::numeric_limits<double>::infinity();
    double radius2 = std::numeric_limits<double>::infinity();
    double reflectivity1 = 0.99, reflectivity2 = 0.99;  // power reflectivities
    double aperture1 = 0.0, aperture2 = 0.0;            // mirror radii, m; 0 = unlimited
    double wavelength = 1064e-9;       // vacuum wavelength, m
    double refractiveIndex = 1.0;
    double extraRoundTripLoss = 0.0;   // fraction of power lost per round trip besides the mirrors
    Status validate() const;
};

enum class ResonatorStability { Stable, Marginal, Unstable };
std::string_view toString(ResonatorStability stability);

struct OpticalResonatorResult {
    double g1 = 0.0, g2 = 0.0;
    ResonatorStability stability = ResonatorStability::Unstable;
    // The fundamental Gaussian mode (stable resonators only).
    double waist = 0.0;               // 1/e^2 intensity radius, m
    double waistPosition = 0.0;       // from mirror 1 toward mirror 2, m
    double rayleighRange = 0.0;       // m
    double spot1 = 0.0, spot2 = 0.0;  // beam radii on the mirrors, m
    double gouyRoundTrip = 0.0;       // rad: 2 arccos(+-sqrt(g1 g2))
    // Spectrum.
    double frequency = 0.0;           // optical frequency c / lambda, Hz
    double freeSpectralRange = 0.0;   // c / (2 n L), Hz
    double transverseModeSpacing = 0.0;  // Hz, between m + n and m + n + 1
    long longitudinalIndex = 0;       // q of the TEM00 mode nearest the given wavelength
    double nearestTem00Frequency = 0.0;  // Hz
    // Losses.
    double clippingLoss1 = 0.0, clippingLoss2 = 0.0;  // TEM00 power outside each aperture, per bounce
    double roundTripLoss = 0.0;       // fraction of the circulating power lost per round trip
    double finesse = 0.0;
    double linewidth = 0.0;           // full width at half maximum, Hz
    double photonLifetime = 0.0;      // s
    double qualityFactor = 0.0;
    double resonantTransmission = 0.0;  // T1 T2 / (1 - sqrt(R1' R2'))^2 with lossless mirror coatings
    double storedEnergyPerWatt = 0.0;   // J per watt of circulating power (both directions)
    std::vector<std::string> assumptions;
    std::vector<std::string> warnings;
};

Result<OpticalResonatorResult> analyzeOpticalResonator(const OpticalResonatorSpec& spec);

// Frequency of the TEM_mn,q mode, Hz.
double opticalModeFrequency(const OpticalResonatorSpec& spec, const OpticalResonatorResult& result, long q, int m, int n);

// The beam radius at distance z from mirror 1 (stable resonators).
double beamRadiusAt(const OpticalResonatorResult& result, double z);

// The complex beam parameter at mirror 1 travelling toward mirror 2 (1/q = 1/R - j lambda/(n pi w^2)).
Complex beamParameterAtMirror1(const OpticalResonatorSpec& spec, const OpticalResonatorResult& result);

// The beam parameter after one round trip (ABCD: mirror 2, the gap, mirror 1, the gap), for checking self-consistency.
Complex roundTripBeamParameter(const OpticalResonatorSpec& spec, Complex q);

}  // namespace resonant::em
