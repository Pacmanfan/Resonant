#include "electromagnetics/OpticalResonator.h"

#include "electromagnetics/Conventions.h"

#include <cmath>
#include <cstdio>

namespace resonant::em {

namespace {

double gOf(double length, double radius)
{
    return std::isinf(radius) ? 1.0 : 1.0 - length / radius;
}

std::string percent(double v)
{
    char text[32];
    std::snprintf(text, sizeof text, "%.3g %%", 100.0 * v);
    return text;
}

}  // namespace

std::string_view toString(ResonatorStability s)
{
    switch (s) {
    case ResonatorStability::Stable: return "stable";
    case ResonatorStability::Marginal: return "marginal";
    case ResonatorStability::Unstable: return "unstable";
    }
    return "unstable";
}

Status OpticalResonatorSpec::validate() const
{
    if (!(length > 0.0) || !std::isfinite(length))
        return Status::failure("the mirror separation must be positive");
    if (radius1 == 0.0 || radius2 == 0.0 || std::isnan(radius1) || std::isnan(radius2))
        return Status::failure("a radius of curvature cannot be zero (use infinity for a plane mirror)");
    for (double r : {reflectivity1, reflectivity2})
        if (!(r > 0.0 && r < 1.0))
            return Status::failure("mirror reflectivities must lie between 0 and 1 (exclusive)");
    if (aperture1 < 0.0 || aperture2 < 0.0)
        return Status::failure("apertures cannot be negative (0 = unlimited)");
    if (!(wavelength > 0.0) || !(refractiveIndex >= 1.0))
        return Status::failure("the wavelength must be positive and the refractive index at least 1");
    if (extraRoundTripLoss < 0.0 || extraRoundTripLoss >= 1.0)
        return Status::failure("the extra round-trip loss must lie in [0, 1)");
    return Status::success();
}

Result<OpticalResonatorResult> analyzeOpticalResonator(const OpticalResonatorSpec& s)
{
    using R = Result<OpticalResonatorResult>;
    if (Status v = s.validate(); !v.ok())
        return R::failure(v.message());
    OpticalResonatorResult r;
    const double L = s.length, lambda = s.wavelength / s.refractiveIndex;
    r.g1 = gOf(L, s.radius1);
    r.g2 = gOf(L, s.radius2);
    const double p = r.g1 * r.g2;
    constexpr double eps = 1e-12;
    if (std::abs(p) <= eps || std::abs(p - 1.0) <= eps)
        r.stability = ResonatorStability::Marginal;
    else if (p > 0.0 && p < 1.0)
        r.stability = ResonatorStability::Stable;
    else
        r.stability = ResonatorStability::Unstable;
    r.assumptions = {"paraxial scalar Gaussian beams (Hermite-Gaussian modes); valid while the waist is many wavelengths",
                     "thin spherical mirrors aligned on one axis; no tilt or decentring",
                     "a lossless homogeneous medium of index n between the mirrors",
                     "aperture clipping estimated from the TEM00 intensity outside each mirror radius (no diffraction calculation)"};
    // The fundamental mode.
    const bool symmetric = std::abs(r.g1 - r.g2) <= eps;
    const bool hasMode = r.stability == ResonatorStability::Stable || (r.stability == ResonatorStability::Marginal && symmetric && std::abs(p) <= eps);
    if (hasMode) {
        if (symmetric) {
            const double g = r.g1;
            r.waist = std::sqrt(lambda * L / kPi * std::sqrt((1.0 + g) / (4.0 * (1.0 - g))));
            r.waistPosition = 0.5 * L;
            r.spot1 = r.spot2 = std::sqrt(lambda * L / kPi * std::sqrt(1.0 / (1.0 - g * g)));
        } else {
            const double den = r.g1 + r.g2 - 2.0 * p;
            r.waist = std::sqrt(lambda * L / kPi * std::sqrt(p * (1.0 - p) / (den * den)));
            r.waistPosition = L * r.g2 * (1.0 - r.g1) / den;
            r.spot1 = std::sqrt(lambda * L / kPi * std::sqrt(r.g2 / (r.g1 * (1.0 - p))));
            r.spot2 = std::sqrt(lambda * L / kPi * std::sqrt(r.g1 / (r.g2 * (1.0 - p))));
        }
        r.rayleighRange = kPi * r.waist * r.waist / lambda;
        if (r.stability == ResonatorStability::Marginal)
            r.warnings.push_back("a confocal resonator is marginally stable: its transverse modes are degenerate; the symmetric mode is reported");
        if (r.waist < 10.0 * lambda)
            r.warnings.push_back("the waist is less than ten wavelengths: the paraxial model is not accurate");
    } else if (r.stability == ResonatorStability::Unstable) {
        r.warnings.push_back("unstable resonator (g1 g2 = " + std::to_string(p) + "): no Gaussian mode exists; losses are geometric, not modeled here");
    } else {
        r.warnings.push_back("marginally stable resonator (g1 g2 = " + std::to_string(p) +
                             "): the Gaussian mode is not confined (plane-parallel or concentric limit); only the longitudinal spectrum is reported");
    }
    // Spectrum.
    r.frequency = kSpeedOfLight / s.wavelength;
    r.freeSpectralRange = kSpeedOfLight / (2.0 * s.refractiveIndex * L);
    const double gouy = hasMode ? std::acos((r.g1 >= 0.0 ? 1.0 : -1.0) * std::sqrt(std::max(0.0, p))) : 0.0;
    r.gouyRoundTrip = 2.0 * gouy;
    r.transverseModeSpacing = r.freeSpectralRange * gouy / kPi;
    r.longitudinalIndex = std::lround(r.frequency / r.freeSpectralRange - gouy / kPi);
    r.nearestTem00Frequency = r.freeSpectralRange * (static_cast<double>(r.longitudinalIndex) + gouy / kPi);
    // Losses.
    if (hasMode) {
        if (s.aperture1 > 0.0)
            r.clippingLoss1 = std::exp(-2.0 * s.aperture1 * s.aperture1 / (r.spot1 * r.spot1));
        if (s.aperture2 > 0.0)
            r.clippingLoss2 = std::exp(-2.0 * s.aperture2 * s.aperture2 / (r.spot2 * r.spot2));
        for (const auto& [aperture, spot] : {std::pair{s.aperture1, r.spot1}, std::pair{s.aperture2, r.spot2}})
            if (aperture > 0.0 && aperture < 1.5 * spot)
                r.warnings.push_back("an aperture smaller than 1.5 beam radii: diffraction changes the mode; the clipping estimate is not accurate");
    } else if (s.aperture1 > 0.0 || s.aperture2 > 0.0) {
        r.warnings.push_back("apertures are not evaluated without a confined Gaussian mode");
    }
    const double r1 = s.reflectivity1 * (1.0 - r.clippingLoss1), r2 = s.reflectivity2 * (1.0 - r.clippingLoss2);
    const double survival = r1 * r2 * (1.0 - s.extraRoundTripLoss);
    r.roundTripLoss = 1.0 - survival;
    r.finesse = kPi * std::pow(survival, 0.25) / (1.0 - std::sqrt(survival));
    r.linewidth = r.freeSpectralRange / r.finesse;
    r.photonLifetime = 1.0 / (2.0 * kPi * r.linewidth);
    r.qualityFactor = r.frequency / r.linewidth;
    const double t1 = 1.0 - s.reflectivity1, t2 = 1.0 - s.reflectivity2;
    r.resonantTransmission = t1 * t2 / std::pow(1.0 - std::sqrt(survival), 2);
    r.storedEnergyPerWatt = 2.0 * s.refractiveIndex * L / kSpeedOfLight;
    if (r.roundTripLoss > 0.5)
        r.warnings.push_back("round-trip loss " + percent(r.roundTripLoss) + ": a low-finesse resonator; the Lorentzian linewidth is approximate");
    return R(std::move(r));
}

double opticalModeFrequency(const OpticalResonatorSpec& s, const OpticalResonatorResult& r, long q, int m, int n)
{
    (void)s;
    return r.freeSpectralRange * (static_cast<double>(q) + (m + n + 1) * r.gouyRoundTrip / (2.0 * kPi));
}

double beamRadiusAt(const OpticalResonatorResult& r, double z)
{
    if (!(r.rayleighRange > 0.0))
        return 0.0;
    const double x = (z - r.waistPosition) / r.rayleighRange;
    return r.waist * std::sqrt(1.0 + x * x);
}

Complex beamParameterAtMirror1(const OpticalResonatorSpec&, const OpticalResonatorResult& r)
{
    return Complex(-r.waistPosition, r.rayleighRange);
}

Complex roundTripBeamParameter(const OpticalResonatorSpec& s, Complex q)
{
    const auto travel = [&](Complex x) { return x + s.length; };
    const auto mirror = [](Complex x, double radius) {
        if (std::isinf(radius))
            return x;
        return 1.0 / (1.0 / x - 2.0 / radius);
    };
    return mirror(travel(mirror(travel(q), s.radius2)), s.radius1);
}

}  // namespace resonant::em
