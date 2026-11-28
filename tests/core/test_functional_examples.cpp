// The reference engineering examples of functional structures (RES-FUNC-006..
// 010, RES-FUNC-013, RES-FUNC-016): each example project's analysis run and
// checked against an analytical or independent result; geometry changes
// propagate; the optimization adapter evaluates candidates.
#include "coupling/FunctionalAdapter.h"
#include "coupling/FunctionalAnalyses.h"
#include "electromagnetics/CavityResonator.h"
#include "electromagnetics/Conventions.h"
#include "functional/FunctionalStudy.h"
#include "mechanics/MechanicalOptimization.h"
#include "optimization/Evaluator.h"
#include "optimization/Persistence.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>

using namespace resonant;
using namespace resonant::functional;
using nlohmann::json;

namespace {

struct Example {
    std::unique_ptr<Project> project;
    FunctionalStructure structure;
};

Example load(const std::string& file)
{
    auto loaded = loadProjectFile(std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "functional" / file);
    EXPECT_TRUE(loaded) << loaded.message();
    Example e{std::move(loaded.value().project), {}};
    auto list = structuresOf(*e.project);
    EXPECT_TRUE(list) << list.message();
    EXPECT_EQ(list.value().size(), 1u);
    e.structure = list.value()[0];
    return e;
}

double metric(const json& doc, const std::string& key)
{
    for (const json& m : doc["metrics"])
        if (m["key"] == key)
            return m["value"].is_number() ? m["value"].get<double>() : std::numeric_limits<double>::infinity();
    ADD_FAILURE() << "no metric " << key;
    return 0.0;
}

json analyze(const Example& e, const std::string& model = "")
{
    auto doc = analyzeDocument(e.structure, *e.project, AnalysisRegistry::instance(), model, nullptr);
    EXPECT_TRUE(doc) << doc.message();
    return doc ? doc.value() : json::object();
}

}  // namespace

TEST(FunctionalAnalyses, RegistryDeclaresModelsMetricsAndRoles)
{
    const auto& kinds = AnalysisRegistry::instance().kinds();
    EXPECT_GE(kinds.size(), 7u);
    for (const AnalysisKind& k : kinds) {
        EXPECT_FALSE(k.models.empty()) << k.id;
        EXPECT_FALSE(k.metrics.empty()) << k.id;
        EXPECT_FALSE(k.mechanism.empty()) << k.id;
        EXPECT_TRUE(static_cast<bool>(k.evaluate)) << k.id;
        const json j = toJson(k);
        EXPECT_EQ(j["id"], k.id);
    }
    EXPECT_NE(AnalysisRegistry::instance().find(coupling::kCavityAnalysis), nullptr);
}

TEST(FunctionalExamples, MicrowaveCavityMatchesTheClosedFormsAndLosses)
{
    const Example e = load("a-microwave-cavity.resonant");
    const json d = analyze(e);
    // f1 of TE101 with the PTFE fill.
    const double a = 0.02286, dd = 0.030, c = em::kSpeedOfLight / std::sqrt(2.1);
    const double f1 = 0.5 * c * std::sqrt(1 / (a * a) + 1 / (dd * dd));
    EXPECT_NEAR(metric(d, "cavity.f1") / f1, 1.0, 1e-12);
    EXPECT_EQ(d["modes"][0]["label"], "TE101");
    // Losses: Qwall from the closed form, Qfill = 1 / tan(delta), the port critically coupled.
    em::CavityResonatorSpec spec;
    spec.a = a;
    spec.b = 0.01016;
    spec.d = dd;
    spec.fillPermittivity = 2.1;
    spec.wallConductivity = 5.8e7;
    EXPECT_NEAR(metric(d, "cavity.wallQ") / em::te10pWallQ(spec, 1, f1), 1.0, 1e-9);
    EXPECT_NEAR(metric(d, "cavity.fillQ"), 5000.0, 1e-6);
    EXPECT_NEAR(1.0 / metric(d, "cavity.unloadedQ"), 1.0 / metric(d, "cavity.wallQ") + 1.0 / 5000.0, 1e-12);
    EXPECT_NEAR(metric(d, "cavity.coupling"), 1.0, 1e-12);
    EXPECT_NEAR(metric(d, "cavity.portReflection"), 0.0, 1e-12);
    EXPECT_NEAR(metric(d, "cavity.loadedQ"), 0.5 * metric(d, "cavity.unloadedQ"), 1e-9);
    EXPECT_EQ(d["behaviour"], "resonant");
    // The c-modes soft constraint and the f1 objective are evaluated.
    EXPECT_TRUE(d["feasible"].get<bool>());
    EXPECT_EQ(d["objectives"][0]["metric"], "cavity.f1");
}

