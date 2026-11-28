// Classical lamination theory and the piezoelectric cantilever (RES-FAM-011,
// -012, -020) against closed forms: the bilayer bending stiffness and neutral
// axis, cross-ply moduli, the Timoshenko bimetal curvature, a power-law graded
// plate, beam and plate modes, and the unimorph's short- and open-circuit
// frequencies, energy balance, optimum load and poling.
#include "mechanics/Laminate.h"
#include "mechanics/PiezoelectricBeam.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

using namespace resonant::mechanics;

TEST(MatterMechanics, BilayerStiffnessNeutralAxisAndVoigtModulus)
{
    // Steel 0.5 mm under aluminium 0.3 mm, equal Poisson's ratios.
    const double nu = 0.3, e1 = 200e9, e2 = 70e9, t1 = 0.5e-3, t2 = 0.3e-3;
    auto l = laminate({Ply::isotropic("steel", t1, e1, nu, 7850), Ply::isotropic("aluminium", t2, e2, nu, 2700)});
    ASSERT_TRUE(l.ok()) << l.message();
    EXPECT_NEAR(l.value().Ex / ((e1 * t1 + e2 * t2) / (t1 + t2)), 1.0, 1e-12);  // exact for equal nu
    EXPECT_NEAR(l.value().nuxy, nu, 1e-12);
    EXPECT_FALSE(l.value().symmetric);
    // The neutral axis and EI by the parallel-axis theorem.
    const double zn = (e1 * t1 * t1 / 2.0 + e2 * t2 * (t1 + t2 / 2.0)) / (e1 * t1 + e2 * t2);
    const double ei = e1 * (t1 * t1 * t1 / 12.0 + t1 * std::pow(t1 / 2.0 - zn, 2)) + e2 * (t2 * t2 * t2 / 12.0 + t2 * std::pow(t1 + t2 / 2.0 - zn, 2));
    const BeamSection beam = beamSection(l.value().plies);
    EXPECT_NEAR(beam.neutralAxis / zn, 1.0, 1e-12);
    EXPECT_NEAR(beam.bendingStiffness / ei, 1.0, 1e-12);
    // The plate's reduced bending stiffness is the beam's with the plate modulus E / (1 - nu^2).
    EXPECT_NEAR(l.value().Dstar(0, 0) / (ei / (1.0 - nu * nu)), 1.0, 1e-10);
    EXPECT_NEAR(l.value().neutralSurface + l.value().thickness / 2.0, zn, 1e-12);
    EXPECT_NEAR(l.value().massPerArea, 7850 * t1 + 2700 * t2, 1e-12);
}

TEST(MatterMechanics, CrossPlyModuliAndAnInvalidPly)
{
    // T300/5208 carbon/epoxy, [0/90]s.
    Ply p;
    p.name = "ply";
    p.thickness = 0.125e-3;
    p.E1 = 181e9;
    p.E2 = 10.3e9;
    p.G12 = 7.17e9;
    p.nu12 = 0.28;
    p.density = 1600;
    Ply q = p;
    q.angle = std::numbers::pi / 2.0;
    auto l = laminate({p, q, q, p});
    ASSERT_TRUE(l.ok()) << l.message();
    const double nu21 = 0.28 * 10.3 / 181.0, d = 1.0 - 0.28 * nu21;
    const double Q11 = 181e9 / d, Q22 = 10.3e9 / d, Q12 = 0.28 * 10.3e9 / d;
    const double a11 = (Q11 + Q22) / 2.0, a12 = Q12;  // per unit thickness
    EXPECT_NEAR(l.value().Ex / ((a11 * a11 - a12 * a12) / a11), 1.0, 1e-12);
    EXPECT_NEAR(l.value().Ey / l.value().Ex, 1.0, 1e-12);
    EXPECT_TRUE(l.value().symmetric);
    EXPECT_NEAR(l.value().Gxy, 7.17e9, 1e-3);
    // +/-45 degrees: the shear modulus rises far above G12.
    Ply a = p, b = p;
    a.angle = std::numbers::pi / 4.0;
    b.angle = -std::numbers::pi / 4.0;
    auto angle = laminate({a, b, b, a});
    ASSERT_TRUE(angle.ok());
    EXPECT_GT(angle.value().Gxy, 5.0 * 7.17e9);
    Ply bad = p;
    bad.nu12 = 5.0;
    EXPECT_FALSE(laminate({bad}).ok());
}

