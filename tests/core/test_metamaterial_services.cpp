// Effective materials through the application services (RES-META-002, RES-META-011,
// RES-META-012): an analysis's effective material saved into the library with its
// derivation (one undoable step), refused when stale or absent, and used by the Coupled
// Energy-Matter Engine, where a homogenized region reproduces the explicit laminate.
#include "coupling/CoupledRun.h"
#include "multiphysics/PhysicalSystem.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "services/Services.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <thread>

using namespace resonant;
using namespace resonant::services;
using nlohmann::json;

namespace {

std::unique_ptr<Project> example(const std::string& file)
{
    auto loaded = loadProjectFile(std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "metamaterials" / file);
    EXPECT_TRUE(loaded) << loaded.message();
    return std::move(loaded.value().project);
}

struct Fixture {
    OwnedProjectHost host;
    Session session{host};
    Services& s = session.services();
    explicit Fixture(const std::string& file) : host(example(file), 2) { host.setSession(&session); }
    CallContext local() const { return CallContext{}; }
    CallContext reader()
    {
        auto opened = session.openSession("reader", Interface::Grpc, static_cast<std::uint32_t>(Permission::Read), kDefaultRemotePermissions);
        CallContext c;
        c.sessionId = opened.value().id;
        c.source = Interface::Grpc;
        return c;
    }
    bool waitForJobs()
    {
        for (int k = 0; k < 3000; ++k) {
            if (!host.jobs().hasUnfinishedJobs()) {
                session.collectFinishedJobs();
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }
    // The finished job's first result id.
    std::string resultOf(const Outcome<JobView>& job)
    {
        EXPECT_TRUE(job) << (job ? "" : job.error().message);
        if (!job)
            return {};
        EXPECT_TRUE(waitForJobs());
        auto view = s.jobs.get(local(), job.value().id);
        EXPECT_TRUE(view);
        EXPECT_EQ(view.value().state, "completed") << (view.value().error ? view.value().error->message : "");
        return view.value().resultIds.empty() ? std::string() : view.value().resultIds[0];
    }
};

double metric(const json& doc, const std::string& key)
{
    for (const json& m : doc["metrics"])
        if (m["key"] == key)
            return m["value"].get<double>();
    ADD_FAILURE() << "no metric " << key;
    return 0.0;
}

double heatLeaving(const multiphysics::CoupledResult& r, const std::string& face)
{
    for (const auto& t : r.ledger.terms())
        if (t.mechanism == "fixed temperature at " + face)
            return t.value;
    ADD_FAILURE() << "no heat flow at " << face;
    return 0.0;
}

}  // namespace

TEST(MetamaterialServices, EffectiveMaterialsKeepTheirDerivation)
{
    Fixture f("e-layered-thermal.resonant");
    const std::string resultId = f.resultOf(f.s.functional.analyze(f.local(), "fs-layered-thermal", ""));
    ASSERT_FALSE(resultId.empty());
    auto doc = f.s.functional.result(f.local(), resultId);
    ASSERT_TRUE(doc);
    const double kPerp = metric(doc.value(), "thermal.perpendicular");
    const size_t before = f.host.project().materials.materials().size();
    // A reader may not change the library.
    auto refused = f.s.functional.createEffectiveMaterial(f.reader(), resultId, json::object());
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code, ErrorCode::PermissionDenied);
    auto saved = f.s.functional.createEffectiveMaterial(f.local(), resultId, {{"id", "mat-laminate-eff"}, {"name", "Laminate (effective)"}});
    ASSERT_TRUE(saved) << saved.error().message;
    const Material* m = f.host.project().materials.find(Id("mat-laminate-eff"));
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->name(), "Laminate (effective)");
    const PropertyValue* k = m->property(PhysicsDomain::Thermal, "thermalConductivity");
    ASSERT_NE(k, nullptr);
    EXPECT_EQ(k->provenance, Provenance::Homogenized);
    EXPECT_NEAR(k->value, kPerp, 1e-12);
    EXPECT_NE(k->source.find("perpendicular"), std::string::npos);
    EXPECT_NE(k->source.find("fs-layered-thermal"), std::string::npos);
    EXPECT_NE(k->source.find(resultId), std::string::npos);
    EXPECT_NE(m->description().find("Limitation"), std::string::npos);
    ASSERT_NE(m->property(PhysicsDomain::Thermal, "density"), nullptr);
    ASSERT_NE(m->property(PhysicsDomain::Thermal, "specificHeat"), nullptr);
    // One undoable step.
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_EQ(f.host.project().materials.find(Id("mat-laminate-eff")), nullptr);
    EXPECT_EQ(f.host.project().materials.materials().size(), before);
    ASSERT_TRUE(f.s.project.redo(f.local()));
    EXPECT_NE(f.host.project().materials.find(Id("mat-laminate-eff")), nullptr);
    // The other principal component, as a second material.
    auto along = f.s.functional.createEffectiveMaterial(f.local(), resultId,
                                                       {{"id", "mat-laminate-along"}, {"components", {{"thermal.thermalConductivity", "parallel"}}}});
    ASSERT_TRUE(along) << along.error().message;
    EXPECT_NEAR(f.host.project().materials.find(Id("mat-laminate-along"))->property(PhysicsDomain::Thermal, "thermalConductivity")->value,
                metric(doc.value(), "thermal.parallel"), 1e-9);
    // The same id twice is a conflict; a stale result is refused.
    EXPECT_FALSE(f.s.functional.createEffectiveMaterial(f.local(), resultId, {{"id", "mat-laminate-eff"}}));
    ASSERT_TRUE(f.s.functional.setParameters(f.local(), "fs-layered-thermal", {{"te", 0.0002}}, std::nullopt));
    auto stale = f.s.functional.createEffectiveMaterial(f.local(), resultId, json::object());
    ASSERT_FALSE(stale);
    EXPECT_EQ(stale.error().code, ErrorCode::FailedPrecondition);
    EXPECT_NE(stale.error().message.find("stale"), std::string::npos);
    EXPECT_FALSE(f.s.functional.createEffectiveMaterial(f.local(), "no-such-result", json::object()));
}

