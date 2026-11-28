// Validation of the coupled reference cases against analytical and independent
// solutions (RES-CEME-013, docs/physics/COUPLED_SIMULATION_VALIDATION.md).
// Every test checks the physical result, not only that the run completes.
#include "coupling/CoupledRun.h"
#include "electromagnetics/LayeredMedia.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <filesystem>

#include <gtest/gtest.h>

#include <cmath>

using namespace resonant;
using namespace resonant::multiphysics;

namespace {

constexpr double kAmbient = 293.15;

PropertyValue value(double v)
{
    PropertyValue p;
    p.value = v;
    p.provenance = Provenance::Measured;
    p.source = "validation test value";
    return p;
}

struct Props {
    double eps = 4.0, tanDelta = 0.05, sigma = 0.0;
    double k = 0.5, rho = 1500.0, cp = 1000.0, alpha = 5e-5;
    double e = 3e9, nu = 0.35;
};

Material material(const std::string& id, const Props& p)
{
    Material m(Id(id), id);
    EXPECT_TRUE(m.setProperty(PhysicsDomain::Electromagnetic, "relativePermittivity", value(p.eps)).ok());
    EXPECT_TRUE(m.setProperty(PhysicsDomain::Electromagnetic, "lossTangent", value(p.tanDelta)).ok());
    EXPECT_TRUE(m.setProperty(PhysicsDomain::Electromagnetic, "conductivity", value(p.sigma)).ok());
    EXPECT_TRUE(m.setProperty(PhysicsDomain::Electromagnetic, "relativePermeability", value(1.0)).ok());
    EXPECT_TRUE(m.setProperty(PhysicsDomain::Thermal, "thermalConductivity", value(p.k)).ok());
    EXPECT_TRUE(m.setProperty(PhysicsDomain::Thermal, "density", value(p.rho)).ok());
    EXPECT_TRUE(m.setProperty(PhysicsDomain::Thermal, "specificHeat", value(p.cp)).ok());
    EXPECT_TRUE(m.setProperty(PhysicsDomain::Thermal, "thermalExpansion", value(p.alpha)).ok());
    EXPECT_TRUE(m.setProperty(PhysicsDomain::Mechanical, "density", value(p.rho)).ok());
    EXPECT_TRUE(m.setProperty(PhysicsDomain::Mechanical, "youngsModulus", value(p.e)).ok());
    EXPECT_TRUE(m.setProperty(PhysicsDomain::Mechanical, "poissonsRatio", value(p.nu)).ok());
    return m;
}

MaterialLibrary library(const Props& p)
{
    MaterialLibrary lib;
    EXPECT_TRUE(lib.add(material("mat-slab", p)).ok());
    return lib;
}

nlohmann::json baseSystem(double thickness, int cells)
{
    nlohmann::json j = nlohmann::json::parse(R"({
      "format": "resonant.physical-system", "version": 1, "id": "sys-test", "name": "test slab",
      "geometry": {"kind": "layered-slab", "lateralArea": 0.01},
      "environment": {"ambientTemperature": 293.15},
      "regions": [{"id": "slab", "name": "Slab", "materialId": "mat-slab", "thickness": 0.01, "cells": 100}],
      "domains": [], "couplings": [], "materialLaws": [],
      "scheme": {"kind": "one-way"}
    })");
    j["regions"] = nlohmann::json::array({{{"id", "slab"}, {"name", "Slab"}, {"materialId", "mat-slab"}, {"thickness", thickness}, {"cells", cells}}});
    return j;
}

nlohmann::json emDomain(double intensity)
{
    return {{"id", "em"}, {"kind", "electromagnetic"}, {"solver", "em.layered.transfer-matrix"},
            {"settings", {{"frequencyHz", 2.45e9}, {"incidentIntensity", intensity}}}};
}

nlohmann::json thermalDomain(double h)
{
    return {{"id", "heat"}, {"kind", "thermal"}, {"solver", "thermal.conduction.fv"},
            {"settings", {{"faces", {{"-z", {{"kind", "convective"}, {"h", h}}}, {"+z", {{"kind", "convective"}, {"h", h}}}}}}}};
}

nlohmann::json link(const std::string& id, const std::string& model, const std::string& from, const std::string& to)
{
    return {{"id", id}, {"model", model}, {"from", from}, {"to", to}};
}

