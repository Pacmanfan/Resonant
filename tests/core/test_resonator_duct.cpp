// A duct loaded by Helmholtz resonators (RES-META-007): the cell transfer matrix, the
// long-wave speed against the effective medium, the hybridization gap at the resonance,
// the Bragg gap edge at c / 2a and a finite array's attenuation against Bloch.
#include "manipulators/ResonatorDuct.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;

namespace {
constexpr double kPi = 3.14159265358979323846;

manipulators::ResonatorDuctSpec duct(bool loss = false)
{
    manipulators::ResonatorDuctSpec d;
    d.ductRadius = 0.025;
    d.spacing = 0.05;
    d.resonator.cavityRadius = 0.02;
    d.resonator.cavityLength = 0.05;
    d.resonator.neckRadius = 0.005;
    d.resonator.neckLength = 0.01;
    d.resonator.viscousLoss = loss;
    return d;
}

double branchF0(const manipulators::ResonatorDuctSpec& d)
{
    manipulators::HelmholtzSpec h = d.resonator;
    h.radiationLoss = false;
    return manipulators::analyzeHelmholtz(h).value().frequencyHz;
}
}  // namespace

TEST(ResonatorDuct, TheCellIsReciprocalAndLongWavesAreSlowed)
{
    const auto d = duct();
    ASSERT_TRUE(d.validate().ok()) << d.validate().message();
    const auto t = manipulators::resonatorDuctCell(d, 123.0);
    EXPECT_NEAR(std::abs(t[0] * t[3] - t[1] * t[2] - 1.0), 0.0, 1e-12);
    const double f0 = branchF0(d), ratio = d.resonator.volume() / (d.ductArea() * d.spacing);
    const double f = 0.02 * f0;
    const double speed = 2 * kPi * f / manipulators::resonatorDuctBloch(d, f).real();
    EXPECT_NEAR(speed, d.resonator.soundSpeed / std::sqrt(1.0 + ratio), 1e-3 * speed);
    // The effective bulk modulus at low frequency: B0 / (1 + V / (S a)).
    const double B0 = d.resonator.density * d.resonator.soundSpeed * d.resonator.soundSpeed;
    EXPECT_NEAR(manipulators::resonatorDuctBulkModulus(d, 1e-3 * f0).real(), B0 / (1.0 + ratio), 1e-4 * B0);
}

TEST(ResonatorDuct, TheHybridizationGapStraddlesTheResonance)
{
    const auto d = duct();
    const double f0 = branchF0(d), ratio = d.resonator.volume() / (d.ductArea() * d.spacing);
    const auto gaps = manipulators::resonatorDuctGaps(d, 1.5 * d.resonator.soundSpeed / (2 * d.spacing), 8000);
    ASSERT_GE(gaps.size(), 2u);
    EXPECT_LT(gaps[0].lowerHz, f0);
    EXPECT_GT(gaps[0].upperHz, f0);
    // The effective medium predicts B_eff < 0 up to f0 sqrt(1 + V / (S a)); the gap closes near it.
    EXPECT_NEAR(gaps[0].upperHz, f0 * std::sqrt(1.0 + ratio), 0.1 * f0);
    EXPECT_LT(manipulators::resonatorDuctBulkModulus(d, 1.05 * f0).real(), 0.0);
    // The Bragg gap opens at ka = pi exactly (the shunt does not change cos(qa) = -1 there).
    const double bragg = d.resonator.soundSpeed / (2 * d.spacing);
    bool edge = false;
    for (const auto& g : gaps)
        edge = edge || std::abs(g.lowerHz - bragg) < 1e-6 * bragg || std::abs(g.upperHz - bragg) < 1e-6 * bragg;
    EXPECT_TRUE(edge);
}

TEST(ResonatorDuct, AFiniteArrayAttenuatesAtTheBlochRate)
{
    const auto d = duct(true);
    const double f0 = branchF0(d);
    const auto gaps = manipulators::resonatorDuctGaps(duct(), 2.0 * f0, 4000);
    ASSERT_FALSE(gaps.empty());
    const double f = 0.5 * (gaps[0].lowerHz + gaps[0].upperHz);
    const double kappa = manipulators::resonatorDuctBloch(d, f).imag() * d.spacing;
    const double t6 = manipulators::resonatorDuctTransmission(d, f, 6), t10 = manipulators::resonatorDuctTransmission(d, f, 10);
    const double measured = (10 * std::log10(t6) - 10 * std::log10(t10)) / 4.0;
    EXPECT_NEAR(measured, 8.685889638065035 * kappa, 0.01 * 8.685889638065035 * kappa);
    // Lossless arrays never transmit more than they receive.
    for (double x = 0.1; x < 3.0; x += 0.05)
        EXPECT_LE(manipulators::resonatorDuctTransmission(duct(), x * f0, 8), 1.0 + 1e-12);
}

TEST(ResonatorDuct, ValidationRefusesNecksWiderThanTheDuct)
{
    auto d = duct();
    d.resonator.neckRadius = 0.03;
    EXPECT_FALSE(d.validate().ok());
    d = duct();
    d.spacing = 0.0;
    EXPECT_FALSE(d.validate().ok());
}
