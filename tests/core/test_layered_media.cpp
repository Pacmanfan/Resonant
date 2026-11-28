// The layered-media plane-wave solution against closed forms (RES-CEME-011).
#include "electromagnetics/LayeredMedia.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

using namespace resonant;
using namespace resonant::em;

namespace {

constexpr double kC0 = 299792458.0;

MediumLayer medium(double eps, double thickness = 0.0, double tanDelta = 0.0, double sigma = 0.0)
{
    MediumLayer m;
    m.permittivityReal = eps;
    m.thickness = thickness;
    m.lossTangent = tanDelta;
    m.conductivity = sigma;
    return m;
}

}  // namespace

TEST(LayeredMedia, ASingleInterfaceGivesTheFresnelCoefficients)
{
    LayeredProblem p;
    p.frequencyHz = 1e9;
    p.exit = medium(4.0);  // n = 2: r = (1 - 2)/(1 + 2)
    auto s = LayeredSolution::solve(p);
    ASSERT_TRUE(s) << s.message();
    EXPECT_NEAR(s.value().reflection().real(), -1.0 / 3.0, 1e-12);
    EXPECT_NEAR(s.value().reflection().imag(), 0.0, 1e-12);
    EXPECT_NEAR(s.value().reflectance(), 1.0 / 9.0, 1e-12);
    EXPECT_NEAR(s.value().transmittance(), 8.0 / 9.0, 1e-12);
    EXPECT_NEAR(s.value().absorptance(), 0.0, 1e-12);
}

TEST(LayeredMedia, AHalfWaveSlabIsTransparentAndAQuarterWaveCoatingMatches)
{
    const double f = 3e9, lambda = kC0 / f;
    LayeredProblem half;
    half.frequencyHz = f;
    half.layers = {medium(4.0, lambda / (2 * 2.0))};
    auto a = LayeredSolution::solve(half);
    ASSERT_TRUE(a) << a.message();
    EXPECT_NEAR(a.value().reflectance(), 0.0, 1e-12);
    EXPECT_NEAR(a.value().transmittance(), 1.0, 1e-12);

    LayeredProblem quarter;  // n0 = 1, coating n1 = 2 = sqrt(1 x 4), substrate n2 = 4
    quarter.frequencyHz = f;
    quarter.layers = {medium(4.0, lambda / (4 * 2.0))};
    quarter.exit = medium(16.0);
    auto b = LayeredSolution::solve(quarter);
    ASSERT_TRUE(b) << b.message();
    EXPECT_NEAR(b.value().reflectance(), 0.0, 1e-12);
    EXPECT_NEAR(b.value().transmittance(), 1.0, 1e-12);
}

TEST(LayeredMedia, AbsorbedPowerEqualsTheIntegralOfTheLossDensity)
{
    LayeredProblem p;
    p.frequencyHz = 2.45e9;
    p.incidentIntensity = 5000.0;
    p.layers = {medium(4.0, 0.012, 0.08), medium(9.0, 0.004, 0.0, 0.2)};
    auto s = LayeredSolution::solve(p);
    ASSERT_TRUE(s) << s.message();
    const LayeredSolution& sol = s.value();
    const double d = p.totalThickness();
    EXPECT_NEAR(sol.poyntingFlux(-1e-9), (1.0 - sol.reflectance()) * p.incidentIntensity, 1e-9 * p.incidentIntensity);
    EXPECT_NEAR(sol.poyntingFlux(d + 1e-9), sol.transmittance() * p.incidentIntensity, 1e-9 * p.incidentIntensity);
    // Midpoint quadrature of 1/2 omega eps0 eps'' |E|^2 converges to the flux difference.
    const int n = 20000;
    double integral = 0.0;
    for (int i = 0; i < n; ++i)
        integral += sol.absorbedPowerDensity((i + 0.5) * d / n) * d / n;
    EXPECT_NEAR(integral, sol.absorptance() * p.incidentIntensity, 1e-5 * sol.absorptance() * p.incidentIntensity);
    EXPECT_GT(sol.absorptance(), 0.05);
    EXPECT_NEAR(sol.reflectance() + sol.transmittance() + sol.absorptance(), 1.0, 1e-12);
    // The fields are continuous across the internal interface.
    EXPECT_NEAR(std::abs(sol.electricField(0.012 - 1e-12) - sol.electricField(0.012 + 1e-12)), 0.0, 1e-6 * std::abs(sol.electricField(0.012)));
}

TEST(LayeredMedia, AThickLossySlabAttenuatesAtTheMaterialRate)
{
    const double f = 1e9, omega = 2 * std::numbers::pi * f;
    LayeredProblem p;
    p.frequencyHz = f;
    p.layers = {medium(10.0, 0.5, 0.0, 0.5)};
    auto s = LayeredSolution::solve(p);
    ASSERT_TRUE(s) << s.message();
    const std::complex<double> k = omega / kC0 * std::sqrt(p.layers[0].relativePermittivity(omega));
    const double alpha = -k.imag();
    ASSERT_GT(alpha * 0.5, 10.0);  // e^{-2 alpha d} < 1e-8: the back face is negligible
    const double ratio = std::abs(s.value().electricField(0.05)) / std::abs(s.value().electricField(0.02));
    EXPECT_NEAR(std::log(ratio), -alpha * 0.03, 1e-6);
    EXPECT_LT(s.value().transmittance(), 1e-10);
}

TEST(LayeredMedia, RefusesActiveMediaAndLossyHalfSpaces)
{
    LayeredProblem p;
    p.layers = {medium(4.0, 0.01, -0.1)};
    EXPECT_FALSE(LayeredSolution::solve(p));
    p.layers = {medium(4.0, 0.0)};
    EXPECT_FALSE(LayeredSolution::solve(p));
    p.layers = {medium(4.0, 0.01)};
    p.exit = medium(2.0, 0.0, 0.01);
    EXPECT_FALSE(LayeredSolution::solve(p));
}
