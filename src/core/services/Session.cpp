#include "services/Session.h"

#include "commands/ProjectChange.h"
#include "project/ProjectSerializer.h"
#include "services/Services.h"

#include <algorithm>

namespace resonant::services {

std::string_view toString(ErrorCode code)
{
    switch (code) {
    case ErrorCode::InvalidArgument: return "invalid-argument";
    case ErrorCode::NotFound: return "not-found";
    case ErrorCode::AlreadyExists: return "already-exists";
    case ErrorCode::Conflict: return "conflict";
    case ErrorCode::FailedPrecondition: return "failed-precondition";
    case ErrorCode::PermissionDenied: return "permission-denied";
    case ErrorCode::ResourceExhausted: return "resource-exhausted";
    case ErrorCode::Unimplemented: return "unimplemented";
    case ErrorCode::Unavailable: return "unavailable";
    case ErrorCode::Cancelled: return "cancelled";
    case ErrorCode::Internal: return "internal";
    }
    return "internal";
}

std::string_view toString(Interface source)
{
    switch (source) {
    case Interface::Gui: return "gui";
    case Interface::Python: return "python";
    case Interface::Grpc: return "grpc";
    case Interface::Mcp: return "mcp";
    case Interface::Internal: return "internal";
    case Interface::Unspecified: break;
    }
    return "unspecified";
}

std::string_view toString(Permission p)
{
    switch (p) {
    case Permission::Read: return "read";
    case Permission::Modify: return "modify";
    case Permission::Execute: return "execute";
    case Permission::Files: return "files";
    case Permission::Admin: return "admin";
    }
    return "read";
}

// --- the owned host -----------------------------------------------------------

OwnedProjectHost::OwnedProjectHost(std::unique_ptr<Project> project, unsigned workers)
    : project_(std::move(project)), jobs_(std::make_unique<JobManager>(workers))
{
}

OwnedProjectHost::~OwnedProjectHost()
{
    jobs_.reset();  // stop the workers before the project they may read goes away
}

void OwnedProjectHost::installProject(std::unique_ptr<Project> project, const std::string& path)
{
    project_ = std::move(project);
    path_ = path;
    commands_.clear();
    if (session_)
        session_->projectReplaced(path.empty() ? "project.created" : "project.opened");
}

// --- the session ------------------------------------------------------------------

namespace {
struct Capture {
    Scene& scene;
    ChangeSummary& out;
    int handle = 0;
    Capture(Scene& s, ChangeSummary& summary) : scene(s), out(summary)
    {
        handle = scene.addListener([this](const SceneEvent& e) {
            const auto erase = [](std::vector<Id>& v, const Id& id) { v.erase(std::remove(v.begin(), v.end(), id), v.end()); };
            const auto contains = [](const std::vector<Id>& v, const Id& id) { return std::find(v.begin(), v.end(), id) != v.end(); };
            switch (e.kind) {
            case SceneEvent::Kind::Added:
                if (!contains(out.created, e.id))
                    out.created.push_back(e.id);
                break;
            case SceneEvent::Kind::Removed:
                if (contains(out.created, e.id))
                    erase(out.created, e.id);
                else if (!contains(out.deleted, e.id))
                    out.deleted.push_back(e.id);
                erase(out.updated, e.id);
                break;
            case SceneEvent::Kind::Changed:
            case SceneEvent::Kind::Moved:
                if (!contains(out.created, e.id) && !contains(out.updated, e.id))
                    out.updated.push_back(e.id);
                break;
            case SceneEvent::Kind::Reset: break;
            }
        });
    }
    ~Capture() { scene.removeListener(handle); }
};

std::unique_ptr<Project> copyOf(const Project& project)
{
    auto loaded = deserializeProject(serializeProject(project));
    return loaded ? std::move(loaded.value().project) : nullptr;
}

std::vector<std::string> ids(const std::vector<Id>& v)
{
    std::vector<std::string> out;
    for (const Id& id : v)
        out.push_back(id.str());
    return out;
}
}  // namespace

Session::Session(ProjectHost& host, std::string hostKind) : host_(host), hostKind_(std::move(hostKind)), started_(std::chrono::steady_clock::now())
{
    services_ = std::make_unique<Services>(*this);
    SessionInfo app;
    app.id = kApplicationSession;
    app.clientName = hostKind_ == "gui" ? "Resonant (GUI)" : "Resonant";
    app.source = hostKind_ == "gui" ? Interface::Gui : Interface::Internal;
    app.permissions = kAllPermissions;
    app.opened = app.lastSeen = std::chrono::system_clock::now();
    sessions_[app.id] = app;
    sceneListener_ = project().scene.addListener([this](const SceneEvent& e) { onSceneEvent(static_cast<int>(e.kind), e.id, e.parentId); });
    commandListener_ = host_.commands().addListener([this] { onCommandsChanged(); });
    commandsFingerprint_ = fingerprintOfCommands();
    materialsSeen_ = project().materials;
    jobListener_ = host_.jobs().addListener([this](const JobInfo& info) { onJobChanged(info); });
}

Session::~Session()
{
    host_.jobs().removeListener(jobListener_);
    host_.commands().removeListener(commandListener_);
    project().scene.removeListener(sceneListener_);
}

void Session::projectReplaced(const std::string& reason)
{
    // The previous project (and its listeners) is gone: listen to the new one.
    sceneListener_ = project().scene.addListener([this](const SceneEvent& e) { onSceneEvent(static_cast<int>(e.kind), e.id, e.parentId); });
    objectRevisions_.clear();
    commandsFingerprint_ = fingerprintOfCommands();
    materialsSeen_ = project().materials;
    pending_ = false;
    ++revision_;
    Event e;
    e.category = EventCategory::Project;
    e.type = reason;
    e.projectId = project().id.str();
    e.resources.push_back({"project", e.projectId});
    e.projectRevision = revision_;
    e.payload = {{"projectId", e.projectId}, {"projectName", project().name}, {"description", reason}};
    publish(std::move(e));
}

std::uint64_t Session::projectRevision() const
{
    // A scene change that no command followed was made directly (Python's own edits): it is committed now.
    if (pending_ && !current_)
        const_cast<Session*>(this)->commitRevision();
    return revision_;
}

void Session::seedRevisions(const Session& from)
{
    revision_ = from.projectRevision();
    objectRevisions_ = from.objectRevisions_;
    pending_ = false;
}

std::uint64_t Session::objectRevision(const Id& id) const
{
    projectRevision();
    const auto it = objectRevisions_.find(id);
    return it == objectRevisions_.end() ? 0 : it->second;
}

void Session::publish(Event e)
{
    if (current_) {
        e.sessionId = current_->sessionId.empty() ? std::string(kApplicationSession) : current_->sessionId;
        e.clientName = current_->clientName;
        e.source = current_->source;
        e.requestId = current_->requestId;
    } else if (e.sessionId.empty()) {
        e.sessionId = kApplicationSession;
        e.source = hostKind_ == "gui" ? Interface::Gui : Interface::Internal;
    }
    if (e.projectId.empty())
        e.projectId = project().id.str();
    events_.publish(std::move(e));
}

std::string Session::fingerprintOfCommands() const
{
    const CommandStack& c = host_.commands();
    return std::to_string(c.size()) + '|' + (c.canUndo() ? c.undoText() : "-") + '|' + (c.canRedo() ? c.redoText() : "-");
}

void Session::commitRevision()
{
    revision_ += 1;
    pending_ = false;
    Event e;
    e.category = EventCategory::Project;
    e.type = "project.changed";
    e.resources.push_back({"project", project().id.str()});
    e.projectRevision = revision_;
    e.payload = {{"projectId", project().id.str()},
                 {"projectName", project().name},
                 {"description", host_.commands().canUndo() ? host_.commands().undoText() : std::string()}};
    publish(std::move(e));
}

void Session::onSceneEvent(int kindValue, const Id& id, const Id& parent)
{
    const auto kind = static_cast<SceneEvent::Kind>(kindValue);
    pending_ = true;
    const std::uint64_t revision = revision_ + 1;
    Event e;
    e.category = EventCategory::Scene;
    e.projectRevision = revision;
    std::string type;
    switch (kind) {
    case SceneEvent::Kind::Added: {
        e.type = "scene.object.created";
        if (const SceneObject* o = project().scene.find(id)) {
            type = objectTypeOf(*o);
            // The whole inserted subtree is new.
            std::function<void(const SceneObject&)> mark = [&](const SceneObject& node) {
                objectRevisions_[node.id()] = revision;
                for (const auto& child : node.children())
                    mark(*child);
            };
            mark(*o);
        }
        break;
    }
    case SceneEvent::Kind::Removed:
        e.type = "scene.object.deleted";
        objectRevisions_.erase(id);
        break;
    case SceneEvent::Kind::Changed:
        e.type = "scene.object.updated";
        objectRevisions_[id] = revision;
        break;
    case SceneEvent::Kind::Moved:
        e.type = "scene.object.moved";
        objectRevisions_[id] = revision;
        break;
    case SceneEvent::Kind::Reset:
        e.type = "scene.reset";
        objectRevisions_.clear();
        break;
    }
    if (type.empty())
        if (const SceneObject* o = project().scene.find(id))
            type = objectTypeOf(*o);
    if (!id.isNull())
        e.resources.push_back({"scene-object", id.str()});
    e.payload = {{"objectId", id.str()}, {"objectType", type}, {"parentId", parent.str()}, {"objectRevision", revision}};
    publish(std::move(e));
}

void Session::onCommandsChanged()
{
    const std::string fp = fingerprintOfCommands();
    if (fp == commandsFingerprint_)
        return;  // marked clean (saved): not a change
    commandsFingerprint_ = fp;
    commitRevision();
    if (!(project().materials == materialsSeen_)) {
        materialsSeen_ = project().materials;
        Event e;
        e.category = EventCategory::Material;
        e.type = "material.changed";
        e.resources.push_back({"project", project().id.str()});
        e.projectRevision = revision_;
        e.payload = {{"description", host_.commands().canUndo() ? host_.commands().undoText() : std::string()}};
        publish(std::move(e));
    }
}

void Session::onJobChanged(const JobInfo& info)
{
    // Worker thread: only the event bus and the throttle state are touched (both locked).
    std::string sessionId;
    bool stateChanged = false;
    {
        std::lock_guard lock(jobMutex_);
        const int state = static_cast<int>(info.state);
        const auto now = std::chrono::steady_clock::now();
        auto& last = lastProgress_[info.id.str()];
        stateChanged = lastState_[info.id.str()] != state + 1;
        if (!stateChanged && info.progress - last.first < 0.05 && now - last.second < std::chrono::milliseconds(250))
            return;  // progress events are throttled
        last = {info.progress, now};
        lastState_[info.id.str()] = state + 1;
        if (const auto it = jobs_.find(info.id.str()); it != jobs_.end())
            sessionId = it->second.sessionId;
    }
    Event e;
    e.category = EventCategory::Job;
    // job.queued / running / completed / failed / cancelled on a change of state, else job.progress.
    e.type = stateChanged ? "job." + std::string(toString(info.state)) : std::string("job.progress");
    e.sessionId = sessionId.empty() ? std::string(kApplicationSession) : sessionId;
    e.source = Interface::Internal;
    e.resources.push_back({"job", info.id.str()});
    e.payload = {{"jobId", info.id.str()},
                 {"name", info.name},
                 {"state", std::string(toString(info.state))},
                 {"progress", info.progress},
                 {"message", info.message},
                 {"elapsedSeconds", info.elapsedSeconds}};
    events_.publish(std::move(e));
}

// --- client sessions --------------------------------------------------------------

Outcome<SessionInfo> Session::openSession(std::string clientName, Interface source, std::uint32_t requested, std::uint32_t policy)
{
    if (clientName.empty())
        return invalid("a session needs a client name", "client_name");
    SessionInfo s;
    s.id = "S-" + Id::generate().str();
    s.clientName = std::move(clientName);
    s.source = source;
    s.permissions = requested == 0 ? policy : (requested & policy);
    s.opened = s.lastSeen = std::chrono::system_clock::now();
    sessions_[s.id] = s;
    Event e;
    e.category = EventCategory::Session;
    e.type = "session.opened";
    e.sessionId = s.id;
    e.clientName = s.clientName;
    e.source = s.source;
    e.resources.push_back({"session", s.id});
    e.payload = {{"sessionId", s.id}, {"clientName", s.clientName}};
    publish(std::move(e));
    return s;
}

Done Session::closeSession(const std::string& id)
{
    const auto it = sessions_.find(id);
    if (it == sessions_.end() || id == kApplicationSession)
        return notFound("no open session '" + id + "'", "session", id);
    Event e;
    e.category = EventCategory::Session;
    e.type = "session.closed";
    e.sessionId = id;
    e.clientName = it->second.clientName;
    e.source = it->second.source;
    e.resources.push_back({"session", id});
    e.payload = {{"sessionId", id}, {"clientName", it->second.clientName}};
    sessions_.erase(it);
    publish(std::move(e));
    return Unit{};
}

std::vector<SessionInfo> Session::sessions() const
{
    std::vector<SessionInfo> out;
    for (const auto& [id, s] : sessions_)
        out.push_back(s);
    return out;
}

Outcome<CallContext> Session::authorize(const CallContext& context, Permission required)
{
    collectFinishedJobs();
    CallContext c = context;
    if (c.sessionId.empty()) {
        // In-process callers (the GUI, a Python project) act as the application.
        if (c.source == Interface::Grpc || c.source == Interface::Mcp || c.source == Interface::Unspecified) {
            ServiceError e = ServiceError::make(ErrorCode::PermissionDenied, "open a session first (SessionService.OpenSession)");
            e.requestId = c.requestId;
            return e;
        }
        c.sessionId = kApplicationSession;
        if (c.clientName.empty())
            c.clientName = sessions_[kApplicationSession].clientName;
    } else {
        const bool remote = c.source == Interface::Grpc || c.source == Interface::Mcp || c.source == Interface::Unspecified;
        if (remote && c.sessionId == kApplicationSession) {
            // The application's own session is never available to a remote client.
            ServiceError e = ServiceError::make(ErrorCode::PermissionDenied, "the application session is not available remotely: open a session");
            e.requestId = c.requestId;
            return e;
        }
        const auto it = sessions_.find(c.sessionId);
        if (it == sessions_.end()) {
            ServiceError e = notFound("no open session '" + c.sessionId + "' (it was closed, or the application restarted)", "session", c.sessionId);
            e.requestId = c.requestId;
            return e;
        }
        it->second.lastSeen = std::chrono::system_clock::now();
        c.clientName = it->second.clientName;
        c.source = it->second.source;  // a client cannot claim another interface
        c.permissions = c.sessionId == kApplicationSession ? c.permissions : it->second.permissions;
    }
    if (!has(c.permissions, required)) {
        ServiceError e = ServiceError::make(ErrorCode::PermissionDenied,
                                            "the session '" + c.clientName + "' lacks the " + std::string(toString(required)) + " permission");
        e.requestId = c.requestId;
        return e;
    }
    return c;
}

// --- mutations ------------------------------------------------------------------------

Outcome<Mutation> Session::refuse(const CallContext& c, const std::string& operation, const std::vector<Id>& resources, ServiceError error,
                                  std::chrono::steady_clock::time_point started)
{
    error.requestId = c.requestId;
    AuditRecord r;
    r.requestId = c.requestId;
    r.sessionId = c.sessionId;
    r.clientName = c.clientName;
    r.source = c.source;
    r.operation = operation;
    r.resourceIds = ids(resources);
    r.revisionBefore = r.revisionAfter = revision_;
    r.succeeded = false;
    r.dryRun = c.dryRun;
    r.error = error;
    r.durationSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    record(std::move(r));
    return error;
}

std::optional<Outcome<Mutation>> Session::replay(const CallContext& c, const std::string& operation) const
{
    if (c.idempotencyKey.empty())
        return std::nullopt;
    const auto it = idempotency_.find(c.sessionId + '\n' + c.idempotencyKey);
    if (it == idempotency_.end())
        return std::nullopt;
    if (it->second.operation != operation)
        return Outcome<Mutation>(ServiceError::make(ErrorCode::AlreadyExists, "the idempotency key '" + c.idempotencyKey +
                                                                                  "' was used for another operation (" + it->second.operation + ")"));
    Mutation m = it->second.outcome;
    m.replayed = true;
    return Outcome<Mutation>(std::move(m));
}

Outcome<Mutation> Session::mutate(const CallContext& context, const std::string& operation, const std::vector<Id>& resources,
                                  std::optional<std::uint64_t> expectedObjectRevision, const Builder& build)
{
    const auto started = std::chrono::steady_clock::now();
    auto authorized = authorize(context, Permission::Modify);
    if (!authorized)
        return refuse(context, operation, resources, authorized.error(), started);
    const CallContext& c = authorized.value();
    if (auto replayed = replay(c, operation))
        return *replayed;
    const std::uint64_t before = projectRevision();
    if (c.expectedProjectRevision && *c.expectedProjectRevision != before) {
        ServiceError e = ServiceError::make(ErrorCode::Conflict, "the project changed (revision " + std::to_string(before) + ", expected " +
                                                                     std::to_string(*c.expectedProjectRevision) + ")");
        e.resourceType = "project";
        e.resourceId = project().id.str();
        e.currentRevision = before;
        return refuse(c, operation, resources, e, started);
    }
    if (expectedObjectRevision && !resources.empty()) {
        const std::uint64_t current = objectRevision(resources.front());
        if (current != *expectedObjectRevision) {
            ServiceError e = ServiceError::make(ErrorCode::Conflict, "the object changed since revision " + std::to_string(*expectedObjectRevision) +
                                                                         " (it is now at " + std::to_string(current) + ")");
            e.resourceType = "scene-object";
            e.resourceId = resources.front().str();
            e.currentRevision = current;
            return refuse(c, operation, resources, e, started);
        }
    }
    Mutation m;
    m.revisionBefore = before;
    if (c.dryRun) {
        // On a copy: validated and planned, nothing changes.
        auto copy = copyOf(project());
        if (!copy)
            return refuse(c, operation, resources, ServiceError::make(ErrorCode::Internal, "the project could not be copied"), started);
        auto command = build(*copy);
        if (!command)
            return refuse(c, operation, resources, command.error(), started);
        if (!command.value()) {
            m.revisionAfter = before;
            m.dryRun = true;
            m.changes.description = "no change";
            return m;
        }
        const MaterialLibrary materials = copy->materials;
        const nlohmann::json extensions = copy->extensions;
        {
            Capture capture(copy->scene, m.changes);
            if (Status s = command.value()->execute(); !s)
                return refuse(c, operation, resources, invalid(s.message()), started);
        }
        m.changes.materials = !(copy->materials == materials);
        m.changes.settings = copy->extensions != extensions;
        m.changes.description = command.value()->description();
        m.revisionAfter = before;
        m.dryRun = true;
    } else {
        auto command = build(project());
        if (!command)
            return refuse(c, operation, resources, command.error(), started);
        if (!command.value()) {
            // Nothing to change: no undo step, no new revision.
            m.revisionAfter = before;
            m.changes.description = "no change";
            return m;
        }
        const MaterialLibrary materials = project().materials;
        const nlohmann::json extensions = project().extensions;
        Status s = Status::success();
        {
            Capture capture(project().scene, m.changes);
            current_ = &c;
            s = host_.commands().push(command.take());
            current_ = nullptr;
        }
        if (!s)
            return refuse(c, operation, resources, invalid(s.message()), started);
        m.changes.materials = !(project().materials == materials);
        m.changes.settings = project().extensions != extensions;
        // material.changed was published by onCommandsChanged (as for undo, redo and the GUI's own edits).
        m.changes.description = host_.commands().canUndo() ? host_.commands().undoText() : std::string();
        m.revisionAfter = projectRevision();
    }
    AuditRecord r;
    r.requestId = c.requestId;
    r.sessionId = c.sessionId;
    r.clientName = c.clientName;
    r.source = c.source;
    r.operation = operation;
    r.resourceIds = ids(resources);
    for (const auto* v : {&m.changes.created, &m.changes.updated, &m.changes.deleted})
        for (const Id& id : *v)
            if (std::find(r.resourceIds.begin(), r.resourceIds.end(), id.str()) == r.resourceIds.end())
                r.resourceIds.push_back(id.str());
    r.revisionBefore = m.revisionBefore;
    r.revisionAfter = m.revisionAfter;
    r.dryRun = m.dryRun;
    r.durationSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    record(std::move(r));
    if (!c.idempotencyKey.empty() && !m.dryRun) {
        const std::string key = c.sessionId + '\n' + c.idempotencyKey;
        idempotency_[key] = {operation, c.sessionId, m, std::chrono::steady_clock::now()};
        idempotencyOrder_.push_back(key);
        while (idempotencyOrder_.size() > 1000) {  // retention: the last 1000 keys
            idempotency_.erase(idempotencyOrder_.front());
            idempotencyOrder_.pop_front();
        }
    }
    return m;
}

Outcome<Mutation> Session::applyCopy(const CallContext& context, const std::string& operation, const Project& changed, const std::string& description)
{
    return mutate(context, operation, {}, std::nullopt, [&changed, description](Project& target) -> Outcome<std::unique_ptr<Command>> {
        auto change = projectChangeCommand(target, changed, description);
        if (!change)
            return invalid(change.message());
        std::unique_ptr<Command> command = std::move(change.value().command);
        return command;
    });
}

Outcome<std::string> Session::undo(const CallContext& context)
{
    const auto started = std::chrono::steady_clock::now();
    auto authorized = authorize(context, Permission::Modify);
    if (!authorized) {
        refuse(context, "ProjectService.Undo", {}, authorized.error(), started);
        return authorized.error();
    }
    if (!host_.commands().canUndo())
        return ServiceError::make(ErrorCode::FailedPrecondition, "there is nothing to undo");
    const std::uint64_t before = projectRevision();
    const std::string text = host_.commands().undoText();
    current_ = &authorized.value();
    const Status s = host_.commands().undo();
    current_ = nullptr;
    if (!s)
        return ServiceError::make(ErrorCode::Internal, s.message());
    AuditRecord r;
    r.requestId = authorized.value().requestId;
    r.sessionId = authorized.value().sessionId;
    r.clientName = authorized.value().clientName;
    r.source = authorized.value().source;
    r.operation = "ProjectService.Undo";
    r.revisionBefore = before;
    r.revisionAfter = projectRevision();
    record(std::move(r));
    return text;
}

Outcome<std::string> Session::redo(const CallContext& context)
{
    const auto started = std::chrono::steady_clock::now();
    auto authorized = authorize(context, Permission::Modify);
    if (!authorized) {
        refuse(context, "ProjectService.Redo", {}, authorized.error(), started);
        return authorized.error();
    }
    if (!host_.commands().canRedo())
        return ServiceError::make(ErrorCode::FailedPrecondition, "there is nothing to redo");
    const std::uint64_t before = projectRevision();
    const std::string text = host_.commands().redoText();
    current_ = &authorized.value();
    const Status s = host_.commands().redo();
    current_ = nullptr;
    if (!s)
        return ServiceError::make(ErrorCode::Internal, s.message());
    AuditRecord r;
    r.requestId = authorized.value().requestId;
    r.sessionId = authorized.value().sessionId;
    r.clientName = authorized.value().clientName;
    r.source = authorized.value().source;
    r.operation = "ProjectService.Redo";
    r.revisionBefore = before;
    r.revisionAfter = projectRevision();
    record(std::move(r));
    return text;
}

// --- audit ---------------------------------------------------------------------------

void Session::record(AuditRecord r)
{
    r.sequence = auditNext_++;
    r.time = std::chrono::system_clock::now();
    audit_.push_back(std::move(r));
    while (audit_.size() > 10000)
        audit_.pop_front();
}

std::vector<AuditRecord> Session::audit(const AuditQuery& q) const
{
    std::vector<AuditRecord> out;
    for (const AuditRecord& r : audit_) {
        if (r.sequence <= q.afterSequence || (q.failuresOnly && r.succeeded))
            continue;
        if (!q.sessionId.empty() && r.sessionId != q.sessionId)
            continue;
        if (!q.operation.empty() && r.operation != q.operation)
            continue;
        if (!q.resourceId.empty() && std::find(r.resourceIds.begin(), r.resourceIds.end(), q.resourceId) == r.resourceIds.end())
            continue;
        out.push_back(r);
        if (out.size() >= std::min<std::size_t>(q.limit == 0 ? 1000 : q.limit, 1000))
            break;
    }
    return out;
}

std::vector<ServiceError> Session::recentErrors() const
{
    std::vector<ServiceError> out;
    for (auto it = audit_.rbegin(); it != audit_.rend() && out.size() < 20; ++it)
        if (it->error)
            out.push_back(*it->error);
    return out;
}

std::string Session::exportAudit() const
{
    std::string out;
    for (const AuditRecord& r : audit_) {
        nlohmann::json j = {{"sequence", r.sequence},
                            {"time", std::chrono::duration_cast<std::chrono::milliseconds>(r.time.time_since_epoch()).count()},
                            {"requestId", r.requestId},
                            {"sessionId", r.sessionId},
                            {"client", r.clientName},
                            {"interface", std::string(toString(r.source))},
                            {"operation", r.operation},
                            {"resources", r.resourceIds},
                            {"revisionBefore", r.revisionBefore},
                            {"revisionAfter", r.revisionAfter},
                            {"succeeded", r.succeeded},
                            {"dryRun", r.dryRun},
                            {"seconds", r.durationSeconds}};
        if (r.error)
            j["error"] = {{"code", std::string(toString(r.error->code))}, {"message", r.error->message}};
        out += j.dump() + "\n";
    }
    return out;
}

// --- results and jobs ------------------------------------------------------------------

std::string Session::addResult(ResultEntry entry)
{
    entry.id = "R-" + Id::generate().str();
    entry.created = std::chrono::system_clock::now();
    const std::string id = entry.id;
    Event e;
    e.category = EventCategory::Result;
    e.type = "result.created";
    e.resources = {{"result", id}, {"job", entry.jobId}};
    e.projectRevision = revision_;
    e.payload = {{"resultId", id}, {"kind", entry.kind}, {"jobId", entry.jobId}, {"name", entry.name}};
    results_.push_back(std::move(entry));
    publish(std::move(e));
    return id;
}

const ResultEntry* Session::result(const std::string& id) const
{
    for (const ResultEntry& r : results_)
        if (r.id == id)
            return &r;
    return nullptr;
}

std::vector<const ResultEntry*> Session::results() const
{
    std::vector<const ResultEntry*> out;
    for (const ResultEntry& r : results_)
        out.push_back(&r);
    return out;
}

void Session::trackJob(JobRecord record)
{
    std::lock_guard lock(jobMutex_);
    jobs_[record.jobId] = std::move(record);
}

const JobRecord* Session::jobRecord(const std::string& jobId) const
{
    std::lock_guard lock(jobMutex_);
    const auto it = jobs_.find(jobId);
    return it == jobs_.end() ? nullptr : &it->second;
}

std::set<std::string> Session::cancelRequested() const
{
    std::lock_guard lock(jobMutex_);
    return cancelRequested_;
}

void Session::markCancelRequested(const std::string& jobId)
{
    std::lock_guard lock(jobMutex_);
    cancelRequested_.insert(jobId);
}

void Session::collectFinishedJobs()
{
    std::vector<std::string> open;
    {
        std::lock_guard lock(jobMutex_);
        for (const auto& [id, record] : jobs_)
            if (!record.handled)
                open.push_back(id);
    }
    for (const std::string& id : open) {
        const auto info = host_.jobs().info(Id(id));
        if (!info || !isFinished(info->state))
            continue;
        std::function<void(Session&, const JobInfo&)> handler;
        {
            std::lock_guard lock(jobMutex_);
            JobRecord& record = jobs_[id];
            record.handled = true;
            handler = record.onFinished;
        }
        if (handler)
            handler(*this, *info);
    }
}

}  // namespace resonant::services
