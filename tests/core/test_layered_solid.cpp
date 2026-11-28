// Layered and coated three-dimensional bodies (REQ-CHAT-20261128-004, RES-MECH-019..022,
// ADR-000232): every result is checked against an analytical reference, the
// lower-fidelity models (beam, laminate theory) included.

#include "mechanics/DisplayMesh.h"
#include "mechanics/Laminate.h"
#include "mechanics/LayeredSolid.h"

#include <gtest/gtest.h>

#include <cmath>
#include <functional>

using namespace resonant;
using namespace resonant::mechanics;

namespace {

constexpr double kPi = 3.14159265358979323846;

ElasticMaterial iso(const std::string& name, double e, double nu, double rho)
{
    ElasticMaterial m;
    m.name = name;
    m.youngsModulus = e;
    m.poissonsRatio = nu;
    m.density = rho;
    return m;
}

BodyLayer layer(const std::string& name, const ElasticMaterial& m, double t, int through)
{
    BodyLayer l;
    l.name = name;
    l.material.base = m;
    l.thickness.start = t;
    l.elementsThrough = through;
    return l;
}

LayerInterface bonded(const std::string& name = "bond")
{
    LayerInterface i;
    i.name = name;
    i.mechanical = InterfaceMechanics::PerfectBonding;
    i.thermal = InterfaceThermal::Perfect;
    return i;
}

LayerInterface springs(double kn, double ks)
{
    LayerInterface i;
    i.name = "springs";
    i.mechanical = InterfaceMechanics::FiniteStiffness;
    i.normalStiffness = kn;
    i.shearStiffness = ks;
    i.thermal = InterfaceThermal::Perfect;
    return i;
}

LayeredBody plate(double length, double width, std::vector<BodyLayer> layers, std::vector<LayerInterface> interfaces, int nu, int nv)
{
    LayeredBody b;
    b.name = "plate";
    b.surface.kind = ReferenceSurface::Kind::Plane;
    b.surface.length = length;
    b.surface.width = width;
    b.layers = std::move(layers);
    b.interfaces = std::move(interfaces);
    b.elementsU = nu;
    b.elementsV = nv;
    return b;
}

// Mean of a displacement component over the nodes that satisfy `where`.
double meanDisplacement(const LayeredStaticResult& r, int axis, const std::function<bool(const Vec3&)>& where)
{
    double sum = 0.0;
    int count = 0;
    const StructuralModel& m = r.mesh.structure;
    for (size_t n = 0; n < m.nodes.size(); ++n)
        if (where(m.nodes[n])) {
            sum += m.translation(r.displacement, n)(axis);
            ++count;
        }
    EXPECT_GT(count, 0);
    return count ? sum / count : 0.0;
}

// The mean radial displacement of the nodes at distance r0 from the z axis (or the origin).
double meanRadial(const LayeredStaticResult& r, double r0, bool spherical)
{
    double sum = 0.0;
    int count = 0;
    const StructuralModel& m = r.mesh.structure;
    for (size_t n = 0; n < m.nodes.size(); ++n) {
        Vec3 x = m.nodes[n];
        if (!spherical)
            x.z() = 0.0;
        if (std::abs(x.norm() - r0) < 1e-9) {
            sum += m.translation(r.displacement, n).dot(x.normalized());
            ++count;
        }
    }
    EXPECT_GT(count, 0);
    return count ? sum / count : 0.0;
}

// A cantilever strip clamped at u = 0 with a tip force along -z at u = 1.
LayeredStaticResult cantilever(const LayeredBody& body, double force)
{
    LayeredSupports s;
    s.clamped = {BodyEdge::U0};
    LayeredLoads l;
    l.edgeForces = {{BodyEdge::U1, Vec3(0, 0, -force)}};
    auto r = solveLayeredStatic(body, s, l);
    EXPECT_TRUE(r.ok()) << r.message();
    return r.value();
}

double tipDeflection(const LayeredStaticResult& r, double length)
{
    return -meanDisplacement(r, 2, [&](const Vec3& x) { return std::abs(x.x() - length) < 1e-9; });
}

}  // namespace

// --- the element and the beam reference ---------------------------------------------------

