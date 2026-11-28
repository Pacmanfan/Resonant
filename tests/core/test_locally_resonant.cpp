// Bloch analysis of lumped lattices and the mass-in-mass locally resonant lattice
// (RES-META-003, RES-META-006, docs/physics/METAMATERIAL_VALIDATION.md): numerical cell
// eigenproblems against closed forms, the gap against the effective-medium prediction,
// and a finite chain against the Bloch attenuation.
#include "mechanics/LocallyResonantLattice.h"
#include "oscillators/BlochLattice.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;

namespace {
constexpr double kPi = 3.14159265358979323846;

mechanics::MassInMassCell cell()
{
    mechanics::MassInMassCell c;
    c.hostMass = 0.1;
    c.hostStiffness = 4e6;
    c.resonatorMass = 0.1;
    c.resonatorStiffness = 1e5;
    c.spacing = 0.02;
    return c;
}
}  // namespace

TEST(Bloch, MonatomicChainMatchesTheClosedForm)
{
    // One mass m per cell on springs k: w = 2 sqrt(k / m) |sin(q a / 2)|.
    oscillators::BlochCell c;
    c.mass = Eigen::MatrixXd::Constant(1, 1, 2.0);
    c.stiffness = Eigen::MatrixXd::Zero(1, 1);
    c.links = {{0, 0, 800.0}};
    for (double q : {0.0, 0.1, 0.37, 0.5, 0.9, 1.0}) {
        const double exact = 2.0 * std::sqrt(800.0 / 2.0) * std::abs(std::sin(0.5 * q * kPi)) / (2 * kPi);
        EXPECT_NEAR(oscillators::blochFrequencies(c, q)[0], exact, 1e-9 * (exact + 1.0)) << q;
    }
}

TEST(Bloch, DiatomicChainHasTheBraggGapAtTheZoneEdge)
{
    // Alternating masses m1 > m2 joined by k: at q a = pi the bands are sqrt(2k/m1) and sqrt(2k/m2).
    const double m1 = 3.0, m2 = 1.0, k = 1000.0;
    oscillators::BlochCell c;
    c.mass = Eigen::Matrix2d::Zero();
    c.mass(0, 0) = m1;
    c.mass(1, 1) = m2;
    c.stiffness = Eigen::Matrix2d::Zero();
    c.stiffness << k, -k, -k, k;
    c.links = {{1, 0, k}};
    const oscillators::BlochBands b = oscillators::blochBands(c, 201);
    ASSERT_EQ(b.gaps.size(), 1u);
    EXPECT_NEAR(b.gaps[0].first, std::sqrt(2 * k / m1) / (2 * kPi), 1e-9);
    EXPECT_NEAR(b.gaps[0].second, std::sqrt(2 * k / m2) / (2 * kPi), 1e-9);
    // The optical branch at q = 0: sqrt(2k (1/m1 + 1/m2)).
    EXPECT_NEAR(b.frequencies[1][0], std::sqrt(2 * k * (1 / m1 + 1 / m2)) / (2 * kPi), 1e-9);
}

TEST(LocallyResonantLattice, NumericalBlochMatchesTheClosedForm)
{
    const auto c = cell();
    const auto bloch = mechanics::massInMassBlochCell(c);
    for (double q : {0.05, 0.3, 0.5, 0.77, 1.0}) {
        const auto exact = mechanics::massInMassDispersion(c, q);
        const auto numeric = oscillators::blochFrequencies(bloch, q);
        EXPECT_NEAR(numeric[0], exact[0], 1e-9 * exact[0]) << q;
        EXPECT_NEAR(numeric[1], exact[1], 1e-9 * exact[1]) << q;
    }
}

