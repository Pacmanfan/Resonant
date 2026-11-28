// Acoustic horns by the Webster equation (RES-FUNC-008): the piston radiation
// impedance, a uniform duct, an infinite exponential horn, a conical horn,
// power conservation, step convergence, profile comparison and the geometry kind.
#include "geometry/Geometry.h"
#include "geometry/SolidModel.h"
#include "manipulators/Horn.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::manipulators;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double rho = 1.204, c = 343.0;

HornSpec horn(HornProfile profile, double rt = 0.02, double rm = 0.2, double L = 0.6)
{
    HornSpec s;
    s.profile = profile;
    s.throatRadius = rt;
    s.mouthRadius = rm;
    s.length = L;
    s.steps = 4000;
    return s;
}

// The power series of H1 (independent of the quadrature).
double struveSeries(double x)
{
    double sum = 0.0;
    for (int k = 0; k < 60; ++k)
        sum += std::pow(-1.0, k) * std::pow(0.5 * x, 2 * k + 2) / (std::tgamma(k + 1.5) * std::tgamma(k + 2.5));
    return sum;
}
}  // namespace

TEST(AcousticHorn, PistonRadiationImpedanceMatchesItsLimitsAndTheSeries)
{
    for (double x : {0.3, 1.0, 2.0, 5.0, 9.0})
        EXPECT_NEAR(struveH1(x), struveSeries(x), 1e-9) << x;
    const double a = 0.1, area = kPi * a * a;
    // Small ka: R = (ka)^2 / 2, X = 8 ka / (3 pi) (normalized).
    const double fSmall = 0.05 * c / (2 * kPi * a);
    const Complex z = bafflePistonImpedance(a, fSmall, rho, c) * area / (rho * c);
    EXPECT_NEAR(z.real() / (0.5 * 0.05 * 0.05), 1.0, 0.01);
    EXPECT_NEAR(z.imag() / (8.0 * 0.05 / (3.0 * kPi)), 1.0, 0.01);
    // Large ka: R -> 1, X -> 0.
    const Complex big = bafflePistonImpedance(a, 50.0 * c / (2 * kPi * a), rho, c) * area / (rho * c);
    EXPECT_NEAR(big.real(), 1.0, 0.02);
    EXPECT_LT(std::abs(big.imag()), 0.05);
    EXPECT_NEAR(pistonDirectivityFactor(1e-6), 2.0, 1e-9);
    EXPECT_NEAR(pistonDirectivity(3.0, 0.0), 1.0, 1e-12);
}

TEST(AcousticHorn, AUniformDuctMatchesTheTransmissionLineFormula)
{
    HornSpec s = horn(HornProfile::Uniform, 0.02, 0.02, 0.5);
    for (double f : {200.0, 800.0, 2300.0}) {
        const auto r = analyzeHorn(s, f).value();
        const double k = 2 * kPi * f / c, S = kPi * 0.02 * 0.02, zc = rho * c / S;
        const Complex ZL = bafflePistonImpedance(0.02, f, rho, c), j(0, 1);
        const Complex expected = zc * (ZL + j * zc * std::tan(k * 0.5)) / (zc + j * ZL * std::tan(k * 0.5));
        EXPECT_NEAR(std::abs(r.throatImpedance - expected) / std::abs(expected), 0.0, 1e-9) << f;
    }
}

TEST(AcousticHorn, AnExponentialHornWithItsOutgoingLoadIsTheInfiniteHorn)
{
    HornSpec s = horn(HornProfile::Exponential);
    const double m = hornFlareConstant(s);
    EXPECT_NEAR(m, 2.0 * std::log(10.0) / 0.6, 1e-12);
    EXPECT_NEAR(hornCutoffHz(s), m * c / (4 * kPi), 1e-9);
    const double Sm = kPi * 0.2 * 0.2;
    for (double f : {600.0, 1500.0, 150.0}) {
        const double k = 2 * kPi * f / c;
        const Complex j(0, 1);
        // beta = sqrt(k^2 - m^2/4): real above cut-off, -j sqrt(m^2/4 - k^2) below (evanescent).
        const Complex beta = k > 0.5 * m ? Complex(std::sqrt(k * k - 0.25 * m * m), 0) : Complex(0, -std::sqrt(0.25 * m * m - k * k));
        s.mouthImpedance = (rho * c / Sm) * (beta / k + j * m / (2 * k));
        const auto r = analyzeHorn(s, f).value();
        const Complex expected = beta / k + j * m / (2 * k);
        EXPECT_NEAR(std::abs(r.normalizedThroatImpedance - expected), 0.0, 1e-8) << f;
        if (f < hornCutoffHz(s)) {
            EXPECT_LT(std::abs(r.normalizedThroatImpedance.real()), 1e-8);
        }
    }
}

TEST(AcousticHorn, AConicalHornWithASphericalLoadHasTheSphericalWaveImpedance)
{
    HornSpec s = horn(HornProfile::Conical);
    const double xt = 0.02 * 0.6 / (0.2 - 0.02), xm = xt + 0.6;
    const Complex j(0, 1);
    for (double f : {100.0, 700.0, 3000.0}) {
        const double k = 2 * kPi * f / c;
        s.mouthImpedance = rho * c / (kPi * 0.2 * 0.2) * (j * k * xm) / (1.0 + j * k * xm);
        const auto r = analyzeHorn(s, f).value();
        const Complex expected = (j * k * xt) / (1.0 + j * k * xt);
        EXPECT_NEAR(std::abs(r.normalizedThroatImpedance - expected) / std::abs(expected), 0.0, 1e-7) << f;
    }
}

