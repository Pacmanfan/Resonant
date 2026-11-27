// The state behind the application services (REQ-CHAT-20261126-012,
// ADR-000216): one open project with its undo history and jobs (owned by a
// host: the GUI's DocumentController, the headless automation host, a Python
// project), the revisions, the event stream, the client sessions, the audit
// trail, idempotency records, tracked jobs and their results.
//
// Every project change made through a service goes through mutate(): the
// permission check, optimistic concurrency (expected project / object
// revision), idempotency (a repeated key replays the first outcome), dry runs
// (applied to a copy, nothing changes), one undoable command, the audit
// record. Changes made elsewhere (GUI commands, Python's direct edits) still
// advance the revision and produce events, because the session listens to the
// scene and the undo history.
//
// Threading: call a Session only on its owner's thread (the GUI thread, the
// host's loop). The event bus is thread-safe; job progress is published from
// worker threads; finished jobs are collected on the owner thread.
#pragma once

#include "commands/Command.h"
#include "materials/Material.h"
#include "services/EventBus.h"
#include "services/ServiceTypes.h"
#include "simulation/Job.h"

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace resonant {
class Project;
namespace acoustics {
struct FieldResult;
}
}  // namespace resonant

namespace resonant::services {

class Services;

// What owns the project. The GUI's DocumentController is one; OwnedProjectHost another.
class ProjectHost {
public:
    virtual ~ProjectHost() = default;
    virtual Project& project() = 0;
    virtual CommandStack& commands() = 0;
    virtual JobManager& jobs() = 0;
    virtual std::string filePath() const = 0;
    virtual void setFilePath(const std::string& path) = 0;
    // Replaces the open project (new / open); the host calls Session::projectReplaced afterwards.
    virtual void installProject(std::unique_ptr<Project> project, const std::string& path) = 0;
};

// A host that owns its project, history and jobs (headless host, Python, tests).
class OwnedProjectHost final : public ProjectHost {
public:
    explicit OwnedProjectHost(std::unique_ptr<Project> project, unsigned workers = 1);
    ~OwnedProjectHost() override;
    Project& project() override { return *project_; }
    CommandStack& commands() override { return commands_; }
    JobManager& jobs() override { return *jobs_; }
    std::string filePath() const override { return path_; }
    void setFilePath(const std::string& path) override { path_ = path; }
    void installProject(std::unique_ptr<Project> project, const std::string& path) override;
    void setSession(class Session* session) { session_ = session; }

private:
    std::unique_ptr<Project> project_;
    CommandStack commands_;
    std::unique_ptr<JobManager> jobs_;
    std::string path_;
    class Session* session_ = nullptr;
};

struct SessionInfo {
    std::string id, clientName;
    Interface source = Interface::Unspecified;
    std::uint32_t permissions = 0;
    std::chrono::system_clock::time_point opened, lastSeen;
};

struct AuditRecord {
    std::uint64_t sequence = 0;
    std::chrono::system_clock::time_point time;
    std::string requestId, sessionId, clientName;
    Interface source = Interface::Unspecified;
    std::string operation;
    std::vector<std::string> resourceIds;
    std::uint64_t revisionBefore = 0, revisionAfter = 0;
    bool succeeded = true;
    bool dryRun = false;
    std::optional<ServiceError> error;
    double durationSeconds = 0.0;
};

struct AuditQuery {
    std::string sessionId, operation, resourceId;
    std::uint64_t afterSequence = 0;
    std::size_t limit = 1000;
    bool failuresOnly = false;
};

// A result produced by a job a service started (or registered by the application).
struct ResultEntry {
    std::string id, kind, name, jobId;
    std::chrono::system_clock::time_point created;
    std::uint64_t projectRevision = 0;
    std::shared_ptr<const acoustics::FieldResult> field;  // kind "acoustic.field"
};

// A job started through the services, with what to do on the owner thread when it ends.
struct JobRecord {
    std::string jobId, type, sessionId;
    std::function<void(class Session&, const JobInfo&)> onFinished;
    std::vector<std::string> resultIds;
    bool handled = false;
    nlohmann::json outcome = nlohmann::json::object();  // e.g. a workflow run record
};

class Session {
public:
    explicit Session(ProjectHost& host, std::string hostKind = "headless");
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    ProjectHost& host() { return host_; }
    Project& project() { return host_.project(); }
    const Project& project() const { return host_.project(); }
    const std::string& hostKind() const { return hostKind_; }
    Services& services() { return *services_; }
    // The host installed another project: listen to it, reset object revisions, tell the clients.
    void projectReplaced(const std::string& reason = "project.replaced");

