// Rectangular cavity resonators with losses (RES-FUNC-006): closed forms,
// energy equipartition, wall and fill Q, the port response, sensitivities and
// the full-wave (Yee) model against the analytical frequencies and decay.
#include "electromagnetics/CavityResonator.h"
#include "electromagnetics/Conventions.h"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

using namespace resonant;
using namespace resonant::em;

namespace {

CavityResonatorSpec wr90Cavity()
{
    CavityResonatorSpec s;
    s.a = 22.86e-3;
    s.b = 10.16e-3;
    s.d = 30e-3;
    s.wallConductivity = 5.8e7;  // copper
    return s;
}

const CavityResonance& find(const std::vector<CavityResonance>& modes, const std::string& label)
{
    for (const auto& m : modes)
        if (m.label == label)
            return m;
    throw std::runtime_error("no mode " + label);
}

}  // namespace

TEST(CavityResonator, FrequenciesAreTheClosedFormAndEnergiesEquipartition)
{
    CavityResonatorSpec s = wr90Cavity();
    s.fillPermittivity = 2.1;
    auto modes = analyzeRectangularCavity(s, 8);
    ASSERT_TRUE(modes) << modes.message();
    ASSERT_EQ(modes.value().size(), 8u);
    for (const CavityResonance& m : modes.value()) {
        const double c = kSpeedOfLight / std::sqrt(2.1);
        const double f = 0.5 * c * std::sqrt(std::pow(m.m / s.a, 2) + std::pow(m.n / s.b, 2) + std::pow(m.p / s.d, 2));
        EXPECT_NEAR(m.frequencyHz / f, 1.0, 1e-12) << m.label;
        // At resonance the time-averaged electric and magnetic energies are equal.
        EXPECT_NEAR(m.electricEnergy / m.magneticEnergy, 1.0, 1e-12) << m.label;
        EXPECT_GT(m.wallQ, 1000.0) << m.label;
        EXPECT_TRUE(std::isinf(m.fillQ));
    }
    EXPECT_EQ(modes.value()[0].label, "TE101");
}

TEST(CavityResonator, WallQOfTE10pMatchesTheClosedForm)
{
    CavityResonatorSpec s = wr90Cavity();
    s.d = 60e-3;  // TE102 among the lowest modes
    auto modes = analyzeRectangularCavity(s, 6);
    ASSERT_TRUE(modes);
    const std::vector<CavityResonance> list = modes.value();
    for (const char* label : {"TE101", "TE102"}) {
        const CavityResonance m = find(list, label);
        EXPECT_NEAR(m.wallQ / te10pWallQ(s, m.p, m.frequencyHz), 1.0, 1e-10) << label;
    }
    // Copper X-band cavities have a Q of several thousand (Pozar, Example 6.3 order of magnitude).
    EXPECT_GT(find(list, "TE101").wallQ, 3000.0);
    EXPECT_LT(find(list, "TE101").wallQ, 20000.0);
}

TEST(CavityResonator, FillQIsTheInverseOfTheEffectiveLossTangent)
{
    CavityResonatorSpec s = wr90Cavity();
    s.wallConductivity = 0.0;  // perfect walls
    s.fillPermittivity = 2.25;
    s.fillLossTangent = 1e-3;
    auto modes = analyzeRectangularCavity(s, 4);
    ASSERT_TRUE(modes);
    for (const auto& m : modes.value()) {
        EXPECT_NEAR(m.fillQ, 1000.0, 1e-7) << m.label;
        EXPECT_NEAR(m.unloadedQ, 1000.0, 1e-7);
        EXPECT_TRUE(std::isinf(m.wallQ));
        EXPECT_NEAR(m.bandwidthHz, m.frequencyHz / 1000.0, 1e-3);
    }
    // A conducting fill: Q = omega eps' / sigma.
    s.fillLossTangent = 0.0;
    s.fillConductivity = 0.01;
    modes = analyzeRectangularCavity(s, 1);
    const auto& m = modes.value()[0];
    EXPECT_NEAR(m.fillQ / (2.0 * kPi * m.frequencyHz * kEpsilon0 * 2.25 / 0.01), 1.0, 1e-12);
    // Wall and fill losses combine: 1/Q0 = 1/Qc + 1/Qd.
    s.wallConductivity = 5.8e7;
    modes = analyzeRectangularCavity(s, 1);
    const auto& both = modes.value()[0];
    EXPECT_NEAR(1.0 / both.unloadedQ, 1.0 / both.wallQ + 1.0 / both.fillQ, 1e-15);
}