TEST(LayeredSolid, CantileverStripMatchesTimoshenkoBeamTheory)
{
    const double L = 0.2, W = 0.02, h = 0.01, E = 200e9, P = 100.0;
    const auto body = plate(L, W, {layer("steel", iso("steel", E, 0.0, 7800), h, 2)}, {}, 20, 2);
    const LayeredStaticResult r = cantilever(body, P);
    const double I = W * h * h * h / 12, G = E / 2, kappa = 5.0 / 6.0;
    const double expected = P * L * L * L / (3 * E * I) + P * L / (kappa * G * W * h);
    EXPECT_NEAR(tipDeflection(r, L), expected, 0.01 * expected);
    EXPECT_NE(r.fidelity.find("three-dimensional"), std::string::npos);
    EXPECT_EQ(r.mesh.structure.kind, ElementKind::Solid20);
}

TEST(LayeredSolid, CantileverStripModesMatchBeamTheory)
{
    const double L = 0.2, W = 0.02, h = 0.01, E = 200e9, rho = 7800;
    const auto body = plate(L, W, {layer("steel", iso("steel", E, 0.0, rho), h, 2)}, {}, 20, 2);
    LayeredSupports s;
    s.clamped = {BodyEdge::U0};
    auto r = solveLayeredModal(body, s, 3);
    ASSERT_TRUE(r.ok()) << r.message();
    const double EI = E * W * h * h * h / 12, f1 = std::pow(cantileverRoot(1), 2) / (2 * kPi * L * L) * std::sqrt(EI / (rho * W * h));
    EXPECT_NEAR(r.value().frequencies.front(), f1, 0.015 * f1);
}

TEST(LayeredSolid, PerfectlyBondedBilayerMatchesLaminateTheoryAsItsThinLimit)
{
    // Steel on aluminium, nu = 0 (narrow beam): laminate theory's reduced bending
    // stiffness is the lower-fidelity reference the 3D body must approach.
    const double L = 0.3, W = 0.02, t1 = 0.002, t2 = 0.004, P = 20.0;
    const auto steel = iso("steel", 200e9, 0.0, 7800), aluminium = iso("aluminium", 70e9, 0.0, 2700);
    const auto body = plate(L, W, {layer("steel", steel, t1, 2), layer("aluminium", aluminium, t2, 2)}, {bonded()}, 30, 2);
    const LayeredStaticResult r = cantilever(body, P);
    const BeamSection section = beamSection({Ply::isotropic("steel", t1, 200e9, 0.0, 7800), Ply::isotropic("aluminium", t2, 70e9, 0.0, 2700)});
    const double expected = P * L * L * L / (3 * section.bendingStiffness * W);
    EXPECT_NEAR(tipDeflection(r, L), expected, 0.015 * expected);
    // Tractions at the shared-node interface are recovered from the layer below.
    bool any = false;
    for (const auto& p : r.interfacePoints)
        any = any || (p.interface == 0 && p.state == "bonded");
    EXPECT_TRUE(any);
}

// --- curved bodies (Lame) ------------------------------------------------------------------

namespace {

LayeredBody quarterCylinder(double a, std::vector<BodyLayer> layers, std::vector<LayerInterface> interfaces, int hoop)
{
    LayeredBody b;
    b.surface.kind = ReferenceSurface::Kind::Cylinder;
    b.surface.radius = a;
    b.surface.length = 0.01;
    b.surface.sweep = kPi / 2;
    b.layers = std::move(layers);
    b.interfaces = std::move(interfaces);
    b.elementsU = hoop;
    b.elementsV = 1;
    return b;
}

LayeredSupports planeStrainQuarter()
{
    LayeredSupports s;
    s.symmetry = {{0, 0.0}, {1, 0.0}};
    s.heldAxes = {2};
    return s;
}

// Plane-strain Lame cylinder (ri, ro) under internal pi and external pe: u(r).
double lameU(double r, double ri, double ro, double pi, double pe, double e, double nu)
{
    const double a = (pi * ri * ri - pe * ro * ro) / (ro * ro - ri * ri), b = (pi - pe) * ri * ri * ro * ro / (ro * ro - ri * ri);
    return (1 + nu) / e * ((1 - 2 * nu) * a * r + b / r);
}

}  // namespace

