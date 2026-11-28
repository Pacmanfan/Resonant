// The functional-structure framework (RES-FUNC-001..004, RES-FUNC-011,
// RES-FUNC-012): the document, parameters, fingerprints and staleness, the
// simulation-geometry cache, sweeps, sensitivities, mode tracking,
// objectives, constraints and explanations, with a synthetic analysis.
#include "functional/Dependencies.h"
#include "functional/FunctionalStudy.h"
#include "geometry/Geometry.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "scene/Scene.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace resonant;
using namespace resonant::functional;
using nlohmann::json;

namespace {

AnalysisKind boxKind()
{
    AnalysisKind k;
    k.id = "test.box";
    k.title = "Box";
    k.mechanism = "a synthetic model for tests";
    k.models = {{"test.model", "Test model", "1", "analytical", {"synthetic"}, "always", 0.0}};
    k.metrics = {{"box.volume", "Volume", "m^3", "w d h"}, {"box.f", "Inverse width", "1/m", "1 / w"}, {"box.scaled", "Free scale", "", "k w"}};
    k.roles = {{"box", "body", "a box", true}};
    k.evaluate = [](const FunctionalStructure& s, const Project& p, const Values& v, const ModelInfo&, JobContext*) -> Result<AnalysisOutcome> {
        const SceneObject* o = p.scene.find(Id(s.body("box")->objectId));
        const Geometry& g = *o->state.geometry;
        AnalysisOutcome out;
        const double w = g.parameter("width"), d = g.parameter("depth"), h = g.parameter("height");
        out.metrics["box.volume"] = w * d * h;
        out.metrics["box.f"] = 1.0 / w;
        out.metrics["box.scaled"] = v.at("k") * w;
        // Two labelled modes that cross when 100 w = 150 d.
        out.modes = {{"A", 100.0 * w, 10.0, {}}, {"B", 150.0 * d, 20.0, {}}};
        out.discretization = {{"cells", 10}};
        out.behaviour = "resonant";
        return out;
    };
    return k;
}

struct Fixture {
    std::unique_ptr<Project> project = Project::createNew("functional");
    AnalysisRegistry registry;
    FunctionalStructure s;
    std::string boxId;
    Fixture()
    {
        registry.add(boxKind());
        Geometry box = Geometry::create("box").take();
        EXPECT_TRUE(box.setParameter("width", 1.0).ok());
        EXPECT_TRUE(box.setParameter("depth", 0.5).ok());
        EXPECT_TRUE(box.setParameter("height", 0.2).ok());
        auto object = makeGeometryObject("Block", box);
        object->state.materialId = Id("mat-aluminum");
        boxId = object->id().str();
        EXPECT_TRUE(project->scene.insert(std::move(object)));
        s = FunctionalStructure::fromJson(document()).take();
    }
    json document() const
    {
        return {{"format", kFormat},
                {"version", kVersion},
                {"id", "fs-test"},
                {"name", "Test structure"},
                {"bodies", {{{"objectId", boxId}, {"role", "box"}}}},
                {"parameters",
                 {{{"id", "w"}, {"kind", "bound"}, {"binding", "object/" + boxId + "/geometry/width"}, {"max", 3.0}},
                  {{"id", "d"}, {"kind", "bound"}, {"binding", "object/" + boxId + "/geometry/depth"}},
                  {{"id", "z"}, {"kind", "bound"}, {"binding", "object/" + boxId + "/position/z"}, {"min", -1.0}, {"max", 1.0}},
                  {{"id", "k"}, {"kind", "free"}, {"value", 2.0}, {"min", 0.0}, {"max", 10.0}},
                  {{"id", "area"}, {"kind", "derived"}, {"expression", "w * d"}, {"unit", "m^2"}}}},
                {"regions", {{{"id", "r1"}, {"objectId", boxId}, {"role", "box"}, {"materialId", "mat-aluminum"}}}},
                {"analysis", {{"kind", "test.box"}, {"model", ""}, {"settings", json::object()}}},
                {"objectives", {{{"id", "o1"}, {"metric", "box.volume"}, {"sense", "target"}, {"target", 0.2}, {"weight", 1.0}},
                                {{"id", "o2"}, {"metric", "box.f"}, {"sense", "maximize"}, {"weight", 0.1}}}},
                {"constraints", {{{"id", "c1"}, {"expression", "w"}, {"relation", ">="}, {"limit", 0.6}, {"process", "cnc"}, {"hard", true}},
                                 {{"id", "c2"}, {"expression", "[box.volume]"}, {"relation", "<="}, {"limit", 0.15}, {"hard", false}}}}};
    }
};

double metricOf(const json& doc, const std::string& key)
{
    for (const json& m : doc["metrics"])
        if (m["key"] == key)
            return m["value"].get<double>();
    return std::numeric_limits<double>::quiet_NaN();
}

}  // namespace

