#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "services/Services.h"

#include <filesystem>

namespace resonant::services {

std::string objectTypeOf(const SceneObject& o)
{
    if (o.state.component)
        return o.state.component->typeId();
    if (o.state.geometry)
        return "geometry";
    return "group";
}

Services::Services(Session& s)
    : project(s), scene(s), geometry(s), materials(s), emitters(s), arrays(s), simulation(s), jobs(s), results(s), optimization(s), workflows(s),
      transactions(s), multiphysics(s), functional(s), resonance(s), multiscale(s), materialState(s), accounting(s), laboratory(s), application(s), session_(s)
{
}

namespace {
bool trusted(const CallContext& c)
{
    return c.source == Interface::Gui || c.source == Interface::Python || c.source == Interface::Internal;
}

void audit(Session& s, const CallContext& c, const std::string& operation, std::uint64_t before, const std::optional<ServiceError>& error = {})
{
    AuditRecord r;
    r.requestId = c.requestId;
    r.sessionId = c.sessionId;
    r.clientName = c.clientName;
    r.source = c.source;
    r.operation = operation;
    r.resourceIds = {s.project().id.str()};
    r.revisionBefore = before;
    r.revisionAfter = s.projectRevision();
    r.succeeded = !error;
    r.error = error;
    s.record(std::move(r));
}
}  // namespace

ProjectView ProjectService::view() const
{
    const Project& p = s_.project();
    ProjectView v;
    v.id = p.id.str();
    v.name = p.name;
    v.fileName = std::filesystem::path(s_.host().filePath()).filename().string();
    v.revision = s_.projectRevision();
    const CommandStack& commands = s_.host().commands();
    v.modified = !commands.isClean();
    v.objectCount = static_cast<int>(p.scene.objectCount());
    v.materialCount = static_cast<int>(p.materials.materials().size());
    v.canUndo = commands.canUndo();
    v.canRedo = commands.canRedo();
    v.undoText = v.canUndo ? commands.undoText() : std::string();
    v.redoText = v.canRedo ? commands.redoText() : std::string();
    v.createdUtc = p.createdUtc;
    v.modifiedUtc = p.modifiedUtc;
    return v;
}

Outcome<ProjectView> ProjectService::get(const CallContext& c)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    return view();
}

Outcome<std::string> ProjectService::resolve(const CallContext& c, const std::string& file) const
{
    if (trusted(c))
        return file;  // the GUI's and Python's own paths
    const std::filesystem::path p(file);
    if (file.empty() || p.is_absolute() || file.find("..") != std::string::npos)
        return invalid("a project file is named relative to the project directory (no absolute path, no '..')", "file_name");
    if (directory_.empty())
        return ServiceError::make(ErrorCode::PermissionDenied, "this server has no project directory: remote file access is off");
    return (std::filesystem::path(directory_) / p).string();
}

Outcome<ProjectView> ProjectService::create(const CallContext& c, const std::string& name, bool discardUnsaved)
{
    auto a = s_.authorize(c, Permission::Modify);
    if (!a)
        return a.error();
    const std::uint64_t before = s_.projectRevision();
    if (!discardUnsaved && !s_.host().commands().isClean()) {
        ServiceError e = ServiceError::make(ErrorCode::FailedPrecondition, "the open project has unsaved changes (save it, or set discard_unsaved_changes)");
        audit(s_, a.value(), "ProjectService.CreateProject", before, e);
        return e;
    }
    s_.host().installProject(Project::createNew(name.empty() ? "Untitled" : name), "");
    audit(s_, a.value(), "ProjectService.CreateProject", before);
    return view();
}