TEST(AcousticHorn, PowerIsConservedAndTheIntegrationConvergesAtFourthOrder)
{
    for (HornProfile p : {HornProfile::Conical, HornProfile::Exponential, HornProfile::Hyperbolic}) {
        const auto r = analyzeHorn(horn(p), 1000.0, 0.1).value();
        EXPECT_GT(r.radiatedPower, 0.0);
        EXPECT_NEAR(r.mouthPower / r.radiatedPower, 1.0, 1e-8) << toString(p);
        for (const HornSample& sample : r.samples)
            EXPECT_NEAR(sample.power / r.radiatedPower, 1.0, 1e-8) << toString(p) << " at " << sample.x;
        EXPECT_NEAR(r.powerTransmission, 1.0 - std::norm(r.throatReflection), 1e-15);
    }
    HornSpec s = horn(HornProfile::Hyperbolic);
    s.steps = 16000;
    const Complex reference = analyzeHorn(s, 1200.0).value().throatImpedance;
    std::vector<double> errors;
    for (int n : {50, 100}) {
        s.steps = n;
        errors.push_back(std::abs(analyzeHorn(s, 1200.0).value().throatImpedance - reference) / std::abs(reference));
    }
    EXPECT_GT(errors[0] / errors[1], 10.0);  // ~16 for fourth order
}

TEST(AcousticHorn, ProfilesCompareUnderEqualThroatMouthAndLength)
{
    // Just above the exponential cut-off (209 Hz) the exponential horn loads its throat much better than the cone.
    const auto exponential = analyzeHorn(horn(HornProfile::Exponential), 300.0, 0.1).value();
    const auto conical = analyzeHorn(horn(HornProfile::Conical), 300.0, 0.1).value();
    EXPECT_GT(exponential.normalizedThroatImpedance.real(), 2.0 * conical.normalizedThroatImpedance.real());
    EXPECT_GT(exponential.radiatedPower, conical.radiatedPower);
    // At high frequency both approach the plane-wave resistance of the throat.
    for (HornProfile p : {HornProfile::Conical, HornProfile::Exponential})
        EXPECT_NEAR(analyzeHorn(horn(p), 6000.0).value().normalizedThroatImpedance.real(), 1.0, 0.1) << toString(p);
    // The mouth directivity grows with frequency (ka).
    EXPECT_GT(analyzeHorn(horn(HornProfile::Exponential), 2000.0).value().directivityIndexDb, exponential.directivityIndexDb);
}

TEST(AcousticHorn, TheGeometryKindDescribesTheSameHorn)
{
    auto g = Geometry::create("horn.acoustic");
    ASSERT_TRUE(g) << g.message();
    Geometry geometry = g.take();
    ASSERT_TRUE(geometry.setParameter("profile", 2.0).ok());
    ASSERT_TRUE(geometry.setParameter("flare", 0.5).ok());
    const HornSpec s = hornSpecOf(geometry);
    EXPECT_EQ(s.profile, HornProfile::Hyperbolic);
    EXPECT_EQ(s.flare, 0.5);
    EXPECT_NEAR(hornRadius(s, s.length), s.mouthRadius, 1e-9);
    EXPECT_NEAR(hornRadius(s, 0.0), s.throatRadius, 1e-15);
    EXPECT_FALSE(geometry.setParameter("mouthRadius", 0.01).ok());  // smaller than the throat
    EXPECT_FALSE(geometry.tessellate().empty());
}

TEST(AcousticHorn, InvalidHornsAreRefused)
{
    EXPECT_FALSE(analyzeHorn(horn(HornProfile::Conical, 0.02, 0.01), 500.0));
    EXPECT_FALSE(analyzeHorn(horn(HornProfile::Conical), -1.0));
    HornSpec s = horn(HornProfile::Hyperbolic);
    s.flare = 0.0;
    EXPECT_FALSE(analyzeHorn(s, 500.0));
}

TEST(AcousticHorn, TheFullWaveSolidModelFollowsTheSameProfile)
{
    for (const double profile : {0.0, 1.0, 2.0}) {
        Geometry g = Geometry::create("horn.acoustic").take();
        ASSERT_TRUE(g.setParameter("profile", profile).ok());
        const HornSpec s = hornSpecOf(g);
        const double wall = g.parameter("wallThickness");
        const auto model = solidModelOf(g, 1e-6);
        ASSERT_TRUE(model.has_value());
        for (double x : {0.05, 0.25, 0.45}) {
            const double z = x - 0.5 * s.length, r = hornRadius(s, x);
            EXPECT_TRUE(model->contains(Vec3(r + 0.5 * wall, 0.0, z))) << profile << " at " << x;
            EXPECT_FALSE(model->contains(Vec3(r - 0.5 * wall, 0.0, z))) << profile << " at " << x;
        }
    }
}
