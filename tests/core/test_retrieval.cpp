// Effective-parameter retrieval (RES-META-004): a homogeneous slab, magnetic and lossy,
// is recovered over many wavelengths (the branch tracked by continuity); a negative-index
// slab gives the negative branch; flags for non-passive, non-local data; a layered slab
// retrieves its parallel permittivity.
#include "electromagnetics/Conventions.h"
#include "electromagnetics/ObliqueLayers.h"
#include "electromagnetics/Retrieval.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;

namespace {
struct Scattering {
    std::vector<double> f;
    std::vector<Complex> r, t;
};

Scattering scatter(const std::vector<em::IsotropicLayer>& stack, double from, double to, int n)
{
    Scattering s;
    for (int i = 0; i < n; ++i) {
        const double f = from + (to - from) * i / (n - 1);
        const auto x = em::obliqueStack(stack, f, 0.0, em::Polarization::TE);
        s.f.push_back(f);
        s.r.push_back(x.r);
        s.t.push_back(x.t);
    }
    return s;
}
}  // namespace

TEST(Retrieval, AHomogeneousSlabIsRecoveredOverSeveralWavelengths)
{
    const Complex eps(4.0, -0.1), mu(1.5, -0.05);
    const double d = 0.02;
    // n d / lambda runs from 0.03 to about 3: the phase wraps several times.
    const Scattering s = scatter({{d, eps, mu}}, 2e8, 1.8e10, 400);
    auto r = em::retrieveSlab(s.f, s.r, s.t, d, d / 20.0);
    ASSERT_TRUE(r) << r.message();
    for (size_t i = 0; i < s.f.size(); i += 7) {
        const auto& p = r.value().points[i];
        EXPECT_NEAR(std::abs(p.permittivity - eps), 0.0, 1e-8) << s.f[i];
        EXPECT_NEAR(std::abs(p.permeability - mu), 0.0, 1e-8) << s.f[i];
        EXPECT_TRUE(p.passive);
    }
}

TEST(Retrieval, ANegativeIndexSlabGivesTheNegativeBranch)
{
    const Complex eps(-2.0, -0.1), mu(-1.0, -0.1);
    const double d = 0.01;
    const Scattering s = scatter({{d, eps, mu}}, 1e9, 6e9, 120);
    auto r = em::retrieveSlab(s.f, s.r, s.t, d, d / 10.0);
    ASSERT_TRUE(r) << r.message();
    for (const auto& p : r.value().points) {
        EXPECT_LT(p.index.real(), 0.0);
        EXPECT_NEAR(std::abs(p.permittivity - eps), 0.0, 1e-8);
        EXPECT_NEAR(std::abs(p.permeability - mu), 0.0, 1e-8);
    }
}

TEST(Retrieval, FlagsNonPassiveAndNonLocalData)
{
    // An ideal gain slab (Im n > 0: the wave grows through it) is not passive.
    const double d = 0.02;
    const Complex n(std::sqrt(3.0), 0.01), z = 1.0 / n, r0 = (z - 1.0) / (z + 1.0);
    std::vector<double> f;
    std::vector<Complex> rr, tt;
    for (int i = 0; i < 10; ++i) {
        f.push_back(1e9 + 1e8 * i);
        const Complex P = std::exp(Complex(0.0, -1.0) * n * (2 * 3.14159265358979323846 * f.back() / em::kSpeedOfLight) * d);
        rr.push_back(r0 * (1.0 - P * P) / (1.0 - r0 * r0 * P * P));
        tt.push_back((1.0 - r0 * r0) * P / (1.0 - r0 * r0 * P * P));
    }
    auto g = em::retrieveSlab(f, rr, tt, d, d / 20.0);
    ASSERT_TRUE(g);
    EXPECT_FALSE(g.value().points.front().passive);
    EXPECT_NEAR(g.value().points.front().index.imag(), 0.01, 1e-9);
    EXPECT_FALSE(g.value().warnings.empty());
    // A cell as large as the slab is not a local material.
    const Scattering big = scatter({{d, Complex(3.0), Complex(1.0)}}, 1e9, 2e9, 10);
    auto b = em::retrieveSlab(big.f, big.r, big.t, d, d);
    ASSERT_TRUE(b);
    EXPECT_TRUE(b.value().points.back().nonlocal);
    EXPECT_FALSE(em::retrieveSlab({2e9, 1e9}, {Complex(0.0), Complex(0.0)}, {Complex(1.0), Complex(1.0)}, d, d));
    EXPECT_FALSE(em::retrieveSlab({1e9}, {}, {}, d, d));
}

TEST(Retrieval, ALossyLayeredSlabShowsTheAntiresonanceButStaysPassive)
{
    // Retrieved as a local medium, a periodic lossy stack gets Im mu > 0: spatial dispersion, reported apart.
    std::vector<em::IsotropicLayer> stack;
    for (int k = 0; k < 10; ++k) {
        stack.push_back({0.0005, Complex(9.0), Complex(1.0)});
        stack.push_back({0.002, Complex(2.1, -4.2e-4), Complex(1.0)});
        stack.push_back({0.0005, Complex(9.0), Complex(1.0)});
    }
    const Scattering s = scatter(stack, 4e8, 3e9, 20);
    auto r = em::retrieveSlab(s.f, s.r, s.t, 0.03, 0.003);
    ASSERT_TRUE(r);
    for (const auto& p : r.value().points) {
        EXPECT_TRUE(p.passive);
        EXPECT_TRUE(p.antiresonance);
        EXPECT_GT(p.permeability.imag(), 0.0);
        EXPECT_LT(p.signViolation, 1e-6);
        EXPECT_LT(p.permittivity.imag(), 0.0);
    }
    ASSERT_FALSE(r.value().warnings.empty());
    EXPECT_NE(r.value().warnings.back().find("antiresonance"), std::string::npos);
}

TEST(Retrieval, ALayeredSlabRetrievesItsParallelPermittivity)
{
    const double d1 = 0.001, d2 = 0.002, a = d1 + d2, e1 = 9.0, e2 = 1.5;
    std::vector<em::IsotropicLayer> stack;
    for (int n = 0; n < 10; ++n) {
        stack.push_back({0.5 * d1, Complex(e1), Complex(1.0)});
        stack.push_back({d2, Complex(e2), Complex(1.0)});
        stack.push_back({0.5 * d1, Complex(e1), Complex(1.0)});
    }
    const double par = (d1 * e1 + d2 * e2) / a;
    const Scattering s = scatter(stack, 1e8, 0.02 * em::kSpeedOfLight / a, 30);
    auto r = em::retrieveSlab(s.f, s.r, s.t, 10 * a, a);
    ASSERT_TRUE(r);
    EXPECT_NEAR(r.value().points.front().permittivity.real(), par, 1e-4 * par);
    EXPECT_NEAR(r.value().points.front().permeability.real(), 1.0, 1e-4);
    EXPECT_NEAR(r.value().points.back().permittivity.real(), par, 5e-3 * par);
}