TEST(LayeredSolid, ThickCylinderUnderInternalPressureMatchesLame)
{
    const double a = 0.1, b = 0.15, E = 200e9, nu = 0.3, p = 10e6;
    const auto body = quarterCylinder(a, {layer("wall", iso("steel", E, nu, 7800), b - a, 6)}, {}, 8);
    LayeredLoads l;
    l.innerPressure = p;
    auto r = solveLayeredStatic(body, planeStrainQuarter(), l);
    ASSERT_TRUE(r.ok()) << r.message();
    const double expected = lameU(a, a, b, p, 0, E, nu);
    EXPECT_NEAR(meanRadial(r.value(), a, false), expected, 0.005 * expected);
    // Hoop and radial stresses through the wall.
    const double A = p * a * a / (b * b - a * a), B = p * a * a * b * b / (b * b - a * a);
    for (const auto& e : r.value().elements) {
        const double rr = std::hypot(e.centre.x(), e.centre.y());
        EXPECT_NEAR(e.surfaceStress(0), A + B / (rr * rr), 0.02 * p) << "hoop at r = " << rr;
        EXPECT_NEAR(e.surfaceStress(2), A - B / (rr * rr), 0.03 * p) << "radial at r = " << rr;
    }
}

TEST(LayeredSolid, FullRingMergesItsSeamAndMatchesLame)
{
    const double a = 0.1, b = 0.13, E = 70e9, nu = 0.33, p = 5e6;
    auto body = quarterCylinder(a, {layer("ring", iso("aluminium", E, nu, 2700), b - a, 4)}, {}, 24);
    body.surface.sweep = 2 * kPi;
    LayeredLoads l;
    l.innerPressure = p;
    auto r = solveLayeredStatic(body, planeStrainQuarter(), l);
    ASSERT_TRUE(r.ok()) << r.message();
    const double expected = lameU(a, a, b, p, 0, E, nu);
    EXPECT_NEAR(meanRadial(r.value(), a, false), expected, 0.01 * expected);
}

TEST(LayeredSolid, CompoundCylinderWithAFiniteStiffnessInterfaceMatchesLame)
{
    const double a = 0.1, c = 0.12, b = 0.15, p = 10e6, kn = 2e12, ks = 1e12;
    const double E1 = 200e9, E2 = 70e9, nu = 0.3;
    const auto body = quarterCylinder(a, {layer("steel", iso("steel", E1, nu, 7800), c - a, 4), layer("aluminium", iso("aluminium", E2, nu, 2700), b - c, 5)},
                                      {springs(kn, ks)}, 8);
    LayeredLoads l;
    l.innerPressure = p;
    auto r = solveLayeredStatic(body, planeStrainQuarter(), l);
    ASSERT_TRUE(r.ok()) << r.message();
    // The interface pressure q: u_outer(c) - u_inner(c) = -q / kn (linear in q).
    const auto gap = [&](double q) { return lameU(c, c, b, q, 0, E2, nu) - lameU(c, a, c, p, q, E1, nu) + q / kn; };
    const double q = -gap(0.0) / (gap(1.0) - gap(0.0));
    EXPECT_NEAR(meanRadial(r.value(), a, false), lameU(a, a, c, p, q, E1, nu), 0.005 * lameU(a, a, c, p, q, E1, nu));
    double opening = 0.0, traction = 0.0;
    int count = 0;
    for (const auto& pt : r.value().interfacePoints) {
        opening += pt.opening;
        traction += pt.normalTraction;
        ++count;
    }
    ASSERT_GT(count, 0);
    EXPECT_NEAR(traction / count, -q, 0.02 * q);
    EXPECT_NEAR(opening / count, -q / kn, 0.02 * q / kn);
}

TEST(LayeredSolid, ThickSphereUnderInternalPressureMatchesLame)
{
    const double a = 0.1, b = 0.13, E = 200e9, nu = 0.3, p = 10e6;
    LayeredBody body;
    body.surface.kind = ReferenceSurface::Kind::Sphere;
    body.surface.radius = a;
    body.surface.polarStart = 0.0;  // the pole: the elements around it collapse, their nodes merged
    body.surface.polarEnd = kPi / 2;
    body.surface.sweep = kPi / 2;
    body.layers = {layer("shell", iso("steel", E, nu, 7800), b - a, 3)};
    body.elementsU = 8;
    body.elementsV = 8;
    LayeredSupports s;
    s.symmetry = {{0, 0.0}, {1, 0.0}, {2, 0.0}};
    LayeredLoads l;
    l.innerPressure = p;
    auto r = solveLayeredStatic(body, s, l);
    ASSERT_TRUE(r.ok()) << r.message();
    const double expected = p * a * a * a / (E * (b * b * b - a * a * a)) * ((1 - 2 * nu) * a + (1 + nu) * b * b * b / (2 * a * a));
    EXPECT_NEAR(meanRadial(r.value(), a, true), expected, 0.01 * expected);
    for (const auto& e : r.value().elements) {
        if (e.parameter[0] < 0.25)
            continue;  // away from the collapsed pole elements
        const double rr = e.centre.norm(), hoop = p * a * a * a * (b * b * b + 2 * rr * rr * rr) / (2 * rr * rr * rr * (b * b * b - a * a * a));
        EXPECT_NEAR(e.surfaceStress(0), hoop, 0.03 * p);
    }
}

