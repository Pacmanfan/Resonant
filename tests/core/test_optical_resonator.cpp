// Two-mirror optical resonators (RES-FUNC-007): stability, Gaussian-beam
// closed forms, self-consistency under the ABCD round trip, losses, and the
// spectrum of a plane Fabry-Perot against the exact layered-media solution.
#include "electromagnetics/Conventions.h"
#include "electromagnetics/LayeredMedia.h"
#include "electromagnetics/OpticalResonator.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace resonant;
using namespace resonant::em;

namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();

OpticalResonatorSpec spec(double L, double r1, double r2)
{
    OpticalResonatorSpec s;
    s.length = L;
    s.radius1 = r1;
    s.radius2 = r2;
    return s;
}
}  // namespace

TEST(OpticalResonator, StabilityIsClassifiedFromG1G2)
{
    const auto stability = [](OpticalResonatorSpec s) { return analyzeOpticalResonator(s).value().stability; };
    EXPECT_EQ(stability(spec(0.1, kInf, kInf)), ResonatorStability::Marginal);     // plane-parallel
    EXPECT_EQ(stability(spec(0.1, 0.1, 0.1)), ResonatorStability::Marginal);       // confocal
    EXPECT_EQ(stability(spec(0.1, 0.05, 0.05)), ResonatorStability::Marginal);     // concentric
    EXPECT_EQ(stability(spec(0.1, 0.2, 0.2)), ResonatorStability::Stable);         // g = 0.5
    EXPECT_EQ(stability(spec(0.1, kInf, 0.3)), ResonatorStability::Stable);        // hemispherical-like
    EXPECT_EQ(stability(spec(0.1, 0.04, 0.04)), ResonatorStability::Unstable);     // g = -1.5
    EXPECT_EQ(stability(spec(0.1, -0.5, 0.08)), ResonatorStability::Unstable);     // convex + strongly concave, g1 g2 < 0
    EXPECT_EQ(stability(spec(0.1, -0.5, 0.2)), ResonatorStability::Stable);       // convex + concave, g1 g2 = 0.6
    const auto unstable = analyzeOpticalResonator(spec(0.1, 0.04, 0.04)).value();
    EXPECT_EQ(unstable.waist, 0.0);
    EXPECT_FALSE(unstable.warnings.empty());
}

TEST(OpticalResonator, ConfocalClosedForms)
{
    OpticalResonatorSpec s = spec(0.1, 0.1, 0.1);
    s.wavelength = 1064e-9;
    const auto r = analyzeOpticalResonator(s).value();
    const double w0 = std::sqrt(s.wavelength * s.length / (2.0 * kPi));
    EXPECT_NEAR(r.waist / w0, 1.0, 1e-12);
    EXPECT_NEAR(r.spot1 / (std::sqrt(2.0) * w0), 1.0, 1e-12);
    EXPECT_NEAR(r.waistPosition, 0.05, 1e-15);
    EXPECT_NEAR(r.rayleighRange, 0.05, 1e-12);  // the confocal parameter equals L
    EXPECT_NEAR(r.freeSpectralRange, kSpeedOfLight / 0.2, 1e-3);
    EXPECT_NEAR(r.transverseModeSpacing / r.freeSpectralRange, 0.5, 1e-12);
    // TEM00,q+1 and TEM20,q... are degenerate in a confocal resonator: m + n = 2 shifts by one FSR.
    EXPECT_NEAR(opticalModeFrequency(s, r, 10, 2, 0) - opticalModeFrequency(s, r, 11, 0, 0), 0.0, 1e-3);
}

TEST(OpticalResonator, TheGaussianModeReproducesItselfAfterOneRoundTrip)
{
    for (const auto& [L, r1, r2] : {std::tuple{0.15, 0.5, 0.25}, std::tuple{0.2, kInf, 0.5}, std::tuple{0.3, 0.2, 0.25}}) {
        OpticalResonatorSpec s = spec(L, r1, r2);
        s.refractiveIndex = 1.5;
        const auto r = analyzeOpticalResonator(s).value();
        ASSERT_EQ(r.stability, ResonatorStability::Stable) << L;
        const Complex q = beamParameterAtMirror1(s, r);
        const Complex back = roundTripBeamParameter(s, q);
        EXPECT_NEAR(std::abs(back - q) / std::abs(q), 0.0, 1e-12) << L;
        // The beam radius at the mirrors, from q, equals the closed-form spot sizes.
        const double lambda = s.wavelength / s.refractiveIndex;
        const double w1 = std::sqrt(-lambda / (kPi * (1.0 / q).imag()));
        EXPECT_NEAR(w1 / r.spot1, 1.0, 1e-10);
        EXPECT_NEAR(beamRadiusAt(r, 0.0) / r.spot1, 1.0, 1e-10);
        EXPECT_NEAR(beamRadiusAt(r, L) / r.spot2, 1.0, 1e-10);
        // The wavefront at each mirror matches the mirror (finite radii only).
        if (!std::isinf(r1)) {
            const Complex q1 = q;
            EXPECT_NEAR(-1.0 / (1.0 / q1).real(), r1, 1e-9 * r1);
        }
    }
}