CoupledResult run(const nlohmann::json& json, const MaterialLibrary& lib)
{
    auto system = PhysicalSystem::fromJson(json);
    EXPECT_TRUE(system) << system.message();
    auto r = coupling::runPhysicalSystem(system.value(), lib);
    EXPECT_TRUE(r) << r.message();
    if (!r)
        return CoupledResult();
    return r.take();
}

em::LayeredSolution uniformSlab(double thickness, const Props& p, double intensity)
{
    em::LayeredProblem lp;
    lp.frequencyHz = 2.45e9;
    lp.incidentIntensity = intensity;
    em::MediumLayer l;
    l.thickness = thickness;
    l.permittivityReal = p.eps;
    l.lossTangent = p.tanDelta;
    l.conductivity = p.sigma;
    lp.layers = {l};
    return em::LayeredSolution::solve(lp).take();
}

}  // namespace

TEST(CoupledValidation, MicrowaveAbsorptionHeatsASlab)
{
    // A 10 mm lossy dielectric (eps' = 4, tan delta = 0.05) in a 2.45 GHz plane wave of
    // 20 kW/m^2, cooled by convection (h = 20 W/(m^2 K)) on both faces. Reference: the
    // exact absorbed power profile (flux differences of the transfer-matrix solution)
    // integrated twice by quadrature with the two Robin conditions.
    const Props props;
    const double d = 0.01, h = 20.0, intensity = 2e4, area = 0.01;
    nlohmann::json j = baseSystem(d, 200);
    j["domains"] = {emDomain(intensity), thermalDomain(h)};
    j["couplings"] = {link("absorb", "em.absorption-to-heat", "em", "heat")};
    const CoupledResult r = run(j, library(props));
    ASSERT_EQ(r.status, RunStatus::Completed) << r.message << " " << r.failedStage;

    const em::LayeredSolution wave = uniformSlab(d, props, intensity);
    const int n = 20000;
    std::vector<double> z(n + 1), F(n + 1), G(n + 1, 0.0);
    for (int i = 0; i <= n; ++i) {
        z[static_cast<size_t>(i)] = d * i / n;
        F[static_cast<size_t>(i)] = wave.absorbedPower(0.0, z[static_cast<size_t>(i)]);  // W/m^2 absorbed in [0, z]
    }
    for (int i = 1; i <= n; ++i)
        G[static_cast<size_t>(i)] = G[static_cast<size_t>(i) - 1] + 0.5 * (F[static_cast<size_t>(i)] + F[static_cast<size_t>(i) - 1]) * d / n;
    const double k = props.k;
    const double u = (F[n] + h * G[n] / k) / (h + h + h * h * d / k);  // T(0) - T_ambient
    const auto reference = [&](double zz) {
        const double t = zz / d * n;
        const size_t i = std::min<size_t>(static_cast<size_t>(t), static_cast<size_t>(n - 1));
        const double g = G[i] + (G[i + 1] - G[i]) * (t - static_cast<double>(i));
        return kAmbient + u + (h * u * zz - g) / k;
    };
    const FieldData& temperature = r.fields.at("heat.temperature");
    double rise = 0.0;
    for (double v : temperature.values)
        rise = std::max(rise, v - kAmbient);
    ASSERT_GT(rise, 10.0);
    for (size_t c = 0; c < temperature.values.size(); ++c)
        EXPECT_NEAR(temperature.values[c], reference(temperature.support.cellCentre(c).z()), 2e-3 * rise) << "cell " << c;

    // Energy: the absorbed power is the heat source is the heat leaving; nothing counted twice.
    const double absorbed = wave.absorptance() * intensity * area;
    ASSERT_EQ(r.ledger.transfers().size(), 1u);
    EXPECT_NEAR(r.ledger.transfers()[0].sourceIntegral, absorbed, 1e-9 * absorbed);
    EXPECT_NEAR(r.ledger.transfers()[0].targetIntegral, absorbed, 1e-9 * absorbed);
    EXPECT_LT(r.ledger.transfers()[0].relativeDifference, 1e-12);
    EXPECT_TRUE(r.audit.balanced) << r.audit.toJson().dump();
    EXPECT_LT(std::abs(r.audit.relativeResidual), 1e-9);
    EXPECT_NEAR(r.audit.externalInput, intensity * area, 1e-9);
    EXPECT_NEAR(r.audit.boundaryOut, intensity * area, 1e-9 * intensity * area);  // reflected + transmitted + heat
}

