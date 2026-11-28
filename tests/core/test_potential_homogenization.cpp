// Computational homogenization of the steady potential problem (RES-MSC-006): exact
// cases (homogeneous, layered), the boundary-condition hierarchy, dilute spheres against
// Maxwell-Garnett inside the Hashin-Shtrikman bounds, a polycrystal against the
// self-consistent estimate, energy consistency, and refusals.
#include "multiscale/Microstructure.h"
#include "multiscale/PotentialHomogenization.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant::multiscale;
using nlohmann::json;

namespace {

PotentialProblem isotropic(const VoxelRve& r, const std::vector<double>& k)
{
    PotentialProblem p;
    p.n = r.n;
    p.size = r.size;
    p.periodic = r.periodic;
    for (auto ph : r.phase)
        p.K.push_back(Eigen::Matrix3d::Identity() * k[ph]);
    return p;
}

Eigen::Matrix3d uniaxial(double ka, double kc, const std::array<double, 3>& c)
{
    const Eigen::Vector3d a(c[0], c[1], c[2]);
    return ka * (Eigen::Matrix3d::Identity() - a * a.transpose()) + kc * a * a.transpose();
}

}  // namespace

TEST(PotentialHomogenization, AHomogeneousRveRecoversItsConstituent)
{
    auto r = generateRve({{"generator", "homogeneous"}, {"voxels", {4, 5, 3}}, {"size", {1e-3, 2e-3, 1e-3}}, {"phases", {"m"}}});
    ASSERT_TRUE(r);
    PotentialProblem p = isotropic(r.value(), {3.0});
    Eigen::Matrix3d K;
    K << 4, 1, 0, 1, 3, 0.5, 0, 0.5, 2;
    std::fill(p.K.begin(), p.K.end(), K);
    for (Boundary b : {Boundary::Periodic, Boundary::Linear}) {
        auto h = homogenize(p, b);
        ASSERT_TRUE(h) << h.message();
        EXPECT_LT((h.value().effective - K).norm(), 1e-8 * K.norm()) << toString(b);
        EXPECT_LT(h.value().hillMandel, 1e-8);
    }
}

TEST(PotentialHomogenization, LayersGiveTheArithmeticAndHarmonicMeansExactly)
{
    auto r = generateRve({{"generator", "layered"}, {"voxels", {3, 3, 12}}, {"size", {1, 1, 1}}, {"phases", {"a", "b"}}, {"axis", 2},
                          {"fractions", {1, 2}}});
    ASSERT_TRUE(r);
    auto h = homogenize(isotropic(r.value(), {100.0, 1.0}), Boundary::Periodic);
    ASSERT_TRUE(h) << h.message();
    const double par = (100.0 + 2.0) / 3.0, perp = 3.0 / (1.0 / 100.0 + 2.0);
    EXPECT_NEAR(h.value().effective(0, 0), par, 1e-8 * par);
    EXPECT_NEAR(h.value().effective(1, 1), par, 1e-8 * par);
    EXPECT_NEAR(h.value().effective(2, 2), perp, 1e-8 * perp);
    EXPECT_NEAR(h.value().effective(0, 2), 0.0, 1e-8);
}

TEST(PotentialHomogenization, ADiluteSphereConvergesToMaxwellGarnettWithResolution)
{
    // One centred sphere per cell (a simple cubic array) at f = 0.1, contrast 20: Rayleigh's
    // exact result lies within 0.1 % of Maxwell-Garnett here; the voxel staircase is the error.
    const double radius = std::cbrt(0.1 * 3.0 / (4.0 * 3.14159265358979323846));
    double previous = 1.0;
    for (int n : {16, 24, 36}) {
        PotentialProblem p;
        p.n = {n, n, n};
        int inside = 0;
        for (int k = 0; k < n; ++k)
            for (int j = 0; j < n; ++j)
                for (int i = 0; i < n; ++i) {
                    const double x = (i + 0.5) / n - 0.5, y = (j + 0.5) / n - 0.5, z = (k + 0.5) / n - 0.5;
                    const bool in = x * x + y * y + z * z <= radius * radius;
                    inside += in ? 1 : 0;
                    p.K.push_back(Eigen::Matrix3d::Identity() * (in ? 20.0 : 1.0));
                }
        const double f = static_cast<double>(inside) / (n * n * n);
        auto h = homogenize(p, Boundary::Periodic);
        ASSERT_TRUE(h) << h.message();
        const double mg = twoPhaseBounds(1.0, 20.0, f).maxwellGarnett;
        const double error = h.value().effective(0, 0) / mg - 1.0;
        EXPECT_GT(error, 0.0);
        EXPECT_LT(error, previous) << n;
        previous = error;
    }
    EXPECT_LT(previous, 0.02);
}

