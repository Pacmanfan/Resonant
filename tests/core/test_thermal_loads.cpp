// Thermal loads of the finite-element model against linear thermoelasticity (RES-CEME-012).
#include "mechanics/Analyses.h"
#include "mechanics/ThermalLoads.h"

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::mechanics;

namespace {

constexpr double kE = 70e9, kNu = 0.3, kAlpha = 2.3e-5, kDeltaT = 80.0, kRef = 293.15;

ElasticMaterial aluminium()
{
    ElasticMaterial m;
    m.name = "test";
    m.density = 2700.0;
    m.youngsModulus = kE;
    m.poissonsRatio = kNu;
    return m;
}

// A column of `n` hexahedra of side `a` and height `h` each, along z.
StructuralModel column(int n, double a, double h)
{
    StructuralModel m;
    m.kind = ElementKind::Solid;
    for (int k = 0; k <= n; ++k) {
        m.nodes.emplace_back(0, 0, k * h);
        m.nodes.emplace_back(a, 0, k * h);
        m.nodes.emplace_back(a, a, k * h);
        m.nodes.emplace_back(0, a, k * h);
    }
    m.fixed.assign(m.nodes.size(), 0);
    m.materials = {aluminium()};
    for (int k = 0; k < n; ++k) {
        Element e;
        for (int q = 0; q < 4; ++q) {
            e.nodes[static_cast<size_t>(q)] = 4 * k + q;
            e.nodes[static_cast<size_t>(q) + 4] = 4 * (k + 1) + q;
        }
        m.elements.push_back(e);
    }
    return m;
}

ThermalLoadInput uniform(const StructuralModel& m, double deltaT = kDeltaT)
{
    ThermalLoadInput in;
    in.elementTemperature.assign(m.elements.size(), kRef + deltaT);
    in.referenceTemperature = kRef;
    in.expansion.assign(m.materials.size(), kAlpha);
    return in;
}

Eigen::VectorXd solve(const StructuralModel& m, const ThermalLoadInput& in)
{
    auto f = thermalLoadVector(m, in);
    EXPECT_TRUE(f) << f.message();
    ForceInput force;
    force.pattern = f.value();
    force.amplitude = 1.0;
    auto s = solveStatic(m, assemble(m), {force});
    EXPECT_TRUE(s) << s.message();
    return s.value().displacement;
}

}  // namespace

TEST(ThermalLoads, AFreeBlockExpandsWithoutStress)
{
    StructuralModel m = column(3, 0.02, 0.01);
    for (size_t n = 0; n < m.nodes.size(); ++n) {
        const size_t q = n % 4;
        if (q == 0 || q == 3)
            m.fix(n, 0);
        if (q == 0 || q == 1)
            m.fix(n, 1);
        if (n < 4)
            m.fix(n, 2);
    }
    const ThermalLoadInput in = uniform(m);
    const Eigen::VectorXd u = solve(m, in);
    const size_t top = m.nodes.size() - 2;  // the (+x, +y) corner of the top face
    const Vec3 d = m.translation(u, top);
    EXPECT_NEAR(d.x(), kAlpha * kDeltaT * 0.02, 1e-12);
    EXPECT_NEAR(d.y(), kAlpha * kDeltaT * 0.02, 1e-12);
    EXPECT_NEAR(d.z(), kAlpha * kDeltaT * 0.03, 1e-12);
    auto stress = thermalStress(m, in, u);
    ASSERT_TRUE(stress) << stress.message();
    EXPECT_LT(stress.value().maxVonMises, 1e-6 * kE * kAlpha * kDeltaT);
}