TEST(LayeredSolid, ARevolvedProfileIsTheSameBodyAsTheCylinder)
{
    const double a = 0.1, b = 0.15, E = 200e9, nu = 0.3, p = 10e6;
    LayeredBody body;
    body.surface.kind = ReferenceSurface::Kind::Revolution;
    body.surface.profile = {{a, 0.0}, {a, 0.005}, {a, 0.01}};
    body.surface.sweep = kPi / 2;
    body.layers = {layer("wall", iso("steel", E, nu, 7800), b - a, 6)};
    body.elementsU = 1;  // along the meridian (axial)
    body.elementsV = 8;  // about the axis
    LayeredLoads l;
    l.innerPressure = p;
    auto r = solveLayeredStatic(body, planeStrainQuarter(), l);
    ASSERT_TRUE(r.ok()) << r.message();
    const double expected = lameU(a, a, b, p, 0, E, nu);
    EXPECT_NEAR(meanRadial(r.value(), a, false), expected, 0.005 * expected);
}

// --- interfaces ------------------------------------------------------------------------------

TEST(LayeredSolid, InterfaceSpringsAddTheirComplianceInSeries)
{
    const double t1 = 1e-3, t2 = 2e-3, E1 = 100e9, E2 = 50e9, kn = 1e13, ks = 5e12;
    const auto body = plate(0.02, 0.02, {layer("a", iso("a", E1, 0.0, 1000), t1, 1), layer("b", iso("b", E2, 0.0, 1000), t2, 2)}, {springs(kn, ks)}, 2, 2);
    LayeredSupports s;
    s.innerFaceClamped = true;
    const auto onTop = [&](const Vec3& x) { return std::abs(x.z() - (t1 + t2)) < 1e-12; };
    {  // tension through the thickness
        const double sigma = 1e6;
        LayeredLoads l;
        l.outerPressure = -sigma;
        auto r = solveLayeredStatic(body, s, l);
        ASSERT_TRUE(r.ok()) << r.message();
        const double expected = sigma * (t1 / E1 + t2 / E2 + 1 / kn);
        EXPECT_NEAR(meanDisplacement(r.value(), 2, onTop), expected, 1e-6 * expected);
        for (const auto& p : r.value().interfacePoints) {
            EXPECT_NEAR(p.normalTraction, sigma, 1e-6 * sigma);
            EXPECT_NEAR(p.opening, sigma / kn, 1e-6 * sigma / kn);
        }
    }
    {  // simple shear (the normal displacement held everywhere)
        const double tau = 1e6;
        LayeredSupports sh = s;
        sh.heldAxes = {2};
        LayeredLoads l;
        l.outerTraction = Vec3(tau, 0, 0);
        auto r = solveLayeredStatic(body, sh, l);
        ASSERT_TRUE(r.ok()) << r.message();
        const double expected = tau * (t1 / (E1 / 2) + t2 / (E2 / 2) + 1 / ks);
        EXPECT_NEAR(meanDisplacement(r.value(), 0, onTop), expected, 1e-6 * expected);
    }
}

