// The metamaterial examples (RES-META-001..010, RES-META-012): each example's analysis
// against closed forms and independent references, the effective material it reports and
// the validity it states; a geometry sweep of the unit cell.
#include "coupling/MetamaterialAnalyses.h"
#include "functional/EffectiveMaterial.h"
#include "functional/FunctionalStudy.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>

using namespace resonant;
using namespace resonant::functional;
using nlohmann::json;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Example {
    std::unique_ptr<Project> project;
    FunctionalStructure structure;
};

Example load(const std::string& file)
{
    auto loaded = loadProjectFile(std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "metamaterials" / file);
    EXPECT_TRUE(loaded) << loaded.message();
    Example e{std::move(loaded.value().project), {}};
    e.structure = structuresOf(*e.project).value().at(0);
    return e;
}

json analyze(const Example& e)
{
    auto d = analyzeDocument(e.structure, *e.project, AnalysisRegistry::instance(), "", nullptr);
    EXPECT_TRUE(d) << d.message();
    return d ? d.value() : json::object();
}

double metric(const json& d, const std::string& key)
{
    for (const json& m : d["metrics"])
        if (m["key"] == key)
            return m["value"].is_number() ? m["value"].get<double>() : std::numeric_limits<double>::infinity();
    ADD_FAILURE() << "no metric " << key;
    return 0.0;
}

EffectiveMaterial effective(const json& d)
{
    auto e = EffectiveMaterial::fromJson(d["details"]["effectiveMaterial"]);
    EXPECT_TRUE(e) << e.message();
    return e ? e.value() : EffectiveMaterial{};
}

const EffectiveProperty* find(const EffectiveMaterial& e, const std::string& key, const std::string& component)
{
    for (const auto& p : e.properties)
        if (p.key == key && p.component == component)
            return &p;
    return nullptr;
}

}  // namespace

TEST(MetamaterialExamples, MassInMassGapComesFromTheCoreResonance)
{
    const Example e = load("a-mass-in-mass-lattice.resonant");
    const json d = analyze(e);
    EXPECT_EQ(d["analysis"]["category"], "effective-medium");
    // The cell's masses are derived from its geometry: 7850 w^3 and 11340 (4/3) pi r^3.
    const double m1 = 7850 * std::pow(0.02, 3), m2 = 11340 * 4.0 / 3.0 * kPi * std::pow(0.008, 3);
    const double f2 = std::sqrt(8.6e4 / m2) / (2 * kPi);
    EXPECT_NEAR(metric(d, "lattice.localResonance"), f2, 1e-9 * f2);
    EXPECT_NEAR(metric(d, "gap.upper"), f2 * std::sqrt(1 + m2 / m1), 1e-9 * f2);
    EXPECT_NEAR(metric(d, "gap.negativeMassUpper"), metric(d, "gap.upper"), 1e-9 * f2);
    EXPECT_LT(metric(d, "gap.lower"), f2);
    EXPECT_LT(metric(d, "lattice.effectiveMassAtGapCentre"), 0.0);
    // The gap lies far below the host chain's own zone edge: a locally resonant, not a Bragg, gap.
    EXPECT_GT(metric(d, "lattice.hostZoneEdge"), 4 * metric(d, "gap.upper"));
    EXPECT_LT(metric(d, "bloch.maxDeviation"), 1e-9);
    EXPECT_NEAR(metric(d, "chain.decayPerCell"), metric(d, "gap.attenuationPerCell"), 0.02 * metric(d, "gap.attenuationPerCell"));
    EXPECT_NEAR(metric(d, "lattice.longWaveSpeedBloch"), metric(d, "lattice.longWaveSpeed"), 1e-4 * metric(d, "lattice.longWaveSpeed"));
    // The dispersive mass is a result, not a library material.
    EXPECT_FALSE(d["details"].contains("effectiveMaterial"));
    EXPECT_TRUE(d["details"]["representations"].contains("effective"));
}

TEST(MetamaterialExamples, AHeavierCoreWidensTheGap)
{
    const Example e = load("a-mass-in-mass-lattice.resonant");
    auto sweep = sweepDocument(e.structure, *e.project, AnalysisRegistry::instance(), {{"r", {0.004, 0.006, 0.008, 0.01}}}, "", nullptr);
    ASSERT_TRUE(sweep) << sweep.message();
    double previous = 0.0;
    for (const json& p : sweep.value()["points"]) {
        const double width = p["metrics"]["gap.relativeWidth"].get<double>();
        EXPECT_GT(width, previous);
        previous = width;
    }
}

