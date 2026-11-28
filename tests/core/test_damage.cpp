// Damage and viscoelasticity (RES-DMS-006, -008, -011): the damage laws' curves,
// irreversibility and secant unloading, the energy split; the damaged cantilever's
// undamaged modes against Euler-Bernoulli, its energy balance, mesh objectivity of the
// non-softening law and the frequency drop computed from the damaged stiffness; the
// standard linear solid's exact relaxation and its loss per cycle.
#include "mechanics/Damage.h"
#include "mechanics/DamagedBeam.h"
#include "mechanics/Viscoelasticity.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant::mechanics;

namespace {
constexpr double kPi = 3.14159265358979323846;

DamageParameters law(const std::string& kind)
{
    DamageParameters p;
    p.youngsModulus = 30e9;
    p.thresholdStrain = 1e-4;
    p.softeningStrain = 1e-3;
    p.law = kind;
    p.exponent = 0.6;
    return p;
}

DamagedBeamSpec beam(int elements, double amplitude)
{
    DamagedBeamSpec s;
    s.length = 0.5;
    s.width = 0.03;
    s.thickness = 0.005;
    s.density = 2500.0;
    s.damage = law("power");
    s.elements = elements;
    for (int k = 1; k <= 40; ++k)
        s.tipDeflections.push_back(amplitude * k / 40.0);
    for (int k = 39; k >= 0; --k)
        s.tipDeflections.push_back(amplitude * k / 40.0);
    return s;
}
}  // namespace

TEST(Damage, LawsFollowTheirCurvesAndNeverHeal)
{
    for (const char* kind : {"exponential", "power"}) {
        const DamageParameters p = law(kind);
        ASSERT_TRUE(p.validate().ok());
        DamageState c;
        double previous = 0.0;
        for (int i = 1; i <= 50; ++i) {
            const double e = 2e-3 * i / 50;
            const DamageResponse r = damageUpdate(p, c, e);
            EXPECT_NEAR(r.stress, (1.0 - p.damage(e)) * p.youngsModulus * e, 1e-9 * p.youngsModulus * 1e-3);
            EXPECT_GE(r.state.damage, previous);
            previous = r.state.damage;
            c = r.state;
        }
        // Unloading is secant and adds no damage; reloading below kappa neither.
        const DamageResponse u = damageUpdate(p, c, 1e-3);
        EXPECT_FALSE(u.loading);
        EXPECT_EQ(u.state.damage, c.damage);
        EXPECT_NEAR(u.stress, (1.0 - c.damage) * p.youngsModulus * 1e-3, 1e-6);
        EXPECT_NEAR(u.tangent, (1.0 - c.damage) * p.youngsModulus, 1e-6);
    }
    // The exponential law softens at once; the power law keeps hardening.
    const DamageParameters e = law("exponential"), w = law("power");
    EXPECT_LT((1 - e.damage(2e-4)) * 2e-4, (1 - e.damage(1.01e-4)) * 1.01e-4);
    EXPECT_GT((1 - w.damage(2e-4)) * 2e-4, (1 - w.damage(1.01e-4)) * 1.01e-4);
    EXPECT_FALSE(law("brittle").validate().ok());
}

TEST(Damage, WorkIsStoredOrDissipated)
{
    const DamageParameters p = law("exponential");
    DamageState c;
    double work = 0.0, e0 = 0.0, s0 = 0.0;
    for (int i = 1; i <= 4000; ++i) {
        const double e = i <= 3000 ? 1.5e-3 * i / 3000 : 1.5e-3 * (4000 - i) / 1000 + 0.5e-3 * (i - 3000) / 1000;
        const DamageResponse r = damageUpdate(p, c, e);
        work += 0.5 * (r.stress + s0) * (e - e0);
        e0 = e;
        s0 = r.stress;
        c = r.state;
        if (i == 4000)
            EXPECT_NEAR(work, r.storedEnergy + c.dissipated, 2e-3 * work);
    }
}

TEST(DamagedBeam, UndamagedModesMatchEulerBernoulli)
{
    const DamagedBeamSpec s = beam(40, 0.0);
    auto r = damagedCantilever(s);
    ASSERT_TRUE(r) << r.message();
    const double E = s.damage.youngsModulus, I = s.width * std::pow(s.thickness, 3) / 12, A = s.width * s.thickness;
    const double beta[3] = {1.875104068711961, 4.694091132974175, 7.854757438237613};
    for (int k = 0; k < 3; ++k) {
        const double f = beta[k] * beta[k] / (2 * kPi) * std::sqrt(E * I / (s.density * A * std::pow(s.length, 4)));
        EXPECT_NEAR(r.value().undamagedFrequencies[static_cast<size_t>(k)], f, 1e-5 * f) << k;
    }
    EXPECT_NEAR(r.value().undamagedTipStiffness, 3 * E * I / std::pow(s.length, 3), 1e-9 * 3 * E * I / std::pow(s.length, 3));
    for (double d : r.value().damage)
        EXPECT_EQ(d, 0.0);
}

