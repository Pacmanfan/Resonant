// Split-ring arrays as a circuit model (RES-META-008): the circuit values, the Lorentzian
// permeability with its negative band, passivity and causality (Kramers-Kronig).
#include "electromagnetics/Conventions.h"
#include "electromagnetics/SplitRing.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;

namespace {
constexpr double kPi = 3.14159265358979323846;

em::SplitRingSpec ring()
{
    em::SplitRingSpec s;
    s.radius = 2e-3;
    s.width = 1e-3;
    s.gap = 5e-5;
    s.metalThickness = 35e-6;
    s.latticeSpacing = 5.5e-3;
    s.substratePermittivity = 10.2;
    s.substrateLossTangent = 0.0023;
    return s;
}
}  // namespace

TEST(SplitRing, CircuitValuesFollowTheFormulas)
{
    const auto s = ring();
    auto m = em::splitRingModel(s);
    ASSERT_TRUE(m) << m.message();
    const double rw = 0.25 * (s.width + s.metalThickness);
    const double L = em::kMu0 * s.radius * (std::log(8 * s.radius / rw) - 2);
    const double C = em::kEpsilon0 * 0.5 * (1 + s.substratePermittivity) * (s.width * s.metalThickness / s.gap + s.width + s.metalThickness);
    EXPECT_NEAR(m.value().inductance, L, 1e-15 * L / L);
    EXPECT_NEAR(m.value().capacitance, C, 1e-6 * C);
    EXPECT_NEAR(m.value().resonanceHz, 1 / (2 * kPi * std::sqrt(L * C)), 1.0);
    EXPECT_NEAR(m.value().fillingFactor, kPi * 4e-6 / (5.5e-3 * 5.5e-3), 1e-12);
    EXPECT_NEAR(1 / m.value().quality, 1 / m.value().qConductor + 1 / m.value().qDielectric, 1e-12);
    EXPECT_LT(m.value().cellOverWavelength, 0.2);
}

TEST(SplitRing, PermeabilityIsLorentzianWithANegativeBand)
{
    auto m = em::splitRingModel(ring());
    ASSERT_TRUE(m);
    const auto& x = m.value();
    EXPECT_NEAR(em::splitRingPermeability(x, 1e-3 * x.resonanceHz).real(), 1.0, 1e-5);
    EXPECT_NEAR(em::splitRingPermeability(x, 1e3 * x.resonanceHz).real(), 1.0 - x.fillingFactor, 1e-5);
    // Paramagnetic below the resonance, negative between f0 and the magnetic plasma frequency.
    EXPECT_GT(em::splitRingPermeability(x, 0.8 * x.resonanceHz).real(), 1.0);
    EXPECT_LT(em::splitRingPermeability(x, 1.05 * x.resonanceHz).real(), 0.0);
    EXPECT_LT(em::splitRingPermeability(x, 0.98 * x.magneticPlasmaHz).real(), 0.0);
    EXPECT_GT(em::splitRingPermeability(x, 1.02 * x.magneticPlasmaHz).real(), 0.0);
    // Passive: Im mu <= 0 (e^{+jwt}) everywhere.
    for (double r = 0.1; r < 3.0; r += 0.01)
        EXPECT_LE(em::splitRingPermeability(x, r * x.resonanceHz).imag(), 0.0);
}

TEST(SplitRing, TheModelIsCausal)
{
    auto m = em::splitRingModel(ring());
    ASSERT_TRUE(m);
    const double f0 = m.value().resonanceHz;
    EXPECT_LT(em::splitRingKramersKronigResidual(m.value(), {0.5 * f0, 0.95 * f0, 1.05 * f0, 2.0 * f0}), 2e-3);
}

TEST(SplitRing, LossNarrowsTheNegativeBandAndBadRingsAreRefused)
{
    auto low = em::splitRingModel(ring());
    auto spec = ring();
    spec.substrateLossTangent = 0.05;
    auto high = em::splitRingModel(spec);
    ASSERT_TRUE(low && high);
    EXPECT_LT(high.value().quality, low.value().quality);
    const double f = 1.02 * low.value().resonanceHz;
    EXPECT_GT(em::splitRingPermeability(high.value(), f).real(), em::splitRingPermeability(low.value(), f).real());
    spec = ring();
    spec.latticeSpacing = 4e-3;  // the ring no longer fits
    EXPECT_FALSE(em::splitRingModel(spec));
    spec = ring();
    spec.gap = 0.0;
    EXPECT_FALSE(em::splitRingModel(spec));
}
