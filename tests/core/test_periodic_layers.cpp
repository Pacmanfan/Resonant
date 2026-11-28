// Periodic two-layer electromagnetic unit cells (RES-FUNC-010): band gaps of
// a quarter-wave stack, the attenuation inside a gap against the exact
// finite-stack transmission, and the homogenized long-wavelength index.
#include "electromagnetics/Conventions.h"
#include "electromagnetics/PeriodicLayers.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::em;

namespace {
constexpr double f0 = 10e9, n1 = 1.5, n2 = 2.5;

PeriodicCell quarterWave()
{
    PeriodicCell c;
    c.thickness1 = kSpeedOfLight / (4 * n1 * f0);
    c.permittivity1 = n1 * n1;
    c.thickness2 = kSpeedOfLight / (4 * n2 * f0);
    c.permittivity2 = n2 * n2;
    return c;
}
}  // namespace

TEST(PeriodicLayers, AQuarterWaveStackHasItsGapWhereTheTheoryPutsIt)
{
    const auto gaps = bandGaps(quarterWave(), 1.5 * f0);
    ASSERT_EQ(gaps.size(), 1u);
    EXPECT_NEAR(gaps[0].centerHz / f0, 1.0, 1e-8);
    // Relative width (4/pi) arcsin(|n2 - n1| / (n2 + n1)).
    EXPECT_NEAR(gaps[0].relativeWidth, 4.0 / kPi * std::asin((n2 - n1) / (n2 + n1)), 1e-8);
    EXPECT_TRUE(gaps[0].atZoneEdge);
    // At the centre ln(n2/n1) Np per cell: |cos(Ka)| = (n1/n2 + n2/n1) / 2.
    EXPECT_NEAR(gaps[0].peakAttenuation, std::acosh(0.5 * (n1 / n2 + n2 / n1)), 1e-6);
    EXPECT_NEAR(gaps[0].peakAttenuation, std::log(n2 / n1), 1e-6);
}

TEST(PeriodicLayers, TheFiniteStackTransmissionFollowsTheBlochAttenuation)
{
    const PeriodicCell cell = quarterWave();
    const auto transmittance = [&](int periods, double f) {
        return LayeredSolution::solve(periodicStack(cell, periods, f)).value().transmittance();
    };
    const double attenuation = blochPoint(cell, f0).wavenumber.imag() * cell.period();
    // Deep in the gap T decays as exp(-2 Im(K) a N).
    const double slope = std::log(transmittance(14, f0) / transmittance(8, f0)) / 6.0;
    EXPECT_NEAR(slope / (-2.0 * attenuation), 1.0, 0.02);
    EXPECT_LT(transmittance(10, f0), 1e-3);
    // In the first band the stack passes most of the power on average.
    double mean = 0.0;
    for (int i = 0; i < 50; ++i)
        mean += transmittance(10, (0.2 + 0.4 * i / 49.0) * f0) / 50.0;
    EXPECT_GT(mean, 0.5);
    // R + T = 1 (lossless).
    const auto solution = LayeredSolution::solve(periodicStack(cell, 10, 0.93 * f0)).value();
    EXPECT_NEAR(solution.reflectance() + solution.transmittance(), 1.0, 1e-12);
}

TEST(PeriodicLayers, TheEffectiveIndexTendsToTheQuasiStaticValue)
{
    const PeriodicCell cell = quarterWave();
    const EffectiveMedium e = effectiveMedium(cell);
    const double parallel = (cell.permittivity1 * cell.thickness1 + cell.permittivity2 * cell.thickness2) / cell.period();
    EXPECT_NEAR(e.permittivityParallel, parallel, 1e-12);
    EXPECT_LT(e.permittivityPerpendicular, e.permittivityParallel);  // a layered medium is uniaxial
    auto index = blochEffectiveIndex(cell, 1e-3 * f0);
    ASSERT_TRUE(index) << index.message();
    EXPECT_NEAR(index.value() / e.index, 1.0, 1e-5);
    EXPECT_LT(e.validBelowHz, f0);  // the gap lies far outside the homogenization range
    EXPECT_FALSE(blochEffectiveIndex(cell, f0));  // in the gap: no effective index
}

TEST(PeriodicLayers, InvalidCellsAreRefused)
{
    PeriodicCell c = quarterWave();
    c.thickness2 = 0.0;
    EXPECT_FALSE(c.validate().ok());
    EXPECT_TRUE(bandGaps(c, f0).empty());
    c = quarterWave();
    c.permittivity1 = 0.5;
    EXPECT_FALSE(blochEffectiveIndex(c, f0));
}