TEST(FunctionalStructures, SerializationRoundTripsAndValidates)
{
    Fixture f;
    const json again = FunctionalStructure::fromJson(f.s.toJson()).value().toJson();
    EXPECT_EQ(again, f.s.toJson());
    EXPECT_TRUE(validateStructure(f.s, *f.project).empty());
    EXPECT_TRUE(validateForAnalysis(f.s, f.registry).empty());
    // Stored in a project: survives save and load.
    storeStructures(*f.project, {f.s});
    auto loaded = deserializeProject(serializeProject(*f.project));
    ASSERT_TRUE(loaded) << loaded.message();
    auto list = structuresOf(*loaded.value().project);
    ASSERT_TRUE(list);
    ASSERT_EQ(list.value().size(), 1u);
    EXPECT_EQ(list.value()[0].toJson(), f.s.toJson());
    // Invalid documents name their problems.
    json bad = f.document();
    bad["parameters"][0]["binding"] = "object/" + f.boxId + "/geometry/nonsense";
    bad["regions"][0]["materialId"] = "mat-missing";
    bad["parameters"].push_back({{"id", "loop1"}, {"kind", "derived"}, {"expression", "loop2"}});
    bad["parameters"].push_back({{"id", "loop2"}, {"kind", "derived"}, {"expression", "loop1 + 1"}});
    bad["objectives"][0]["metric"] = "box.nothing";
    const auto issues = validateStructure(FunctionalStructure::fromJson(bad).value(), *f.project);
    const auto text = [&] {
        std::string t;
        for (const auto& i : issues)
            t += i.field + ": " + i.message + "\n";
        return t;
    }();
    EXPECT_NE(text.find("no geometry parameter 'nonsense'"), std::string::npos) << text;
    EXPECT_NE(text.find("mat-missing"), std::string::npos) << text;
    EXPECT_NE(text.find("cycle"), std::string::npos) << text;
    EXPECT_FALSE(validateForAnalysis(FunctionalStructure::fromJson(bad).value(), f.registry).empty());
    EXPECT_FALSE(FunctionalStructure::fromJson({{"format", "other"}}));
}

TEST(FunctionalStructures, ParametersResolveApplyAndRefuse)
{
    Fixture f;
    auto values = resolveParameters(f.s, *f.project);
    ASSERT_TRUE(values) << values.message();
    const Values v = valuesOf(values.value());
    EXPECT_EQ(v.at("w"), 1.0);
    EXPECT_EQ(v.at("k"), 2.0);
    EXPECT_NEAR(v.at("area"), 0.5, 1e-15);
    EXPECT_EQ(values.value()[0].unit, "m");      // from the geometry kind
    EXPECT_EQ(values.value()[0].maximum, 3.0);   // the structure narrows the kind's range
    EXPECT_FALSE(values.value()[4].editable);
    // Predictable updates through the geometry kind's rules.
    FunctionalStructure s = f.s;
    ASSERT_TRUE(applyParameters(s, *f.project, {{"w", 2.0}, {"k", 3.0}, {"z", 0.25}}));
    const Values after = valuesOf(resolveParameters(s, *f.project).value());
    EXPECT_EQ(after.at("w"), 2.0);
    EXPECT_EQ(after.at("k"), 3.0);
    EXPECT_EQ(after.at("z"), 0.25);
    EXPECT_NEAR(after.at("area"), 1.0, 1e-15);
    EXPECT_FALSE(applyParameters(s, *f.project, {{"area", 2.0}}));   // derived
    EXPECT_FALSE(applyParameters(s, *f.project, {{"w", 5.0}}));      // outside the structure's range
    EXPECT_FALSE(applyParameters(s, *f.project, {{"w", -1.0}}));     // the geometry kind refuses
    EXPECT_FALSE(applyParameters(s, *f.project, {{"nope", 1.0}}));
}