TEST(MatterMechanics, ThermalCurvatureIsTheTimoshenkoBimetal)
{
    // Timoshenko (1925): kappa = 6 (a2 - a1) dT (1 + m)^2 / (h (3 (1 + m)^2 + (1 + m n)(m^2 + 1/(m n)))),
    // m = t1 / t2, n = E1 / E2; with equal Poisson's ratios the free plate's curvature is the same.
    const double nu = 0.3, e1 = 200e9, e2 = 70e9, t1 = 0.5e-3, t2 = 0.3e-3, a1 = 12e-6, a2 = 23e-6, dT = 100.0;
    auto l = laminate({Ply::isotropic("steel", t1, e1, nu, 7850, a1), Ply::isotropic("aluminium", t2, e2, nu, 2700, a2)});
    ASSERT_TRUE(l.ok());
    const ThermalResponse r = thermalResponse(l.value(), dT);
    const double m = t1 / t2, n = e1 / e2, h = t1 + t2;
    const double kappa = 6.0 * (a2 - a1) * dT * (1.0 + m) * (1.0 + m) / (h * (3.0 * (1.0 + m) * (1.0 + m) + (1.0 + m * n) * (m * m + 1.0 / (m * n))));
    EXPECT_NEAR(std::abs(r.curvature(0)) / kappa, 1.0, 1e-10);
    EXPECT_NEAR(r.curvature(1), r.curvature(0), 1e-12 * std::abs(kappa));
    // The aluminium (expanding more) is in compression next to the interface, the steel in tension.
    EXPECT_LT(r.stress[1].bottom(0), 0.0);
    EXPECT_GT(r.stress[0].top(0), 0.0);
    // A free single ply expands without stress.
    auto single = laminate({Ply::isotropic("steel", t1, e1, nu, 7850, a1)});
    const ThermalResponse s = thermalResponse(single.value(), dT);
    EXPECT_NEAR(s.strain(0), a1 * dT, 1e-15);
    EXPECT_NEAR(s.stress[0].top.norm(), 0.0, 1e-3);
}

TEST(MatterMechanics, PowerLawGradedPlateAgainstItsIntegrals)
{
    // E(z') = Eb + (Et - Eb) (z'/h)^p through the thickness, constant nu: 400 midpoint sublayers.
    const double eb = 70e9, et = 380e9, p = 2.0, h = 2e-3, nu = 0.3;
    std::vector<Ply> plies;
    const int n = 400;
    for (int i = 0; i < n; ++i) {
        const double s = (i + 0.5) / n;
        plies.push_back(Ply::isotropic("g" + std::to_string(i), h / n, eb + (et - eb) * std::pow(s, p), nu, 3000.0));
    }
    auto l = laminate(plies);
    ASSERT_TRUE(l.ok());
    const double A = eb * h + (et - eb) * h / (p + 1.0);
    const double B = eb * h * h / 2.0 + (et - eb) * h * h / (p + 2.0);
    const double D = eb * h * h * h / 3.0 + (et - eb) * h * h * h / (p + 3.0);
    const double dstar = (D - B * B / A) / (1.0 - nu * nu);
    EXPECT_NEAR(l.value().Dstar(0, 0) / dstar, 1.0, 1e-5);
    EXPECT_NEAR(l.value().A(0, 0) / (A / (1.0 - nu * nu)), 1.0, 1e-5);
}