TEST(MetamaterialServices, AnalysesWithoutAnEffectiveMaterialAreRefused)
{
    Fixture f("a-mass-in-mass-lattice.resonant");
    const std::string resultId = f.resultOf(f.s.functional.analyze(f.local(), "fs-mass-in-mass", ""));
    ASSERT_FALSE(resultId.empty());
    auto refused = f.s.functional.createEffectiveMaterial(f.local(), resultId, json::object());
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code, ErrorCode::FailedPrecondition);
    // A sweep result is not an analysis.
    const std::string sweepId = f.resultOf(f.s.functional.sweep(f.local(), "fs-mass-in-mass", {{"axes", {{{"parameter", "r"}, {"values", {0.006, 0.008}}}}}}));
    ASSERT_FALSE(sweepId.empty());
    EXPECT_FALSE(f.s.functional.createEffectiveMaterial(f.local(), sweepId, json::object()));
    // The analyses are listed with their category.
    auto kinds = f.s.functional.analysisKinds(f.local());
    ASSERT_TRUE(kinds);
    int effective = 0;
    const json list = kinds.value().is_array() ? kinds.value() : kinds.value().value("kinds", json::array());
    for (const json& k : list)
        effective += k.value("category", "") == "effective-medium" ? 1 : 0;
    EXPECT_EQ(effective, 5);
}

TEST(MetamaterialServices, AHomogenizedRegionReproducesTheExplicitLaminateInTheCoupledEngine)
{
    Fixture f("e-layered-thermal.resonant");
    const std::string resultId = f.resultOf(f.s.functional.analyze(f.local(), "fs-layered-thermal", ""));
    ASSERT_TRUE(f.s.functional.createEffectiveMaterial(f.local(), resultId, {{"id", "mat-laminate-eff"}}));
    auto stored = f.s.multiphysics.system(f.local(), "sys-laminate-explicit");
    ASSERT_TRUE(stored) << stored.error().message;
    auto explicitSystem = multiphysics::PhysicalSystem::fromJson(stored.value());
    ASSERT_TRUE(explicitSystem) << explicitSystem.message();
    auto a = coupling::runPhysicalSystem(explicitSystem.value(), f.host.project().materials);
    ASSERT_TRUE(a) << a.message();
    ASSERT_EQ(a.value().status, multiphysics::RunStatus::Completed) << a.value().message;
    // The same slab as one region of the saved effective material.
    json homogenized = stored.value();
    homogenized["id"] = "sys-laminate-homogenized";
    homogenized["regions"] = json::array({{{"id", "laminate"}, {"name", "Laminate"}, {"materialId", "mat-laminate-eff"}, {"thickness", 0.003}, {"cells", 30}}});
    auto system = multiphysics::PhysicalSystem::fromJson(homogenized);
    ASSERT_TRUE(system) << system.message();
    EXPECT_TRUE(coupling::validatePhysicalSystem(system.value(), f.host.project().materials).empty());
    auto b = coupling::runPhysicalSystem(system.value(), f.host.project().materials);
    ASSERT_TRUE(b) << b.message();
    ASSERT_EQ(b.value().status, multiphysics::RunStatus::Completed) << b.value().message;
    // Steady conduction across the layers: the series conductivity is exact.
    const double qa = heatLeaving(a.value(), "+z"), qb = heatLeaving(b.value(), "+z");
    const double expected = 10.0 * 0.0025 * (0.6 / (0.5 / 400 + 0.1 / 0.2)) / 0.003;
    EXPECT_NEAR(qa, expected, 1e-9 * expected);
    EXPECT_NEAR(qb, qa, 1e-9 * qa);
    EXPECT_NEAR(heatLeaving(b.value(), "-z"), -qb, 1e-9 * qb);
}