TEST(FunctionalStructures, StalenessNamesWhatChanged)
{
    Fixture f;
    const DependencyRecord before = dependenciesOf(f.s, *f.project, {{"model", "test.model"}});
    EXPECT_TRUE(staleness(before, dependenciesOf(f.s, *f.project, {{"model", "test.model"}}), *f.project).empty());
    // Geometry.
    FunctionalStructure s = f.s;
    ASSERT_TRUE(applyParameters(s, *f.project, {{"w", 1.5}}));
    auto reasons = staleness(before, dependenciesOf(f.s, *f.project, {{"model", "test.model"}}), *f.project);
    ASSERT_EQ(reasons.size(), 1u);
    EXPECT_EQ(reasons[0].what, "geometry");
    EXPECT_NE(reasons[0].detail.find("'Block'"), std::string::npos);
    // Material.
    const DependencyRecord geometryChanged = dependenciesOf(f.s, *f.project, {{"model", "test.model"}});
    Material* aluminium = f.project->materials.findMutable(Id("mat-aluminum"));
    ASSERT_NE(aluminium, nullptr);
    aluminium->setName("Aluminium (anodized)");
    reasons = staleness(geometryChanged, dependenciesOf(f.s, *f.project, {{"model", "test.model"}}), *f.project);
    ASSERT_EQ(reasons.size(), 1u);
    EXPECT_EQ(reasons[0].what, "material");
    // Definition and discretization.
    const DependencyRecord now = dependenciesOf(f.s, *f.project, {{"model", "test.model"}});
    FunctionalStructure edited = f.s;
    edited.objectives.pop_back();
    reasons = staleness(now, dependenciesOf(edited, *f.project, {{"model", "other"}}), *f.project);
    ASSERT_EQ(reasons.size(), 2u);
    EXPECT_EQ(reasons[0].what, "definition");
    EXPECT_EQ(reasons[1].what, "discretization");
    // Survives save and load (content keys, not counters).
    auto loaded = deserializeProject(serializeProject(*f.project));
    EXPECT_TRUE(staleness(now, dependenciesOf(f.s, *loaded.value().project, {{"model", "test.model"}}), *loaded.value().project).empty());
    EXPECT_EQ(DependencyRecord::fromJson(now.toJson()).toJson(), now.toJson());
}

TEST(FunctionalStructures, SimulationGeometryCacheReusesOnlyWhileTheKeyIsUnchanged)
{
    SimulationGeometryCache cache;
    const auto mesh = std::make_shared<int>(42);
    EXPECT_EQ(cache.find("geometry-1"), nullptr);
    cache.store("geometry-1", mesh);
    EXPECT_EQ(cache.find("geometry-1"), mesh);
    EXPECT_EQ(cache.find("geometry-2"), nullptr);  // a changed geometry key: rebuilt, never reused
    EXPECT_EQ(cache.hits(), 1u);
    EXPECT_EQ(cache.misses(), 2u);
    for (int i = 0; i < 20; ++i)
        cache.store("k" + std::to_string(i), mesh);
    EXPECT_EQ(cache.find("geometry-1"), nullptr);  // bounded: the oldest entries are dropped
}