TEST(CoupledValidation, TemperatureDependentLossConvergesToTheLumpedFixedPoint)
{
    // A thin, highly conducting slab (Biot number 2e-4): its temperature is nearly
    // uniform, so the coupled solution must match the scalar fixed point
    // 2 h (T - T_amb) = A(tan delta(T)) I of a lumped slab.
    Props props;
    props.tanDelta = 0.02;
    props.k = 200.0;
    const double d = 0.002, h = 20.0, intensity = 1e5, slope = 4e-4;
    nlohmann::json j = baseSystem(d, 40);
    j["domains"] = {emDomain(intensity), thermalDomain(h)};
    j["couplings"] = {link("absorb", "em.absorption-to-heat", "em", "heat"),
                      link("loss", "thermal.temperature-to-em-properties", "heat", "em")};
    j["materialLaws"] = {{{"region", "slab"}, {"property", "electromagnetic.lossTangent"}, {"law", "linear"}, {"value", 0.02},
                          {"reference", kAmbient}, {"slope", slope}, {"validity", {250.0, 900.0}}}};
    j["scheme"] = {{"kind", "iterative"}, {"maxIterations", 60}, {"temperatureTolerance", 1e-7}};
    const MaterialLibrary lib = library(props);
    const CoupledResult r = run(j, lib);
    ASSERT_EQ(r.status, RunStatus::Converged) << r.message;
    EXPECT_LT(r.passes, 60);

    const auto residual = [&](double t) {
        Props p = props;
        p.tanDelta = 0.02 + slope * (t - kAmbient);
        return 2 * h * (t - kAmbient) - uniformSlab(d, p, intensity).absorptance() * intensity;
    };
    double lo = kAmbient, hi = kAmbient + 2000.0;
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        (residual(mid) > 0 ? hi : lo) = mid;
    }
    const double lumped = 0.5 * (lo + hi);
    const double mean = r.participants[1]["meanTemperatureK"].get<double>();
    ASSERT_GT(lumped - kAmbient, 20.0);
    EXPECT_NEAR(mean, lumped, 1e-3 * (lumped - kAmbient));

    // The feedback is physical: loss grows with temperature, so the coupled slab is hotter
    // than the one-way one, and under-relaxation reaches the same fixed point.
    nlohmann::json oneWay = j;
    oneWay["couplings"] = {link("absorb", "em.absorption-to-heat", "em", "heat")};
    oneWay["scheme"] = {{"kind", "one-way"}};
    const CoupledResult first = run(oneWay, lib);
    EXPECT_GT(mean - kAmbient, 1.05 * (first.participants[1]["meanTemperatureK"].get<double>() - kAmbient));
    j["scheme"]["relaxation"] = 0.5;
    j["scheme"]["maxIterations"] = 200;
    const CoupledResult relaxed = run(j, lib);
    ASSERT_EQ(relaxed.status, RunStatus::Converged);
    EXPECT_NEAR(relaxed.participants[1]["meanTemperatureK"].get<double>(), mean, 1e-5);
    EXPECT_TRUE(r.audit.balanced);
    // The convergence history decreases.
    EXPECT_LT(r.history.back().maxTemperatureChange, r.history[1].maxTemperatureChange);
}