TEST(MetamaterialExamples, ResonatorDuctSlowsSoundAndBlocksItAboveTheResonance)
{
    const Example e = load("b-resonator-duct.resonant");
    const json d = analyze(e);
    const double f0 = metric(d, "duct.resonatorF0");
    EXPECT_GT(f0, 300.0);
    EXPECT_LT(f0, 700.0);
    EXPECT_NEAR(metric(d, "duct.lowFrequencySpeedBloch"), metric(d, "duct.lowFrequencySpeed"), 1e-3 * 343.0);
    EXPECT_LT(metric(d, "gap1.lower"), f0);
    EXPECT_GT(metric(d, "gap1.upper"), f0);
    EXPECT_NEAR(metric(d, "gap1.upper"), metric(d, "duct.negativeModulusUpper"), 0.1 * f0);
    EXPECT_NEAR(metric(d, "gap1.measuredAttenuationDbPerCell"), metric(d, "gap1.blochAttenuationDbPerCell"),
                0.01 * metric(d, "gap1.blochAttenuationDbPerCell"));
    EXPECT_LT(metric(d, "gap1.transmissionDb"), -20.0);
    EXPECT_NEAR(metric(d, "gap2.lower"), metric(d, "duct.braggFrequency"), 0.2 * metric(d, "duct.braggFrequency"));
    const EffectiveMaterial m = effective(d);
    EXPECT_EQ(m.kind, "homogenized");
    EXPECT_NEAR(m.validToHz, 0.5 * f0, 1e-9 * f0);
    const EffectiveProperty* c = find(m, "soundSpeed", "along the duct");
    ASSERT_NE(c, nullptr);
    EXPECT_NEAR(c->value, metric(d, "duct.lowFrequencySpeed"), 1e-9);
    ASSERT_FALSE(c->frequencyTable.empty());
    // Dispersive: the speed falls towards the resonance.
    EXPECT_LT(c->frequencyTable.back().second, c->frequencyTable.front().second);
}

TEST(MetamaterialExamples, LayeredAnisotropyMatchesTheExplicitLayersInsideItsValidity)
{
    const Example e = load("c-layered-anisotropy.resonant");
    const json d = analyze(e);
    EXPECT_NEAR(metric(d, "effective.permittivityParallel"), (1 * 9.0 + 2 * 2.1) / 3, 1e-6);
    EXPECT_NEAR(metric(d, "effective.permittivityPerpendicular"), 3 / (1 / 9.0 + 2 / 2.1), 1e-6);
    // Inside the validity the uniaxial slab reproduces the explicit layers ...
    EXPECT_LT(metric(d, "compare.maxDifferenceTE"), 0.02);
    EXPECT_LT(metric(d, "compare.maxDifferenceTM"), 0.02);
    // ... and beyond it the error grows; an isotropic description misses TM.
    EXPECT_GT(metric(d, "compare.maxDifferenceBeyondTM"), 3 * metric(d, "compare.maxDifferenceTM"));
    EXPECT_GT(metric(d, "compare.isotropicDifferenceTM"), 5 * metric(d, "compare.maxDifferenceTM"));
    EXPECT_LT(metric(d, "bloch.deviationTE"), 5e-3);
    EXPECT_LT(metric(d, "bloch.deviationTM"), 5e-3);
    EXPECT_NEAR(metric(d, "retrieval.permittivity"), metric(d, "effective.permittivityParallel"), 1e-3 * 4.4);
    EXPECT_NEAR(metric(d, "retrieval.permeability"), 1.0, 1e-3);
    EXPECT_LT(metric(d, "retrieval.maxDeviation"), 0.02);
    EXPECT_EQ(metric(d, "retrieval.flagged"), 0.0);
    EXPECT_LT(metric(d, "retrieval.antiresonance"), 1e-5);  // spatial dispersion, small inside the validity
    const EffectiveMaterial m = effective(d);
    ASSERT_NE(find(m, "relativePermittivity", "parallel"), nullptr);
    ASSERT_NE(find(m, "relativePermittivity", "perpendicular"), nullptr);
    EXPECT_NEAR(m.validToHz, metric(d, "effective.validTo"), 1.0);
    EXPECT_EQ(m.derivation["structure"]["id"], "fs-layered-anisotropy");
    EXPECT_EQ(m.derivation["cell"]["layers"].size(), 2u);
}

