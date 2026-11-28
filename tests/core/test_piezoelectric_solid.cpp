// Piezoelectric continua (REQ-CHAT-20261128-005, RES-CONV-002, ADR-000233): the converse and
// direct effects against exact uniform-field solutions, the series bimorph against beam
// theory, and the unimorph harvester against the lumped cantilever kept as the reference.

#include "coupling/ConversionChains.h"
#include "mechanics/PiezoelectricBeam.h"
#include "mechanics/PiezoelectricSolid.h"

#include <gtest/gtest.h>

#include <cmath>
#include <functional>

using namespace resonant;
using namespace resonant::mechanics;

namespace {

constexpr double kEps0 = 8.8541878128e-12;

BodyLayer plainLayer(const std::string& name, double e, double nu, double rho, double t, int through)
{
    BodyLayer l;
    l.name = name;
    l.material.base.name = name;
    l.material.base.youngsModulus = e;
    l.material.base.poissonsRatio = nu;
    l.material.base.density = rho;
    l.thickness.start = t;
    l.elementsThrough = through;
    return l;
}

LayerInterface bond()
{
    LayerInterface i;
    i.name = "bond";
    i.mechanical = InterfaceMechanics::PerfectBonding;
    i.thermal = InterfaceThermal::Perfect;
    return i;
}

// An isotropic-elastic, d31-only piezoelectric (nu = 0): comparable with beam theory.
PiezoelectricConstants transverse(double e, double d31, double d32, double epsR, double rho)
{
    Eigen::Matrix<double, 6, 6> s = Eigen::Matrix<double, 6, 6>::Zero();
    for (int i = 0; i < 3; ++i)
        s(i, i) = 1 / e;
    for (int i = 3; i < 6; ++i)
        s(i, i) = 2 / e;
    Eigen::Matrix<double, 3, 6> d = Eigen::Matrix<double, 3, 6>::Zero();
    d(2, 0) = d31;
    d(2, 1) = d32;
    auto k = PiezoelectricConstants::fromStrainCharge(s, d, Eigen::Matrix3d::Identity() * epsR * kEps0, rho, "test transducer", "synthetic, for comparison with beam theory");
    EXPECT_TRUE(k.ok()) << k.message();
    return k.value();
}

double meanAt(const LayeredMesh& mesh, const Eigen::VectorXd& u, int axis, const std::function<bool(const Vec3&)>& where)
{
    double sum = 0;
    int n = 0;
    for (size_t i = 0; i < mesh.structure.nodes.size(); ++i)
        if (where(mesh.structure.nodes[i])) {
            sum += mesh.structure.translation(u, i)(axis);
            ++n;
        }
    EXPECT_GT(n, 0);
    return n ? sum / n : 0.0;
}

PiezoelectricBody freePlate(const std::string& topTerminal, Terminal::Kind kind, double voltage)
{
    PiezoelectricBody pb;
    pb.body.surface.kind = ReferenceSurface::Kind::Plane;
    pb.body.surface.length = 0.01;
    pb.body.surface.width = 0.01;
    pb.body.layers = {plainLayer("pzt", 60e9, 0.3, 7750, 0.001, 2)};
    pb.body.elementsU = 2;
    pb.body.elementsV = 2;
    pb.piezo = {{0, PiezoelectricConstants::pzt5a(), +1}};
    pb.electrodes = {{"bottom", 0, false, std::nullopt, "ground"}, {"top", 0, true, std::nullopt, topTerminal}};
    pb.terminals = {{topTerminal, kind, voltage, 0.0}};
    return pb;
}

LayeredSupports cornerSymmetry()
{
    LayeredSupports s;
    s.symmetry = {{0, 0.0}, {1, 0.0}, {2, 0.0}};
    return s;
}

}  // namespace

TEST(PiezoelectricSolid, AFreePlateUnderVoltageStrainsByItsD33AndD31)
{
    const double v = 100.0, t = 0.001, l = 0.01;
    auto r = solvePiezoStatic(freePlate("drive", Terminal::Kind::Driven, v), cornerSymmetry(), {});
    ASSERT_TRUE(r.ok()) << r.message();
    const auto k = PiezoelectricConstants::pzt5a();
    const double d33 = k.d()(2, 2), d31 = k.d()(2, 0), eps33T = k.epsT()(2, 2);
    // The field E3 = -V / t opposes the poling: the plate thins by d33 V, shrinks across by d31.
    EXPECT_NEAR(meanAt(r.value().mesh, r.value().displacement, 2, [&](const Vec3& x) { return std::abs(x.z() - t) < 1e-12; }), -d33 * v, 1e-9 * d33 * v);
    EXPECT_NEAR(meanAt(r.value().mesh, r.value().displacement, 0, [&](const Vec3& x) { return std::abs(x.x() - l) < 1e-12; }), -d31 * v * l / t,
                1e-9 * std::abs(d31 * v * l / t));
    // The free capacitance eps33^T A / t holds the charge.
    EXPECT_NEAR(std::real(r.value().terminals.at("drive").charge), eps33T * l * l / t * v, 1e-9 * eps33T * l * l / t * v);
    EXPECT_NE(r.value().fidelity.find("piezoelectricity"), std::string::npos);
}

