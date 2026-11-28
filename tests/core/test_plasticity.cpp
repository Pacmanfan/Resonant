// J2 plasticity (RES-DMS-004, -005, -012): the uniaxial response against the bilinear
// closed form, elastic unloading and residual strain, the Bauschinger shift of kinematic
// hardening, the consistent tangent against finite differences, the energy split, and a
// statically indeterminate bar against its analytic solution with residual stresses.
#include "mechanics/Plasticity.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant::mechanics;

namespace {

J2Parameters steel(double H = 2e9, double Hk = 0.0)
{
    J2Parameters p;
    p.youngsModulus = 200e9;
    p.poissonsRatio = 0.3;
    p.yieldStress = 250e6;
    p.isotropicHardening = H;
    p.kinematicHardening = Hk;
    return p;
}

// Follows a uniaxial strain path, committing every step.
std::vector<UniaxialPoint> path(const J2Parameters& p, const std::vector<double>& strains)
{
    std::vector<UniaxialPoint> out;
    J2State c;
    Vec6 guess = Vec6::Zero();
    for (double e : strains) {
        auto u = j2Uniaxial(p, c, e, guess);
        EXPECT_TRUE(u) << u.message();
        c = u.value().response.state;
        guess = u.value().strain;
        out.push_back(u.value());
    }
    return out;
}

std::vector<double> ramp(double from, double to, int n)
{
    std::vector<double> v;
    for (int i = 1; i <= n; ++i)
        v.push_back(from + (to - from) * i / n);
    return v;
}

// The bilinear uniaxial law with linear isotropic hardening, independent of the 3D code.
double bilinear(double E, double sy, double H, double eps)
{
    const double ey = sy / E, Et = E * H / (E + H);
    return std::abs(eps) <= ey ? E * eps : (eps > 0 ? 1.0 : -1.0) * (sy + Et * (std::abs(eps) - ey));
}

}  // namespace

TEST(Plasticity, ElasticThenHardeningAgainstTheBilinearLaw)
{
    const J2Parameters p = steel();
    const double ey = p.yieldStress / p.youngsModulus;
    const auto r = path(p, ramp(0.0, 4.0 * ey, 40));
    for (size_t i = 0; i < r.size(); ++i) {
        const double e = 4.0 * ey * (i + 1) / 40.0;
        EXPECT_NEAR(r[i].stress, bilinear(p.youngsModulus, p.yieldStress, p.isotropicHardening, e), 1e-6 * p.yieldStress) << i;
    }
    EXPECT_TRUE(r.back().response.plastic);
    // The condensed uniaxial tangent is E H / (E + H) on the plastic branch.
    EXPECT_NEAR(r.back().tangent, p.youngsModulus * p.isotropicHardening / (p.youngsModulus + p.isotropicHardening), 1e-6 * p.isotropicHardening);
    // Lateral strains: elastic Poisson plus plastic incompressibility.
    const double ep = r.back().response.state.plasticStrain(0);
    EXPECT_NEAR(r.back().strain(1), -p.poissonsRatio * r.back().stress / p.youngsModulus - 0.5 * ep, 1e-12);
}

TEST(Plasticity, UnloadingIsElasticAndLeavesResidualStrain)
{
    const J2Parameters p = steel();
    const double ey = p.yieldStress / p.youngsModulus;
    auto r = path(p, ramp(0.0, 5.0 * ey, 50));
    const double sMax = r.back().stress;
    auto down = path(p, {});
    J2State c = r.back().response.state;
    const double eMax = 5.0 * ey;
    // Unload to zero stress: elastic, slope E.
    const double eZero = eMax - sMax / p.youngsModulus;
    auto u = j2Uniaxial(p, c, eZero, r.back().strain);
    ASSERT_TRUE(u);
    EXPECT_FALSE(u.value().response.plastic);
    EXPECT_NEAR(u.value().stress, 0.0, 1e-6 * p.yieldStress);
    EXPECT_NEAR(u.value().tangent, p.youngsModulus, 1e-6 * p.youngsModulus);
    // The residual strain is the plastic strain (sigma_max - sigma_y) / H.
    EXPECT_NEAR(eZero, (sMax - p.yieldStress) / p.isotropicHardening, 1e-9);
    EXPECT_NEAR(u.value().response.state.plasticStrain(0), eZero, 1e-12);
}

TEST(Plasticity, KinematicHardeningShiftsTheReverseYield)
{
    // Pure kinematic hardening: the elastic range keeps its size 2 sigma_y and moves.
    const J2Parameters p = steel(0.0, 10e9);
    const double ey = p.yieldStress / p.youngsModulus;
    auto r = path(p, ramp(0.0, 4.0 * ey, 40));
    const double sMax = r.back().stress;
    std::vector<double> back = ramp(4.0 * ey, -4.0 * ey, 400);
    J2State c = r.back().response.state;
    Vec6 guess = r.back().strain;
    double reverseYield = 0.0;
    for (double e : back) {
        auto u = j2Uniaxial(p, c, e, guess);
        ASSERT_TRUE(u);
        if (u.value().response.plastic) {
            reverseYield = u.value().stress;
            break;
        }
        c = u.value().response.state;
        guess = u.value().strain;
    }
    EXPECT_NEAR(reverseYield, sMax - 2.0 * p.yieldStress, 0.02 * p.yieldStress);  // within one strain increment
}