TEST(LayeredSolid, ASlidingInterfaceCarriesNoShear)
{
    LayerInterface slide;
    slide.name = "slide";
    slide.mechanical = InterfaceMechanics::Sliding;
    slide.normalStiffness = 1e13;
    const auto body = plate(0.2, 0.02, {layer("a", iso("a", 200e9, 0.0, 7800), 0.002, 2), layer("b", iso("b", 70e9, 0.0, 2700), 0.004, 2)}, {slide}, 30, 2);
    {  // a shear the interface cannot pass on: refused, not invented
        LayeredSupports s;
        s.innerFaceClamped = true;
        LayeredLoads l;
        l.outerTraction = Vec3(1e5, 0, 0);
        auto r = solveLayeredStatic(body, s, l);
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.message().find("not held"), std::string::npos) << r.message();
    }
    // Clamped at the root, the layers bend separately: no shear traction, and the
    // deflection of the two independent beams (sum of EI).
    const LayeredStaticResult r = cantilever(body, 5.0);
    double maxShear = 0.0, maxNormal = 0.0;
    for (const auto& p : r.interfacePoints) {
        maxShear = std::max(maxShear, p.shearTraction);
        maxNormal = std::max(maxNormal, std::abs(p.normalTraction));
    }
    EXPECT_LT(maxShear, 1e-9 * std::max(1.0, maxNormal));
    const double ei = (200e9 * 0.002 * 0.002 * 0.002 + 70e9 * 0.004 * 0.004 * 0.004) / 12 * 0.02;
    EXPECT_NEAR(tipDeflection(r, 0.2), 5.0 * 0.2 * 0.2 * 0.2 / (3 * ei), 0.02 * 5.0 * 0.2 * 0.2 * 0.2 / (3 * ei));
}

TEST(LayeredSolid, FrictionalContactSticksBelowAndSlipsAtTheCoulombLimit)
{
    LayerInterface fric;
    fric.name = "contact";
    fric.mechanical = InterfaceMechanics::FrictionalContact;
    fric.frictionCoefficient = 0.3;
    const auto body = plate(0.04, 0.01, {layer("base", iso("base", 70e9, 0.3, 2700), 0.002, 1), layer("slider", iso("slider", 70e9, 0.3, 2700), 0.002, 1)},
                            {fric}, 8, 1);
    LayeredSupports s;
    s.innerFaceClamped = true;
    s.clamped = {BodyEdge::U1};  // the slider is held at one end
    s.heldAxes = {1};
    const double sigmaN = 10e6, mu = 0.3;
    {  // well below the limit: every point sticks
        LayeredLoads l;
        l.outerPressure = sigmaN;
        l.outerTraction = Vec3(0.05 * mu * sigmaN, 0, 0);
        auto r = solveLayeredStatic(body, s, l);
        ASSERT_TRUE(r.ok()) << r.message();
        EXPECT_TRUE(r.value().converged);
        for (const auto& p : r.value().interfacePoints)
            EXPECT_EQ(p.state, "stick");
    }
    {  // above it: points slip with a shear traction of mu p, none exceeds it
        LayeredLoads l;
        l.outerPressure = sigmaN;
        l.outerTraction = Vec3(2 * mu * sigmaN, 0, 0);
        auto r = solveLayeredStatic(body, s, l);
        ASSERT_TRUE(r.ok()) << r.message();
        EXPECT_TRUE(r.value().converged);
        EXPECT_GT(r.value().iterations, 1);
        int slipping = 0;
        for (const auto& p : r.value().interfacePoints) {
            if (p.state == "open")
                continue;
            EXPECT_LE(p.shearTraction, mu * p.contactPressure * (1 + 1e-3) + 1e-6);
            if (p.state == "slip") {
                ++slipping;
                EXPECT_NEAR(p.shearTraction, mu * p.contactPressure, 1e-3 * mu * p.contactPressure);
            }
        }
        EXPECT_GT(slipping, 0);
    }
    {  // pulled apart: no tension, the contact opens (the slider then hangs from its clamp)
        LayeredLoads l;
        l.outerPressure = -1e4;
        auto r = solveLayeredStatic(body, s, l);
        ASSERT_TRUE(r.ok()) << r.message();
        for (const auto& p : r.value().interfacePoints) {
            EXPECT_EQ(p.state, "open");
            EXPECT_NEAR(p.normalTraction, 0.0, 1e-9);
        }
    }
    // Modes are not defined for a frictional contact.
    auto modal = solveLayeredModal(body, s, 2);
    ASSERT_FALSE(modal.ok());
    EXPECT_NE(modal.message().find("frictional"), std::string::npos);
}

