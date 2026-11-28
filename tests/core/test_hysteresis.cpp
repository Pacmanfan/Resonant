// Preisach hysteresis (RES-DMS-008): the first-order reversal curve and the major loop's
// area against the uniform-density closed form, return-point memory (wiping out),
// congruent minor loops, and the reversible part adding no loss.
#include "electromagnetics/Hysteresis.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant::em;

namespace {
PreisachParameters uniform(int levels = 200, double chi = 0.0)
{
    PreisachParameters p;
    p.saturationMagnetization = 1e6;
    p.saturationField = 1000.0;
    p.reversibleSusceptibility = chi;
    p.levels = levels;
    return p;
}

// Loss per cycle per volume / mu0: the closed integral of H dM over the sampled loop.
double loopArea(PreisachModel& m, double Hmax, int n)
{
    double area = 0.0, H0 = m.field(), M0 = m.magnetization();
    for (int i = 1; i <= 2 * n; ++i) {
        const double H = i <= n ? Hmax - 2.0 * Hmax * i / n : -Hmax + 2.0 * Hmax * (i - n) / n;
        const double M = m.apply(H);
        area += 0.5 * (H + H0) * (M - M0);
        H0 = H;
        M0 = M;
    }
    return area;  // the closed integral of H dM: positive for a hysteretic (lossy) loop
}
}  // namespace

TEST(Hysteresis, TheAscendingBranchAndTheMajorLoopFollowTheClosedForm)
{
    auto m = PreisachModel::create(uniform());
    ASSERT_TRUE(m) << m.message();
    PreisachModel& x = m.value();
    x.reset();
    EXPECT_NEAR(x.magnetization(), -1e6, 1e-6);
    for (double H = -1000.0; H <= 1000.0; H += 50.0) {
        const double expected = 1e6 * (-1.0 + (H + 1000.0) * (H + 1000.0) / (2.0 * 1e6));
        EXPECT_NEAR(x.apply(H), expected, 2.0 / 200 * 1e6) << H;
    }
    EXPECT_NEAR(x.magnetization(), 1e6, 1e-6);  // positive saturation
    const double area = loopArea(x, 1000.0, 400);
    EXPECT_NEAR(area, 4.0 / 3.0 * 1e6 * 1000.0, 3.0 / 200 * area);
}

TEST(Hysteresis, ReturnPointMemoryWipesOutMinorLoops)
{
    auto m = PreisachModel::create(uniform(120));
    ASSERT_TRUE(m);
    PreisachModel& x = m.value();
    x.reset();
    x.apply(600.0);
    x.apply(-200.0);
    const auto before = x.relays();
    const double Mbefore = x.magnetization();
    // A minor loop inside: up to 300, down to -100, up to 300 again ...
    x.apply(300.0);
    x.apply(-100.0);
    x.apply(300.0);
    // ... and back down to -200: the state is as before the minor loop.
    x.apply(-200.0);
    EXPECT_EQ(x.relays(), before);
    EXPECT_DOUBLE_EQ(x.magnetization(), Mbefore);
}

TEST(Hysteresis, MinorLoopsBetweenTheSameFieldsAreCongruent)
{
    auto m = PreisachModel::create(uniform(160));
    ASSERT_TRUE(m);
    PreisachModel& x = m.value();
    const auto loop = [&](double start) {
        x.reset();
        x.apply(800.0);
        x.apply(start);  // different histories ...
        x.apply(400.0);  // ... then cycles between the same reversal fields 400 and 100
        std::vector<double> M;
        for (double H = 400.0; H >= 100.0; H -= 25.0)
            M.push_back(x.apply(H));
        for (double H = 100.0; H <= 400.0; H += 25.0)
            M.push_back(x.apply(H));
        const double first = M.front();
        for (double& v : M)
            v -= first;
        return M;
    };
    const auto a = loop(-300.0), b = loop(-600.0);
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i)
        EXPECT_NEAR(a[i], b[i], 1e-6) << i;
}

TEST(Hysteresis, TheReversiblePartAddsNoLossAndGaussianLoopsHaveACoercivity)
{
    auto plain = PreisachModel::create(uniform(100));
    auto withReversible = PreisachModel::create(uniform(100, 50.0));
    ASSERT_TRUE(plain && withReversible);
    plain.value().reset();
    withReversible.value().reset();
    plain.value().apply(1000.0);
    withReversible.value().apply(1000.0);
    EXPECT_NEAR(loopArea(withReversible.value(), 1000.0, 200), loopArea(plain.value(), 1000.0, 200), 1e-6 * loopArea(plain.value(), 1000.0, 200));
    PreisachParameters g = uniform(200);
    g.density = "gaussian";
    g.coercivity = 300.0;
    g.coerciveSpread = 80.0;
    g.interactionSpread = 60.0;
    auto gm = PreisachModel::create(g);
    ASSERT_TRUE(gm) << gm.message();
    PreisachModel& x = gm.value();
    x.reset();
    x.apply(1000.0);
    // The descending branch crosses M = 0 near -Hc.
    double crossing = 0.0, previous = x.magnetization();
    for (double H = 1000.0; H >= -1000.0; H -= 5.0) {
        const double M = x.apply(H);
        if (previous > 0.0 && M <= 0.0)
            crossing = H;
        previous = M;
    }
    EXPECT_NEAR(crossing, -300.0, 30.0);
}

TEST(Hysteresis, LoopLossEqualsTheHysteronSum)
{
    // RES-ACC-014: the loop integral against the relays' own dissipation, converging with the increment.
    resonant::em::PreisachParameters p;
    p.saturationMagnetization = 1e6;
    p.saturationField = 1000.0;
    p.levels = 100;
    double previous = 0.0;
    for (int n : {100, 1000}) {
        auto m = resonant::em::PreisachModel::create(p);
        ASSERT_TRUE(m);
        m.value().reset();
        const double a = 700.0, mu0 = 1.25663706212e-6;
        for (int i = 1; i <= n; ++i)
            m.value().apply(-1000.0 + 1700.0 * i / n);
        double area = 0.0, h0 = a, m0 = m.value().magnetization();
        for (int i = 1; i <= 4 * n; ++i) {
            const double h = i <= 2 * n ? a - a * i / n : -a + a * (i - 2 * n) / n;
            const double mm = m.value().apply(h);
            area += mu0 * 0.5 * (h + h0) * (mm - m0);
            h0 = h;
            m0 = mm;
        }
        const double relays = m.value().hysteronLoopLoss(-a, a);
        EXPECT_GT(relays, 0.0);
        EXPECT_LE(std::abs(area - relays), 2.0 * mu0 * p.saturationMagnetization * a / n);  // the increment bound
        if (previous > 0.0)
            EXPECT_LT(std::abs(area - relays), previous);
        previous = std::abs(area - relays);
    }
}