TEST(MatterMechanics, CantileverAndPlateModes)
{
    // A steel strip 60 x 10 x 0.5 mm: f1 = 1.8751^2 / (2 pi L^2) sqrt(E I / (rho A)).
    const double e = 200e9, rho = 7850, L = 0.06, b = 0.01, t = 0.5e-3;
    const double ei = e * b * t * t * t / 12.0, mpl = rho * b * t;
    const auto f = cantileverFrequencies(ei, mpl, L, 3);
    EXPECT_NEAR(f[0], std::pow(1.875104068711961, 2) / (2.0 * std::numbers::pi * L * L) * std::sqrt(ei / mpl), 1e-9);
    EXPECT_NEAR(f[1] / f[0], std::pow(4.694091132974175 / 1.875104068711961, 2), 1e-12);
    EXPECT_NEAR(cantileverRoot(6), 11.0 * std::numbers::pi / 2.0, 1e-9);
    // A simply supported square isotropic plate: f11 = pi (2 / a^2) sqrt(D / rho h) / 2.
    auto l = laminate({Ply::isotropic("steel", t, e, 0.3, rho)});
    const auto modes = simplySupportedPlateModes(l.value().D, l.value().massPerArea, 0.1, 0.1, 4);
    const double dp = e * t * t * t / (12.0 * (1.0 - 0.09));
    EXPECT_NEAR(modes[0].frequency, std::numbers::pi / 2.0 * (2.0 / 0.01) * std::sqrt(dp / (rho * t)), 1e-6);
    EXPECT_EQ(modes[0].m, 1);
    EXPECT_NEAR(modes[1].frequency, modes[2].frequency, 1e-9);  // (1, 2) and (2, 1)
}

namespace {

HarvesterSpec unimorph(double d31 = -171e-12)
{
    // Steel 0.5 mm under PZT-5A 0.2 mm, 40 x 10 mm, a 2 g tip mass.
    HarvesterSpec s;
    s.plies = {Ply::isotropic("steel", 0.5e-3, 200e9, 0.29, 7850), Ply::isotropic("pzt", 0.2e-3, 61e9, 0.35, 7750)};
    s.width = 0.01;
    s.length = 0.04;
    s.tipMass = 0.002;
    s.dampingRatio = 0.01;
    s.piezo = {{1, d31, 1700.0, +1, 0.0, 1.0}};
    return s;
}

}  // namespace

TEST(MatterMechanics, UnimorphFrequenciesCouplingAndEnergyBalance)
{
    auto m = harvesterModel(unimorph());
    ASSERT_TRUE(m.ok()) << m.message();
    const HarvesterModel& h = m.value();
    EXPECT_GT(h.openCircuitHz, h.shortCircuitHz);
    EXPECT_NEAR(h.openCircuitHz / h.shortCircuitHz, std::sqrt(1.0 + h.couplingSquared), 1e-12);
    EXPECT_GT(h.couplingSquared, 0.01);
    // The response peaks at the short-circuit frequency for a vanishing load and moves to the
    // open-circuit one for a very large load.
    auto peak = [&](double load) {
        double best = 0.0, at = 0.0;
        for (double f = 0.8 * h.shortCircuitHz; f < 1.2 * h.openCircuitHz; f += 0.01)
            if (const double x = std::abs(harvesterResponse(h, f, load, 1.0).displacement); x > best) {
                best = x;
                at = f;
            }
        return at;
    };
    EXPECT_NEAR(peak(1e-3) / h.shortCircuitHz, 1.0, 2e-4);
    EXPECT_NEAR(peak(1e12) / h.openCircuitHz, 1.0, 2e-4);
    // The base's power goes into damping and the load, at every frequency and load.
    for (double f : {0.5 * h.shortCircuitHz, h.shortCircuitHz, 1.03 * h.shortCircuitHz, 3.0 * h.shortCircuitHz})
        for (double r : {1e2, 1e4, 1e6}) {
            const HarvesterPoint p = harvesterResponse(h, f, r, 2.0);
            EXPECT_NEAR(p.inputPower, p.dissipatedPower + p.electricalPower, 1e-12 * p.inputPower) << f << " Hz, " << r << " ohm";
            EXPECT_LT(p.efficiency, 1.0);
        }
}

