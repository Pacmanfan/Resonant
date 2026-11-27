// The application service layer (REQ-CHAT-20261126-012, RES-API-SVC-*):
// one implementation of each operation with revisions and optimistic
// concurrency, dry runs, idempotency, permissions, events (also for changes
// made outside the services), atomic batches and transactions, undo, jobs and
// results with bounded reads, workflows, capabilities and the audit trail.
#include "acoustics/Components.h"
#include "commands/SceneCommands.h"
#include "coupling/WorkflowActions.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "services/Services.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace resonant;
using namespace resonant::services;

namespace {

struct Fixture {
    OwnedProjectHost host{Project::createNew("services")};
    Session session{host};
    Services& s = session.services();
    Fixture() { host.setSession(&session); }
    CallContext local() const { return CallContext{}; }  // in-process: the application session
    CallContext remote(const std::string& client = "test client", std::uint32_t permissions = kDefaultRemotePermissions)
    {
        auto opened = session.openSession(client, Interface::Grpc, permissions, kDefaultRemotePermissions | static_cast<std::uint32_t>(Permission::Files));
        CallContext c;
        c.sessionId = opened.value().id;
        c.source = Interface::Grpc;
        return c;
    }
    Id createBox(const std::string& name = "Box")
    {
        GeometryService::Create create;
        create.kind = "box";
        create.name = name;
        auto m = s.geometry.createPrimitive(local(), create);
        EXPECT_TRUE(m.ok()) << (m.ok() ? "" : m.error().message);
        return m.value().changes.created.front();
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

}  // namespace

TEST(Services, CreateUpdateDeleteWithRevisionsUndoAndEvents)
{
    Fixture f;
    const std::uint64_t start = f.session.projectRevision();
    const std::uint64_t firstEvent = f.session.events().lastSequence();
    GeometryService::Create create;
    create.kind = "sphere";
    create.name = "Ball";
    create.parameters = {{"radius", 0.25}};
    auto created = f.s.geometry.createPrimitive(f.local(), create);
    ASSERT_TRUE(created.ok()) << created.error().message;
    const Id ball = created.value().changes.created.front();
    EXPECT_EQ(created.value().revisionAfter, start + 1);
    EXPECT_EQ(f.session.objectRevision(ball), start + 1);
    auto view = f.s.scene.object(f.local(), ball);
    ASSERT_TRUE(view.ok());
    EXPECT_EQ(view.value().geometryKind, "sphere");
    EXPECT_DOUBLE_EQ(view.value().parameters.at("radius"), 0.25);
    // Optimistic concurrency: a stale expected revision is refused with the current one.
    SceneService::Update rename;
    rename.name = "Ball 2";
    auto stale = f.s.scene.update(f.local(), ball, start, rename);
    ASSERT_FALSE(stale.ok());
    EXPECT_EQ(stale.error().code, ErrorCode::Conflict);
    EXPECT_EQ(stale.error().currentRevision, start + 1);
    auto renamed = f.s.scene.update(f.local(), ball, start + 1, rename);
    ASSERT_TRUE(renamed.ok()) << renamed.error().message;
    EXPECT_EQ(f.s.scene.object(f.local(), ball).value().name, "Ball 2");
    // Nothing to change: no undo step, no new revision.
    const std::uint64_t r = f.session.projectRevision();
    EXPECT_TRUE(f.s.scene.update(f.local(), ball, std::nullopt, rename).ok());
    EXPECT_EQ(f.session.projectRevision(), r);
    // Invalid values are refused with the field.
    auto invalidParams = f.s.geometry.updateParameters(f.local(), ball, std::nullopt, {{"radius", -1.0}});
    ASSERT_FALSE(invalidParams.ok());
    EXPECT_EQ(invalidParams.error().code, ErrorCode::InvalidArgument);
    EXPECT_EQ(invalidParams.error().violations.front().field, "parameters.radius");
    // Undo through the service restores the name; the events tell the story.
    ASSERT_TRUE(f.s.project.undo(f.local()).ok());
    EXPECT_EQ(f.s.scene.object(f.local(), ball).value().name, "Ball");
    ASSERT_TRUE(f.s.scene.remove(f.local(), {ball}).ok());
    EXPECT_FALSE(f.s.scene.object(f.local(), ball).ok());
    const auto batch = f.session.events().since(firstEvent);
    std::vector<std::string> types;
    for (const Event& e : batch.events)
        types.push_back(e.type);
    EXPECT_NE(std::find(types.begin(), types.end(), "scene.object.created"), types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), "scene.object.updated"), types.end());
    EXPECT_NE(std::find(types.begin(), types.end(), "scene.object.deleted"), types.end());
    for (size_t k = 1; k < batch.events.size(); ++k)
        EXPECT_EQ(batch.events[k].sequence, batch.events[k - 1].sequence + 1);
}

