// Nonlinear and parametric resonance (RES-RSN-010): the Duffing backbone by
// harmonic balance against time integration, bistability and hysteresis of
// the forced response, and the Mathieu instability tongue by Floquet analysis.
#include "oscillators/Nonlinear.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant::oscillators;

TEST(NonlinearResonance, DuffingBackboneMatchesTimeIntegration)
{
    DuffingOscillator d;
    d.mass = 1.0;
    d.stiffness = 1.0;
    for (double k3 : {0.05, 0.2, -0.05}) {
        d.cubic = k3;
        for (double a : {0.5, 1.0}) {
            auto measured = duffingMeasuredFrequency(d, a, 40);
            ASSERT_TRUE(measured) << measured.message();
            const double hb = duffingBackbone(d, a);
            // First-harmonic balance is accurate to O((k3 A^2 / k)^2).
            EXPECT_NEAR(measured.value() / hb, 1.0, 4e-3 + 0.05 * std::pow(k3 * a * a, 2)) << k3 << " " << a;
        }
    }
    d.cubic = 0.0;
    EXPECT_NEAR(duffingMeasuredFrequency(d, 1.0).value(), 1.0, 1e-6);  // linear: amplitude-independent
}

TEST(NonlinearResonance, AHardeningDuffingOscillatorIsBistableAndHysteretic)
{
    DuffingOscillator d;
    d.mass = 1.0;
    d.stiffness = 1.0;
    d.damping = 0.02;
    d.cubic = 0.5;
    const double force = 0.03;
    // Above the linear resonance three steady amplitudes coexist (the middle one unstable).
    const auto branches = duffingResponse(d, force, 1.15);
    ASSERT_EQ(branches.size(), 3u);
    EXPECT_TRUE(branches[0].stable);
    EXPECT_FALSE(branches[1].stable);
    EXPECT_TRUE(branches[2].stable);
    // Time integration lands on the low or the high branch depending on where it starts.
    auto low = duffingForcedAmplitude(d, force, 1.15, 0.0);
    auto high = duffingForcedAmplitude(d, force, 1.15, branches[2].amplitude);
    ASSERT_TRUE(low && high);
    EXPECT_NEAR(low.value() / branches[0].amplitude, 1.0, 0.05);
    EXPECT_NEAR(high.value() / branches[2].amplitude, 1.0, 0.05);
    // Far from resonance: one solution, the linear one.
    const auto one = duffingResponse(d, force, 0.5);
    ASSERT_EQ(one.size(), 1u);
}

TEST(NonlinearResonance, MathieuTongueAroundTwiceTheNaturalFrequency)
{
    MathieuOscillator m;
    m.naturalOmega = 1.0;
    m.modulation = 0.1;
    // Undamped: the Mathieu boundaries a = 1 +- q - q^2/8 with a = 4 w0^2 / W^2, q = a h / 2
    // (W = 1.94967 and 2.04969 for h = 0.1; first order: 2 w0 (1 -+ h/4)).
    const auto tongue = mathieuTongue(m);
    ASSERT_EQ(tongue.size(), 2u);
    const auto edge = [](double sign) {  // a (1 - sign h/2) + a^2 h^2 / 32 = 1
        const double h = 0.1, A = h * h / 32, B = 1 - sign * h / 2;
        const double a = (-B + std::sqrt(B * B + 4 * A)) / (2 * A);
        return 2.0 / std::sqrt(a);
    };
    EXPECT_NEAR(tongue[0], edge(1.0), 2e-4);
    EXPECT_NEAR(tongue[1], edge(-1.0), 2e-4);
    EXPECT_GT(mathieuMultiplier(m, 2.0), 1.0);
    EXPECT_LE(mathieuMultiplier(m, 1.5), 1.0 + 1e-9);  // ordinary pumping off the tongue: stable
    // Damping raises the threshold: zeta = 0.03 needs h > 4 zeta = 0.12 at W = 2 w0.
    m.dampingRatio = 0.03;
    m.modulation = 0.10;
    EXPECT_TRUE(mathieuTongue(m).empty());
    m.modulation = 0.14;
    EXPECT_FALSE(mathieuTongue(m).empty());
    EXPECT_FALSE((MathieuOscillator{1.0, 0.0, 1.2}.validate()));
}