TEST(ThermalLoads, LateralConfinementGivesTheBiaxialStress)
{
    StructuralModel m = column(4, 0.01, 0.005);
    for (size_t n = 0; n < m.nodes.size(); ++n) {
        m.fix(n, 0);
        m.fix(n, 1);
        if (n < 4)
            m.fix(n, 2);
    }
    const ThermalLoadInput in = uniform(m);
    const Eigen::VectorXd u = solve(m, in);
    // Uniaxial strain: eps_zz = alpha dT (1 + nu) / (1 - nu), sigma_xx = sigma_yy = -E alpha dT / (1 - nu), sigma_zz = 0.
    EXPECT_NEAR(m.translation(u, m.nodes.size() - 1).z(), kAlpha * kDeltaT * 0.02 * (1 + kNu) / (1 - kNu), 1e-12);
    auto stress = thermalStress(m, in, u);
    ASSERT_TRUE(stress) << stress.message();
    const double expected = -kE * kAlpha * kDeltaT / (1 - kNu);
    for (const auto& s : stress.value().stress) {
        EXPECT_NEAR(s[0], expected, 1e-6 * std::abs(expected));
        EXPECT_NEAR(s[1], expected, 1e-6 * std::abs(expected));
        EXPECT_NEAR(s[2], 0.0, 1e-6 * std::abs(expected));
    }
}

TEST(ThermalLoads, FullConfinementGivesTheHydrostaticStress)
{
    StructuralModel m = column(1, 0.01, 0.01);
    for (size_t n = 0; n < m.nodes.size(); ++n)
        for (int d = 0; d < 3; ++d)
            m.fix(n, d);
    const ThermalLoadInput in = uniform(m);
    auto stress = thermalStress(m, in, Eigen::VectorXd::Zero(static_cast<Eigen::Index>(m.dofCount())));
    ASSERT_TRUE(stress) << stress.message();
    const double expected = -kE * kAlpha * kDeltaT / (1 - 2 * kNu);
    for (int c = 0; c < 3; ++c)
        EXPECT_NEAR(stress.value().stress[0][static_cast<size_t>(c)], expected, 1e-9 * std::abs(expected));
    EXPECT_NEAR(stress.value().vonMises[0], 0.0, 1e-6 * std::abs(expected));
}

TEST(ThermalLoads, AFrameBarExpandsFreelyOrIsCompressedBetweenSupports)
{
    StructuralModel m;
    m.kind = ElementKind::Frame;
    m.nodes = {Vec3(0, 0, 0), Vec3(0.5, 0, 0), Vec3(1.0, 0, 0)};
    m.fixed.assign(3, 0);
    m.materials = {aluminium()};
    Section s;
    s.area = 1e-4;
    s.iy = s.iz = 1e-9;
    s.torsion = s.polar = 2e-9;
    m.sections = {s};
    for (int e = 0; e < 2; ++e) {
        Element el;
        el.nodes[0] = e;
        el.nodes[1] = e + 1;
        el.up = Vec3::UnitZ();
        m.elements.push_back(el);
    }
    for (int d = 0; d < 6; ++d)
        m.fix(0, d);
    ThermalLoadInput in = uniform(m);
    const Eigen::VectorXd u = solve(m, in);
    EXPECT_NEAR(m.translation(u, 2).x(), kAlpha * kDeltaT * 1.0, 1e-12);
    auto free = thermalStress(m, in, u);
    ASSERT_TRUE(free);
    EXPECT_LT(free.value().maxVonMises, 1e-6 * kE * kAlpha * kDeltaT);
    for (int d = 0; d < 6; ++d)
        m.fix(2, d);
    const Eigen::VectorXd held = solve(m, in);
    auto stress = thermalStress(m, in, held);
    ASSERT_TRUE(stress);
    for (const auto& st : stress.value().stress)
        EXPECT_NEAR(st[0], -kE * kAlpha * kDeltaT, 1e-6 * kE * kAlpha * kDeltaT);
}

TEST(ThermalLoads, RefusesPlatesAndMismatchedInputs)
{
    StructuralModel m = column(2, 0.01, 0.01);
    ThermalLoadInput in = uniform(m);
    in.elementTemperature.pop_back();
    EXPECT_FALSE(thermalLoadVector(m, in));
    in = uniform(m);
    in.expansion.clear();
    EXPECT_FALSE(thermalLoadVector(m, in));
    m.kind = ElementKind::Plate;
    EXPECT_FALSE(thermalLoadVector(m, uniform(m)));
}