TEST(LocallyResonantLattice, TheGapIsWhereTheEffectiveMassIsNegative)
{
    const auto c = cell();
    const double f2 = mechanics::localResonanceHz(c);
    EXPECT_NEAR(f2, 1000.0 / (2 * kPi), 1e-9);
    const auto g = mechanics::massInMassGap(c);
    // The optical branch starts at w2 sqrt(1 + m2/m1), exactly where m_eff returns to zero.
    EXPECT_NEAR(g.upperHz, f2 * std::sqrt(2.0), 1e-9 * f2);
    EXPECT_NEAR(g.upperHz, g.negativeMassUpperHz, 1e-9 * f2);
    EXPECT_NEAR(mechanics::effectiveMass(c, g.upperHz).real(), 0.0, 1e-9);
    // The acoustic branch ends just below the local resonance (k1 >> k2).
    EXPECT_LT(g.lowerHz, f2);
    EXPECT_GT(g.lowerHz, 0.99 * f2);
    for (double f : {0.5 * f2, 0.9 * f2, 1.5 * g.upperHz})
        EXPECT_GT(mechanics::effectiveMass(c, f).real(), 0.0) << f;
    for (double f : {1.01 * f2, 0.5 * (f2 + g.upperHz), 0.99 * g.upperHz})
        EXPECT_LT(mechanics::effectiveMass(c, f).real(), 0.0) << f;
    // Inside the gap the Bloch wave decays: cos(q a) > 1 where m_eff < 0.
    const double centre = 0.5 * (g.lowerHz + g.upperHz);
    const double w = 2 * kPi * centre;
    const double expected = std::acosh(1.0 - w * w * mechanics::effectiveMass(c, centre).real() / (2 * c.hostStiffness));
    EXPECT_NEAR(mechanics::massInMassBlochWavenumber(c, centre).imag() * c.spacing, expected, 1e-9);
    // Outside the gap it propagates.
    EXPECT_NEAR(mechanics::massInMassBlochWavenumber(c, 0.5 * f2).imag(), 0.0, 1e-6);
}

TEST(LocallyResonantLattice, AFiniteChainDecaysAtTheBlochRate)
{
    const auto c = cell();
    const auto g = mechanics::massInMassGap(c);
    const double centre = 0.5 * (g.lowerHz + g.upperHz);
    // Long enough that the wave reflected by the free far end does not reach the measured hosts.
    const int cells = 40;
    const auto chain = mechanics::massInMassChain(c, cells);
    ASSERT_TRUE(chain.validate().ok());
    std::vector<std::complex<double>> force(static_cast<size_t>(chain.size()), 0.0);
    force[0] = 1.0;
    auto r = oscillators::drivenResponse(chain, force, {centre});
    ASSERT_TRUE(r) << r.message();
    const auto& u = r.value()[0].displacement;
    const double decay = -std::log(std::abs(u[2 * 20]) / std::abs(u[2 * 2])) / 18.0;
    const double bloch = mechanics::massInMassBlochWavenumber(c, centre).imag() * c.spacing;
    EXPECT_GT(bloch, 0.1);
    EXPECT_NEAR(decay, bloch, 0.01 * bloch);
}

TEST(LocallyResonantLattice, LongWavesMoveTheWholeCell)
{
    const auto c = cell();
    const double q = 2e-4;  // w / w2 ~ 3e-3: the resonator's own dispersion is 4e-6
    const double speed = 2 * kPi * mechanics::massInMassDispersion(c, q)[0] / (q * kPi / c.spacing);
    EXPECT_NEAR(speed, c.spacing * std::sqrt(c.hostStiffness / (c.hostMass + c.resonatorMass)), 1e-4 * speed);
}

TEST(LocallyResonantLattice, LossAttenuatesThePassBandAndValidationRefusesBadCells)
{
    auto c = cell();
    c.resonatorLossFactor = 0.05;
    EXPECT_GT(mechanics::massInMassBlochWavenumber(c, 0.9 * mechanics::localResonanceHz(c)).imag(), 0.0);
    EXPECT_TRUE(c.validate().ok());
    c.hostMass = 0.0;
    EXPECT_FALSE(c.validate().ok());
    c = cell();
    c.resonatorLossFactor = 1.5;
    EXPECT_FALSE(c.validate().ok());
}