TEST(FunctionalExamples, MicrowaveCavityTemperatureAndSensitivityComeFromTheModel)
{
    Example e = load("a-microwave-cavity.resonant");
    const double f0 = metric(analyze(e), "cavity.f1");
    // At 373.15 K the copper walls expand by alpha dT: f scales by 1 / (1 + alpha dT).
    auto hot = withParameters(e.structure, *e.project, {{"temperature", 373.15}});
    ASSERT_TRUE(hot) << hot.message();
    const json d = analyzeDocument(hot.value().second, *hot.value().first, AnalysisRegistry::instance(), "", nullptr).value();
    const double scale = 1.0 + 16.5e-6 * 80.0;
    EXPECT_NEAR(metric(d, "cavity.f1") / (f0 / scale), 1.0, 1e-12);
    EXPECT_NEAR(metric(d, "cavity.thermalShift"), f0 / scale - f0, 1e-3);
    // Sensitivity of f1 to the width a: finite differences of the analysis against the analytical derivative.
    auto s = sensitivityDocument(e.structure, *e.project, AnalysisRegistry::instance(), {"a"}, 1e-4, "", nullptr);
    ASSERT_TRUE(s) << s.message();
    bool checked = false;
    for (const json& entry : s.value()["entries"])
        if (entry["metric"] == "cavity.f1") {
            EXPECT_NEAR(entry["derivative"].get<double>() / metric(analyze(e), "cavity.dfda"), 1.0, 1e-6);
            checked = true;
        }
    EXPECT_TRUE(checked);
    // A sweep of a tracks the labelled modes and finds where TE101 meets the 6 GHz target.
    auto sweep = sweepDocument(e.structure, *e.project, AnalysisRegistry::instance(), {{"a", {0.018, 0.02, 0.022, 0.024}}}, "", nullptr);
    ASSERT_TRUE(sweep) << sweep.message();
    EXPECT_EQ(sweep.value()["tracks"]["tracks"][0]["matchedBy"], "label");
    double previous = 1e300;
    for (const json& p : sweep.value()["points"]) {
        EXPECT_LT(p["metrics"]["cavity.f1"].get<double>(), previous);  // a wider cavity resonates lower
        previous = p["metrics"]["cavity.f1"].get<double>();
    }
}

TEST(FunctionalExamples, MicrowaveCavityFullWaveModelAgreesWithTheClosedForm)
{
    const Example e = load("a-microwave-cavity.resonant");
    const json analytical = analyze(e);
    const json yee = analyze(e, em::kCavityYeeModelId);
    ASSERT_TRUE(yee.contains("metrics"));
    EXPECT_EQ(yee["analysis"]["model"]["id"], em::kCavityYeeModelId);
    // 8 cells across b; the length rounds to whole cells (30.48 mm): within 2 %.
    EXPECT_LT(std::abs(metric(yee, "cavity.f1Deviation")), 0.02);
    EXPECT_NEAR(metric(yee, "cavity.f1") / metric(analytical, "cavity.f1"), 1.0, 0.02);
    EXPECT_EQ(yee["discretization"]["model"], "Yee grid");
    // The PTFE loss (a loss tangent as an equivalent conductivity) gives the fill Q from the decay.
    EXPECT_NEAR(metric(yee, "cavity.fillQ") / 5000.0, 1.0, 0.1);
}