TEST(PiezoelectricSolid, AnOpenCircuitPlateUnderPressureShowsMinusG33PT)
{
    const double p = 1e6, t = 0.001;
    LayeredLoads loads;
    loads.outerPressure = p;
    auto r = solvePiezoStatic(freePlate("open", Terminal::Kind::Floating, 0.0), cornerSymmetry(), loads);
    ASSERT_TRUE(r.ok()) << r.message();
    const auto k = PiezoelectricConstants::pzt5a();
    const double g33 = k.d()(2, 2) / k.epsT()(2, 2);
    EXPECT_NEAR(std::real(r.value().terminals.at("open").voltage), -g33 * p * t, 1e-9 * g33 * p * t);
    EXPECT_NEAR(std::real(r.value().terminals.at("open").charge), 0.0, 1e-15);
}

TEST(PiezoelectricSolid, ASeriesBimorphBendsByThreeHalvesD31VLSquaredOverTSquared)
{
    const double l = 0.1, w = 0.005, t = 0.001, d31 = 2.2e-11;
    PiezoelectricBody pb;
    pb.body.surface.kind = ReferenceSurface::Kind::Plane;
    pb.body.surface.length = l;
    pb.body.surface.width = w;
    pb.body.layers = {plainLayer("lower", 2e9, 0.0, 1780, t / 2, 1), plainLayer("upper", 2e9, 0.0, 1780, t / 2, 1)};
    pb.body.interfaces = {bond()};
    pb.body.elementsU = 20;
    pb.body.elementsV = 1;
    const auto pvdf = transverse(2e9, d31, d31, 12, 1780);
    pb.piezo = {{0, pvdf, +1}, {1, pvdf, -1}};  // opposite poling: a series bimorph
    pb.electrodes = {{"bottom", 0, false, std::nullopt, "ground"}, {"top", 1, true, std::nullopt, "drive"}};
    LayeredSupports s;
    s.clamped = {BodyEdge::U0};
    double tips[2] = {0, 0};
    for (int k = 0; k < 2; ++k) {
        pb.terminals = {{"drive", Terminal::Kind::Driven, k == 0 ? 1.0 : -1.0, 0.0}};
        auto r = solvePiezoStatic(pb, s, {});
        ASSERT_TRUE(r.ok()) << r.message();
        tips[k] = meanAt(r.value().mesh, r.value().displacement, 2, [&](const Vec3& x) { return std::abs(x.x() - l) < 1e-12; });
    }
    const double expected = 1.5 * d31 * 1.0 * l * l / (t * t);  // 0.33 um per volt
    EXPECT_NEAR(std::abs(tips[0]), expected, 0.03 * expected);
    EXPECT_NEAR(tips[1], -tips[0], 1e-9 * std::abs(tips[0]));  // linear in the voltage
}

namespace {

// A steel unimorph with a d31-only piezoelectric layer, comparable with the lumped model.
struct Unimorph {
    PiezoelectricBody body;
    HarvesterModel lumped;
};

Unimorph unimorph()
{
    const double l = 0.04, w = 0.01, ts = 5e-4, tp = 2e-4, ep = 60e9, d31 = -1.9e-10, epsR = 1800;
    Unimorph u;
    PiezoelectricBody& pb = u.body;
    pb.body.surface.kind = ReferenceSurface::Kind::Plane;
    pb.body.surface.length = l;
    pb.body.surface.width = w;
    pb.body.layers = {plainLayer("steel", 200e9, 0.0, 7800, ts, 2), plainLayer("pzt", ep, 0.0, 7750, tp, 1)};
    pb.body.interfaces = {bond()};
    pb.body.elementsU = 16;
    pb.body.elementsV = 1;
    pb.piezo = {{1, transverse(ep, d31, 0.0, epsR, 7750), +1}};
    pb.electrodes = {{"inner", 1, false, std::nullopt, "ground"}, {"outer", 1, true, std::nullopt, "load"}};
    pb.terminals = {{"load", Terminal::Kind::Load, 0.0, 1e4}};
    HarvesterSpec spec;
    spec.plies = {Ply::isotropic("steel", ts, 200e9, 0.0, 7800), Ply::isotropic("pzt", tp, ep, 0.0, 7750)};
    spec.width = w;
    spec.length = l;
    PiezoLayer layer;
    layer.ply = 1;
    layer.d31 = d31;
    layer.eps33T = epsR;
    spec.piezo = {layer};
    spec.dampingRatio = 0.01;
    auto m = harvesterModel(spec);
    EXPECT_TRUE(m.ok()) << m.message();
    u.lumped = m.value();
    return u;
}

}  // namespace

