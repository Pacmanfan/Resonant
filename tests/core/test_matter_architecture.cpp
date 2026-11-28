// The canonical architecture (RES-FAM-001..007, -016, -020): every example
// round-trips without loss and validates; compositions, phases,
// microstructure, regions, stacks, patterns, fields and adaptive classes; the
// validation finds what is wrong; projects without an architecture are unchanged.
#include "MatterFixture.h"

#include "matter/PropertyResolution.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace matter_test;
using namespace resonant::matter;

namespace {

bool hasCode(const std::vector<Issue>& issues, const std::string& code, const std::string& severity = "error")
{
    for (const auto& i : issues)
        if (i.code == code && i.severity == severity)
            return true;
    return false;
}

const char* kExamples[] = {"a-homogeneous-plate.resonant",   "b-energy-converter.resonant",       "c-thermal-barrier.resonant",
                           "d-graded-metal-ceramic.resonant", "e-em-absorber.resonant",            "f-multimaterial-structure.resonant",
                           "g-thickness-tradeoff.resonant",   "h-oriented-film.resonant"};

}  // namespace

TEST(MatterArchitecture, EveryExampleRoundTripsWithoutLossAndValidates)
{
    for (const char* file : kExamples) {
        auto p = example(file);
        ASSERT_TRUE(p) << file;
        auto w = workspaceOf(*p);
        ASSERT_TRUE(w) << file << ": " << w.message();
        ASSERT_FALSE(w.value().architectures.empty()) << file;
        for (const auto& a : w.value().architectures) {
            const json j = a.toJson();
            auto back = Architecture::fromJson(j);
            ASSERT_TRUE(back) << back.message();
            EXPECT_EQ(back.value().toJson(), j) << file << " " << a.id;
            EXPECT_EQ(revision(back.value()), revision(a));
            const auto issues = validate(a, p.get());
            for (const auto& i : issues)
                EXPECT_NE(i.severity, "error") << file << " " << a.id << ": " << i.message;
        }
        // Through the project file: serialized and loaded again.
        auto again = deserializeProject(serializeProject(*p));
        ASSERT_TRUE(again) << again.message();
        auto w2 = workspaceOf(*again.value().project);
        ASSERT_TRUE(w2);
        ASSERT_EQ(w2.value().architectures.size(), w.value().architectures.size());
        for (size_t i = 0; i < w.value().architectures.size(); ++i)
            EXPECT_EQ(w2.value().architectures[i].toJson(), w.value().architectures[i].toJson()) << file;
    }
}

TEST(MatterArchitecture, ProjectsWithoutAnArchitectureAreUnchanged)
{
    auto p = Project::createNew("plain");
    const json before = serializeProject(*p);
    auto w = workspaceOf(*p);
    ASSERT_TRUE(w);
    EXPECT_TRUE(w.value().architectures.empty());
    storeWorkspace(*p, w.value());
    EXPECT_EQ(serializeProject(*p), before);
    p->extensions["matter"] = {{"format", "something-else"}};
    EXPECT_FALSE(workspaceOf(*p).ok());
}

TEST(MatterArchitecture, CompositionPhasesAndMicrostructure)
{
    auto p = example("f-multimaterial-structure.resonant");
    const Architecture a = architecture(*p, "arch-multi");
    const FunctionalRegion* core = a.region("core");
    ASSERT_NE(core, nullptr);
    ASSERT_TRUE(core->microstructure.has_value());
    EXPECT_NEAR(core->microstructure->porosity->value, 0.8, 1e-12);
    // Gibson-Ashby for the stiffness of the open cells, with the bounds alongside.
    const ResolvedProperty e = resolve(a, *core, "mechanical.youngsModulus", 0.5, p->materials);
    ASSERT_TRUE(e.ok()) << e.message;
    EXPECT_EQ(e.origin, "composition");
    EXPECT_EQ(e.source, "gibson-ashby-open-cell");
    EXPECT_NEAR(e.value, 0.04 * 69e9, 1.0);
    ASSERT_TRUE(e.effective.has_value());
    EXPECT_DOUBLE_EQ(e.effective->lower, 0.0);  // pores: the lower bound vanishes
    EXPECT_GT(e.effective->upper, e.value);
    // The conductivity only as its upper bound: the method says so.
    const ResolvedProperty k = resolve(a, *core, "thermal.thermalConductivity", 0.5, p->materials);
    ASSERT_TRUE(k.ok()) << k.message;
    EXPECT_EQ(k.source, "hashin-shtrikman-upper");
    EXPECT_NEAR(k.value, 167.0 * 2.0 * 0.2 / (3.0 - 0.2), 1e-9);  // HS upper with a void phase: k_s 2 f / (3 - f)
    EXPECT_DOUBLE_EQ(resolve(a, *core, "mechanical.density", 0.5, p->materials).value, 0.2 * 2700.0);  // exact
    // No estimator for the loss factor: it is missing, not averaged.
    EXPECT_FALSE(resolve(a, *core, "mechanical.lossFactor", 0.5, p->materials).ok());
    // A void channel and a region-stated value.
    EXPECT_DOUBLE_EQ(resolve(a, *a.region("channel"), "thermal.thermalConductivity", 0.5, p->materials).value, 0.0);
    // A composition without a method and without a material is missing, with its bounds.
    Architecture b = a;
    b.region("core")->composition.method.clear();
    const ResolvedProperty none = resolve(b, *b.region("core"), "thermal.thermalConductivity", 0.5, p->materials);
    EXPECT_FALSE(none.ok());
    EXPECT_NE(none.message.find("bounds"), std::string::npos) << none.message;
    // Phases must sum to one.
    b.region("frame")->phases = {{"alpha", "alpha", "mat-fam-al6061", 0.7, "continuous", json::object(), false, ""},
                                 {"precipitates", "beta", "mat-fam-al6061", 0.2, "dispersed", json::object(), false, ""}};
    EXPECT_TRUE(hasCode(validate(b, p.get()), "fractions-do-not-sum"));
    b.region("frame")->phases[1].fraction = 0.3;
    EXPECT_FALSE(hasCode(validate(b, p.get()), "fractions-do-not-sum"));
}