TEST(FunctionalExamples, OpticalResonatorStabilityModesAndSweep)
{
    const Example e = load("b-optical-resonator.resonant");
    const json d = analyze(e);
    // Symmetric, L = 0.25 m, R = 0.5 m: g = 0.5; w0^2 = (lambda L / pi) sqrt((1 + g) / (4 (1 - g))).
    const double lambda = 1064e-9, L = 0.25, g = 0.5;
    EXPECT_NEAR(metric(d, "optics.g1g2"), g * g, 1e-12);
    EXPECT_NEAR(metric(d, "optics.waist") / std::sqrt(lambda * L / em::kPi * std::sqrt((1 + g) / (4 * (1 - g)))), 1.0, 1e-9);
    EXPECT_NEAR(metric(d, "optics.fsr"), em::kSpeedOfLight / (2 * L), 1e-3);
    EXPECT_NEAR(metric(d, "optics.transverseSpacing") / (metric(d, "optics.fsr") * std::acos(0.5) / em::kPi), 1.0, 1e-12);
    EXPECT_NEAR(metric(d, "optics.finesse"), em::kPi * std::sqrt(0.99) / 0.01, 1e-6);  // 1-inch mirrors: no clipping
    EXPECT_LT(metric(d, "optics.clipping1"), 1e-12);
    EXPECT_TRUE(d["feasible"].get<bool>());
    // Moving mirror 2 out: stable until L = 2 R = 1 m, then unstable; the derived g follows.
    auto sweep = sweepDocument(e.structure, *e.project, AnalysisRegistry::instance(), {{"L", {0.2, 0.6, 0.95, 1.2}}}, "", nullptr);
    ASSERT_TRUE(sweep) << sweep.message();
    const json& points = sweep.value()["points"];
    EXPECT_EQ(points[0]["metrics"]["optics.stable"], 1.0);
    EXPECT_EQ(points[2]["metrics"]["optics.stable"], 1.0);
    EXPECT_EQ(points[3]["metrics"]["optics.stable"], 0.0);
    EXPECT_FALSE(points[3]["feasible"].get<bool>() && points[3]["metrics"].contains("optics.waist"));
}

TEST(FunctionalExamples, AcousticHornConservesPowerAndProfilesCompare)
{
    const Example e = load("c-acoustic-horn.resonant");
    const json d = analyze(e);
    const double m = 2.0 * std::log(0.15 / 0.0254) / 0.5;
    EXPECT_NEAR(metric(d, "horn.cutoff"), m * 343.0 / (4.0 * em::kPi), 1e-9);
    EXPECT_LT(std::abs(metric(d, "horn.powerBalance")), 1e-6);
    EXPECT_GT(metric(d, "horn.radiatedPower"), 0.0);
    EXPECT_NEAR(metric(d, "horn.transmission"), 1.0 - std::pow(metric(d, "horn.reflection"), 2), 1e-12);
    // Equal throat, mouth, length and drive: the exponential profile transmits more on average than the cone.
    auto sweep = sweepDocument(e.structure, *e.project, AnalysisRegistry::instance(), {{"profile", {0.0, 1.0, 2.0}}}, "", nullptr);
    ASSERT_TRUE(sweep) << sweep.message();
    const json& p = sweep.value()["points"];
    EXPECT_GT(p[1]["metrics"]["horn.meanTransmission"].get<double>(), p[0]["metrics"]["horn.meanTransmission"].get<double>());
    ASSERT_TRUE(sweep.value()["best"].is_object());
    EXPECT_FALSE(sweep.value()["best"]["explanation"]["mechanism"].get<std::string>().empty());
    // The manufacturing constraint rejects a thin wall.
    auto thin = withParameters(e.structure, *e.project, {{"wall", 0.001}});
    ASSERT_TRUE(thin);
    const json t = analyzeDocument(thin.value().second, *thin.value().first, AnalysisRegistry::instance(), "", nullptr).value();
    EXPECT_FALSE(t["feasible"].get<bool>());
    EXPECT_EQ(t["constraints"][0]["process"], "injection-molding");
}