TEST(FunctionalStructures, ResultsNameTheirRepresentationsObjectivesAndConstraints)
{
    Fixture f;
    auto doc = analyzeDocument(f.s, *f.project, f.registry, "", nullptr);
    ASSERT_TRUE(doc) << doc.message();
    const json& d = doc.value();
    EXPECT_EQ(d["format"], kResultFormat);
    EXPECT_EQ(d["analysis"]["model"]["id"], "test.model");
    EXPECT_NEAR(metricOf(d, "box.volume"), 0.1, 1e-15);
    EXPECT_NEAR(metricOf(d, "box.scaled"), 2.0, 1e-15);  // the free parameter reaches the model
    EXPECT_EQ(d["representations"]["simulation"]["cells"], 10);
    EXPECT_EQ(d["representations"]["regions"], 1);
    EXPECT_FALSE(d["dependencies"]["geometry"].get<std::string>().empty());
    // Objectives: the volume target 0.2 is missed by 50 %; constraints: w >= 0.6 holds, volume <= 0.15 holds.
    EXPECT_NEAR(d["objectives"][0]["deviation"].get<double>(), 0.5, 1e-12);
    EXPECT_FALSE(d["objectives"][0]["met"].get<bool>());
    EXPECT_TRUE(d["constraints"][0]["satisfied"].get<bool>());
    EXPECT_TRUE(d["constraints"][1]["satisfied"].get<bool>());
    EXPECT_TRUE(d["feasible"].get<bool>());
    // A structure that is not valid is refused with the reason.
    FunctionalStructure broken = f.s;
    broken.analysis.kind = "test.none";
    EXPECT_FALSE(analyzeDocument(broken, *f.project, f.registry, "", nullptr));
}

TEST(FunctionalStructures, SweepEvaluatesEachPointOnItsOwnCopyAndRanksCandidates)
{
    Fixture f;
    auto doc = sweepDocument(f.s, *f.project, f.registry, {{"w", {0.5, 1.0, 2.0}}, {"k", {1.0, 4.0}}}, "", nullptr);
    ASSERT_TRUE(doc) << doc.message();
    const json& d = doc.value();
    ASSERT_EQ(d["points"].size(), 6u);
    // Row-major over the axes: the last axis varies fastest.
    EXPECT_EQ(d["points"][1]["values"]["w"], 0.5);
    EXPECT_EQ(d["points"][1]["values"]["k"], 4.0);
    EXPECT_NEAR(d["points"][1]["metrics"]["box.scaled"].get<double>(), 2.0, 1e-15);
    EXPECT_NEAR(d["points"][4]["metrics"]["box.volume"].get<double>(), 0.2, 1e-15);
    // w = 0.5 violates the hard constraint w >= 0.6: infeasible.
    EXPECT_FALSE(d["points"][0]["feasible"].get<bool>());
    // The best feasible candidate meets the volume target (w = 2 gives 0.2).
    ASSERT_TRUE(d["best"].is_object());
    const json& best = d["points"][d["best"]["index"].get<size_t>()];
    EXPECT_EQ(best["values"]["w"], 2.0);
    const json& why = d["best"]["explanation"];
    bool widthChanged = false;
    for (const json& c : why["parametersChanged"])
        widthChanged = widthChanged || (c["id"] == "w" && c["to"] == 2.0);
    EXPECT_TRUE(widthChanged);
    EXPECT_EQ(why["objectivesImproved"][0]["id"], "o1");
    EXPECT_EQ(why["objectivesWorsened"][0]["id"], "o2");  // 1/w decreased: a trade-off, reported
    EXPECT_EQ(why["mechanism"], "a synthetic model for tests");
    // The project itself is unchanged.
    EXPECT_EQ(valuesOf(resolveParameters(f.s, *f.project).value()).at("w"), 1.0);
    // Refusals.
    EXPECT_FALSE(sweepDocument(f.s, *f.project, f.registry, {{"area", {1.0}}}, "", nullptr));   // derived
    EXPECT_FALSE(sweepDocument(f.s, *f.project, f.registry, {{"w", {9.0}}}, "", nullptr));      // out of range
    EXPECT_FALSE(sweepDocument(f.s, *f.project, f.registry, {{"w", std::vector<double>(401, 1.0)}}, "", nullptr));
}