TEST(LayeredSolid, ADebondedRegionIsTractionFreeAndSoftensTheStrip)
{
    const auto steel = iso("steel", 200e9, 0.0, 7800), aluminium = iso("aluminium", 70e9, 0.0, 2700);
    const auto make = [&](const LayerInterface& i) {
        return plate(0.2, 0.02, {layer("steel", steel, 0.002, 2), layer("aluminium", aluminium, 0.004, 2)}, {i}, 30, 2);
    };
    const double bondedTip = tipDeflection(cantilever(make(bonded()), 5.0), 0.2);
    LayerInterface partial = bonded();
    partial.debonded = {{0.3, 0.7, 0.0, 1.0}};
    const LayeredStaticResult r = cantilever(make(partial), 5.0);
    LayerInterface slide;
    slide.name = "slide";
    slide.mechanical = InterfaceMechanics::Sliding;
    slide.normalStiffness = 1e14;
    const double slidingTip = tipDeflection(cantilever(make(slide), 5.0), 0.2);
    const double partialTip = tipDeflection(r, 0.2);
    EXPECT_GT(partialTip, bondedTip * 1.01);
    EXPECT_LT(partialTip, slidingTip);
    int debonded = 0;
    for (const auto& p : r.interfacePoints)
        if (p.state == "debonded") {
            ++debonded;
            EXPECT_EQ(p.normalTraction, 0.0);
            EXPECT_EQ(p.shearTraction, 0.0);
        }
    EXPECT_GT(debonded, 0);
    // The bonded remainder is enforced by a penalty, and that is stated.
    bool stated = false;
    for (const auto& a : r.assumptions)
        stated = stated || a.find("penalty") != std::string::npos;
    EXPECT_TRUE(stated);
}

TEST(LayeredSolid, TheQuadraticCriterionPredictsInitiationUnderUniformTension)
{
    const double sigma = 1e6, N = 2e6, S = 3e6;
    for (const bool shared : {false, true}) {
        LayerInterface i = shared ? bonded() : springs(1e13, 5e12);
        i.normalStrength = N;
        i.shearStrength = S;
        const auto body = plate(0.02, 0.02, {layer("a", iso("a", 100e9, 0.0, 1000), 1e-3, 1), layer("b", iso("b", 50e9, 0.0, 1000), 2e-3, 2)}, {i}, 2, 2);
        LayeredSupports s;
        s.innerFaceClamped = true;
        LayeredLoads l;
        l.outerPressure = -sigma;
        auto r = solveLayeredStatic(body, s, l);
        ASSERT_TRUE(r.ok()) << r.message();
        const auto worst = r.value().maxFailureIndex(0);
        ASSERT_TRUE(worst.has_value());
        EXPECT_NEAR(*worst, (sigma / N) * (sigma / N), shared ? 1e-3 : 1e-6) << (shared ? "shared nodes" : "interface elements");
    }
}

TEST(LayeredSolid, AnInterfaceWithoutAModelIsRefusedNeverAssumedPerfect)
{
    LayerInterface undeclared;
    undeclared.name = "unknown";
    const auto body = plate(0.02, 0.02, {layer("a", iso("a", 100e9, 0.3, 1000), 1e-3, 1), layer("b", iso("b", 50e9, 0.3, 1000), 2e-3, 1)}, {undeclared}, 2, 2);
    LayeredSupports s;
    s.innerFaceClamped = true;
    auto r = solveLayeredStatic(body, s, {});
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.message().find("never assumed"), std::string::npos) << r.message();
    // A missing interface record (two layers, no interface) is refused too.
    auto missing = body;
    missing.interfaces.clear();
    EXPECT_FALSE(missing.validate().ok());
}

// --- variable thickness, grading, anisotropy ----------------------------------------------

TEST(LayeredSolid, ATaperedLayerHasItsVolumeAndBendsLikeATaperedBeam)
{
    const double L = 0.2, W = 0.02, t0 = 0.012, t1 = 0.006, E = 200e9, P = 50.0;
    BodyLayer taper = layer("taper", iso("steel", E, 0.0, 1000), t0, 2);
    taper.thickness.along = LayerThickness::Along::U;
    taper.thickness.end = t1;
    const auto body = plate(L, W, {taper}, {}, 24, 2);
    auto meshed = meshLayeredBody(body, {});
    ASSERT_TRUE(meshed.ok()) << meshed.message();
    EXPECT_NEAR(meshed.value().structure.totalMass(), 1000 * W * L * (t0 + t1) / 2, 1e-9 * 1000 * W * L * t0);
    // Euler-Bernoulli with I(x), plus Timoshenko shear, by Simpson's rule.
    const int n = 2000;
    double bend = 0.0, shear = 0.0;
    for (int i = 0; i <= n; ++i) {
        const double x = L * i / n, t = t0 + (t1 - t0) * x / L, w = (i == 0 || i == n) ? 1 : (i % 2 ? 4 : 2);
        bend += w * P * (L - x) * (L - x) / (E * W * t * t * t / 12);
        shear += w * P / (5.0 / 6.0 * E / 2 * W * t);
    }
    const double expected = (bend + shear) * L / (3.0 * n);
    EXPECT_NEAR(tipDeflection(cantilever(body, P), L), expected, 0.03 * expected);
}