TEST(FunctionalExamples, MechanicalResonatorMatchesEulerBernoulliReusesItsMeshAndHasTheLengthSensitivity)
{
    const Example e = load("d-mechanical-resonator.resonant");
    SimulationGeometryCache::shared().clear();
    const json first = analyze(e);
    EXPECT_LT(std::abs(metric(first, "mech.f1Deviation")), 0.01);
    EXPECT_FALSE(first["discretization"]["reused"].get<bool>());
    const json again = analyze(e);
    EXPECT_TRUE(again["discretization"]["reused"].get<bool>());  // unchanged geometry: the mesh is reused
    EXPECT_EQ(metric(again, "mech.f1"), metric(first, "mech.f1"));
    auto longer = withParameters(e.structure, *e.project, {{"length", 0.55}});
    ASSERT_TRUE(longer);
    const json changed = analyzeDocument(longer.value().second, *longer.value().first, AnalysisRegistry::instance(), "", nullptr).value();
    EXPECT_FALSE(changed["discretization"]["reused"].get<bool>());  // a changed geometry is meshed again
    // Euler-Bernoulli: f ~ t / L^2, so d ln f / d ln L = -2 and d ln f / d ln t = +1.
    auto s = sensitivityDocument(e.structure, *e.project, AnalysisRegistry::instance(), {"length", "thickness"}, 1e-3, "mechanics.modal.analytical", nullptr);
    ASSERT_TRUE(s) << s.message();
    for (const json& entry : s.value()["entries"]) {
        if (entry["metric"] == "mech.f1") {
            EXPECT_NEAR(entry["normalized"].get<double>(), entry["parameter"] == "length" ? -2.0 : 1.0, 1e-5) << entry["parameter"];
        }
    }
}

TEST(FunctionalExamples, UnitCellGapMatchesTheQuarterWaveDesignAndTheStack)
{
    const Example e = load("e-photonic-unit-cell.resonant");
    const json d = analyze(e);
    EXPECT_NEAR(metric(d, "gap1.center") / 10e9, 1.0, 1e-7);
    const double n1 = 3.0, n2 = std::sqrt(2.9);
    EXPECT_NEAR(metric(d, "gap1.relativeWidth"), 4.0 / em::kPi * std::asin((n1 - n2) / (n1 + n2)), 1e-7);
    // Ten cells: T at the gap centre is C exp(-2 N Im(K) a) with an interface factor C > 1, so
    // ln T / (-2 N Im(K) a) lies a little below 1 (0.88 here; the rate itself is checked in PeriodicLayers.*).
    EXPECT_LT(metric(d, "stack.transmissionAtGapCenter"), 1e-3);
    const double ratio = std::log(metric(d, "stack.transmissionAtGapCenter")) / (-2.0 * 10.0 * metric(d, "gap1.attenuationPerCell"));
    EXPECT_GT(ratio, 0.8);
    EXPECT_LT(ratio, 1.0);
    // At the homogenization limit (a / lambda = 0.1) the Bloch index differs from sqrt(eps_parallel) by 0.13 %.
    EXPECT_NEAR(metric(d, "cell.effectiveIndex") / std::sqrt(metric(d, "cell.permittivityParallel")), 1.0, 2e-3);
    EXPECT_LT(metric(d, "cell.homogenizationLimit"), metric(d, "gap1.lower"));
    EXPECT_EQ(d["behaviour"], "mixed");
}