TEST(Services, ChangesMadeOutsideTheServicesAdvanceRevisionsAndProduceEvents)
{
    Fixture f;
    const std::uint64_t before = f.session.projectRevision();
    const std::uint64_t seq = f.session.events().lastSequence();
    // As the GUI's own commands do: pushed directly onto the undo history.
    auto group = makeGroup("Direct");
    const Id id = group->id();
    ASSERT_TRUE(f.host.commands().push(std::make_unique<InsertObjectCommand>(f.host.project().scene, std::move(group))).ok());
    EXPECT_EQ(f.session.projectRevision(), before + 1);
    EXPECT_EQ(f.session.objectRevision(id), before + 1);
    // As Python's direct edits do: no command at all.
    ASSERT_TRUE(coupling::addSceneObject(f.host.project(), {{"type", "geometry"}, {"id", "direct-box"}}).ok());
    EXPECT_EQ(f.session.projectRevision(), before + 2);
    const auto events = f.session.events().since(seq);
    int created = 0;
    for (const Event& e : events.events)
        created += e.type == "scene.object.created" ? 1 : 0;
    EXPECT_EQ(created, 2);
}

TEST(Services, EventPagesResumeWithoutLosingEvents)
{
    EventBus bus(100);
    for (int k = 0; k < 10; ++k) {
        Event e;
        e.category = k % 2 ? EventCategory::Job : EventCategory::Scene;
        e.type = "test";
        bus.publish(e);
    }
    std::vector<std::uint64_t> seen;
    std::uint64_t after = 0;
    for (int page = 0; page < 10; ++page) {
        const EventBatch b = bus.since(after, {}, 3);
        for (const Event& e : b.events)
            seen.push_back(e.sequence);
        if (b.lastSequence == after)
            break;
        after = b.lastSequence;
    }
    ASSERT_EQ(seen.size(), 10u);
    for (std::size_t k = 0; k < seen.size(); ++k)
        EXPECT_EQ(seen[k], k + 1);
    // Filtered: the scan still moves past events that do not match.
    EventFilter jobs;
    jobs.categories = {EventCategory::Job};
    const EventBatch j = bus.since(0, jobs, 1000);
    EXPECT_EQ(j.events.size(), 5u);
    EXPECT_EQ(j.lastSequence, 10u);
    // Beyond the retention (at least 16 events are kept): a gap.
    EventBus small(16);
    for (int k = 0; k < 20; ++k)
        small.publish(Event{});
    const EventBatch g = small.since(1);
    ASSERT_TRUE(g.gap.has_value());
    EXPECT_EQ(g.gap->firstMissed, 2u);
    EXPECT_EQ(g.gap->lastMissed, 4u);
    EXPECT_EQ(g.gap->oldestRetained, 5u);
    EXPECT_EQ(g.events.size(), 16u);
    EXPECT_FALSE(small.since(10).gap.has_value());
}

