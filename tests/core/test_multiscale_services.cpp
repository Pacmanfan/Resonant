// The multiscale services (RES-MSC-011, -012, -014): derivations as jobs, assignment as one
// undoable step, lineage navigation, staleness after a source change, permissions,
// generated and imported representations, local states and the iterative coupling.
#include "MultiscaleFixture.h"

#include "coupling/CoupledRun.h"
#include "coupling/MultiscaleCoupling.h"

#include <cmath>

using namespace multiscale_test;

TEST(MultiscaleServices, AssignmentIsOneUndoableStepWithItsDerivation)
{
    Fixture f("c-porous-composite.resonant");
    const std::string id = f.resultOf(f.s.multiscale.derive(f.local(), "rve-porous-alumina", "rve.thermal-conductivity", {{"realizations", 2}}));
    ASSERT_FALSE(id.empty());
    auto result = f.s.multiscale.result(f.local(), id);
    ASSERT_TRUE(result);
    EXPECT_FALSE(result.value()["stale"].get<bool>());
    const double k = prop(result.value(), "thermalConductivity", "mean")->at("value").get<double>();
    // A reader may not assign.
    EXPECT_FALSE(f.s.multiscale.assign(f.reader(), id, {{"materialId", "mat-ex-porous-alumina"}}));
    auto m = f.s.multiscale.assign(f.local(), id, {{"materialId", "mat-ex-porous-alumina"}});
    ASSERT_TRUE(m) << m.error().message;
    const PropertyValue* v = f.material("mat-ex-porous-alumina")->property(PhysicsDomain::Thermal, "thermalConductivity");
    ASSERT_NE(v, nullptr);
    EXPECT_NEAR(v->value, k, 1e-12);
    EXPECT_EQ(v->provenance, Provenance::Homogenized);
    ASSERT_TRUE(v->uncertainty);
    auto model = f.s.multiscale.model(f.local());
    ASSERT_TRUE(model);
    ASSERT_EQ(model.value()["derivations"].size(), 1u);
    EXPECT_EQ(model.value()["derivations"][0]["id"], v->derivation);
    EXPECT_EQ(model.value()["derivations"][0]["calculation"].get<std::string>().find("result " + id), 0u);
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_EQ(f.material("mat-ex-porous-alumina")->property(PhysicsDomain::Thermal, "thermalConductivity"), nullptr);
    EXPECT_TRUE(f.s.multiscale.model(f.local()).value()["derivations"].empty());
}

TEST(MultiscaleServices, LineageNavigatesToTheSourceAndChangesMakeItStale)
{
    Fixture f("c-porous-composite.resonant");
    const std::string id = f.resultOf(f.s.multiscale.derive(f.local(), "rve-porous-alumina", "rve.thermal-conductivity"));
    ASSERT_TRUE(f.s.multiscale.assign(f.local(), id, {{"materialId", "mat-ex-porous-alumina"}, {"properties", {"thermal.thermalConductivity"}}}));
    auto l = f.s.multiscale.lineage(f.local(), "mat-ex-porous-alumina");
    ASSERT_TRUE(l);
    ASSERT_EQ(l.value()["properties"].size(), 1u);
    const json& k = l.value()["properties"][0];
    EXPECT_EQ(k["representation"]["id"], "rve-porous-alumina");
    EXPECT_EQ(k["derivationRecord"]["method"], "rve.thermal-conductivity");
    EXPECT_EQ(k["derivationRecord"]["sourceScale"], "microstructure");
    EXPECT_FALSE(k["stale"].get<bool>());
    ASSERT_EQ(k["chain"].size(), 2u);  // the two constituents
    // Changing the RVE marks the property and the result stale; the result can no longer be assigned.
    auto rep = f.s.multiscale.representation(f.local(), "rve-porous-alumina");
    ASSERT_TRUE(rep);
    json changed = rep.value();
    changed["data"]["rve"]["volumeFraction"] = 0.25;
    ASSERT_TRUE(f.s.multiscale.setRepresentation(f.local(), changed));
    l = f.s.multiscale.lineage(f.local(), "mat-ex-porous-alumina");
    EXPECT_TRUE(l.value()["properties"][0]["stale"].get<bool>());
    EXPECT_TRUE(f.s.multiscale.result(f.local(), id).value()["stale"].get<bool>());
    auto refused = f.s.multiscale.assign(f.local(), id, {{"materialId", "mat-ex-porous-alumina"}});
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code, ErrorCode::FailedPrecondition);
    EXPECT_FALSE(f.s.multiscale.model(f.local()).value()["staleProperties"].empty());
}