TEST(CoupledValidation, UniformHeatingExpandsAndStressesTheColumn)
{
    Props props;
    props.e = 70e9;
    props.nu = 0.3;
    props.alpha = 2.3e-5;
    const double d = 0.02, dT = 50.0;
    for (const std::string lateral : {"constrained", "free"}) {
        nlohmann::json j = baseSystem(d, 10);
        j["domains"] = {{{"id", "heat"}, {"kind", "thermal"}, {"solver", "thermal.conduction.fv"},
                         {"settings", {{"faces", {{"-z", {{"kind", "temperature"}, {"temperature", kAmbient + dT}}},
                                                  {"+z", {{"kind", "temperature"}, {"temperature", kAmbient + dT}}}}}}}},
                        {{"id", "solid"}, {"kind", "mechanical"}, {"solver", "mechanics.fe.solid-column"}, {"settings", {{"lateral", lateral}}}}};
        j["couplings"] = {link("expand", "thermal.expansion", "heat", "solid")};
        const CoupledResult r = run(j, library(props));
        ASSERT_EQ(r.status, RunStatus::Completed) << r.message << " " << r.failedStage;
        const nlohmann::json& solid = r.participants[1];
        const FieldData& stress = r.fields.at("solid.stress");
        if (lateral == "constrained") {
            EXPECT_NEAR(solid["topDisplacementZM"].get<double>(), props.alpha * dT * d * (1 + props.nu) / (1 - props.nu), 1e-12);
            const double expected = -props.e * props.alpha * dT / (1 - props.nu);
            for (size_t c = 0; c < stress.support.cellCount(); ++c) {
                EXPECT_NEAR(stress.value(c, 0), expected, 1e-6 * std::abs(expected));
                EXPECT_NEAR(stress.value(c, 2), 0.0, 1e-6 * std::abs(expected));
            }
        } else {
            EXPECT_NEAR(solid["topDisplacementZM"].get<double>(), props.alpha * dT * d, 1e-12);
            EXPECT_LT(solid["maxVonMisesPa"].get<double>(), 1e-3 * props.e * props.alpha * dT);
        }
    }
}

TEST(CoupledValidation, TemperatureDependentModulusScalesTheThermalStress)
{
    Props props;
    props.e = 70e9;
    props.nu = 0.3;
    props.alpha = 2.3e-5;
    const double dT = 50.0, slope = -3e7;  // E falls by 30 MPa per kelvin
    nlohmann::json j = baseSystem(0.01, 4);
    j["domains"] = {{{"id", "heat"}, {"kind", "thermal"}, {"solver", "thermal.conduction.fv"},
                     {"settings", {{"faces", {{"-z", {{"kind", "temperature"}, {"temperature", kAmbient + dT}}},
                                              {"+z", {{"kind", "temperature"}, {"temperature", kAmbient + dT}}}}}}}},
                    {{"id", "solid"}, {"kind", "mechanical"}, {"solver", "mechanics.fe.solid-column"}, {"settings", {{"lateral", "constrained"}}}}};
    j["couplings"] = {link("expand", "thermal.expansion", "heat", "solid"), link("modulus", "thermal.temperature-to-mechanical-properties", "heat", "solid")};
    j["materialLaws"] = {{{"region", "slab"}, {"property", "mechanical.youngsModulus"}, {"law", "linear"}, {"value", props.e}, {"reference", kAmbient},
                          {"slope", slope}, {"validity", {200.0, 600.0}}}};
    j["scheme"] = {{"kind", "sequential"}, {"passes", 1}};
    const CoupledResult r = run(j, library(props));
    ASSERT_EQ(r.status, RunStatus::Completed) << r.message;
    const double modulus = props.e + slope * dT;
    const double expected = -modulus * props.alpha * dT / (1 - props.nu);
    EXPECT_NEAR(r.fields.at("solid.stress").value(0, 0), expected, 1e-6 * std::abs(expected));
}

TEST(CoupledValidation, ExpansionFeedsBackIntoTheElectromagneticSolution)
{
    // Microwave heating, conduction, expansion, and the thicker slab's new wave solution,
    // iterated to a fixed point. The expansion coefficient is exaggerated (2e-4 1/K) so
    // that the geometry feedback is clearly visible.
    Props props;
    props.alpha = 2e-4;
    const double d = 0.01, intensity = 2e4;
    nlohmann::json j = baseSystem(d, 50);
    j["domains"] = {emDomain(intensity), thermalDomain(20.0),
                    {{"id", "solid"}, {"kind", "mechanical"}, {"solver", "mechanics.fe.solid-column"}, {"settings", {{"lateral", "free"}}}}};
    j["couplings"] = {link("absorb", "em.absorption-to-heat", "em", "heat"), link("expand", "thermal.expansion", "heat", "solid"),
                      link("deform", "mechanics.deformation-to-geometry", "solid", "em")};
    j["scheme"] = {{"kind", "iterative"}, {"maxIterations", 50}, {"temperatureTolerance", 1e-6}, {"geometryTolerance", 1e-10}};
    const CoupledResult r = run(j, library(props));
    ASSERT_EQ(r.status, RunStatus::Converged) << r.message;
    const double current = r.geometry["regions"]["slab"]["thickness"]["current"].get<double>();
    const double reference = r.geometry["regions"]["slab"]["thickness"]["reference"].get<double>();
    // The thickness grew by the column's top displacement.
    EXPECT_NEAR(current - reference, r.participants[2]["topDisplacementZM"].get<double>(), 1e-12);
    EXPECT_GT(current - reference, 1e-5);
    // The final wave solution is that of the deformed slab (the same layered model, rebuilt).
    const em::LayeredSolution deformed = uniformSlab(current, props, intensity);
    EXPECT_NEAR(r.participants[0]["absorptance"].get<double>(), deformed.absorptance(), 1e-9);
    const double undeformed = uniformSlab(reference, props, intensity).absorptance();
    EXPECT_GT(std::abs(deformed.absorptance() - undeformed), 1e-5);
    EXPECT_GE(r.provenance["geometryRevision"].get<std::uint64_t>(), 2u);
    EXPECT_TRUE(r.audit.balanced);
}