TEST(CavityResonator, PortResponseHalfPowerBandwidthIsF0OverQL)
{
    CavityResonatorSpec s = wr90Cavity();
    auto modes = analyzeRectangularCavity(s, 1);
    ASSERT_TRUE(modes);
    for (const double beta : {1.0, 3.0, 0.4}) {
        s.externalQ = modes.value()[0].unloadedQ / beta;
        const CavityResonance m = analyzeRectangularCavity(s, 1).value()[0];
        EXPECT_NEAR(m.coupling, beta, 1e-12);
        EXPECT_NEAR(m.loadedQ, m.unloadedQ / (1.0 + beta), 1e-6 * m.loadedQ);
        // On resonance |Gamma| = |beta - 1| / (beta + 1).
        EXPECT_NEAR(std::abs(portReflection(m, m.frequencyHz)), std::abs(beta - 1.0) / (beta + 1.0), 1e-12);
        // The absorbed fraction 1 - |Gamma|^2 falls to half at f0 +- (f0 / QL) / 2.
        const double peak = 1.0 - std::norm(portReflection(m, m.frequencyHz));
        const auto halfPoint = [&](double sign) {
            double lo = m.frequencyHz, hi = m.frequencyHz * (1.0 + sign * 20.0 / m.loadedQ);
            for (int i = 0; i < 200; ++i) {
                const double mid = 0.5 * (lo + hi);
                ((1.0 - std::norm(portReflection(m, mid))) > 0.5 * peak ? lo : hi) = mid;
            }
            return 0.5 * (lo + hi);
        };
        EXPECT_NEAR((halfPoint(1.0) - halfPoint(-1.0)) / m.bandwidthHz, 1.0, 1e-9) << beta;
    }
}

TEST(CavityResonator, ModeFieldsMeetTheWallsAndIntegrateToTheStoredEnergy)
{
    CavityResonatorSpec s = wr90Cavity();
    s.d = 45e-3;
    auto modes = analyzeRectangularCavity(s, 6);
    ASSERT_TRUE(modes);
    for (const CavityResonance& m : modes.value()) {
        // Tangential E vanishes on the walls.
        for (double u : {0.13, 0.5, 0.77})
            for (double v : {0.21, 0.66}) {
                EXPECT_LT(std::abs(cavityModeField(s, m, Vec3(0.0, u * s.b, v * s.d)).e[1]), 1e-12) << m.label;
                EXPECT_LT(std::abs(cavityModeField(s, m, Vec3(s.a, u * s.b, v * s.d)).e[2]), 1e-12) << m.label;
                EXPECT_LT(std::abs(cavityModeField(s, m, Vec3(u * s.a, 0.0, v * s.d)).e[0]), 1e-12) << m.label;
                EXPECT_LT(std::abs(cavityModeField(s, m, Vec3(u * s.a, v * s.b, s.d)).e[0]), 1e-12) << m.label;
                EXPECT_LT(std::abs(cavityModeField(s, m, Vec3(u * s.a, v * s.b, 0.0)).e[1]), 1e-12) << m.label;
            }
        // Midpoint quadrature of |E|^2 (exact for these trigonometric products) gives the electric energy;
        // the peak |E| is 1 V/m.
        const int n = 24;
        double sum = 0.0, peak = 0.0;
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                for (int k = 0; k < n; ++k) {
                    const auto f = cavityModeField(s, m, Vec3((i + 0.5) * s.a / n, (j + 0.5) * s.b / n, (k + 0.5) * s.d / n));
                    const double e2 = std::norm(f.e[0]) + std::norm(f.e[1]) + std::norm(f.e[2]);
                    sum += e2;
                    peak = std::max(peak, e2);
                }
        const double volume = s.a * s.b * s.d;
        EXPECT_NEAR(0.25 * kEpsilon0 * sum * volume / (n * n * n) / m.electricEnergy, 1.0, 1e-10) << m.label;
        EXPECT_LE(peak, 1.0 + 1e-12);
        EXPECT_GT(peak, 0.8) << m.label;
    }
}