TEST(Plasticity, TheConsistentTangentMatchesFiniteDifferences)
{
    const J2Parameters p = steel(3e9, 5e9);
    J2State c;
    c.equivalentPlasticStrain = 0.002;
    c.plasticStrain << 0.002, -0.001, -0.001, 0.0005, 0.0, -0.0003;
    c.backStress << 1e7, -5e6, -5e6, 2e6, 0.0, 0.0;
    Vec6 e;
    e << 0.006, -0.002, -0.0015, 0.002, 0.0007, -0.001;
    const J2Response r = j2Update(p, c, e);
    ASSERT_TRUE(r.plastic);
    const double h = 1e-9;
    for (int j = 0; j < 6; ++j) {
        Vec6 a = e, b = e;
        a(j) += h;
        b(j) -= h;
        const Vec6 fd = (j2Update(p, c, a).stress - j2Update(p, c, b).stress) / (2.0 * h);
        for (int i = 0; i < 6; ++i)
            EXPECT_NEAR(r.tangent(i, j), fd(i), 1e-5 * p.youngsModulus) << i << "," << j;
    }
}

TEST(Plasticity, WorkIsStoredOrDissipated)
{
    const J2Parameters p = steel();
    const double ey = p.yieldStress / p.youngsModulus;
    std::vector<double> strains = ramp(0.0, 6.0 * ey, 600);
    for (double e : ramp(6.0 * ey, 2.0 * ey, 400))
        strains.push_back(e);
    const auto r = path(p, strains);
    double work = 0.0, previousStrain = 0.0, previousStress = 0.0;
    for (size_t i = 0; i < r.size(); ++i) {
        work += 0.5 * (r[i].stress + previousStress) * (strains[i] - previousStrain);
        previousStrain = strains[i];
        previousStress = r[i].stress;
    }
    const auto& last = r.back().response;
    const double ep = last.state.equivalentPlasticStrain;
    EXPECT_NEAR(last.state.dissipated, p.yieldStress * ep, 1e-6 * p.yieldStress * ep);  // linear isotropic: sigma_y e_p
    EXPECT_NEAR(work, last.elasticEnergy + last.hardeningEnergy + last.state.dissipated, 1e-4 * work);
}

TEST(Plasticity, AnIndeterminateBarRedistributesAndKeepsResidualStress)
{
    BarSpec s;
    s.material = steel(1e9);
    s.length = 1.0;
    s.loadPoint = 0.3;
    s.area = 1e-4;
    const double sy = s.material.yieldStress, A = s.area, a = 0.3, b = 0.7, E = s.material.youngsModulus, H = s.material.isotropicHardening;
    const double firstYield = sy * A * s.length / b;  // the short segment carries P b / L
    for (double P : ramp(0.0, 2.1 * sy * A, 60))
        s.loads.push_back(P);
    for (double P : ramp(2.1 * sy * A, 0.0, 30))
        s.loads.push_back(P);
    auto r = j2Bar(s);
    ASSERT_TRUE(r) << r.message();
    ASSERT_EQ(r.value().size(), s.loads.size());
    for (const BarStep& st : r.value()) {
        EXPECT_TRUE(st.converged);
        EXPECT_LE(st.iterations, 8);
    }
    // Loading: P(u) = A [sigma(u / a) + sigma(u / b)] with the bilinear law (both segments loaded monotonically).
    for (size_t i = 0; i < 60; ++i) {
        const BarStep& st = r.value()[i];
        const double analytic = A * (bilinear(E, sy, H, st.displacement / a) + bilinear(E, sy, H, st.displacement / b));
        EXPECT_NEAR(st.load, analytic, 1e-6 * sy * A) << i;
        if (st.load < 0.99 * firstYield)
            EXPECT_EQ(st.plasticStrain1, 0.0);
    }
    EXPECT_GT(r.value()[59].plasticStrain2, -1.0);   // the long segment yields in compression near the limit load 2 sigma_y A
    EXPECT_LT(r.value()[59].plasticStrain2, 0.0);
    // Unloaded: the same stress in both segments (P = 0), not zero; u = (ep1 - ep2) a b / L.
    const BarStep& end = r.value().back();
    EXPECT_NEAR(end.force1, end.force2, 1e-6 * sy * A);
    EXPECT_GT(std::abs(end.force1), 0.01 * sy * A);
    EXPECT_NEAR(end.displacement, (end.plasticStrain1 - end.plasticStrain2) * a * b / s.length, 1e-12);
    // Energy: work = elastic + hardening + dissipated.
    EXPECT_NEAR(end.externalWork, end.elasticEnergy + end.hardeningEnergy + end.dissipated, 2e-3 * end.externalWork);
}