TEST(Services, DryRunsChangeNothingAndIdempotencyKeysReplay)
{
    Fixture f;
    CallContext dry = f.local();
    dry.dryRun = true;
    GeometryService::Create create;
    create.kind = "box";
    const std::uint64_t before = f.session.projectRevision();
    auto planned = f.s.geometry.createPrimitive(dry, create);
    ASSERT_TRUE(planned.ok());
    EXPECT_TRUE(planned.value().dryRun);
    EXPECT_EQ(planned.value().changes.created.size(), 1u);
    EXPECT_EQ(f.host.project().scene.objectCount(), 0u);
    EXPECT_EQ(f.session.projectRevision(), before);
    // A retried request with the same key creates nothing new.
    CallContext keyed = f.remote();
    keyed.idempotencyKey = "create-box-1";
    auto first = f.s.geometry.createPrimitive(keyed, create);
    auto second = f.s.geometry.createPrimitive(keyed, create);
    ASSERT_TRUE(first.ok() && second.ok());
    EXPECT_TRUE(second.value().replayed);
    EXPECT_EQ(first.value().changes.created, second.value().changes.created);
    EXPECT_EQ(f.host.project().scene.objectCount(), 1u);
    // The same key for another operation is refused.
    auto other = f.s.scene.remove(keyed, first.value().changes.created);
    ASSERT_FALSE(other.ok());
    EXPECT_EQ(other.error().code, ErrorCode::AlreadyExists);
}

TEST(Services, SessionsAndPermissionsAreEnforced)
{
    Fixture f;
    CallContext anonymous;
    anonymous.source = Interface::Grpc;
    auto refused = f.s.project.get(anonymous);
    ASSERT_FALSE(refused.ok());
    EXPECT_EQ(refused.error().code, ErrorCode::PermissionDenied);
    CallContext impostor;
    impostor.source = Interface::Grpc;
    impostor.sessionId = Session::kApplicationSession;
    GeometryService::Create box;
    box.kind = "box";
    auto impersonated = f.s.geometry.createPrimitive(impostor, box);
    ASSERT_FALSE(impersonated.ok());
    EXPECT_EQ(impersonated.error().code, ErrorCode::PermissionDenied);
    CallContext reader = f.remote("reader", static_cast<std::uint32_t>(Permission::Read));
    EXPECT_TRUE(f.s.project.get(reader).ok());
    GeometryService::Create create;
    create.kind = "box";
    auto write = f.s.geometry.createPrimitive(reader, create);
    ASSERT_FALSE(write.ok());
    EXPECT_EQ(write.error().code, ErrorCode::PermissionDenied);
    // Remote file access only within the project directory, with the files permission.
    CallContext writer = f.remote("writer");
    auto save = f.s.project.save(writer, "x.resonant");
    ASSERT_FALSE(save.ok());
    EXPECT_EQ(save.error().code, ErrorCode::PermissionDenied);
    CallContext files = f.remote("files", kDefaultRemotePermissions | static_cast<std::uint32_t>(Permission::Files));
    auto escape = f.s.project.save(files, "../x.resonant");
    ASSERT_FALSE(escape.ok());
    EXPECT_EQ(escape.error().code, ErrorCode::InvalidArgument);
    const auto dir = std::filesystem::temp_directory_path() / "resonant-services-files";
    std::filesystem::create_directories(dir);
    f.s.project.setProjectDirectory(dir.string());
    ASSERT_TRUE(f.s.project.save(files, "saved.resonant").ok());
    EXPECT_TRUE(std::filesystem::exists(dir / "saved.resonant"));
    EXPECT_EQ(f.s.project.get(files).value().fileName, "saved.resonant");  // no directory sent
    // Every mutation, refused or not, is in the audit trail (own session only without admin).
    auto audit = f.s.application.audit(reader, {});
    ASSERT_TRUE(audit.ok());
    ASSERT_FALSE(audit.value().empty());
    EXPECT_FALSE(audit.value().back().succeeded);
    for (const AuditRecord& r : audit.value())
        EXPECT_EQ(r.sessionId, reader.sessionId);
}

