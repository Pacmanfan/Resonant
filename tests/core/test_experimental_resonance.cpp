// Experimental resonance (RES-LAB-012): peaks of a measured response classified with
// their evidence - natural modes, an excitation peak, mains interference, unclassified
// without phase - and matched with predictions; a ring-down's frequency, Q and drift.
#include "laboratory/ExperimentalResonance.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::laboratory;

namespace {

constexpr double kPi = 3.14159265358979323846;

std::complex<double> mode(double f, double f0, double q, double a)
{
    const double r = f / f0;
    return a / std::complex<double>(1.0 - r * r, r / q);
}

}  // namespace

TEST(ExperimentalResonance, PeaksAreClassifiedWithEvidence)
{
    std::vector<double> f;
    std::vector<std::complex<double>> H;
    for (int i = 0; i < 4000; ++i) {
        const double x = 20.0 + 380.0 * i / 3999;
        f.push_back(x);
        std::complex<double> h = mode(x, 120.0, 30.0, 1.0) + mode(x, 260.0, 40.0, 0.5);
        // A narrow line at 150 Hz (three times 50 Hz) whose phase hardly turns, and a forced peak near 330 Hz.
        // (strong enough to be candidates: the fitter considers peaks above 5 % of the largest power)
        h += 15.0 * std::exp(-std::pow((x - 150.0) / 0.4, 2));
        h += 10.0 * std::exp(-std::pow((x - 330.0) / 3.0, 2));
        H.push_back(h);
    }
    ResonanceOptions o;
    o.referencePeaksHz = {330.0};
    o.predictedHz = {118.0, 265.0};
    auto r = resonancesFromResponse(f, H, true, o);
    ASSERT_TRUE(r) << r.message();
    const auto near = [&](double hz) -> const ResonanceCandidate* {
        for (const auto& c : r.value())
            if (std::abs(c.frequencyHz - hz) < 3.0)
                return &c;
        return nullptr;
    };
    ASSERT_NE(near(120.0), nullptr);
    EXPECT_EQ(near(120.0)->classification, "natural mode");
    EXPECT_NEAR(near(120.0)->quality, 30.0, 3.0);
    EXPECT_EQ(near(120.0)->predictedIndex, 0);
    ASSERT_NE(near(260.0), nullptr);
    EXPECT_EQ(near(260.0)->classification, "natural mode");
    EXPECT_EQ(near(260.0)->predictedIndex, 1);
    ASSERT_NE(near(150.0), nullptr);
    EXPECT_EQ(near(150.0)->classification, "possible interference");
    ASSERT_NE(near(330.0), nullptr);
    EXPECT_EQ(near(330.0)->classification, "excitation or instrument peak");
    for (const auto& c : r.value())
        EXPECT_FALSE(c.evidence.empty());
    // Without the phase nothing is called a mode.
    auto blind = resonancesFromResponse(f, H, false, {});
    ASSERT_TRUE(blind);
    for (const auto& c : blind.value())
        EXPECT_NE(c.classification, "natural mode");
}

TEST(ExperimentalResonance, RingDownGivesFrequencyQAndDrift)
{
    const double f0 = 100.0, q = 50.0, alpha = kPi * f0 / q, dt = 1.0 / 5000.0;
    std::vector<double> x;
    for (int i = 0; i < 4000; ++i)
        x.push_back(std::exp(-alpha * i * dt) * std::cos(2 * kPi * f0 * i * dt + 0.3));
    auto r = ringDown(x, dt, 10.0, 1000.0);
    ASSERT_TRUE(r) << r.message();
    EXPECT_NEAR(r.value().frequencyHz, f0, 1e-3);
    EXPECT_NEAR(r.value().quality, q, 0.01 * q);
    EXPECT_NEAR(r.value().energyDecayRate, 2 * alpha, 0.01 * 2 * alpha);
    EXPECT_NEAR(r.value().dampingRatio, 1.0 / (2 * q), 1e-3);
    EXPECT_NEAR(r.value().frequencyDriftHz, 0.0, 1e-2);
    EXPECT_EQ(r.value().assumptions.size(), 3u);
    EXPECT_NE(r.value().assumptions.back().find("model-derived"), std::string::npos);
}
