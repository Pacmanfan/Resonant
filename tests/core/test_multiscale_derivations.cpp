// Derivations (RES-MSC-006, -007, -009, -011): RVE homogenization with a size study and
// realizations (uncertainty, representativeness), bounds and references, a polycrystal
// against the self-consistent estimate, atomistic elastic constants as a continuum
// material, refused transitions, and diagnostic methods that derive no property.
#include "coupling/MultiscaleDerivations.h"
#include "multiscale/PotentialHomogenization.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>

using namespace resonant;
namespace ms = resonant::multiscale;
using nlohmann::json;

namespace {

std::unique_ptr<Project> example(const std::string& file)
{
    auto loaded = loadProjectFile(std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "multiscale" / file);
    EXPECT_TRUE(loaded) << loaded.message();
    return std::move(loaded.value().project);
}

ms::Representation rep(const Project& p, const std::string& id)
{
    const auto m = ms::modelOf(p).value();
    const ms::Representation* r = m.find(id);
    EXPECT_NE(r, nullptr) << id;
    return r ? *r : ms::Representation{};
}

const json* prop(const json& d, const std::string& key, const std::string& component)
{
    for (const json& p : d["properties"])
        if (p["key"] == key && p["component"] == component)
            return &p;
    return nullptr;
}

}  // namespace

TEST(MultiscaleDerivations, RveStudyConvergesAndGivesAnUncertainty)
{
    auto p = example("c-porous-composite.resonant");
    const ms::Representation r = rep(*p, "rve-porous-alumina");
    auto d = coupling::derive(r, *p, "rve.thermal-conductivity", {{"sizes", {12, 18}}, {"realizations", 3}, {"bounds", true}}, nullptr);
    ASSERT_TRUE(d) << d.message();
    const json& doc = d.value();
    const json* k = prop(doc, "thermalConductivity", "mean");
    ASSERT_NE(k, nullptr);
    const double value = (*k)["value"].get<double>();
    ASSERT_TRUE(k->contains("uncertainty"));
    EXPECT_GT((*k)["uncertainty"].get<double>(), 0.0);
    EXPECT_LT((*k)["uncertainty"].get<double>(), 0.05 * value);
    ASSERT_EQ(doc["convergence"]["sizeStudy"].size(), 2u);
    EXPECT_EQ(doc["convergence"]["sizeStudy"][1]["realizations"], 3);
    // Pores in a conductor: inside the Hashin-Shtrikman bounds, near Maxwell-Garnett with alumina as the matrix (its upper bound).
    const double f = doc["observables"]["volumeFractions"][1].get<double>();
    const auto b = ms::twoPhaseBounds(0.026, 30.0, 1.0 - f);  // phase 2 = alumina
    EXPECT_LT(value, b.hashinUpper);
    EXPECT_GT(value, b.hashinLower);
    EXPECT_NEAR(value, b.hashinUpper, 0.06 * value);
    // The apparent values bracket the periodic one.
    EXPECT_LE(doc["bounds"]["flux"].get<double>(), value * 1.001);
    EXPECT_GE(doc["bounds"]["linear"].get<double>(), value * 0.999);
    // Density by volume, specific heat by mass.
    const double rho = prop(doc, "density", "isotropic")->at("value").get<double>();
    EXPECT_NEAR(rho, (1 - f) * 3950.0 + f * 1.2, 1e-9 * rho);
    EXPECT_EQ(doc["boundaryConditions"]["kind"], "periodic");
    EXPECT_FALSE(doc["representation"]["revision"].get<std::string>().empty());
}

TEST(MultiscaleDerivations, TheArrangementNotTheFractionSetsTheConductivity)
{
    auto p = example("c-porous-composite.resonant");
    auto layered = coupling::derive(rep(*p, "rve-layered-pores"), *p, "rve.thermal-conductivity", json::object(), nullptr);
    ASSERT_TRUE(layered) << layered.message();
    const double across = prop(layered.value(), "thermalConductivity", "zz")->at("value").get<double>();
    const double along = prop(layered.value(), "thermalConductivity", "xx")->at("value").get<double>();
    EXPECT_NEAR(across, 1.0 / (0.8 / 30.0 + 0.2 / 0.026), 1e-6 * across);  // series: exact
    EXPECT_NEAR(along, 0.8 * 30.0 + 0.2 * 0.026, 1e-6 * along);            // parallel: exact
    EXPECT_GT(layered.value()["anisotropy"].get<double>(), 100.0);
    auto spheres = coupling::derive(rep(*p, "rve-porous-alumina"), *p, "rve.thermal-conductivity", json::object(), nullptr);
    ASSERT_TRUE(spheres);
    const double k = prop(spheres.value(), "thermalConductivity", "mean")->at("value").get<double>();
    EXPECT_GT(k, 50.0 * across);  // the same 20 % porosity: isolated pores barely matter, cracks across the flow dominate
    EXPECT_LT(k, along);
}