TEST(CavityResonator, SensitivityToTheDimensionsMatchesFiniteDifferences)
{
    const CavityResonatorSpec s = wr90Cavity();
    const CavityResonance m = analyzeRectangularCavity(s, 1).value()[0];
    for (int dim = 0; dim < 3; ++dim) {
        const double h = 1e-7;
        CavityResonatorSpec up = s, down = s;
        (dim == 0 ? up.a : dim == 1 ? up.b : up.d) += h;
        (dim == 0 ? down.a : dim == 1 ? down.b : down.d) -= h;
        const double fd = (analyzeRectangularCavity(up, 1).value()[0].frequencyHz - analyzeRectangularCavity(down, 1).value()[0].frequencyHz) / (2 * h);
        const double exact = resonanceSensitivity(s, m, dim);
        if (dim == 1)
            EXPECT_EQ(exact, 0.0);  // TE101 does not depend on b
        else
            EXPECT_NEAR(fd / exact, 1.0, 1e-5) << dim;
    }
}

TEST(CavityResonator, YeeResonancesConvergeToTheAnalyticalFrequencies)
{
    // A box that the grids represent exactly: 30 x 18 x 42 mm.
    CavityResonatorSpec s;
    s.a = 0.030;
    s.b = 0.018;
    s.d = 0.042;
    const auto analytical = analyzeRectangularCavity(s, 3).value();
    std::vector<double> errors;
    for (const int cells : {6, 12}) {
        CavityYeeSettings settings;
        settings.cellsAcrossSmallest = cells;
        settings.durationPeriods = 150;
        auto run = simulateRectangularCavity(s, settings);
        ASSERT_TRUE(run) << run.message();
        ASSERT_FALSE(run.value().resonancesHz.empty());
        double nearest = 1e300;
        for (double f : run.value().resonancesHz)
            if (std::abs(f - analytical[0].frequencyHz) < std::abs(nearest - analytical[0].frequencyHz))
                nearest = f;
        errors.push_back(nearest / analytical[0].frequencyHz - 1.0);
        EXPECT_NEAR(run.value().simulatedSize[0], s.a, 1e-12);
    }
    EXPECT_LT(std::abs(errors[1]), std::abs(errors[0]));  // refinement converges
    EXPECT_LT(std::abs(errors[1]), 0.005);
}

TEST(CavityResonator, YeeEnergyDecayOfALossyFillIsSigmaOverEpsilon)
{
    CavityResonatorSpec s;
    s.a = 0.030;
    s.b = 0.018;
    s.d = 0.042;
    s.fillPermittivity = 2.0;
    const double f1 = analyzeRectangularCavity(s, 1).value()[0].frequencyHz;
    s.fillConductivity = 2.0 * kPi * f1 * kEpsilon0 * 2.0 / 100.0;  // Q = 100
    CavityYeeSettings settings;
    settings.cellsAcrossSmallest = 6;
    settings.durationPeriods = 60;
    auto run = simulateRectangularCavity(s, settings);
    ASSERT_TRUE(run) << run.message();
    const double expected = s.fillConductivity / (kEpsilon0 * 2.0);
    EXPECT_NEAR(run.value().energyDecayRate / expected, 1.0, 0.03);
    // The analytical fill Q of the same cavity.
    EXPECT_NEAR(analyzeRectangularCavity(s, 1).value()[0].fillQ, 100.0, 1e-9);
}

TEST(CavityResonator, InvalidCavitiesAreRefused)
{
    CavityResonatorSpec s = wr90Cavity();
    s.b = 0.0;
    EXPECT_FALSE(analyzeRectangularCavity(s, 3));
    s = wr90Cavity();
    s.fillPermittivity = 0.5;
    EXPECT_FALSE(analyzeRectangularCavity(s, 3));
    s = wr90Cavity();
    EXPECT_FALSE(analyzeRectangularCavity(s, 0));
    CavityYeeSettings bad;
    bad.cellsAcrossSmallest = 2;
    EXPECT_FALSE(simulateRectangularCavity(wr90Cavity(), bad));
}