TEST(OpticalResonator, FinesseLinewidthLifetimeAndClipping)
{
    OpticalResonatorSpec s = spec(0.25, 1.0, 1.0);
    s.reflectivity1 = s.reflectivity2 = 0.99;
    auto r = analyzeOpticalResonator(s).value();
    EXPECT_NEAR(r.finesse, kPi * std::sqrt(0.99) / 0.01, 1e-9);
    EXPECT_NEAR(r.linewidth, r.freeSpectralRange / r.finesse, 1e-9);
    EXPECT_NEAR(r.photonLifetime * 2.0 * kPi * r.linewidth, 1.0, 1e-12);
    EXPECT_NEAR(r.qualityFactor, r.frequency / r.linewidth, 1e-3);
    EXPECT_NEAR(r.resonantTransmission, 1.0, 1e-12);  // symmetric lossless mirrors
    EXPECT_NEAR(r.storedEnergyPerWatt, 2.0 * 0.25 / kSpeedOfLight, 1e-20);
    // An aperture equal to the beam radius clips e^-2 of the power per bounce.
    s.aperture1 = r.spot1;
    r = analyzeOpticalResonator(s).value();
    EXPECT_NEAR(r.clippingLoss1, std::exp(-2.0), 1e-12);
    EXPECT_FALSE(r.warnings.empty());
    const double survival = 0.99 * (1.0 - std::exp(-2.0)) * 0.99;
    EXPECT_NEAR(r.finesse, kPi * std::pow(survival, 0.25) / (1.0 - std::sqrt(survival)), 1e-9);
}

TEST(OpticalResonator, PlaneFabryPerotSpectrumMatchesTheExactLayeredSolution)
{
    // Two quarter-wave mirrors of index 4 at f0, 50 wavelengths apart, in vacuum.
    const double f0 = 1e10, lambda0 = kSpeedOfLight / f0, n = 4.0;
    const double gap = 50.0 * lambda0;
    const double rLayer = (1.0 - n * n) / (1.0 + n * n);
    const double R = rLayer * rLayer;
    const auto transmittance = [&](double f) {
        LayeredProblem p;
        p.frequencyHz = f;
        MediumLayer mirror;
        mirror.thickness = lambda0 / (4.0 * n);
        mirror.permittivityReal = n * n;
        MediumLayer space;
        space.thickness = gap;
        p.layers = {mirror, space, mirror};
        return LayeredSolution::solve(p).value().transmittance();
    };
    OpticalResonatorSpec s = spec(gap, kInf, kInf);
    s.reflectivity1 = s.reflectivity2 = R;
    s.wavelength = lambda0;
    const auto model = analyzeOpticalResonator(s).value();
    // Transmission peaks near f0 by golden-section search in successive FSR windows.
    const auto peakNear = [&](double guess) {
        double lo = guess - 0.3 * model.freeSpectralRange, hi = guess + 0.3 * model.freeSpectralRange;
        for (int i = 0; i < 200; ++i) {
            const double m1 = lo + 0.382 * (hi - lo), m2 = lo + 0.618 * (hi - lo);
            (transmittance(m1) < transmittance(m2) ? lo : hi) = (transmittance(m1) < transmittance(m2) ? m1 : m2);
        }
        return 0.5 * (lo + hi);
    };
    const double p1 = peakNear(f0), p2 = peakNear(f0 + model.freeSpectralRange);
    EXPECT_NEAR((p2 - p1) / model.freeSpectralRange, 1.0, 0.005);
    EXPECT_NEAR(transmittance(p1), model.resonantTransmission, 1e-6);
    // Full width at half maximum of the exact peak against FSR / finesse.
    const auto half = [&](double sign) {
        double lo = p1, hi = p1 + sign * 0.5 * model.freeSpectralRange;
        for (int i = 0; i < 200; ++i) {
            const double mid = 0.5 * (lo + hi);
            (transmittance(mid) > 0.5 * transmittance(p1) ? lo : hi) = mid;
        }
        return 0.5 * (lo + hi);
    };
    EXPECT_NEAR((half(1.0) - half(-1.0)) / model.linewidth, 1.0, 0.03);
}

TEST(OpticalResonator, InvalidResonatorsAreRefused)
{
    EXPECT_FALSE(analyzeOpticalResonator(spec(0.0, 1.0, 1.0)));
    EXPECT_FALSE(analyzeOpticalResonator(spec(0.1, 0.0, 1.0)));
    OpticalResonatorSpec s = spec(0.1, 1.0, 1.0);
    s.reflectivity1 = 1.0;
    EXPECT_FALSE(analyzeOpticalResonator(s));
}