TEST(LayeredSolid, AGradedLayerBendsWithTheStiffnessOfItsProfile)
{
    const double L = 0.2, W = 0.02, h = 0.01, Ea = 70e9, Eb = 200e9, P = 50.0;
    BodyLayer g = layer("graded", iso("aluminium", Ea, 0.0, 2700), h, 8);
    g.material.graded = iso("steel", Eb, 0.0, 7800);
    g.material.exponent = 2.0;
    const auto body = plate(L, W, {g}, {}, 20, 2);
    // E(z) = Ea + (Eb - Ea) (z / h)^2: the neutral axis and EI of the continuous profile.
    const int n = 4000;
    double s0 = 0, s1 = 0;
    for (int i = 0; i < n; ++i) {
        const double z = h * (i + 0.5) / n, e = Ea + (Eb - Ea) * (z / h) * (z / h);
        s0 += e * h / n;
        s1 += e * z * h / n;
    }
    const double z0 = s1 / s0;
    double ei = 0;
    for (int i = 0; i < n; ++i) {
        const double z = h * (i + 0.5) / n, e = Ea + (Eb - Ea) * (z / h) * (z / h);
        ei += W * e * (z - z0) * (z - z0) * h / n;
    }
    const double expected = P * L * L * L / (3 * ei);
    EXPECT_NEAR(tipDeflection(cantilever(body, P), L), expected, 0.02 * expected);
}

TEST(LayeredSolid, AnOrthotropicLayerFollowsItsMaterialAxes)
{
    OrthotropicConstants c;
    c.e1 = 140e9;
    c.e2 = c.e3 = 10e9;
    c.g12 = c.g13 = 5e9;
    c.g23 = 3.5e9;
    c.nu12 = c.nu13 = 0.3;
    c.nu23 = 0.4;
    const double L = 0.04, W = 0.02, t = 0.002, sigma = 10e6;
    for (const double angle : {0.0, kPi / 2}) {
        BodyLayer ply = layer("ply", iso("cfrp", 140e9, 0.3, 1600), t, 1);
        ply.material.orthotropic = c;
        ply.material.angle = angle;
        const auto body = plate(L, W, {ply}, {}, 4, 2);
        LayeredSupports s;
        s.symmetry = {{0, 0.0}, {1, 0.0}, {2, 0.0}};
        LayeredLoads l;
        l.edgeForces = {{BodyEdge::U1, Vec3(sigma * W * t, 0, 0)}};
        auto r = solveLayeredStatic(body, s, l);
        ASSERT_TRUE(r.ok()) << r.message();
        const double modulus = angle == 0.0 ? c.e1 : c.e2;
        EXPECT_NEAR(meanDisplacement(r.value(), 0, [&](const Vec3& x) { return std::abs(x.x() - L) < 1e-12; }), sigma * L / modulus,
                    1e-6 * sigma * L / modulus)
            << "angle " << angle;
    }
    // Constants that would make the material unstable are refused, not repaired.
    OrthotropicConstants bad = c;
    bad.nu12 = 4.0;  // stability needs nu12^2 < E1 / E2 = 14
    auto stiffness = orthotropicStiffness(bad);
    ASSERT_FALSE(stiffness.ok());
    EXPECT_NE(stiffness.message().find("positive-definite"), std::string::npos);
}

TEST(LayeredSolid, AnInvalidElasticityMatrixIsRefusedByTheModel)
{
    const auto body = plate(0.02, 0.02, {layer("a", iso("a", 100e9, 0.3, 1000), 1e-3, 1)}, {}, 1, 1);
    auto meshed = meshLayeredBody(body, {});
    ASSERT_TRUE(meshed.ok()) << meshed.message();
    StructuralModel m = meshed.value().structure;
    Constitutive c;
    c.stiffness = -Eigen::Matrix<double, 6, 6>::Identity() * 1e9;
    c.density = 1000;
    c.description = "negative";
    m.constitutive.push_back(c);
    m.elements.front().constitutive = 0;
    const Status s = m.validate();
    ASSERT_FALSE(s.ok());
    EXPECT_NE(s.message().find("positive definite"), std::string::npos) << s.message();
}