TEST(Services, BatchesAreAtomicAndTransactionsDetectConflicts)
{
    Fixture f;
    const Id box = f.createBox();
    const auto createOp = [](const std::string& kind) {
        return TransactionService::Operation{kind, "GeometryService.CreatePrimitive", [kind](Services& s, const CallContext& c) {
                                                 GeometryService::Create create;
                                                 create.kind = kind;
                                                 return s.geometry.createPrimitive(c, create);
                                             }};
    };
    // One bad operation: nothing is applied.
    const size_t objects = f.host.project().scene.objectCount();
    auto failed = f.s.transactions.executeBatch(f.local(), {createOp("sphere"), createOp("no-such-kind"), createOp("cone")}, "three");
    ASSERT_TRUE(failed.ok());
    EXPECT_FALSE(failed.value().committed);
    EXPECT_TRUE(failed.value().results[0].succeeded);
    EXPECT_EQ(failed.value().results[1].error->code, ErrorCode::InvalidArgument);
    EXPECT_EQ(failed.value().results[2].error->code, ErrorCode::FailedPrecondition);
    EXPECT_EQ(f.host.project().scene.objectCount(), objects);
    // All good: one commit, one undo step.
    const size_t steps = f.host.commands().size();
    auto ok = f.s.transactions.executeBatch(f.local(), {createOp("sphere"), createOp("cone")}, "two shapes");
    ASSERT_TRUE(ok.ok()) << ok.error().message;
    EXPECT_TRUE(ok.value().committed);
    EXPECT_EQ(f.host.project().scene.objectCount(), objects + 2);
    EXPECT_EQ(f.host.commands().size(), steps + 1);
    EXPECT_EQ(ok.value().mutation.changes.created.size(), 2u);
    ASSERT_TRUE(f.s.project.undo(f.local()).ok());
    EXPECT_EQ(f.host.project().scene.objectCount(), objects);
    // A transaction whose base moved on is refused at commit.
    auto t = f.s.transactions.begin(f.local(), "later");
    ASSERT_TRUE(t.ok());
    ASSERT_TRUE(f.s.transactions.stage(f.local(), t.value().id, {createOp("box")}).ok());
    auto validated = f.s.transactions.validate(f.local(), t.value().id);
    ASSERT_TRUE(validated.ok());
    EXPECT_TRUE(validated.value().first.validation.valid);
    SceneService::Update u;
    u.name = "moved on";
    ASSERT_TRUE(f.s.scene.update(f.local(), box, std::nullopt, u).ok());
    auto conflict = f.s.transactions.commit(f.local(), t.value().id);
    ASSERT_FALSE(conflict.ok());
    EXPECT_EQ(conflict.error().code, ErrorCode::Conflict);
    EXPECT_EQ(f.s.transactions.abort(f.local(), t.value().id).value().state, "aborted");
}

TEST(Services, MaterialChangesProduceEventsAlsoOnUndoAndRedo)
{
    Fixture f;
    const auto materialEvents = [&](std::uint64_t after) {
        int n = 0;
        for (const Event& e : f.session.events().since(after).events)
            n += e.type == "material.changed" ? 1 : 0;
        return n;
    };
    std::uint64_t seq = f.session.events().lastSequence();
    ASSERT_TRUE(f.s.materials.create(f.local(), {{"name", "Test foam"}, {"domains", nlohmann::json::object()}}).ok());
    EXPECT_EQ(materialEvents(seq), 1);
    seq = f.session.events().lastSequence();
    ASSERT_TRUE(f.s.project.undo(f.local()).ok());
    EXPECT_EQ(materialEvents(seq), 1);
    seq = f.session.events().lastSequence();
    ASSERT_TRUE(f.s.project.redo(f.local()).ok());
    EXPECT_EQ(materialEvents(seq), 1);
    // A scene change alone is not a material change.
    seq = f.session.events().lastSequence();
    f.createBox();
    EXPECT_EQ(materialEvents(seq), 0);
}