TEST(MultiscaleServices, RepresentationsAreGeneratedAndImported)
{
    Fixture f("c-porous-composite.resonant");
    ASSERT_TRUE(f.s.multiscale.generateMicrostructure(
        f.local(), {{"id", "rve-new"}, {"name", "New pores"},
                    {"rve", {{"generator", "spheres"}, {"voxels", {12, 12, 12}}, {"size", {1e-3, 1e-3, 1e-3}}, {"phases", {"a", "p"}}, {"radius", 1e-4},
                             {"volumeFraction", 0.1}, {"seed", 4}}},
                    {"phaseMaterials", {"mat-ex-alumina-dense", "mat-ex-pore-air"}}}));
    auto r = f.s.multiscale.representation(f.local(), "rve-new");
    ASSERT_TRUE(r);
    EXPECT_EQ(r.value()["provenance"], "synthetic");
    EXPECT_NE(r.value()["source"].get<std::string>().find("not a measured microstructure"), std::string::npos);
    EXPECT_TRUE(r.value().contains("modelCardDocument"));
    bool thermal = false;
    for (const json& m : r.value()["methods"])
        if (m["id"] == "rve.thermal-conductivity")
            thermal = m["applicable"].get<bool>();
    EXPECT_TRUE(thermal);
    // A phase material outside the library is refused.
    EXPECT_FALSE(f.s.multiscale.generateMicrostructure(
        f.local(), {{"rve", {{"generator", "homogeneous"}, {"voxels", {2, 2, 2}}, {"size", {1, 1, 1}}, {"phases", {"a"}}}}, {"phaseMaterials", {"mat-none"}}}));
    // An imported POSCAR; a structure the force field does not cover is refused.
    const std::string poscar = "Ar fcc\n5.25\n1 0 0\n0 1 0\n0 0 1\nAr\n4\nDirect\n0 0 0\n0.5 0.5 0\n0.5 0 0.5\n0 0.5 0.5\n";
    ASSERT_TRUE(f.s.multiscale.createAtomisticStructure(f.local(), {{"id", "rep-poscar"}, {"format", "poscar"}, {"text", poscar}, {"forceField", "lj.argon"}}));
    auto imported = f.s.multiscale.representation(f.local(), "rep-poscar");
    EXPECT_EQ(imported.value()["provenance"], "imported");
    EXPECT_EQ(imported.value()["kind"], "crystal");
    EXPECT_FALSE(f.s.multiscale.createAtomisticStructure(f.local(), {{"structure", "fcc"}, {"latticeParameter", 3.6e-10}, {"species", "Cu"}, {"forceField", "lj.argon"}}));
    // Catalogues.
    EXPECT_GE(f.s.multiscale.modelCards(f.local()).value().size(), 10u);
    EXPECT_GE(f.s.multiscale.couplings(f.local()).value().size(), 5u);
    EXPECT_GE(f.s.multiscale.externalSolvers(f.local()).value().size(), 6u);
    EXPECT_FALSE(f.s.multiscale.derive(f.reader(), "rve-new", "rve.thermal-conductivity", json::object()));
    EXPECT_FALSE(f.s.multiscale.derive(f.local(), "rve-new", "atomistic.elastic-constants", json::object()));
    // Undo removes the import.
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_FALSE(f.s.multiscale.representation(f.local(), "rep-poscar"));
}

TEST(MultiscaleServices, IterativeCouplingReachesAFixedPoint)
{
    Fixture f("e-cross-scale-investigation.resonant");
    auto state = f.s.multiscale.localState(f.local(), "sys-thermal-pad", "pad", "");
    ASSERT_TRUE(state) << state.error().message;
    EXPECT_GT(state.value()["meanTemperatureK"].get<double>(), 300.0);
    EXPECT_EQ(state.value()["cells"], 40);
    const std::string id = f.resultOf(f.s.multiscale.iterate(f.local(), {{"system", "sys-thermal-pad"}, {"region", "pad"}, {"representation", "rve-filled-epoxy"},
                                                                       {"tolerance", 1e-6}, {"maxIterations", 30}}));
    ASSERT_FALSE(id.empty());
    auto r = f.s.multiscale.result(f.local(), id);
    ASSERT_TRUE(r);
    EXPECT_TRUE(r.value()["converged"].get<bool>()) << r.value()["history"].dump();
    const json& history = r.value()["history"];
    ASSERT_GE(history.size(), 2u);
    const json& last = history.back();
    EXPECT_NEAR(last["conductivity"].get<double>(), last["conductivityUsed"].get<double>(), 1e-6 * last["conductivity"].get<double>());
    // The first step replaced the series-bound guess with the RVE's (much higher) value.
    EXPECT_GT(history[0]["conductivity"].get<double>(), 1.5 * history[0]["conductivityUsed"].get<double>());
    // Assign the converged value; the system then reproduces the converged temperature.
    ASSERT_TRUE(f.s.multiscale.assign(f.local(), id, {{"materialId", "mat-ex-filled-epoxy"}, {"properties", {"thermal.thermalConductivity"}}}));
    const PropertyValue* k = f.material("mat-ex-filled-epoxy")->property(PhysicsDomain::Thermal, "thermalConductivity");
    EXPECT_NEAR(k->value, last["conductivity"].get<double>(), 1e-12);
    EXPECT_NE(k->source.find(" K;"), std::string::npos);  // derived at the pad's temperature
    auto after = f.s.multiscale.localState(f.local(), "sys-thermal-pad", "pad", "");
    EXPECT_NEAR(after.value()["meanTemperatureK"].get<double>(), last["temperatureK"].get<double>(), 1e-3);
}