TEST(FunctionalExamples, CoupledSlabRunsTheEngineWithTheThicknessOfItsObject)
{
    const Example e = load("f-coupled-slab.resonant");
    const json d = analyze(e);
    EXPECT_EQ(metric(d, "coupled.converged"), 1.0);
    EXPECT_EQ(metric(d, "coupled.balanced"), 1.0);
    EXPECT_GT(metric(d, "coupled.peakTemperature"), 293.15);
    // A thicker box gives a thicker region in the coupled system, and a different absorptance.
    auto thick = withParameters(e.structure, *e.project, {{"thickness", 0.02}});
    ASSERT_TRUE(thick);
    const json t = analyzeDocument(thick.value().second, *thick.value().first, AnalysisRegistry::instance(), "", nullptr).value();
    EXPECT_EQ(t["details"]["linkedRegions"][0]["thickness"], 0.02);
    EXPECT_NE(metric(t, "coupled.absorptance"), metric(d, "coupled.absorptance"));
    EXPECT_EQ(metric(t, "coupled.balanced"), 1.0);
}

TEST(FunctionalExamples, GeometryChangesMakeResultsStale)
{
    Example e = load("a-microwave-cavity.resonant");
    const json d = analyze(e);
    const DependencyRecord recorded = DependencyRecord::fromJson(d["dependencies"]);
    auto changed = withParameters(e.structure, *e.project, {{"d", 0.032}});
    ASSERT_TRUE(changed);
    const DependencyRecord now = dependenciesOf(changed.value().second, *changed.value().first,
                                                {{"kind", e.structure.analysis.kind}, {"model", em::kCavityModelId}, {"version", "1"},
                                                 {"settings", e.structure.analysis.settings}});
    const auto reasons = staleness(recorded, now, *changed.value().first);
    ASSERT_FALSE(reasons.empty());
    EXPECT_EQ(reasons[0].what, "geometry");
}

TEST(FunctionalAnalyses, OptimizationAdapterEvaluatesCandidatesThroughTheAnalysis)
{
    const Example e = load("d-mechanical-resonator.resonant");
    auto bound = optimization::AdapterRegistry::instance().bind(coupling::kFunctionalAdapterId, optimization::designSnapshot(*e.project),
                                                                {{"structure", "fs-mechanical-resonator"}});
    ASSERT_TRUE(bound) << bound.message();
    const auto& caps = bound.value()->capabilities();
    ASSERT_EQ(caps.variables.size(), 3u);  // length, thickness, width (the derived slenderness is no variable)
    EXPECT_NE(caps.quantity("mech.f1"), nullptr);
    EXPECT_NE(caps.quantity("constraint.c-thick"), nullptr);
    std::map<std::string, double, std::less<>> point;
    for (const auto& v : caps.variables)
        point[v.key()] = v.initial;
    const auto base = bound.value()->evaluate(point, "mechanics.modal.analytical", nullptr);
    ASSERT_TRUE(base.ok()) << base.message;
    point[caps.variables[0].key()] = 0.25;  // half the length: four times the frequency
    const auto shorter = bound.value()->evaluate(point, "mechanics.modal.analytical", nullptr);
    ASSERT_TRUE(shorter.ok()) << shorter.message;
    EXPECT_NEAR(shorter.quantities.at("mech.f1") / base.quantities.at("mech.f1"), 4.0, 1e-9);
    EXPECT_EQ(shorter.quantities.at("constraint.c-thick"), 0.005);
    EXPECT_EQ(shorter.model.adapterId, "functional");
    // A free parameter is a domain binding.
    const Example o = load("b-optical-resonator.resonant");
    auto optical = optimization::AdapterRegistry::instance().bind(coupling::kFunctionalAdapterId, optimization::designSnapshot(*o.project),
                                                                  {{"structure", "fs-optical-resonator"}});
    ASSERT_TRUE(optical) << optical.message();
    std::map<std::string, double, std::less<>> q;
    for (const auto& v : optical.value()->capabilities().variables)
        q[v.key()] = v.initial;
    q["parameter/R"] = 0.9;
    const auto lowR = optical.value()->evaluate(q, "", nullptr);
    ASSERT_TRUE(lowR.ok()) << lowR.message;
    EXPECT_NEAR(lowR.quantities.at("optics.finesse"), em::kPi * std::sqrt(0.9) / 0.1, 1e-6);
}