TEST(Services, MaterialsEmittersAndArrays)
{
    Fixture f;
    const Id box = f.createBox();
    auto materials = f.s.materials.list(f.local(), "acoustic");
    ASSERT_TRUE(materials.ok());
    ASSERT_FALSE(materials.value().empty());
    const Id material(materials.value().front()["id"].get<std::string>());
    ASSERT_TRUE(f.s.materials.assign(f.local(), material, {box}).ok());
    EXPECT_EQ(f.s.scene.object(f.local(), box).value().material, material);
    ASSERT_TRUE(f.s.materials.remove(f.local(), material).ok());  // the assignment goes with it
    EXPECT_TRUE(f.s.scene.object(f.local(), box).value().material.isNull());
    auto emitter = f.s.emitters.create(f.local(), "E", std::nullopt, Id(), {{"frequencyHz", 25000.0}});
    ASSERT_TRUE(emitter.ok()) << emitter.error().message;
    const Id e = emitter.value().changes.created.front();
    EXPECT_DOUBLE_EQ(f.s.emitters.get(f.local(), e).value().component["frequencyHz"].get<double>(), 25000.0);
    auto bad = f.s.emitters.update(f.local(), e, std::nullopt, {{"amplitudePa", -2.0}}, std::nullopt);
    ASSERT_FALSE(bad.ok());
    EXPECT_EQ(bad.error().code, ErrorCode::InvalidArgument);
    auto array = f.s.arrays.create(f.local(), "A", std::nullopt, Id(), {{"countX", 4}});
    ASSERT_TRUE(array.ok()) << array.error().message;
    const Id a = array.value().changes.created.front();
    ASSERT_TRUE(f.s.arrays.setExcitation(f.local(), a, std::nullopt, {{1, 1.5, std::nullopt, std::nullopt, std::nullopt}}, false).ok());
    const SceneObject* object = f.host.project().scene.find(a);
    auto elements = f.s.arrays.elements(*object);
    ASSERT_EQ(elements.size(), 4u);
    EXPECT_DOUBLE_EQ(elements[1].second["phaseRad"].get<double>(), 1.5);
    EXPECT_FALSE(f.s.arrays.setExcitation(f.local(), a, std::nullopt, {{9, 0.0, std::nullopt, std::nullopt, std::nullopt}}, false).ok());
    ASSERT_TRUE(f.s.arrays.focus(f.local(), a, std::nullopt, Id(), Vec3(0, 0, 0.05), false).ok());
    EXPECT_NE(f.s.arrays.elements(*f.host.project().scene.find(a))[0].second["phaseRad"].get<double>(), 0.0);
}

