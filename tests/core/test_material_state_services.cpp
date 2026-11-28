// The material-state services (RES-DMS-003, -011, -014): studies stored undoably, runs as
// jobs, records that survive save and reload, commits of an evolved state to a material
// with provenance computed and lineage, and the staleness that commit causes downstream.
#include "MaterialStateFixture.h"

#include "materialstate/StateModel.h"
#include "multiscale/Representation.h"

using namespace materialstate_test;

TEST(MaterialStateServices, StudiesRunAsJobsAndRecordsSurviveSaveAndReload)
{
    Fixture f("c-plastic-bar.resonant");
    auto studies = f.s.materialState.studies(f.local());
    ASSERT_TRUE(studies);
    ASSERT_EQ(studies.value().size(), 3u);
    const json bar = f.ran(f.s.materialState.run(f.local(), "st-steel-bar", json()));
    EXPECT_EQ(bar["model"], "mechanics.j2.bar");
    EXPECT_TRUE(bar["reliable"].get<bool>()) << bar["reliability"].dump();
    EXPECT_NEAR(bar["energy"]["residual"].get<double>(), 0.0, 2e-3 * bar["energy"]["work"].get<double>());
    // A reader may not save.
    EXPECT_FALSE(f.s.materialState.saveRecord(f.reader(), bar["resultId"], "rec-bar", "Bar"));
    ASSERT_TRUE(f.s.materialState.saveRecord(f.local(), bar["resultId"], "rec-bar", "Steel bar, one cycle"));
    auto records = f.s.materialState.records(f.local());
    ASSERT_EQ(records.value().size(), 1u);
    // The record survives save and reload.
    auto copy = deserializeProject(serializeProject(f.host.project()));
    ASSERT_TRUE(copy);
    auto w = resonant::materialstate::workspaceOf(*copy.value().project);
    ASSERT_TRUE(w);
    ASSERT_NE(w.value().record("rec-bar"), nullptr);
    EXPECT_EQ(w.value().record("rec-bar")->history, bar["history"]);
    // One undo removes it.
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_TRUE(f.s.materialState.records(f.local()).value().empty());
    // An inline study; a not-implemented model is refused.
    const json point = f.ran(f.s.materialState.run(f.local(), "", {{"model", "mechanics.j2.point"}, {"loading", {{"amplitudes", {0.004}}}}}));
    EXPECT_FALSE(point["curves"].empty());
    EXPECT_FALSE(f.s.materialState.run(f.local(), "", {{"model", "mechanics.fatigue"}}));
    EXPECT_FALSE(f.s.materialState.setStudy(f.local(), {{"id", "x"}, {"model", "mechanics.fatigue"}}));
}

TEST(MaterialStateServices, CommittingAStateKeepsItsLineageAndMakesDependentsStale)
{
    Fixture f("d-damage-resonance.resonant");
    const json d = f.ran(f.s.materialState.run(f.local(), "st-damage-point", json()));
    ASSERT_FALSE(d["committable"].empty());
    ASSERT_TRUE(f.s.materialState.saveRecord(f.local(), d["resultId"], "rec-damage", "Damage after three cycles"));
    // An effective material derived from the laminate (so that the commit can make it stale).
    ASSERT_TRUE(f.s.multiscale.generateMicrostructure(
        f.local(), {{"id", "rve-laminate"},
                    {"rve", {{"generator", "homogeneous"}, {"voxels", {2, 2, 2}}, {"size", {1e-3, 1e-3, 1e-3}}, {"phases", {"laminate"}}}},
                    {"phaseMaterials", {"mat-ex-laminate"}}}));
    // (The laminate has no thermal data: give it some so that the RVE derives a property.)
    auto lam = f.s.materials.get(f.local(), Id("mat-ex-laminate"));
    ASSERT_TRUE(lam);
    json withThermal = lam.value();
    withThermal["domains"]["thermal"] = {{"thermalConductivity", {{"value", 0.3}, {"provenance", "illustrative"}, {"source", "test"}}},
                                         {"density", {{"value", 2000.0}, {"provenance", "illustrative"}, {"source", "test"}}},
                                         {"specificHeat", {{"value", 900.0}, {"provenance", "illustrative"}, {"source", "test"}}}};
    withThermal["id"] = "mat-ex-laminate-2";
    withThermal["name"] = "Laminate copy";
    ASSERT_TRUE(f.s.materials.create(f.local(), withThermal));
    ASSERT_TRUE(f.s.multiscale.generateMicrostructure(
        f.local(), {{"id", "rve-copy"},
                    {"rve", {{"generator", "homogeneous"}, {"voxels", {2, 2, 2}}, {"size", {1e-3, 1e-3, 1e-3}}, {"phases", {"laminate"}}}},
                    {"phaseMaterials", {"mat-ex-laminate-2"}}}));
    const std::string rid = f.resultOf(f.s.multiscale.derive(f.local(), "rve-copy", "rve.thermal-conductivity"));
    ASSERT_TRUE(f.s.multiscale.assign(f.local(), rid, {{"newMaterial", {{"id", "mat-effective"}}}, {"properties", {"thermal.thermalConductivity"}}}));
    EXPECT_TRUE(f.s.multiscale.model(f.local()).value()["staleProperties"].empty());
    // Commit the damaged modulus to the laminate copy (the RVE's constituent).
    auto m = f.s.materialState.commit(f.local(), "rec-damage", {{"materialId", "mat-ex-laminate-2"}});
    ASSERT_TRUE(m) << m.error().message;
    const PropertyValue* E = f.material("mat-ex-laminate-2")->property(PhysicsDomain::Mechanical, "youngsModulus");
    ASSERT_NE(E, nullptr);
    EXPECT_EQ(E->provenance, Provenance::Computed);
    EXPECT_NEAR(E->value, d["final"]["secantModulus"].get<double>(), 1e-6);
    EXPECT_LT(E->value, 25e9);
    // Lineage: the committed value leads to the state record.
    auto l = f.s.multiscale.lineage(f.local(), "mat-ex-laminate-2");
    ASSERT_TRUE(l);
    bool traced = false;
    for (const json& p : l.value()["properties"])
        if (p["key"] == "youngsModulus")
            traced = p["representation"]["id"] == "state-rec-damage" && p["derivationRecord"]["method"] == "state.mechanics.damage.point" && !p["stale"].get<bool>();
    EXPECT_TRUE(traced) << l.value().dump();
    // The effective material derived from the copy is now stale: its constituent changed.
    const json stale = f.s.multiscale.model(f.local()).value()["staleProperties"];
    ASSERT_EQ(stale.size(), 1u);
    EXPECT_EQ(stale[0]["material"], "mat-effective");
    // One undo reverts the commit.
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_EQ(f.material("mat-ex-laminate-2")->property(PhysicsDomain::Mechanical, "youngsModulus")->provenance, Provenance::Illustrative);
    EXPECT_FALSE(f.s.materialState.commit(f.local(), "rec-none", json::object()));
}