TEST(MatterMechanics, OptimumLoadAndWeakCoupling)
{
    auto m = harvesterModel(unimorph());
    ASSERT_TRUE(m.ok());
    const double f = m.value().shortCircuitHz;
    const double r = optimalLoad(m.value(), f);
    for (double factor : {0.7, 1.4})
        EXPECT_GT(harvesterResponse(m.value(), f, r, 1.0).electricalPower, harvesterResponse(m.value(), f, r * factor, 1.0).electricalPower);
    // Weak coupling: the optimum load tends to 1 / (omega C).
    auto weak = harvesterModel(unimorph(-1e-14));
    ASSERT_TRUE(weak.ok());
    const double fw = weak.value().shortCircuitHz;
    EXPECT_LT(weak.value().couplingSquared, 1e-6);
    EXPECT_NEAR(optimalLoad(weak.value(), fw) * 2.0 * std::numbers::pi * fw * weak.value().capacitance, 1.0, 0.01);
    // The single-mode estimate lies 1.5 % above Euler-Bernoulli without a tip mass.
    HarvesterSpec bare = unimorph();
    bare.tipMass = 0.0;
    auto noTip = harvesterModel(bare);
    ASSERT_TRUE(noTip.ok());
    EXPECT_NEAR(noTip.value().shortCircuitHz / noTip.value().eulerBernoulliHz, std::sqrt(140.0 * 3.0 / 33.0) / std::pow(1.875104068711961, 2), 1e-9);
}

TEST(MatterMechanics, PolingElectrodesAndDamping)
{
    // A symmetric bimorph: equal poling cancels, opposite poling adds.
    HarvesterSpec s = unimorph();
    s.plies = {Ply::isotropic("pzt-b", 0.2e-3, 61e9, 0.35, 7750), Ply::isotropic("shim", 0.1e-3, 200e9, 0.29, 7850),
               Ply::isotropic("pzt-t", 0.2e-3, 61e9, 0.35, 7750)};
    s.piezo = {{0, -171e-12, 1700.0, +1, 0.0, 1.0}, {2, -171e-12, 1700.0, +1, 0.0, 1.0}};
    s.connection = "parallel";
    auto same = harvesterModel(s);
    ASSERT_TRUE(same.ok()) << same.message();
    EXPECT_NEAR(same.value().theta, 0.0, 1e-9 * std::abs(same.value().layerTheta[0]));
    s.piezo[1].poling = -1;
    auto opposite = harvesterModel(s);
    ASSERT_TRUE(opposite.ok());
    EXPECT_NEAR(opposite.value().theta, 2.0 * opposite.value().layerTheta[0], 1e-12 * std::abs(opposite.value().theta));
    s.connection = "series";
    auto series = harvesterModel(s);
    ASSERT_TRUE(series.ok());
    EXPECT_NEAR(series.value().capacitance, opposite.value().capacitance / 4.0, 1e-9 * series.value().capacitance);
    // An electrode over the root half collects most of the charge (the strain is largest there).
    HarvesterSpec root = unimorph();
    root.piezo[0].end = 0.5;
    auto half = harvesterModel(root);
    auto full = harvesterModel(unimorph());
    ASSERT_TRUE(half.ok());
    EXPECT_NEAR(half.value().theta / full.value().theta, (6.0 * 0.5 - 3.0 * 0.25) / 3.0, 1e-12);
    EXPECT_NEAR(half.value().capacitance / full.value().capacitance, 0.5, 1e-12);
    // Without a damping ratio or loss factors the model refuses.
    HarvesterSpec undamped = unimorph();
    undamped.dampingRatio = -1.0;
    EXPECT_FALSE(harvesterModel(undamped).ok());
    undamped.plies[0].lossFactor = 0.004;
    auto lossy = harvesterModel(undamped);
    ASSERT_TRUE(lossy.ok());
    EXPECT_GT(lossy.value().dampingRatio, 0.0);
    EXPECT_LT(lossy.value().dampingRatio, 0.002);
    // The converse effect: a static voltage bends the beam by theta V / k.
    EXPECT_NEAR(std::abs(actuatorDisplacement(full.value(), 0.0, 10.0)), std::abs(full.value().theta) * 10.0 / full.value().stiffness, 1e-15);
}