TEST(CoupledValidation, TransientHeatingConservesEnergy)
{
    const Props props;
    nlohmann::json j = baseSystem(0.01, 50);
    nlohmann::json heat = thermalDomain(15.0);
    heat["settings"]["transient"] = {{"duration", 120.0}, {"steps", 240}};
    j["domains"] = {emDomain(2e4), heat};
    j["couplings"] = {link("absorb", "em.absorption-to-heat", "em", "heat")};
    const CoupledResult r = run(j, library(props));
    ASSERT_EQ(r.status, RunStatus::Completed) << r.message;
    EXPECT_EQ(r.audit.mode, BalanceMode::EnergyOverInterval);
    const double absorbed = uniformSlab(0.01, props, 2e4).absorptance() * 2e4 * 0.01 * 120.0;  // J
    EXPECT_NEAR(r.ledger.transfers()[0].targetIntegral, absorbed, 1e-9 * absorbed);
    EXPECT_TRUE(r.audit.balanced) << r.audit.toJson().dump();
    double stored = 0.0;
    for (const auto& b : r.audit.participants)
        if (b.participant == "heat")
            stored = b.stored;
    EXPECT_GT(stored, 0.5 * absorbed);  // most heat is still in the slab after two minutes
    EXPECT_LT(stored, absorbed);
}

TEST(CoupledValidation, ValidationRefusesPlannedAndExternalCouplings)
{
    const Props props;
    nlohmann::json j = baseSystem(0.01, 10);
    j["domains"] = {emDomain(1e3), thermalDomain(10.0)};
    j["couplings"] = {link("force", "em.lorentz-force", "em", "heat")};
    auto system = PhysicalSystem::fromJson(j);
    ASSERT_TRUE(system);
    const auto issues = coupling::validatePhysicalSystem(system.value(), library(props));
    ASSERT_FALSE(issues.empty());
    EXPECT_NE(issues[0].message.find("planned"), std::string::npos);
    EXPECT_FALSE(coupling::runPhysicalSystem(system.value(), library(props)));
}

TEST(CoupledValidation, TheMultiphysicsExampleConvergesWithABalancedLedger)
{
    const auto path = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "multiphysics" / "a-microwave-heated-slab.resonant";
    auto loaded = loadProjectFile(path);
    ASSERT_TRUE(loaded) << loaded.message();
    const Project& project = *loaded.value().project;
    const auto& systems = project.extensions.at("multiphysics").at("systems");
    ASSERT_EQ(systems.size(), 1u);
    auto system = PhysicalSystem::fromJson(systems[0]);
    ASSERT_TRUE(system) << system.message();
    EXPECT_EQ(system.value().regions[0].objectId, "ex-multiphysics-slab");
    EXPECT_NE(project.scene.find(Id(system.value().regions[0].objectId)), nullptr);
    auto r = coupling::runPhysicalSystem(system.value(), project.materials);
    ASSERT_TRUE(r) << r.message();
    EXPECT_EQ(r.value().status, RunStatus::Converged) << r.value().message;
    EXPECT_TRUE(r.value().audit.balanced);
    for (const std::string& w : r.value().warnings)  // the loss law is never evaluated outside its validity
        EXPECT_EQ(w.find("outside its validity"), std::string::npos) << w;
    const double peak = r.value().fields.at("heat.temperature").maxAbs();
    EXPECT_GT(peak, 293.15 + 20.0);
    EXPECT_LT(peak, 450.0);
}