    // Revisions: the project's advances by one per committed change (also GUI changes and undo / redo).
    std::uint64_t projectRevision() const;
    std::uint64_t objectRevision(const Id& id) const;

    EventBus& events() { return events_; }
    // Publishes with the current call's source when there is one.
    void publish(Event event);

    // Client sessions. The application itself has a built-in session ("application").
    Outcome<SessionInfo> openSession(std::string clientName, Interface source, std::uint32_t requested, std::uint32_t policy);
    Done closeSession(const std::string& sessionId);
    std::vector<SessionInfo> sessions() const;
    static constexpr const char* kApplicationSession = "application";
    // Checks the session and the permission; fills in the session's client and permissions.
    Outcome<CallContext> authorize(const CallContext& context, Permission required);

    // The mutation funnel. `build` makes the command for the given project (the
    // live one, or a copy for a dry run); `expectedObjectRevision` applies to resources.front().
    using Builder = std::function<Outcome<std::unique_ptr<Command>>(Project& target)>;
    Outcome<Mutation> mutate(const CallContext& context, const std::string& operation, const std::vector<Id>& resources,
                             std::optional<std::uint64_t> expectedObjectRevision, const Builder& build);
    // Applies a changed copy of the project (transactions, workflows) as one undoable step.
    Outcome<Mutation> applyCopy(const CallContext& context, const std::string& operation, const Project& changed, const std::string& description);
    Outcome<std::string> undo(const CallContext& context);
    Outcome<std::string> redo(const CallContext& context);

    // Audit trail of mutations (and refused mutations); bounded ring, exportable.
    void record(AuditRecord record);
    std::vector<AuditRecord> audit(const AuditQuery& query) const;
    std::size_t auditSize() const { return audit_.size(); }
    std::vector<ServiceError> recentErrors() const;
    std::string exportAudit() const;  // JSON lines

    // Results and the jobs that produce them.
    std::string addResult(ResultEntry entry);
    const ResultEntry* result(const std::string& id) const;
    std::vector<const ResultEntry*> results() const;
    void trackJob(JobRecord record);
    const JobRecord* jobRecord(const std::string& jobId) const;
    std::set<std::string> cancelRequested() const;
    void markCancelRequested(const std::string& jobId);
    // Handles jobs that ended (results, applied workflow changes): call on the owner thread.
    void collectFinishedJobs();

    // Idempotency: a key seen before replays its outcome (the same operation) or is refused (another one).
    std::optional<Outcome<Mutation>> replay(const CallContext& context, const std::string& operation) const;

    std::chrono::steady_clock::time_point started() const { return started_; }
    // A staging session (transactions) starts from another session's revisions.
    void seedRevisions(const Session& from);

private:
    void attach();
    void detach();
    void onSceneEvent(int kind, const Id& id, const Id& parent);
    void onCommandsChanged();
    void onJobChanged(const JobInfo& info);
    void commitRevision();
    std::string fingerprintOfCommands() const;
    Outcome<Mutation> refuse(const CallContext& context, const std::string& operation, const std::vector<Id>& resources, ServiceError error,
                             std::chrono::steady_clock::time_point started);

    ProjectHost& host_;
    std::string hostKind_;
    std::unique_ptr<Services> services_;
    EventBus events_;
    std::chrono::steady_clock::time_point started_;

    std::uint64_t revision_ = 0;
    bool pending_ = false;  // a change is in progress or was made outside a command
    std::map<Id, std::uint64_t> objectRevisions_;
    std::string commandsFingerprint_;
    MaterialLibrary materialsSeen_;  // the library at the last committed change (material.changed on any difference)
    int sceneListener_ = 0, commandListener_ = 0, jobListener_ = 0;
    const CallContext* current_ = nullptr;  // the call being executed (its source goes into events)
    std::string currentOperation_;

    std::map<std::string, SessionInfo> sessions_;
    std::deque<AuditRecord> audit_;
    std::uint64_t auditNext_ = 1;
    struct IdempotencyEntry {
        std::string operation, sessionId;
        Mutation outcome;
        std::chrono::steady_clock::time_point time;
    };
    std::map<std::string, IdempotencyEntry> idempotency_;  // key: session + key
    std::deque<std::string> idempotencyOrder_;

    std::vector<ResultEntry> results_;
    std::map<std::string, JobRecord> jobs_;
    mutable std::mutex jobMutex_;  // throttle state and cancel requests are touched from worker threads
    std::map<std::string, std::pair<double, std::chrono::steady_clock::time_point>> lastProgress_;
    std::map<std::string, int> lastState_;
    std::set<std::string> cancelRequested_;
};

}  // namespace resonant::services
