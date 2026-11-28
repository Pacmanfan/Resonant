// The multiphysics application services (RES-CEME-005, RES-CEME-014, RES-CEME-015):
// physical systems stored in the project (undoable, surviving save and load and
// material renames), validation, coupled simulations as jobs with provenance,
// and permissions.
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "services/Services.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace resonant;
using namespace resonant::services;

namespace {

PropertyValue measured(double v)
{
    PropertyValue p;
    p.value = v;
    p.provenance = Provenance::Measured;
    p.source = "test";
    return p;
}

struct Fixture {
    OwnedProjectHost host{Project::createNew("multiphysics")};
    Session session{host};
    Services& s = session.services();
    Fixture()
    {
        host.setSession(&session);
        Material m(Id("mat-lossy"), "Lossy dielectric");
        for (const auto& [domain, key, value] : std::initializer_list<std::tuple<PhysicsDomain, const char*, double>>{
                 {PhysicsDomain::Electromagnetic, "relativePermittivity", 4.0},
                 {PhysicsDomain::Electromagnetic, "lossTangent", 0.05},
                 {PhysicsDomain::Thermal, "density", 1500.0},
                 {PhysicsDomain::Thermal, "thermalConductivity", 0.5},
                 {PhysicsDomain::Thermal, "specificHeat", 1000.0}})
            EXPECT_TRUE(m.setProperty(domain, key, measured(value)).ok());
        EXPECT_TRUE(host.project().materials.add(m).ok());
    }
    CallContext local() const { return CallContext{}; }
    CallContext remote(std::uint32_t permissions)
    {
        auto opened = session.openSession("test client", Interface::Grpc, permissions, kDefaultRemotePermissions);
        CallContext c;
        c.sessionId = opened.value().id;
        c.source = Interface::Grpc;
        return c;
    }
    bool waitForJobs()
    {
        for (int k = 0; k < 600; ++k) {
            if (!host.jobs().hasUnfinishedJobs()) {
                session.collectFinishedJobs();
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }
};

nlohmann::json system(const std::string& id = "sys-1")
{
    return {{"format", "resonant.physical-system"},
            {"version", 1},
            {"id", id},
            {"name", "Heated slab"},
            {"geometry", {{"kind", "layered-slab"}, {"lateralArea", 0.01}}},
            {"regions", {{{"id", "slab"}, {"name", "Slab"}, {"materialId", "mat-lossy"}, {"thickness", 0.01}, {"cells", 30}}}},
            {"domains",
             {{{"id", "em"}, {"kind", "electromagnetic"}, {"solver", "em.layered.transfer-matrix"},
               {"settings", {{"frequencyHz", 2.45e9}, {"incidentIntensity", 1e4}}}},
              {{"id", "heat"}, {"kind", "thermal"}, {"solver", "thermal.conduction.fv"},
               {"settings", {{"faces", {{"-z", {{"kind", "convective"}, {"h", 15}}}, {"+z", {{"kind", "convective"}, {"h", 15}}}}}}}}}},
            {"couplings", {{{"id", "absorb"}, {"model", "em.absorption-to-heat"}, {"from", "em"}, {"to", "heat"}}}},
            {"scheme", {{"kind", "one-way"}}}};
}

}  // namespace

TEST(MultiphysicsServices, SystemsAreStoredUndoablyAndSurviveSaveLoadAndRenames)
{
    Fixture f;
    auto m = f.s.multiphysics.setSystem(f.local(), system());
    ASSERT_TRUE(m) << m.error().message;
    ASSERT_EQ(f.s.multiphysics.systems(f.local()).value().size(), 1u);
    // Replacing by id keeps one system.
    nlohmann::json changed = system();
    changed["name"] = "Renamed slab";
    ASSERT_TRUE(f.s.multiphysics.setSystem(f.local(), changed));
    ASSERT_EQ(f.s.multiphysics.systems(f.local()).value().size(), 1u);
    EXPECT_EQ(f.s.multiphysics.system(f.local(), "sys-1").value()["name"], "Renamed slab");
    // Undo restores the first version, undo again removes it.
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_EQ(f.s.multiphysics.system(f.local(), "sys-1").value()["name"], "Heated slab");
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_TRUE(f.s.multiphysics.systems(f.local()).value().empty());
    ASSERT_TRUE(f.s.project.redo(f.local()));

    // Save and load; a renamed material keeps the reference (by id).
    f.host.project().materials.findMutable(Id("mat-lossy"))->setName("Renamed dielectric");
    auto loaded = deserializeProject(serializeProject(f.host.project()));
    ASSERT_TRUE(loaded) << loaded.message();
    const nlohmann::json& ext = loaded.value().project->extensions.at(MultiphysicsService::kExtension);
    ASSERT_EQ(ext["systems"].size(), 1u);
    EXPECT_EQ(ext["systems"][0]["regions"][0]["materialId"], "mat-lossy");
    auto valid = f.s.multiphysics.validate(f.local(), "sys-1", {});
    ASSERT_TRUE(valid);
    EXPECT_TRUE(valid.value().valid);

    EXPECT_FALSE(f.s.multiphysics.setSystem(f.local(), {{"format", "not a system"}}));
    ASSERT_TRUE(f.s.multiphysics.deleteSystem(f.local(), "sys-1"));
    EXPECT_FALSE(f.s.multiphysics.system(f.local(), "sys-1"));
    EXPECT_FALSE(f.s.multiphysics.deleteSystem(f.local(), "sys-1"));
}

TEST(MultiphysicsServices, ValidationNamesEveryProblem)
{
    Fixture f;
    nlohmann::json bad = system();
    bad["regions"][0]["materialId"] = "mat-missing";
    bad["couplings"][0]["model"] = "acoustic.pressure-to-structure";
    auto v = f.s.multiphysics.validate(f.local(), "", bad);
    ASSERT_TRUE(v);
    EXPECT_FALSE(v.value().valid);
    EXPECT_GE(v.value().issues.size(), 2u);
    EXPECT_FALSE(f.s.multiphysics.start(f.local(), "", bad));
    EXPECT_FALSE(f.s.multiphysics.validate(f.local(), "sys-none", {}));
}

TEST(MultiphysicsServices, ACoupledSimulationRunsAsAJobWithProvenance)
{
    Fixture f;
    ASSERT_TRUE(f.s.multiphysics.setSystem(f.local(), system()));
    auto job = f.s.multiphysics.start(f.local(), "sys-1", {});
    ASSERT_TRUE(job) << job.error().message;
    ASSERT_TRUE(f.waitForJobs());
    auto view = f.s.jobs.get(f.local(), job.value().id);
    ASSERT_TRUE(view);
    EXPECT_EQ(view.value().state, "completed") << (view.value().error ? view.value().error->message : "");
    ASSERT_EQ(view.value().resultIds.size(), 1u);
    auto doc = f.s.multiphysics.result(f.local(), view.value().resultIds[0]);
    ASSERT_TRUE(doc) << doc.error().message;
    const nlohmann::json& r = *doc.value();
    EXPECT_EQ(r["status"], "completed");
    EXPECT_TRUE(r["audit"]["balanced"].get<bool>());
    EXPECT_EQ(r["provenance"]["system"]["id"], "sys-1");
    EXPECT_EQ(r["provenance"]["systemChecksum"].get<std::string>().size(), 16u);
    EXPECT_EQ(r["participants"][0]["solver"], "em.layered.transfer-matrix");
    EXPECT_TRUE(r["fields"].contains("heat.temperature"));
    EXPECT_EQ(r["jobId"], job.value().id);
    // A coupled result is not an acoustic field result.
    EXPECT_FALSE(f.s.results.slice(f.local(), view.value().resultIds[0], 2, 0, 1));
    EXPECT_FALSE(f.s.multiphysics.result(f.local(), "R-none"));
}

TEST(MultiphysicsServices, PermissionsApply)
{
    Fixture f;
    const CallContext reader = f.remote(static_cast<std::uint32_t>(Permission::Read));
    EXPECT_TRUE(f.s.multiphysics.couplingModels(reader));
    EXPECT_FALSE(f.s.multiphysics.setSystem(reader, system()));
    ASSERT_TRUE(f.s.multiphysics.setSystem(f.local(), system()));
    EXPECT_TRUE(f.s.multiphysics.validate(reader, "sys-1", {}));
    EXPECT_FALSE(f.s.multiphysics.start(reader, "sys-1", {}));
}