TEST(MatterArchitecture, AGradedCompositionFollowsItsField)
{
    auto p = example("d-graded-metal-ceramic.resonant");
    const Architecture a = architecture(*p, "arch-fgm");
    const FunctionalRegion& r = *a.region("graded");
    // Nickel at the bottom, alumina at the top: s^2 of alumina between.
    EXPECT_NEAR(resolve(a, r, "thermal.thermalConductivity", 0.0, p->materials).value, 90.9, 1e-9);
    EXPECT_NEAR(resolve(a, r, "thermal.thermalConductivity", 1.0, p->materials).value, 30.0, 1e-9);
    const ResolvedProperty mid = resolve(a, r, "thermal.thermalConductivity", 0.5, p->materials);
    ASSERT_TRUE(mid.ok());
    ASSERT_TRUE(mid.effective.has_value());
    EXPECT_GT(mid.value, mid.effective->lower);
    EXPECT_NEAR(mid.value, mid.effective->upper, 1e-12);  // the stated method: the upper bound
    EXPECT_NEAR(resolve(a, r, "mechanical.density", 0.5, p->materials).value, 0.75 * 8900 + 0.25 * 3950, 1e-9);
    EXPECT_TRUE(hasCode(validate(a, p.get()), "idealized-field", "info"));
}

TEST(MatterArchitecture, AMultimaterialStructureWithRegions)
{
    auto p = example("f-multimaterial-structure.resonant");
    Architecture a = architecture(*p, "arch-multi");
    EXPECT_EQ(a.objects.size(), 2u);
    EXPECT_EQ(a.regions.size(), 4u);
    EXPECT_EQ(a.region("sensor")->extent, "subvolume");
    EXPECT_TRUE(a.region("sensor")->anisotropic.count("piezoelectric"));
    EXPECT_NE(a.interfaceBetween("frame", "core"), nullptr);
    a.region("sensor")->object = "nowhere";
    EXPECT_TRUE(hasCode(validate(a, p.get()), "unknown-object"));
    a = architecture(*p, "arch-multi");
    a.region("sensor")->subvolume = {{"min", json::array({0.0, 0.0, 0.0})}, {"max", json::array({0.0, 0.01, 0.01})}};
    EXPECT_TRUE(hasCode(validate(a, p.get()), "bad-subvolume"));
    a = architecture(*p, "arch-multi");
    a.objects.push_back("ex-not-in-scene");
    EXPECT_TRUE(hasCode(validate(a, p.get()), "unknown-object"));
}