TEST(PiezoelectricSolid, TheUnimorphsModesAgreeWithTheLumpedReference)
{
    const Unimorph u = unimorph();
    LayeredSupports s;
    s.clamped = {BodyEdge::U0};
    auto sc = solvePiezoModal(u.body, s, 2, false);
    auto oc = solvePiezoModal(u.body, s, 2, true);
    ASSERT_TRUE(sc.ok()) << sc.message();
    ASSERT_TRUE(oc.ok()) << oc.message();
    const double fsc = sc.value().frequencies.front(), foc = oc.value().frequencies.front();
    EXPECT_GT(foc, fsc);  // the open circuit stiffens the transducer
    // The lumped Rayleigh model is 1.5 % above Euler-Bernoulli; the 3D model close to it.
    EXPECT_NEAR(fsc, u.lumped.shortCircuitHz, 0.03 * u.lumped.shortCircuitHz);
    // The coupling: f_oc^2 = f_sc^2 (1 + kappa^2); the lumped model's kappa^2 = theta^2 / (k C)
    // uses the static tip-load shape, which overestimates it against the exact first mode:
    // by (slope ratio)^2 (k_exact / k_static), the exact mode's tip slope phi'(L) L / phi(L)
    // and modal stiffness 0.25 (beta L)^4 EI / L^3 against 1.5 and 3 EI / L^3 (about 22 %).
    const double kappa2 = foc * foc / (fsc * fsc) - 1;
    const double bl = cantileverRoot(1), sigma = (std::cosh(bl) + std::cos(bl)) / (std::sinh(bl) + std::sin(bl));
    const double tip = std::cosh(bl) - std::cos(bl) - sigma * (std::sinh(bl) - std::sin(bl));
    const double slope = bl * (std::sinh(bl) + std::sin(bl) - sigma * (std::cosh(bl) - std::cos(bl)));  // phi'(L) L
    const double shapeFactor = std::pow(1.5 / (slope / tip), 2) * (0.25 * std::pow(bl, 4) / 3.0);
    EXPECT_GT(shapeFactor, 1.15);
    EXPECT_NEAR(kappa2, u.lumped.couplingSquared / shapeFactor, 0.08 * u.lumped.couplingSquared / shapeFactor);
}

TEST(PiezoelectricSolid, AHarvesterAtResonanceBalancesItsPowers)
{
    const Unimorph u = unimorph();
    LayeredSupports s;
    s.clamped = {BodyEdge::U0};
    auto sc = solvePiezoModal(u.body, s, 1, false);
    ASSERT_TRUE(sc.ok()) << sc.message();
    PiezoelectricBody body = u.body;
    // Near the optimal load 1 / (omega C) of the lumped model.
    const double rOpt = 1.0 / (2 * 3.14159265358979323846 * u.lumped.shortCircuitHz * u.lumped.capacitance);
    body.terminals = {{"load", Terminal::Kind::Load, 0.0, rOpt}};
    PiezoHarmonicSettings h;
    h.frequency = sc.value().frequencies.front();
    h.baseAcceleration = Vec3(0, 0, 1.0);
    h.lossFactor = 0.02;  // the lumped model's damping ratio 0.01
    auto r = solvePiezoHarmonic(body, s, h);
    ASSERT_TRUE(r.ok()) << r.message();
    const auto& p = r.value();
    EXPECT_GT(p.loadPower, 0.0);
    // Base work = damping + electrical, from independent terms.
    EXPECT_NEAR(p.residual(), 0.0, 1e-6 * p.inputPower);
    // The electrical power is that of the load's voltage.
    const auto& term = p.terminals.at("load");
    EXPECT_NEAR(std::abs(term.current), std::abs(term.voltage) / rOpt, 1e-6 * std::abs(term.current));
    // The same order as the lumped single-mode model at its own resonance (within 30 %).
    const HarvesterPoint lumped = harvesterResponse(u.lumped, u.lumped.shortCircuitHz, rOpt, 1.0);
    EXPECT_NEAR(p.loadPower, lumped.electricalPower, 0.3 * lumped.electricalPower);
}