TEST(MultiscaleDerivations, AnUntexturedPolycrystalIsNearTheSelfConsistentEstimate)
{
    auto p = example("b-polycrystal.resonant");
    auto d = coupling::derive(rep(*p, "rve-graphite-random"), *p, "rve.thermal-conductivity", json::object(), nullptr);
    ASSERT_TRUE(d) << d.message();
    const double k = prop(d.value(), "thermalConductivity", "mean")->at("value").get<double>();
    const double sc = ms::selfConsistentUniaxial(1700.0, 8.0);
    EXPECT_NEAR(k, sc, 0.08 * sc);
    EXPECT_LT(k, (2 * 1700.0 + 8.0) / 3.0);
    // Texturing the c axes along z makes it anisotropic: poorer across z.
    auto t = coupling::derive(rep(*p, "rve-graphite-textured"), *p, "rve.thermal-conductivity", json::object(), nullptr);
    ASSERT_TRUE(t) << t.message();
    const double zz = prop(t.value(), "thermalConductivity", "zz")->at("value").get<double>();
    const double xx = prop(t.value(), "thermalConductivity", "xx")->at("value").get<double>();
    EXPECT_LT(zz, 0.3 * xx);
    EXPECT_FALSE(t.value()["reliability"].empty());  // one realization of a random RVE: its spread is unknown
}

TEST(MultiscaleDerivations, ArgonElasticConstantsBecomeAContinuumMaterial)
{
    auto p = example("a-crystalline-argon.resonant");
    const ms::Representation r = rep(*p, "rep-argon-fcc");
    auto d = coupling::derive(r, *p, "atomistic.elastic-constants", json::object(), nullptr);
    ASSERT_TRUE(d) << d.message();
    const json& doc = d.value();
    EXPECT_TRUE(doc["reliable"].get<bool>()) << doc["reliability"].dump();
    EXPECT_TRUE(doc["observables"]["bornStable"].get<bool>());
    EXPECT_LT(doc["observables"]["cauchyDeviation"].get<double>(), 1e-4);
    EXPECT_NEAR(doc["observables"]["cohesiveEnergyPerAtomEv"].get<double>(), 8.6102 * 119.8 * 1.380649e-23 / 1.602176634e-19, 1e-4);
    const json* E = prop(doc, "youngsModulus", "isotropic (Voigt-Reuss-Hill)");
    ASSERT_NE(E, nullptr);
    ASSERT_TRUE(E->contains("uncertainty"));  // the cutoff sensitivity
    EXPECT_LT((*E)["uncertainty"].get<double>(), 0.01 * (*E)["value"].get<double>());
    auto plan = coupling::planAssignment(doc, *p, ms::modelOf(*p).value(), {{"materialId", "mat-ex-solid-argon"}}, "test");
    ASSERT_TRUE(plan) << plan.message();
    const PropertyValue* e = plan.value().material.property(PhysicsDomain::Mechanical, "youngsModulus");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->provenance, Provenance::Computed);
    EXPECT_EQ(e->derivation, plan.value().derivation.id);
    EXPECT_NE(e->source.find("at 0 K"), std::string::npos);
    EXPECT_EQ(plan.value().derivation.sourceScale, ms::Scale::Atomistic);
    EXPECT_EQ(plan.value().derivation.representationRevision, r.revision());
    EXPECT_NE(plan.value().material.property(PhysicsDomain::Acoustic, "soundSpeed"), nullptr);
}

TEST(MultiscaleDerivations, UnsupportedTransitionsAndDiagnosticsAreRefused)
{
    auto p = example("c-porous-composite.resonant");
    const ms::Representation rve = rep(*p, "rve-porous-alumina");
    auto wrong = coupling::derive(rve, *p, "atomistic.elastic-constants", json::object(), nullptr);
    ASSERT_FALSE(wrong);
    EXPECT_NE(wrong.message().find("atomistic scale"), std::string::npos) << wrong.message();
    EXPECT_FALSE(coupling::derive(rve, *p, "rve.thermal-conductivity", {{"boundary", "sideways"}}, nullptr));
    // The pores have no electrical data: the method does not apply.
    EXPECT_FALSE(coupling::derive(rve, *p, "rve.electrical-conductivity", json::object(), nullptr));
    auto a = example("a-crystalline-argon.resonant");
    ms::Representation crystal = rep(*a, "rep-argon-fcc");
    EXPECT_FALSE(coupling::derive(crystal, *a, "rve.thermal-conductivity", json::object(), nullptr));
    crystal.data["forceField"] = "lj.reduced";  // its species is "LJ", not "Ar"
    auto incompatible = coupling::derive(crystal, *a, "atomistic.elastic-constants", json::object(), nullptr);
    ASSERT_FALSE(incompatible);
    EXPECT_NE(incompatible.message().find("not applicable to the species 'Ar'"), std::string::npos) << incompatible.message();
    // Molecular dynamics is diagnostic: it derives no library property.
    auto md = coupling::derive(rep(*a, "rep-argon-fcc"), *a, "atomistic.md-nve",
                               {{"supercell", {4, 4, 4}}, {"temperatureK", 10.0}, {"timeStep", 1e-14}, {"steps", 400}}, nullptr);
    ASSERT_TRUE(md) << md.message();
    EXPECT_TRUE(md.value()["properties"].empty());
    EXPECT_TRUE(md.value()["convergence"].contains("driftRatio"));
    EXPECT_FALSE(coupling::planAssignment(md.value(), *a, ms::modelOf(*a).value(), json::object(), "test"));
}