TEST(PotentialHomogenization, RandomSpheresLieInsideTheBoundsWithTheBoundaryHierarchy)
{
    auto r = generateRve({{"generator", "spheres"}, {"voxels", {28, 28, 28}}, {"size", {1, 1, 1}}, {"phases", {"matrix", "inclusion"}},
                          {"radius", 0.11}, {"volumeFraction", 0.12}, {"seed", 11}});
    ASSERT_TRUE(r) << r.message();
    const double f = r.value().volumeFractions()[1], k1 = 1.0, k2 = 20.0;
    const PotentialProblem p = isotropic(r.value(), {k1, k2});
    auto per = homogenize(p, Boundary::Periodic), lin = homogenize(p, Boundary::Linear), mix = homogenize(p, Boundary::Mixed),
         flux = homogenize(p, Boundary::Flux);
    ASSERT_TRUE(per && lin && mix && flux) << per.message();
    const TwoPhaseBounds b = twoPhaseBounds(k1, k2, f);
    EXPECT_NEAR(b.maxwellGarnett, b.hashinLower, 1e-12);  // MG = the HS bound with the poorer conductor as matrix
    for (int a = 0; a < 3; ++a) {
        const double k = per.value().effective(a, a);
        EXPECT_GT(k, b.hashinLower);
        EXPECT_LT(k, b.hashinUpper);
        // static uniform <= periodic, mixed <= kinematic uniform (apparent values of one RVE)
        EXPECT_LE(flux.value().effective(a, a), k * (1 + 1e-9));
        EXPECT_GE(lin.value().effective(a, a), k * (1 - 1e-9));
        EXPECT_LE(flux.value().effective(a, a), mix.value().effective(a, a) * (1 + 1e-9));
        EXPECT_LE(mix.value().effective(a, a), lin.value().effective(a, a) * (1 + 1e-9));
    }
    for (const auto* h : {&per, &lin, &mix, &flux})
        EXPECT_LT(h->value().hillMandel, 1e-8);
}

TEST(PotentialHomogenization, AnUntexturedPolycrystalIsIsotropicNearTheSelfConsistentEstimate)
{
    auto r = generateRve({{"generator", "voronoi"}, {"voxels", {24, 24, 24}}, {"size", {1, 1, 1}}, {"phases", {"grain"}}, {"grains", 60},
                          {"texture", "random"}, {"seed", 5}});
    ASSERT_TRUE(r) << r.message();
    const double ka = 10.0, kc = 1.0;
    PotentialProblem p;
    p.n = r.value().n;
    p.size = r.value().size;
    for (size_t v = 0; v < r.value().count(); ++v)
        p.K.push_back(uniaxial(ka, kc, r.value().axes[static_cast<size_t>(r.value().grain[v])]));
    auto h = homogenize(p, Boundary::Periodic);
    ASSERT_TRUE(h) << h.message();
    const double mean = h.value().effective.trace() / 3.0;
    const double voigt = (2 * ka + kc) / 3.0, reuss = 3.0 / (2 / ka + 1 / kc);
    EXPECT_LT(mean, voigt);
    EXPECT_GT(mean, reuss);
    EXPECT_NEAR(mean, selfConsistentUniaxial(ka, kc), 0.06 * mean);
    for (int a = 0; a < 3; ++a)
        EXPECT_NEAR(h.value().effective(a, a), mean, 0.15 * mean);  // 60 grains: statistically isotropic within their spread
}

TEST(PotentialHomogenization, PeriodicConditionsNeedAPeriodicStructure)
{
    auto r = generateRve({{"generator", "voxels"}, {"voxels", {2, 2, 2}}, {"size", {1, 1, 1}}, {"phases", {"a", "b"}}, {"data", {0, 1, 0, 1, 1, 0, 1, 0}},
                          {"periodic", false}});
    ASSERT_TRUE(r);
    PotentialProblem p = isotropic(r.value(), {1.0, 2.0});
    EXPECT_FALSE(homogenize(p, Boundary::Periodic));
    EXPECT_TRUE(homogenize(p, Boundary::Linear));
    p.K[0] = -Eigen::Matrix3d::Identity();
    EXPECT_FALSE(homogenize(p, Boundary::Linear));
}
