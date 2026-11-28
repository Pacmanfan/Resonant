// The functional-structure application services (RES-FUNC-001, RES-FUNC-003,
// RES-FUNC-004, RES-FUNC-012, RES-FUNC-014): storage and undo, parameters as
// one undoable scene change, optimistic concurrency, analyses, sweeps and
// sensitivities as jobs, results that become stale, optimization with
// explained candidates, capabilities and permissions.
#include "optimization/Persistence.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "services/Services.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <thread>

using namespace resonant;
using namespace resonant::services;
using nlohmann::json;

namespace {

std::unique_ptr<Project> example(const std::string& file)
{
    auto loaded = loadProjectFile(std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "functional" / file);
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
    json finished(const Outcome<JobView>& job)
    {
        EXPECT_TRUE(job) << (job ? "" : job.error().message);
        if (!job)
            return json::object();
        EXPECT_TRUE(waitForJobs());
        auto view = s.jobs.get(local(), job.value().id);
        EXPECT_TRUE(view);
        EXPECT_EQ(view.value().state, "completed") << (view.value().error ? view.value().error->message : "");
        if (view.value().resultIds.empty())
            return json::object();
        auto doc = s.functional.result(local(), view.value().resultIds[0]);
        EXPECT_TRUE(doc) << (doc ? "" : doc.error().message);
        return doc ? doc.value() : json::object();
    }
    double geometry(const std::string& object, const std::string& key)
    {
        return host.project().scene.find(Id(object))->state.geometry->parameter(key);
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

}  // namespace

TEST(FunctionalServices, StructuresAreStoredUndoablyAndValidated)
{
    Fixture f("a-microwave-cavity.resonant");
    auto list = f.s.functional.structures(f.local());
    ASSERT_TRUE(list);
    ASSERT_EQ(list.value().size(), 1u);
    json changed = list.value()[0];
    changed["name"] = "Renamed cavity";
    ASSERT_TRUE(f.s.functional.setStructure(f.local(), changed));
    EXPECT_EQ(f.s.functional.structure(f.local(), "fs-microwave-cavity").value()["name"], "Renamed cavity");
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_EQ(f.s.functional.structure(f.local(), "fs-microwave-cavity").value()["name"], "Microwave cavity resonator");
    // A structure that does not validate is refused with the reason.
    json bad = changed;
    bad["parameters"][0]["binding"] = "object/ex-fs-cavity/geometry/zzz";
    auto refused = f.s.functional.setStructure(f.local(), bad);
    ASSERT_FALSE(refused);
    EXPECT_NE(refused.error().message.find("zzz"), std::string::npos);
    auto v = f.s.functional.validate(f.local(), "", bad);
    ASSERT_TRUE(v);
    EXPECT_FALSE(v.value().valid);
    EXPECT_TRUE(f.s.functional.validate(f.local(), "fs-microwave-cavity", {}).value().valid);
    // A second structure, then delete it (undoable).
    json second = changed;
    second["id"] = "fs-second";
    ASSERT_TRUE(f.s.functional.setStructure(f.local(), second));
    EXPECT_EQ(f.s.functional.structures(f.local()).value().size(), 2u);
    ASSERT_TRUE(f.s.functional.deleteStructure(f.local(), "fs-second"));
    EXPECT_EQ(f.s.functional.structures(f.local()).value().size(), 1u);
    EXPECT_FALSE(f.s.functional.deleteStructure(f.local(), "fs-second"));
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_EQ(f.s.functional.structures(f.local()).value().size(), 2u);
    EXPECT_FALSE(f.s.functional.structure(f.local(), "fs-none"));
}

TEST(FunctionalServices, ParametersChangeTheSceneAndStructureAsOneUndoableStep)
{
    Fixture f("a-microwave-cavity.resonant");
    auto p = f.s.functional.parameters(f.local(), "fs-microwave-cavity");
    ASSERT_TRUE(p);
    ASSERT_EQ(p.value().size(), 5u);
    EXPECT_EQ(p.value()[0]["object"], "X-band cavity");
    EXPECT_FALSE(p.value()[4]["editable"].get<bool>());  // derived aspect ratio
    const std::uint64_t revision = f.session.projectRevision();
    ASSERT_TRUE(f.s.functional.setParameters(f.local(), "fs-microwave-cavity", {{"a", 0.025}, {"temperature", 350.0}}, revision));
    EXPECT_EQ(f.geometry("ex-fs-cavity", "a"), 0.025);
    auto after = f.s.functional.parameters(f.local(), "fs-microwave-cavity").value();
    EXPECT_EQ(after[3]["value"], 350.0);
    EXPECT_NEAR(after[4]["value"].get<double>(), 0.030 / 0.025, 1e-12);
    // One undo restores both the object and the free parameter.
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_EQ(f.geometry("ex-fs-cavity", "a"), 0.02286);
    EXPECT_EQ(f.s.functional.parameters(f.local(), "fs-microwave-cavity").value()[3]["value"], 293.15);
    // Optimistic concurrency, derived and out-of-range values.
    auto conflict = f.s.functional.setParameters(f.local(), "fs-microwave-cavity", {{"a", 0.024}}, revision);
    ASSERT_FALSE(conflict);
    EXPECT_EQ(conflict.error().code, ErrorCode::Conflict);
    EXPECT_FALSE(f.s.functional.setParameters(f.local(), "fs-microwave-cavity", {{"aspect", 2.0}}, std::nullopt));
    EXPECT_FALSE(f.s.functional.setParameters(f.local(), "fs-microwave-cavity", {{"temperature", 900.0}}, std::nullopt));
    EXPECT_FALSE(f.s.functional.setParameters(f.local(), "fs-microwave-cavity", {}, std::nullopt));
}

TEST(FunctionalServices, AnAnalysisRunsAsAJobAndItsResultBecomesStaleWhenTheGeometryChanges)
{
    Fixture f("a-microwave-cavity.resonant");
    const json d = f.finished(f.s.functional.analyze(f.local(), "fs-microwave-cavity", ""));
    ASSERT_TRUE(d.contains("metrics"));
    EXPECT_FALSE(d["stale"].get<bool>());
    EXPECT_FALSE(d["staleOnCompletion"].get<bool>());
    EXPECT_EQ(d["provenance"]["model"], "em.cavity.rectangular.analytical");
    EXPECT_GT(metric(d, "cavity.f1"), 5e9);
    const std::string resultId = d["resultId"];
    ASSERT_TRUE(f.s.functional.setParameters(f.local(), "fs-microwave-cavity", {{"d", 0.032}}, std::nullopt));
    const json now = f.s.functional.result(f.local(), resultId).value();
    EXPECT_TRUE(now["stale"].get<bool>());
    EXPECT_EQ(now["currentStaleness"][0]["what"], "geometry");
    // Undo makes it current again: the fingerprints are of content, not counters.
    ASSERT_TRUE(f.s.project.undo(f.local()));
    EXPECT_FALSE(f.s.functional.result(f.local(), resultId).value()["stale"].get<bool>());
    // A change while the job runs marks the result stale on completion. The full-wave model runs
    // long enough that the change lands while it runs (the analytical one can finish first: the
    // change's own call collects finished jobs before applying it).
    auto job = f.s.functional.analyze(f.local(), "fs-microwave-cavity", "em.cavity.rectangular.yee");
    ASSERT_TRUE(job);
    ASSERT_TRUE(f.s.functional.setParameters(f.local(), "fs-microwave-cavity", {{"b", 0.011}}, std::nullopt));
    const json late = [&] {
        EXPECT_TRUE(f.waitForJobs());
        auto view = f.s.jobs.get(f.local(), job.value().id).value();
        return f.s.functional.result(f.local(), view.resultIds.at(0)).value();
    }();
    EXPECT_TRUE(late["staleOnCompletion"].get<bool>());
    EXPECT_TRUE(late["stale"].get<bool>());
    EXPECT_FALSE(f.s.functional.result(f.local(), "R-none"));
}

TEST(FunctionalServices, SweepsAndSensitivitiesAreJobs)
{
    Fixture f("c-acoustic-horn.resonant");
    const json sweep = f.finished(f.s.functional.sweep(f.local(), "fs-acoustic-horn", {{"axes", {{{"parameter", "length"}, {"from", 0.3}, {"to", 0.7}, {"points", 3}}}}}));
    ASSERT_EQ(sweep["points"].size(), 3u);
    EXPECT_EQ(sweep["kind"], "sweep");
    EXPECT_EQ(f.geometry("ex-fs-horn", "length"), 0.5);  // the design is unchanged
    const json sens = f.finished(f.s.functional.sensitivity(f.local(), "fs-acoustic-horn", {{"parameters", {"mouth"}}, {"relativeStep", 1e-3}}));
    EXPECT_EQ(sens["kind"], "sensitivity");
    EXPECT_FALSE(sens["entries"].empty());
    EXPECT_FALSE(f.s.functional.sweep(f.local(), "fs-acoustic-horn", json::object()));
    EXPECT_FALSE(f.s.functional.sweep(f.local(), "fs-acoustic-horn", {{"axes", {{{"parameter", "length"}, {"from", 0.3}, {"to", 0.7}, {"points", 1}}}}}));
}

TEST(FunctionalServices, OptimizationFindsAndExplainsCandidates)
{
    Fixture f("d-mechanical-resonator.resonant");
    const json base = f.finished(f.s.functional.analyze(f.local(), "fs-mechanical-resonator", "mechanics.modal.analytical"));
    const double f0 = metric(base, "mech.f1");
    auto job = f.s.functional.optimize(f.local(), "fs-mechanical-resonator",
                                       {{"algorithm", "nelder-mead"}, {"maxEvaluations", 30}, {"model", "mechanics.modal.analytical"}});
    ASSERT_TRUE(job) << job.error().message;
    ASSERT_TRUE(f.waitForJobs());
    EXPECT_EQ(f.s.jobs.get(f.local(), job.value().id).value().state, "completed");
    const auto ws = optimization::workspaceOf(f.host.project());
    ASSERT_EQ(ws.runs.size(), 1u);
    auto why = f.s.functional.explain(f.local(), ws.runs[0].runId, 2);
    ASSERT_TRUE(why) << why.error().message;
    ASSERT_FALSE(why.value()["candidates"].empty());
    const json& best = why.value()["candidates"][0];
    // The 25 Hz target: the best candidate is closer than the present design.
    EXPECT_LT(std::abs(best["metrics"]["mech.f1"].get<double>() - 25.0), std::abs(f0 - 25.0));
    EXPECT_EQ(best["model"], "mechanics.modal.analytical");
    EXPECT_FALSE(best["explanation"]["parametersChanged"].empty());
    EXPECT_FALSE(best["explanation"]["mechanism"].get<std::string>().empty());
    EXPECT_GE(best["metrics"].size(), 1u);
    // Every explained candidate respects the hard manufacturing constraint (3 mm stock).
    for (const json& c : why.value()["candidates"])
        EXPECT_TRUE(c["feasible"].get<bool>());
    EXPECT_FALSE(f.s.functional.explain(f.local(), "run-none", 1));
}

TEST(FunctionalServices, CapabilitiesAndPermissions)
{
    Fixture f("b-optical-resonator.resonant");
    auto caps = f.s.application.capabilities(f.local(), "functional-structure");
    ASSERT_TRUE(caps);
    bool optical = false;
    for (const auto& c : caps.value())
        optical = optical || c.id == "functional.analysis.optics.resonator.two-mirror";
    EXPECT_TRUE(optical);
    EXPECT_GE(f.s.functional.analysisKinds(f.local()).value().size(), 7u);
    const CallContext reader = f.reader();
    EXPECT_TRUE(f.s.functional.structures(reader));
    EXPECT_TRUE(f.s.functional.parameters(reader, "fs-optical-resonator"));
    EXPECT_TRUE(f.s.functional.validate(reader, "fs-optical-resonator", {}));
    EXPECT_FALSE(f.s.functional.setParameters(reader, "fs-optical-resonator", {{"R", 0.9}}, std::nullopt));
    EXPECT_FALSE(f.s.functional.analyze(reader, "fs-optical-resonator", ""));
    EXPECT_FALSE(f.s.functional.optimize(reader, "fs-optical-resonator", json::object()));
    EXPECT_FALSE(f.s.functional.deleteStructure(reader, "fs-optical-resonator"));
}