TEST(DamagedBeam, DamageLowersTheModesThroughTheDamagedStiffness)
{
    // A tip deflection whose root strain is about ten times the threshold.
    const double amplitude = 0.02;
    auto r = damagedCantilever(beam(40, amplitude));
    ASSERT_TRUE(r) << r.message();
    const auto& v = r.value();
    for (const auto& st : v.steps)
        ASSERT_TRUE(st.converged);
    // Damage is largest where the moment is: at the root.
    EXPECT_EQ(std::max_element(v.damage.begin(), v.damage.end()) - v.damage.begin(), 0);
    EXPECT_GT(v.damage.front(), 0.3);
    EXPECT_LT(v.frequencies[0], v.undamagedFrequencies[0]);
    EXPECT_LT(v.damagedTipStiffness, v.undamagedTipStiffness);
    // The frequency drop follows from the stiffness: Rayleigh bounds it between the undamaged value and
    // the value with the root's damage everywhere.
    EXPECT_GT(v.frequencies[0], v.undamagedFrequencies[0] * std::sqrt(1.0 - v.damage.front()));
    // Energy: the work done is stored or dissipated at every step, to second order in the step.
    double worst = 0.0;
    for (const auto& st : v.steps)
        if (st.externalWork > 0.0)
            worst = std::max(worst, std::abs(st.externalWork - st.storedEnergy - st.dissipated) / st.externalWork);
    EXPECT_LT(worst, 2.5e-3);
    DamagedBeamSpec fine = beam(40, amplitude);
    fine.tipDeflections.clear();
    for (int k = 1; k <= 160; ++k)
        fine.tipDeflections.push_back(amplitude * k / 160.0);
    auto f = damagedCantilever(fine);
    ASSERT_TRUE(f);
    double worstFine = 0.0;
    for (const auto& st : f.value().steps)
        worstFine = std::max(worstFine, std::abs(st.externalWork - st.storedEnergy - st.dissipated) / st.externalWork);
    EXPECT_LT(worstFine, worst / 8.0);  // four times finer steps: sixteen times smaller (second order)
    EXPECT_NEAR(f.value().frequencies[0], v.frequencies[0], 1e-6 * v.frequencies[0]);  // monotonic loading: path independent
    // Unloaded at the end: nothing stored, the work was dissipated.
    EXPECT_NEAR(v.steps.back().storedEnergy, 0.0, 1e-12);
    // Mesh objective (no softening): 20 and 40 elements agree.
    auto coarse = damagedCantilever(beam(20, amplitude));
    ASSERT_TRUE(coarse);
    EXPECT_NEAR(coarse.value().frequencies[0], v.frequencies[0], 0.01 * v.frequencies[0]);
}

TEST(Viscoelasticity, RelaxationIsExactAndACycleDissipatesPiEps2LossModulus)
{
    ZenerParameters p;
    p.relaxedModulus = 1e9;
    p.branchModulus = 2e9;
    p.relaxationTime = 0.1;
    std::vector<double> t{0.0}, e{0.0};
    t.push_back(0.0);
    e.push_back(1e-3);  // a step
    for (int i = 1; i <= 100; ++i) {
        t.push_back(0.01 * i);
        e.push_back(1e-3);
    }
    auto r = zenerResponse(p, t, e);
    ASSERT_TRUE(r) << r.message();
    for (size_t i = 2; i < r.value().size(); ++i)
        EXPECT_NEAR(r.value()[i].stress, 1e-3 * (1e9 + 2e9 * std::exp(-t[i] / 0.1)), 1e-3);
    // Harmonic strain at w tau = 1: after the transient one cycle dissipates pi eps0^2 E''.
    const double w = 10.0, eps0 = 1e-3, T = 2 * kPi / w;
    std::vector<double> th, eh;
    for (int i = 0; i <= 20000; ++i) {
        th.push_back(10 * T * i / 20000.0);
        eh.push_back(eps0 * std::sin(w * th.back()));
    }
    auto h = zenerResponse(p, th, eh);
    ASSERT_TRUE(h);
    const double perCycle = h.value().back().dissipated - h.value()[18000].dissipated;
    EXPECT_NEAR(perCycle, kPi * eps0 * eps0 * p.lossModulus(w), 1e-3 * perCycle);
    EXPECT_NEAR(p.lossModulus(w), 1e9, 1e-6);  // E1 / 2 at w tau = 1
}