TEST(PiezoelectricSolid, WhatCannotBeWiredIsRefused)
{
    auto pb = freePlate("drive", Terminal::Kind::Driven, 1.0);
    // No grounded or driven electrode: the potential has no reference.
    auto floating = pb;
    floating.terminals = {{"open", Terminal::Kind::Floating, 0.0, 0.0}};
    floating.electrodes = {{"top", 0, true, std::nullopt, "open"}};
    EXPECT_FALSE(solvePiezoStatic(floating, cornerSymmetry(), {}).ok());
    // An electrode wired to an unknown terminal.
    auto unknown = pb;
    unknown.electrodes.back().terminal = "nowhere";
    EXPECT_FALSE(unknown.validate().ok());
    // Two terminals on the same face would short them.
    auto shorted = pb;
    shorted.terminals.push_back({"other", Terminal::Kind::Floating, 0.0, 0.0});
    shorted.electrodes.push_back({"also-top", 0, true, std::nullopt, "other"});
    auto r = solvePiezoStatic(shorted, cornerSymmetry(), {});
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.message().find("shorted"), std::string::npos) << r.message();
    // Strain-charge data beyond the material's stability are refused.
    Eigen::Matrix<double, 6, 6> s = Eigen::Matrix<double, 6, 6>::Identity() * 1e-11;
    Eigen::Matrix<double, 3, 6> d = Eigen::Matrix<double, 3, 6>::Zero();
    d(2, 2) = 1e-8;  // far beyond what the permittivity allows (k^2 > 1)
    EXPECT_FALSE(PiezoelectricConstants::fromStrainCharge(s, d, Eigen::Matrix3d::Identity() * 1000 * kEps0, 7500, "impossible", "test").ok());
}

TEST(PiezoelectricSolid, AnAcousticWaveIsFollowedToTheElectricalLoad)
{
    // A clamped steel and PZT plate closing a duct; a plane wave of 10 Pa.
    const double side = 0.02;
    coupling::DuctPiezoelectricHarvester spec;
    PiezoelectricBody& pb = spec.body;
    pb.body.surface.kind = ReferenceSurface::Kind::Plane;
    pb.body.surface.length = side;
    pb.body.surface.width = side;
    pb.body.layers = {plainLayer("steel", 200e9, 0.3, 7800, 3e-4, 1), plainLayer("pzt", 60e9, 0.3, 7750, 1e-4, 1)};
    pb.body.interfaces = {bond()};
    pb.body.elementsU = 6;
    pb.body.elementsV = 6;
    pb.piezo = {{1, PiezoelectricConstants::pzt5a(), +1}};
    pb.electrodes = {{"inner", 1, false, std::nullopt, "ground"}, {"outer", 1, true, std::nullopt, "load"}};
    pb.terminals = {{"load", Terminal::Kind::Load, 0.0, 1e4}};
    spec.supports.clamped = {BodyEdge::U0, BodyEdge::U1, BodyEdge::V0, BodyEdge::V1};
    spec.incidentPressure = 10.0;
    spec.lossFactor = 0.02;
    auto modes = solvePiezoModal(pb, spec.supports, 1, false);
    ASSERT_TRUE(modes.ok()) << modes.message();
    double absorbed[2] = {0, 0};
    for (int k = 0; k < 2; ++k) {
        spec.frequency = modes.value().frequencies.front() * (k == 0 ? 1.0 : 0.5);
        auto r = coupling::ductPiezoelectricHarvester(spec);
        ASSERT_TRUE(r.ok()) << r.message();
        const auto& c = r.value();
        // Every stage balances from its own terms, and every join holds.
        EXPECT_EQ(c.report.status, accounting::CheckStatus::Verified) << c.report.toJson().dump(2);
        EXPECT_NEAR(c.chain.stages[0].residual(), 0.0, 1e-6 * c.incidentPower);  // incident = reflected + absorbed
        EXPECT_NEAR(c.chain.stages[1].residual(), 0.0, 1e-6 * c.absorbedPower);  // absorbed = damping + terminals
        EXPECT_NEAR(c.chain.stages[2].residual(), 0.0, 1e-6 * c.terminalPower);  // terminals = load
        EXPECT_LE(std::abs(c.reflectionCoefficient), 1.0 + 1e-12);
        EXPECT_GT(c.loadPower, 0.0);
        EXPECT_NEAR(c.report.overallEfficiency, c.loadPower / c.incidentPower, 1e-12);
        EXPECT_NEAR(c.area, side * side, 1e-12);
        absorbed[k] = c.absorbedPower;
    }
    EXPECT_GT(absorbed[0], 10 * absorbed[1]) << "the plate absorbs far more at its resonance";
}