TEST(MatterArchitecture, StacksLayersPatternsAndInterfaces)
{
    auto p = example("b-energy-converter.resonant");
    Architecture a = architecture(*p, "arch-bonded");
    const Stack& s = *a.stack("st-beam");
    ASSERT_EQ(s.layers.size(), 6u);
    EXPECT_NEAR(s.thickness(), 5e-4 + 2e-5 + 5e-6 + 2e-4 + 1e-6 + 1e-5, 1e-15);
    EXPECT_EQ(s.layers[4].pattern.kind, "segmented");
    EXPECT_NEAR(s.layers[4].pattern.coverage(), 0.8, 1e-12);
    EXPECT_EQ(s.layerOf("pzt"), 3);
    // The interface's model for each physics: cohesive for mechanics, perfect for heat.
    const Interface* i = a.interfaceBetween("substrate", "bond");
    ASSERT_NE(i, nullptr);
    EXPECT_EQ(i->modelFor("mechanical")->kind, "cohesive-zone");
    EXPECT_EQ(i->modelFor("thermal")->kind, "perfect");
    EXPECT_EQ(a.interfaceBetween("pzt", "electrode-top")->modelFor("electrical")->kind, "perfect");
    // What validation finds.
    Architecture bad = a;
    bad.stack("st-beam")->layers[1].thickness = 0.0;
    EXPECT_TRUE(hasCode(validate(bad, p.get()), "bad-thickness"));
    bad = a;
    bad.stack("st-beam")->layers[2].region = "missing";
    EXPECT_TRUE(hasCode(validate(bad, p.get()), "unknown-region"));
    bad = a;
    bad.interfaces.erase(bad.interfaces.begin());
    EXPECT_TRUE(hasCode(validate(bad, p.get()), "undefined-interface", "warning")) << "nothing is assumed about a missing interface";
    bad = a;
    bad.interfaces.push_back({"i-far", "far", {"substrate", "pzt"}, {{"perfect", {}}}, std::nullopt, std::nullopt, "", {}, "", ""});
    EXPECT_TRUE(hasCode(validate(bad, p.get()), "not-adjacent"));
    bad = a;
    bad.region("pzt")->anisotropic["piezoelectric"].poling.clear();
    EXPECT_TRUE(hasCode(validate(bad, p.get()), "no-poling"));
    bad = a;
    bad.region("bond")->materialId = "mat-unknown";
    EXPECT_TRUE(hasCode(validate(bad, p.get()), "unknown-material"));
    bad = a;
    bad.region("bond")->process = "proc-unknown";
    EXPECT_TRUE(hasCode(validate(bad, p.get()), "unknown-process"));
    bad = a;
    bad.interfaces.front().models.push_back({"finite-stiffness", {}});
    EXPECT_TRUE(hasCode(validate(bad, p.get()), "missing-parameter"));
    bad = a;
    bad.stack("st-beam")->layers[4].pattern.end = 1.5;
    EXPECT_TRUE(hasCode(validate(bad, p.get()), "bad-pattern"));
    EXPECT_NE(revision(bad), revision(a));
}

TEST(MatterArchitecture, AdaptiveClassesAndStates)
{
    auto p = example("b-energy-converter.resonant");
    Architecture a = architecture(*p, "arch-bonded");
    EXPECT_EQ(a.adaptive.kind, "static");
    a.adaptive.kind = "externally-controlled";
    EXPECT_TRUE(hasCode(validate(a, p.get()), "no-control-input"));
    EXPECT_TRUE(hasCode(validate(a, p.get()), "no-states", "warning"));
    a.adaptive.controlInput = "a bias voltage on the electrodes";
    a.adaptive.states = {{"tuned", "Tuned", {{"voltage", 50.0}}, "", "stiffened by the converse effect"}};
    EXPECT_FALSE(hasCode(validate(a, p.get()), "no-control-input"));
    EXPECT_TRUE(hasCode(validate(a, p.get()), "reversibility-not-declared", "info")) << "reversibility is declared, never assumed";
    a.adaptive.reversible = true;
    EXPECT_FALSE(hasCode(validate(a, p.get()), "reversibility-not-declared", "info"));
    auto back = Architecture::fromJson(a.toJson());
    ASSERT_TRUE(back);
    EXPECT_EQ(back.value().adaptive.states.size(), 1u);
    EXPECT_TRUE(*back.value().adaptive.reversible);
    a.adaptive.kind = "self-healing";
    EXPECT_TRUE(hasCode(validate(a, p.get()), "unknown-adaptive-class"));
}

TEST(MatterArchitecture, OrientationAndProvenanceKinds)
{
    Orientation o{0.3, 0.0, 0.2, "stack"};
    const auto r = o.rotation();
    EXPECT_TRUE(o.inPlane());
    // Orthonormal, and an in-plane rotation by phi1 + phi2.
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double dot = 0.0;
            for (int k = 0; k < 3; ++k)
                dot += r[k][i] * r[k][j];
            EXPECT_NEAR(dot, i == j ? 1.0 : 0.0, 1e-12);
        }
    EXPECT_NEAR(r[0][0], std::cos(0.5), 1e-12);
    EXPECT_FALSE((Orientation{0.0, 0.4, 0.0, "stack"}).inPlane());
    EXPECT_TRUE(isProvenance("first-principles"));
    EXPECT_FALSE(isProvenance("guessed"));
    EXPECT_FALSE(SourcedValue::fromJson({{"value", 1.0}, {"provenance", "guessed"}}).ok());
    EXPECT_FALSE(SourcedValue::fromJson({{"value", 1.0}, {"uncertainty", -1.0}}).ok());
}
