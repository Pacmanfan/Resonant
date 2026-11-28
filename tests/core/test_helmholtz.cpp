// Helmholtz resonators (RES-RSN-006): the lumped model against its closed
// forms, against the exact plane-wave stepped-duct model in its validity
// limit (which also yields the outer end correction 8a/3pi from the piston's
// radiation reactance), Q by mechanism and bandwidth, validity warnings, and
// the geometry kind with its full-wave solid model.
#include "geometry/Geometry.h"
#include "geometry/SolidModel.h"
#include "manipulators/Helmholtz.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::manipulators;

namespace {
constexpr double kPi = 3.14159265358979323846;

HelmholtzSpec resonator(double cavityLength = 0.05)
{
    HelmholtzSpec s;
    s.cavityRadius = 0.025;
    s.cavityLength = cavityLength;
    s.neckRadius = 0.005;
    s.neckLength = 0.01;
    return s;
}
}  // namespace

TEST(HelmholtzResonator, LumpedModelFollowsItsClosedForms)
{
    const HelmholtzSpec s = resonator();
    auto r = analyzeHelmholtz(s);
    ASSERT_TRUE(r) << r.message();
    const double V = kPi * 0.025 * 0.025 * 0.05, S = kPi * 0.005 * 0.005;
    const double Leff = 0.01 + 8.0 / (3 * kPi) * 0.005 + 8.0 / (3 * kPi) * 0.005 * (1 - 1.25 * 0.2);
    EXPECT_NEAR(r.value().effectiveLength, Leff, 1e-15);
    EXPECT_NEAR(r.value().frequencyHz, 343.0 / (2 * kPi) * std::sqrt(S / (V * Leff)), 1e-9);
    // Losses: each mechanism separately, combined as 1/Q = 1/Q_rad + 1/Q_visc.
    EXPECT_NEAR(1.0 / r.value().quality, 1.0 / r.value().qRadiation + 1.0 / r.value().qViscous, 1e-12);
    EXPECT_GT(r.value().qRadiation, r.value().qViscous);  // a small neck: viscous loss dominates
    const double k = 2 * kPi * r.value().frequencyHz / 343.0;
    EXPECT_NEAR(r.value().radiationResistance, 1.204 * 343.0 * k * k / (2 * kPi), 1e-9);
    // The bandwidth of the cavity pressure response is f0 / Q (a light resonance: Lorentzian).
    const double f0 = r.value().frequencyHz;
    const auto gain = [&](double f) { return std::pow(helmholtzPressureGain(s, f) * f * f / (f0 * f0), 2); };  // |U|^2 shape
    const double peak = gain(f0);
    const auto half = [&](double sign) {
        double a = f0, b = f0 * (1 + sign * 0.5);
        for (int i = 0; i < 200; ++i)
            ((gain(0.5 * (a + b)) > 0.5 * peak) ? a : b) = 0.5 * (a + b);
        return 0.5 * (a + b);
    };
    EXPECT_NEAR((half(1) - half(-1)) / r.value().bandwidthHz, 1.0, 0.02);
    EXPECT_GT(r.value().pressureGainAtResonance, 10.0);
    EXPECT_NEAR(r.value().storedEnergyPerPa2, 0.5 * r.value().compliance * std::pow(r.value().pressureGainAtResonance, 2), 1e-18);
}

TEST(HelmholtzResonator, TheSteppedDuctConvergesToTheLumpedModelAndYieldsTheOuterEndCorrection)
{
    // The plane-wave model has no inner end correction: compare with the lumped model without it.
    std::vector<double> deviation;
    for (double h : {0.04, 0.02, 0.01}) {
        HelmholtzSpec s = resonator(h);
        s.innerCorrection = false;
        s.viscousLoss = false;
        const auto lumped = analyzeHelmholtz(s).value();
        auto duct = steppedDuctResonance(s);
        ASSERT_TRUE(duct) << duct.message();
        deviation.push_back(std::abs(duct.value().frequencyHz / lumped.frequencyHz - 1.0));
        // The distributed corrections scale as (k size)^2.
        EXPECT_LT(deviation.back(), 0.3 * std::pow(lumped.cavityKSize, 2) + 0.3 * std::pow(lumped.neckKL, 2)) << h;
        // Radiation Q of the duct's response and of the lumped model, to the order of the distributed corrections.
        EXPECT_NEAR(duct.value().quality / lumped.qRadiation, 1.0, 3.0 * deviation.back() + 0.01) << h;
    }
    EXPECT_LT(deviation[2], deviation[1]);
    EXPECT_LT(deviation[1], deviation[0]);
    // The outer end correction: the plane-wave duct, loaded by the piston's radiation impedance, resonates
    // where the lumped model with L + 8a/3pi does, not where the bare neck length L would put it.
    HelmholtzSpec s = resonator(0.005);
    s.innerCorrection = false;
    s.viscousLoss = false;
    const double f = steppedDuctResonance(s).value().frequencyHz;
    const double withCorrection = analyzeHelmholtz(s).value().frequencyHz;
    const double bare = 343.0 / (2 * kPi) * std::sqrt(s.neckArea() / (s.volume() * s.neckLength));
    EXPECT_LT(std::abs(f / withCorrection - 1.0), 0.02);
    EXPECT_GT(std::abs(f / bare - 1.0), 0.12);
}

TEST(HelmholtzResonator, ValidityAndRefusals)
{
    HelmholtzSpec big = resonator(0.6);  // a long cavity: k h is not small
    auto r = analyzeHelmholtz(big);
    ASSERT_TRUE(r);
    EXPECT_FALSE(r.value().warnings.empty());
    HelmholtzSpec bad = resonator();
    bad.neckRadius = 0.03;
    EXPECT_FALSE(analyzeHelmholtz(bad));
    bad = resonator();
    bad.neckLength = 0.0;
    EXPECT_FALSE(analyzeHelmholtz(bad));
}

TEST(HelmholtzResonator, TheGeometryKindAndItsSolidModel)
{
    Geometry g = Geometry::create("resonator.helmholtz").take();
    const HelmholtzSpec s = helmholtzSpecOf(g);
    EXPECT_TRUE(s.validate().ok());
    EXPECT_FALSE(g.tessellate().empty());
    EXPECT_FALSE(g.setParameter("neckRadius", 0.05).ok());  // wider than the cavity
    const auto model = solidModelOf(g, 1e-6);
    ASSERT_TRUE(model.has_value());
    const double R = s.cavityRadius, h = 0.5 * s.cavityLength, a = s.neckRadius, t = g.parameter("wallThickness");
    EXPECT_TRUE(model->contains(Vec3(R + 0.5 * t, 0, 0)));        // the side wall
    EXPECT_FALSE(model->contains(Vec3(0.5 * R, 0, 0)));           // the air inside
    EXPECT_FALSE(model->contains(Vec3(0, 0, h + 0.5 * t)));       // the neck's bore through the top
    EXPECT_TRUE(model->contains(Vec3(a + 0.5 * t, 0, h + 0.5 * s.neckLength)));  // the neck wall
}