TEST(MetamaterialExamples, SplitRingArrayHasANegativePermeabilityBand)
{
    const Example e = load("d-split-ring-array.resonant");
    const json d = analyze(e);
    const double f0 = metric(d, "srr.resonance");
    EXPECT_GT(f0, 5e9);
    EXPECT_LT(f0, 10e9);
    EXPECT_LT(metric(d, "srr.minPermeability"), 0.0);
    EXPECT_GT(metric(d, "srr.negativeBandLower"), f0);
    EXPECT_NEAR(metric(d, "srr.negativeBandUpper"), metric(d, "srr.magneticPlasma"), 0.02 * f0);
    EXPECT_EQ(metric(d, "srr.passive"), 1.0);
    EXPECT_LT(metric(d, "srr.kramersKronigResidual"), 2e-3);
    EXPECT_EQ(d["details"]["approximation"], "a quasi-static circuit model; not compared with a full-wave calculation");
    const EffectiveMaterial m = effective(d);
    EXPECT_EQ(m.kind, "equivalent-model");
    const EffectiveProperty* mu = find(m, "relativePermeability", "normal to the rings");
    ASSERT_NE(mu, nullptr);
    for (const auto& [f, v] : mu->frequencyTable)
        EXPECT_GT(v, 1.0) << f;  // paramagnetic below the resonance: the stored range
    // A coarse lattice leaves the effective-medium regime: no effective material.
    FunctionalStructure coarse = e.structure;
    for (auto& p : coarse.parameters)
        if (p.id == "pitch")
            p.value = 9e-3;
    coarse.analysis.settings["maxScaleRatio"] = 0.1;
    auto c = analyzeDocument(coarse, *e.project, AnalysisRegistry::instance(), "", nullptr);
    ASSERT_TRUE(c) << c.message();
    EXPECT_FALSE(c.value()["details"].contains("effectiveMaterial"));
    EXPECT_FALSE(c.value()["warnings"].empty());
}

TEST(MetamaterialExamples, LayeredThermalTensorMatchesFiniteVolumes)
{
    const Example e = load("e-layered-thermal.resonant");
    const json d = analyze(e);
    EXPECT_NEAR(metric(d, "thermal.parallel"), (0.5 * 400 + 0.1 * 0.2) / 0.6, 1e-9);
    EXPECT_NEAR(metric(d, "thermal.perpendicular"), 0.6 / (0.5 / 400 + 0.1 / 0.2), 1e-9);
    EXPECT_LT(metric(d, "explicit.deviationParallel"), 1e-8);
    EXPECT_LT(metric(d, "explicit.deviationPerpendicular"), 1e-8);
    EXPECT_GT(metric(d, "thermal.anisotropy"), 250.0);
    const EffectiveMaterial m = effective(d);
    EXPECT_EQ(m.properties.front().component, "perpendicular");  // the default for a laminate
    EXPECT_NEAR(find(m, "density", "isotropic")->value, (0.5 * 8960 + 0.1 * 1200) / 0.6, 1e-9);
}

TEST(MetamaterialExamples, EffectiveMaterialsBecomeLibraryMaterialsWithTheirDerivation)
{
    const Example e = load("c-layered-anisotropy.resonant");
    const EffectiveMaterial m = effective(analyze(e));
    auto lib = toLibraryMaterial(m, Id("mat-eff"), "", json::object(), "result r-1");
    ASSERT_TRUE(lib) << lib.message();
    const PropertyValue* eps = lib.value().property(PhysicsDomain::Electromagnetic, "relativePermittivity");
    ASSERT_NE(eps, nullptr);
    EXPECT_EQ(eps->provenance, Provenance::Homogenized);
    EXPECT_NEAR(eps->value, find(m, "relativePermittivity", "parallel")->value, 1e-12);
    EXPECT_NE(eps->source.find("fs-layered-anisotropy"), std::string::npos);
    EXPECT_NE(eps->source.find("result r-1"), std::string::npos);
    ASSERT_TRUE(eps->validity.maxFrequencyHz);
    EXPECT_NEAR(*eps->validity.maxFrequencyHz, m.validToHz, 1e-6);
    // The other principal component on request; an unknown one is refused.
    auto perp = toLibraryMaterial(m, Id("mat-eff2"), "Across", json{{"electromagnetic.relativePermittivity", "perpendicular"}}, "");
    ASSERT_TRUE(perp) << perp.message();
    EXPECT_NEAR(perp.value().property(PhysicsDomain::Electromagnetic, "relativePermittivity")->value,
                find(m, "relativePermittivity", "perpendicular")->value, 1e-12);
    EXPECT_FALSE(toLibraryMaterial(m, Id("mat-eff3"), "", json{{"electromagnetic.relativePermittivity", "sideways"}}, ""));
    // The record round-trips.
    auto back = EffectiveMaterial::fromJson(m.toJson());
    ASSERT_TRUE(back);
    EXPECT_EQ(back.value().toJson(), m.toJson());
}