TEST(FunctionalStructures, SensitivityIsTheCentralDifference)
{
    Fixture f;
    auto doc = sensitivityDocument(f.s, *f.project, f.registry, {"w", "k"}, 1e-3, "", nullptr);
    ASSERT_TRUE(doc) << doc.message();
    bool found = false;
    for (const json& e : doc.value()["entries"]) {
        if (e["parameter"] == "w" && e["metric"] == "box.volume") {
            EXPECT_NEAR(e["derivative"].get<double>(), 0.5 * 0.2, 1e-12);  // d(w d h)/dw
            EXPECT_NEAR(e["normalized"].get<double>(), 1.0, 1e-12);
            EXPECT_EQ(e["scheme"], "central");
            found = true;
        }
        if (e["parameter"] == "w" && e["metric"] == "box.f") {
            EXPECT_NEAR(e["normalized"].get<double>(), -1.0, 1e-5);
        }
        if (e["parameter"] == "k" && e["metric"] == "box.volume") {
            EXPECT_EQ(e["derivative"].get<double>(), 0.0);
        }
    }
    EXPECT_TRUE(found);
    EXPECT_FALSE(sensitivityDocument(f.s, *f.project, f.registry, {"area"}, 1e-3, "", nullptr));
}

TEST(FunctionalStructures, ModeTrackingFollowsLabelsAndFlagsAmbiguity)
{
    // Labelled modes crossing: tracked by label, the crossing reported.
    const std::vector<std::vector<Mode>> labelled{{{"A", 100, 0, {}}, {"B", 150, 0, {}}}, {{"A", 140, 0, {}}, {"B", 150, 0, {}}},
                                                  {{"B", 150, 0, {}}, {"A", 180, 0, {}}}};
    const json t = trackModes(labelled);
    EXPECT_EQ(t["tracks"][0]["label"], "A");
    EXPECT_EQ(t["tracks"][0]["frequencyHz"][2], 180.0);
    EXPECT_EQ(t["tracks"][0]["matchedBy"], "label");
    ASSERT_EQ(t["crossings"].size(), 1u);
    EXPECT_TRUE(t["tracks"][0]["ambiguousAt"].empty());
    // Unlabelled modes: continuity; two candidates within 2 % are ambiguous.
    const std::vector<std::vector<Mode>> plain{{{"", 100, 0, {}}, {"", 200, 0, {}}}, {{"", 102, 0, {}}, {"", 198, 0, {}}},
                                               {{"", 150, 0, {}}, {"", 151, 0, {}}}};
    const json u = trackModes(plain);
    EXPECT_EQ(u["tracks"][0]["matchedBy"], "continuity");
    EXPECT_EQ(u["tracks"][0]["frequencyHz"][1], 102.0);
    EXPECT_EQ(u["tracks"][1]["frequencyHz"][1], 198.0);
    EXPECT_FALSE(u["tracks"][0]["ambiguousAt"].empty());
}

TEST(FunctionalStructures, ConstraintsAndObjectivesEvaluate)
{
    Fixture f;
    const Values parameters{{"w", 0.5}, {"d", 0.5}, {"z", 0.0}, {"k", 2.0}, {"area", 0.25}};
    const Values metrics{{"box.volume", 0.3}, {"box.f", 2.0}};
    const auto c = evaluateConstraints(f.s, parameters, metrics);
    ASSERT_EQ(c.size(), 2u);
    EXPECT_FALSE(c[0].satisfied);
    EXPECT_TRUE(c[0].hard);
    EXPECT_NE(c[0].message.find("cnc"), std::string::npos);
    EXPECT_FALSE(c[1].satisfied);
    EXPECT_FALSE(c[1].hard);
    FunctionalStructure s = f.s;
    s.objectives.push_back({"o3", "box.volume", "within", 0, 0.1, 0.2, 1.0, ""});
    const auto o = evaluateObjectives(s, metrics);
    EXPECT_NEAR(o[2].deviation, 1.0, 1e-12);  // 0.1 outside a range 0.1 wide
    EXPECT_FALSE(o[2].met);
    EXPECT_FALSE(evaluateObjectives(s, {})[0].available);
}