TEST(Services, AFieldJobProducesAResultReadInBoundedSlices)
{
    Fixture f;
    ASSERT_TRUE(f.s.arrays.create(f.local(), "Array", std::nullopt, Id(), {{"countX", 8}}).ok());
    ASSERT_TRUE(coupling::addSceneObject(f.host.project(), {{"type", "region"}, {"id", "plane"}, {"position", {0, 0, 0.05}},
                                                            {"component", {{"countU", 21}, {"countV", 11}}}})
                    .ok());
    auto plan = f.s.simulation.estimateAcousticField(f.local(), Id("plane"));
    ASSERT_TRUE(plan.ok()) << plan.error().message;
    EXPECT_TRUE(plan.value().estimate.allowed);
    EXPECT_EQ(plan.value().activeSources, 8);
    const std::uint64_t seq = f.session.events().lastSequence();
    auto started = f.s.simulation.startAcousticField(f.local(), Id("plane"));
    ASSERT_TRUE(started.ok()) << started.error().message;
    ASSERT_TRUE(f.waitForJobs());
    std::vector<std::string> jobTypes;
    for (const Event& e : f.session.events().since(seq).events)
        if (e.category == EventCategory::Job)
            jobTypes.push_back(e.type);
    ASSERT_FALSE(jobTypes.empty());
    EXPECT_EQ(jobTypes.back(), "job.completed");
    EXPECT_NE(std::find(jobTypes.begin(), jobTypes.end(), "job.running"), jobTypes.end());
    auto job = f.s.jobs.get(f.local(), started.value().job.id);
    ASSERT_TRUE(job.ok());
    EXPECT_EQ(job.value().state, "completed");
    ASSERT_EQ(job.value().resultIds.size(), 1u);
    auto result = f.s.results.get(f.local(), job.value().resultIds.front());
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value()->kind, "acoustic.field");
    auto slice = f.s.results.slice(f.local(), result.value()->id, 2, 0, 1);
    ASSERT_TRUE(slice.ok());
    EXPECT_EQ(slice.value().rows * slice.value().columns, 21 * 11);
    auto coarse = f.s.results.slice(f.local(), result.value()->id, 2, 0, 2);
    ASSERT_TRUE(coarse.ok());
    EXPECT_EQ(coarse.value().rows, 11);
    auto tooBig = f.s.results.slice(f.local(), result.value()->id, 2, 0, 1, 10);
    ASSERT_FALSE(tooBig.ok());
    EXPECT_EQ(tooBig.error().code, ErrorCode::ResourceExhausted);
    const acoustics::FieldResult& field = *result.value()->field;
    auto samples = f.s.results.samples(f.local(), result.value()->id, {field.frame.point(3, 4, 0), Vec3(5, 5, 5)});
    ASSERT_TRUE(samples.ok());
    EXPECT_TRUE(samples.value()[0].inside);
    EXPECT_EQ(samples.value()[0].value, field.pressure[field.frame.index(3, 4, 0)]);
    EXPECT_FALSE(samples.value()[1].inside);
    // A region without a field component is refused before any job starts.
    const Id box = f.createBox();
    EXPECT_FALSE(f.s.simulation.startAcousticField(f.local(), box).ok());
}

TEST(Services, WorkflowsCapabilitiesAndDiagnostics)
{
    Fixture f;
    auto run = f.s.workflows.execute(f.local(), "em.dipole-pattern", nullptr, nullptr, true);
    ASSERT_TRUE(run.ok()) << run.error().message;
    ASSERT_TRUE(f.waitForJobs());
    auto outcome = f.s.workflows.run(f.local(), run.value().id);
    ASSERT_TRUE(outcome.ok());
    EXPECT_TRUE(outcome.value().second);
    EXPECT_EQ(outcome.value().first["status"], "completed");
    EXPECT_GT(f.host.project().scene.objectCount(), 0u);  // its changes were applied
    auto capabilities = f.s.application.capabilities(f.local(), "solver");
    ASSERT_TRUE(capabilities.ok());
    bool freeField = false, fdtdStartable = true;
    for (const CapabilityView& c : capabilities.value()) {
        if (c.id == "solver.acoustic.freefield.analytical")
            freeField = c.available;
        if (c.id == "solver.acoustic.fdtd")
            fdtdStartable = c.limitations.empty();
    }
    EXPECT_TRUE(freeField);
    EXPECT_FALSE(fdtdStartable);  // available in the application, not (yet) through the API: said so
    auto outputs = f.s.application.capability(f.local(), "feature.hardware-outputs");
    ASSERT_TRUE(outputs.ok());
    EXPECT_FALSE(outputs.value().available);
    auto d = f.s.application.diagnostics(f.local());
    ASSERT_TRUE(d.ok());
    EXPECT_GE(d.value().activeSessions, 1);
    EXPECT_GT(d.value().lastEventSequence, 0u);
}