// --- conduction --------------------------------------------------------------------------------

TEST(LayeredSolid, ConductionThroughLayersWithAContactResistance)
{
    const double k1 = 50, t1 = 0.01, k2 = 1, t2 = 0.005, R = 1e-3, area = 0.05 * 0.05;
    BodyLayer a = layer("metal", iso("metal", 200e9, 0.3, 7800), t1, 2), b = layer("coating", iso("coating", 5e9, 0.3, 1200), t2, 2);
    a.material.conductivity = k1;
    b.material.conductivity = k2;
    LayerInterface i = bonded();
    i.thermal = InterfaceThermal::Resistance;
    i.thermalResistance = R;
    const auto body = plate(0.05, 0.05, {a, b}, {i}, 2, 2);
    LayeredThermalBoundary bc;
    bc.innerTemperature = 400;
    bc.outerTemperature = 300;
    auto r = solveLayeredConduction(body, bc);
    ASSERT_TRUE(r.ok()) << r.message();
    const double q = 100 / (t1 / k1 + R + t2 / k2);
    EXPECT_NEAR(r.value().heatIn, q * area, 1e-6 * q * area);
    EXPECT_NEAR(r.value().interfaceJump[0], -q * R, 1e-6 * q * R);  // the temperature falls across it
    // Outer convection instead of a temperature.
    LayeredThermalBoundary conv;
    conv.innerTemperature = 400;
    conv.outerConvection = std::array<double, 2>{10.0, 300.0};
    auto rc = solveLayeredConduction(body, conv);
    ASSERT_TRUE(rc.ok()) << rc.message();
    const double qc = 100 / (t1 / k1 + R + t2 / k2 + 1 / 10.0);
    EXPECT_NEAR(rc.value().heatIn, qc * area, 1e-6 * qc * area);
    // Nothing thermal is assumed.
    auto undeclared = body;
    undeclared.interfaces[0].thermal.reset();
    EXPECT_FALSE(solveLayeredConduction(undeclared, bc).ok());
    auto noConductivity = body;
    noConductivity.layers[1].material.conductivity.reset();
    EXPECT_FALSE(solveLayeredConduction(noConductivity, bc).ok());
}

TEST(LayeredSolid, RadialConductionThroughACoatedPipeMatchesTheLogarithmicProfile)
{
    const double r0 = 0.05, t1 = 0.01, t2 = 0.02, k1 = 15, k2 = 0.5, R = 2e-3, length = 0.1;
    BodyLayer a = layer("pipe", iso("steel", 200e9, 0.3, 7800), t1, 3), b = layer("insulation", iso("foam", 1e9, 0.3, 100), t2, 4);
    a.material.conductivity = k1;
    b.material.conductivity = k2;
    LayerInterface i = bonded();
    i.thermal = InterfaceThermal::Resistance;
    i.thermalResistance = R;
    LayeredBody body = quarterCylinder(r0, {a, b}, {i}, 6);
    body.surface.length = length;
    LayeredThermalBoundary bc;
    bc.innerTemperature = 450;
    bc.outerTemperature = 300;
    auto r = solveLayeredConduction(body, bc);
    ASSERT_TRUE(r.ok()) << r.message();
    const double r1 = r0 + t1, r2 = r1 + t2;
    const double expected = (kPi / 2) * length * 150 / (std::log(r1 / r0) / k1 + R / r1 + std::log(r2 / r1) / k2);
    EXPECT_NEAR(r.value().heatIn, expected, 0.005 * expected);
}

// --- display ------------------------------------------------------------------------------------

TEST(LayeredSolid, TheDisplayMeshDrawsTheOuterQuadraticFaces)
{
    const auto body = plate(0.02, 0.01, {layer("a", iso("a", 100e9, 0.3, 1000), 1e-3, 1)}, {}, 2, 1);
    auto meshed = meshLayeredBody(body, {});
    ASSERT_TRUE(meshed.ok()) << meshed.message();
    const StructuralModel& m = meshed.value().structure;
    const SurfaceMesh mesh = deformedSurface(m, {0, 0, 0}, Eigen::VectorXd::Zero(static_cast<Eigen::Index>(m.dofCount())), 1.0, {});
    // Two elements: 10 outer faces, 8 triangles each.
    EXPECT_EQ(mesh.indices.size(), 10u * 8u * 3u);
    EXPECT_FALSE(mesh.outline.empty());
}