Outcome<ProjectView> ProjectService::open(const CallContext& c, const std::string& file, bool discardUnsaved, std::vector<std::string>* warnings)
{
    auto a = s_.authorize(c, Permission::Files);
    if (!a)
        return a.error();
    if (!has(a.value().permissions, Permission::Modify))
        return ServiceError::make(ErrorCode::PermissionDenied, "opening a project needs the modify permission too");
    const std::uint64_t before = s_.projectRevision();
    auto path = resolve(a.value(), file);
    if (!path)
        return path.error();
    if (!discardUnsaved && !s_.host().commands().isClean())
        return ServiceError::make(ErrorCode::FailedPrecondition, "the open project has unsaved changes (save it, or set discard_unsaved_changes)");
    auto loaded = loadProjectFile(path.value());
    if (!loaded) {
        ServiceError e = ServiceError::make(ErrorCode::NotFound, loaded.message());
        audit(s_, a.value(), "ProjectService.OpenProject", before, e);
        return e;
    }
    if (warnings)
        *warnings = loaded.value().warnings;
    s_.host().installProject(std::move(loaded.value().project), path.value());
    audit(s_, a.value(), "ProjectService.OpenProject", before);
    return view();
}

Outcome<ProjectView> ProjectService::save(const CallContext& c, const std::string& file)
{
    auto a = s_.authorize(c, Permission::Files);
    if (!a)
        return a.error();
    const std::uint64_t before = s_.projectRevision();
    std::string path = s_.host().filePath();
    if (!file.empty()) {
        auto resolved = resolve(a.value(), file);
        if (!resolved)
            return resolved.error();
        path = resolved.value();
    }
    if (path.empty())
        return ServiceError::make(ErrorCode::FailedPrecondition, "the project has no file yet: give a file name");
    Project& p = s_.project();
    const std::string previousName = p.name;
    if (path != s_.host().filePath())
        p.name = std::filesystem::path(path).stem().string();
    if (Status st = saveProjectFile(p, path); !st) {
        p.name = previousName;
        ServiceError e = ServiceError::make(ErrorCode::Unavailable, st.message());
        audit(s_, a.value(), "ProjectService.SaveProject", before, e);
        return e;
    }
    s_.host().setFilePath(path);
    s_.host().commands().markClean();
    Event e;
    e.category = EventCategory::Project;
    e.type = "project.saved";
    e.sessionId = a.value().sessionId;
    e.clientName = a.value().clientName;
    e.source = a.value().source;
    e.requestId = a.value().requestId;
    e.resources.push_back({"project", p.id.str()});
    e.projectRevision = s_.projectRevision();
    e.payload = {{"projectId", p.id.str()}, {"projectName", p.name}, {"fileName", std::filesystem::path(path).filename().string()}};
    s_.publish(std::move(e));
    audit(s_, a.value(), "ProjectService.SaveProject", before);
    return view();
}

Outcome<ValidationView> ProjectService::validate(const CallContext& c)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    ValidationView v;
    const Project& p = s_.project();
    p.scene.forEach([&](const SceneObject& o) {
        if (o.state.geometry)
            if (Status st = o.state.geometry->validate(); !st)
                v.issues.push_back({3, "geometry", "'" + o.state.name + "': " + st.message(), o.id().str()});
        if (!o.state.materialId.isNull() && !p.materials.find(o.state.materialId))
            v.issues.push_back({3, "material_id", "'" + o.state.name + "' refers to a material that is not in the library", o.id().str()});
        if (!o.state.local.position.allFinite() || !o.state.local.rotation.allFinite() || (o.state.local.scale.array() <= 0.0).any())
            v.issues.push_back({3, "local_transform", "'" + o.state.name + "' has an invalid transform", o.id().str()});
    });
    for (const Material& m : p.materials.materials())
        if (Status st = m.validate(); !st)
            v.issues.push_back({3, "material", "'" + m.name() + "': " + st.message(), m.id().str()});
    v.valid = v.issues.empty();
    return v;
}

Outcome<std::string> ProjectService::exportJson(const CallContext& c)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    return serializeProject(s_.project()).dump(2);
}

}  // namespace resonant::services
