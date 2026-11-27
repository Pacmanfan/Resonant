#include "DocumentController.h"

#include "commands/SceneCommands.h"
#include "foundation/Log.h"
#include "project/ProjectSerializer.h"
#include "services/Services.h"

#include <QFileInfo>

using namespace resonant;

// The document as the services' project host.
class DocumentHost final : public services::ProjectHost {
public:
    explicit DocumentHost(DocumentController& d) : d_(d) {}
    Project& project() override { return d_.project(); }
    CommandStack& commands() override { return d_.commands(); }
    JobManager& jobs() override { return d_.jobs(); }
    std::string filePath() const override { return d_.filePath().toStdString(); }
    void setFilePath(const std::string& path) override { d_.filePath_ = QString::fromStdString(path); }
    void installProject(std::unique_ptr<Project> project, const std::string& path) override
    {
        d_.installProject(std::move(project), QString::fromStdString(path), path.empty() ? "project.created" : "project.opened");
    }

private:
    DocumentController& d_;
};

DocumentController::DocumentController(QObject* parent)
    : QObject(parent), project_(Project::createNew()), jobs_(std::make_unique<JobManager>(1))
{
    attachSceneListener();
    commands_.addListener([this] { emit documentStateChanged(); });
    // Job events arrive on worker threads; hand them to this object's thread.
    jobListener_ = jobs_->addListener([this](const JobInfo& info) {
        QMetaObject::invokeMethod(this, [this, info] { emit jobChanged(info); }, Qt::QueuedConnection);
    });
    host_ = std::make_unique<DocumentHost>(*this);
    session_ = std::make_unique<services::Session>(*host_, "gui");
    // Every material change (any interface, undo and redo) reaches the panels; finished jobs are collected.
    eventListener_ = session_->events().subscribe([this](const services::Event& e) {
        if (e.category == services::EventCategory::Material)
            QMetaObject::invokeMethod(this, [this] { emit materialsChanged(); }, Qt::AutoConnection);
    });
    connect(this, &DocumentController::jobChanged, this, [this](const JobInfo& info) {
        if (isFinished(info.state))
            session_->collectFinishedJobs();
    });
}

DocumentController::~DocumentController()
{
    session_->events().unsubscribe(eventListener_);
    session_.reset();  // detaches from the project, history and jobs first
    host_.reset();
    jobs_->removeListener(jobListener_);
    // Stop the workers before the project they may be reading goes away.
    jobs_.reset();
}

services::Services& DocumentController::services()
{
    return session_->services();
}

services::CallContext DocumentController::guiContext()
{
    services::CallContext c;
    c.source = services::Interface::Gui;
    c.clientName = "Resonant";
    return c;
}

namespace {
template <class T>
Status statusOf(const services::Outcome<T>& outcome)
{
    return outcome ? Status::success() : Status::failure(outcome.error().message);
}
}  // namespace

QString DocumentController::displayName() const
{
    return filePath_.isEmpty() ? QString::fromStdString(project_->name) : QFileInfo(filePath_).fileName();
}

void DocumentController::attachSceneListener()
{
    sceneListener_ = project_->scene.addListener([this](const SceneEvent& event) {
        if (event.kind == SceneEvent::Kind::Removed || event.kind == SceneEvent::Kind::Reset)
            pruneSelection();
        emit sceneChanged(event);
    });
}

void DocumentController::pruneSelection()
{
    const qsizetype before = selection_.size();
    selection_.removeIf([this](const Id& id) { return project_->scene.find(id) == nullptr; });
    if (selection_.size() != before)
        emit selectionChanged();
}

void DocumentController::installProject(std::unique_ptr<Project> project, const QString& path, const char* reason)
{
    selection_.clear();
    project_ = std::move(project);
    filePath_ = path;
    commands_.clear();
    attachSceneListener();
    if (session_)
        session_->projectReplaced(reason);
    emit projectReplaced();
    emit selectionChanged();
    emit materialsChanged();
    emit documentStateChanged();
}

void DocumentController::newProject()
{
    installProject(Project::createNew(), QString(), "project.created");
    logInfo("project", "new project created");
}

Status DocumentController::openProject(const QString& path, QStringList* warnings)
{
    auto loaded = loadProjectFile(path.toStdString());
    if (!loaded)
        return Status::failure(loaded.message());
    if (warnings)
        for (const std::string& warning : loaded.value().warnings)
            warnings->append(QString::fromStdString(warning));
    installProject(std::move(loaded.value().project), path);
    return Status::success();
}

Status DocumentController::saveProject()
{
    if (filePath_.isEmpty())
        return Status::failure("the project has not been saved yet; choose a file name with Save As");
    return saveProjectAs(filePath_);
}

Status DocumentController::saveProjectAs(const QString& path)
{
    // The service names the project after a new file, saves, sets the path and marks the history clean.
    // The GUI may save anywhere the user chooses (the application session holds the files permission).
    return statusOf(services().project.save(guiContext(), QFileInfo(path).absoluteFilePath().toStdString()));
}

void DocumentController::setSelection(const QList<Id>& ids)
{
    QList<Id> valid;
    for (const Id& id : ids)
        if (project_->scene.find(id) && !valid.contains(id))
            valid.append(id);
    if (valid == selection_)
        return;
    selection_ = valid;
    emit selectionChanged();
}

void DocumentController::select(const Id& id, bool toggle)
{
    if (!toggle) {
        setSelection(id.isNull() ? QList<Id>{} : QList<Id>{id});
        return;
    }
    QList<Id> ids = selection_;
    if (ids.contains(id))
        ids.removeAll(id);
    else
        ids.append(id);
    setSelection(ids);
}

void DocumentController::selectAll()
{
    QList<Id> ids;
    project_->scene.forEach([&ids](const SceneObject& object) { ids.append(object.id()); });
    setSelection(ids);
}

Status DocumentController::pushCommand(std::unique_ptr<Command> command)
{
    Status status = commands_.push(std::move(command));
    if (!status)
        logWarning("scene", "operation refused: " + status.message());
    return status;
}

Result<Id> DocumentController::insertObject(std::unique_ptr<SceneObject> object, const Id& parentId, const QString& verb)
{
    auto command = std::make_unique<InsertObjectCommand>(project_->scene, std::move(object), parentId, -1,
                                                         verb.toStdString());
    const Id id = command->objectId();
    const std::string description = command->description();
    if (Status status = pushCommand(std::move(command)); !status)
        return Result<Id>::failure(status.message());
    logInfo("scene", description);
    select(id);
    return id;
}

Result<Id> DocumentController::createPrimitive(const QString& kindId)
{
    services::GeometryService::Create create;
    create.kind = kindId.toStdString();
    auto m = services().geometry.createPrimitive(guiContext(), create);
    if (!m)
        return Result<Id>::failure(m.error().message);
    const Id id = m.value().changes.created.front();
    logInfo("scene", m.value().changes.description);
    select(id);
    return id;
}

Result<Id> DocumentController::createGroup()
{
    return insertObject(makeGroup(project_->scene.uniqueName("Group")));
}

Status DocumentController::deleteSelection()
{
    if (selection_.isEmpty())
        return Status::failure("nothing is selected");
    // The service deletes descendants with their ancestors (a selected child of a selected parent is covered).
    const std::vector<Id> ids(selection_.begin(), selection_.end());
    auto m = services().scene.remove(guiContext(), ids);
    if (m)
        logInfo("scene", m.value().changes.description);
    return statusOf(m);
}

Status DocumentController::duplicateSelection()
{
    if (selection_.isEmpty())
        return Status::failure("nothing is selected");
    const std::vector<Id> ids(selection_.begin(), selection_.end());
    auto m = services().scene.duplicate(guiContext(), ids);
    if (!m)
        return Status::failure(m.error().message);
    logInfo("scene", m.value().changes.description);
    // The copies are the top-level objects created (their children were copied with them).
    QList<Id> copies;
    for (const Id& id : m.value().changes.created) {
        const SceneObject* copy = project_->scene.find(id);
        const SceneObject* parent = copy ? copy->parent() : nullptr;
        const bool nested = parent && !project_->scene.isRoot(*parent) &&
                            std::find(m.value().changes.created.begin(), m.value().changes.created.end(), parent->id()) != m.value().changes.created.end();
        if (copy && !nested)
            copies.append(id);
    }
    setSelection(copies);
    return Status::success();
}

Status DocumentController::setObjectState(const Id& id, ObjectState state, const QString& description)
{
    // A whole state from a domain panel (its component settings): applied as one undo step.
    const SceneObject* object = project_->scene.find(id);
    if (!object)
        return Status::failure("the object no longer exists");
    if (object->state == state)
        return Status::success();  // nothing to do, and nothing to undo
    return pushCommand(
        std::make_unique<SetObjectStateCommand>(project_->scene, id, std::move(state), description.toStdString()));
}

namespace {
QString nameOf(const Scene& scene, const Id& id)
{
    const SceneObject* o = scene.find(id);
    return o ? QString::fromStdString(o->state.name) : QString();
}
}  // namespace

Status DocumentController::renameObject(const Id& id, const QString& name)
{
    if (!project_->scene.find(id))
        return Status::failure("the object no longer exists");
    services::SceneService::Update u;
    u.name = name.trimmed().toStdString();
    return statusOf(services().scene.update(guiContext(), id, std::nullopt, u));
}

Status DocumentController::setTransform(const Id& id, const Transform& transform, const QString& verb)
{
    if (!project_->scene.find(id))
        return Status::failure("the object no longer exists");
    services::SceneService::Update u;
    u.transform = transform;
    u.description = (verb + QLatin1Char(' ') + nameOf(project_->scene, id)).toStdString();
    return statusOf(services().scene.update(guiContext(), id, std::nullopt, u));
}

Status DocumentController::setGeometryParameter(const Id& id, const std::string& key, double value)
{
    const SceneObject* object = project_->scene.find(id);
    if (!object || !object->state.geometry)
        return Status::failure("the object has no geometry");
    return statusOf(services().geometry.updateParameters(guiContext(), id, std::nullopt, {{key, value}}));
}

Status DocumentController::setMaterial(const Id& id, const Id& materialId)
{
    if (!project_->scene.find(id))
        return Status::failure("the object no longer exists");
    if (!materialId.isNull() && !project_->materials.find(materialId))
        return Status::failure("the material is not in the project's library");
    return statusOf(services().materials.assign(guiContext(), materialId, {id}));
}

Status DocumentController::setVisible(const Id& id, bool visible)
{
    if (!project_->scene.find(id))
        return Status::failure("the object no longer exists");
    services::SceneService::Update u;
    u.visible = visible;
    u.description = ((visible ? QStringLiteral("Show ") : QStringLiteral("Hide ")) + nameOf(project_->scene, id)).toStdString();
    return statusOf(services().scene.update(guiContext(), id, std::nullopt, u));
}

Status DocumentController::moveObject(const Id& id, const Id& newParentId)
{
    if (!project_->scene.find(id))
        return Status::failure("the object no longer exists");
    services::SceneService::Update u;
    u.parent = newParentId.isNull() ? std::string() : newParentId.str();
    u.description = ("Move " + nameOf(project_->scene, id)).toStdString();
    return statusOf(services().scene.update(guiContext(), id, std::nullopt, u));
}

Status DocumentController::setMaterialLibrary(MaterialLibrary library, const QString& description)
{
    // References to removed materials are cleared in the same undo step (the service does it);
    // the panels hear of it through the session's material.changed event.
    return statusOf(services().materials.replaceLibrary(guiContext(), library, description.toStdString()));
}

void DocumentController::undo()
{
    if (!commands_.canUndo())
        return;
    const std::string text = commands_.undoText();
    if (auto undone = services().project.undo(guiContext()); !undone) {
        logError("scene", "undo failed: " + undone.error().message);
        emit operationFailed(QStringLiteral("Undo failed: ") + QString::fromStdString(undone.error().message));
        return;
    }
    logInfo("scene", "undo: " + text);
}

void DocumentController::redo()
{
    if (!commands_.canRedo())
        return;
    const std::string text = commands_.redoText();
    if (auto redone = services().project.redo(guiContext()); !redone) {
        logError("scene", "redo failed: " + redone.error().message);
        emit operationFailed(QStringLiteral("Redo failed: ") + QString::fromStdString(redone.error().message));
        return;
    }
    logInfo("scene", "redo: " + text);
}

Id DocumentController::submitJob(std::shared_ptr<Job> job)
{
    return jobs_->submit(std::move(job));
}

Id DocumentController::startDemonstratorJob()
{
    return submitJob(std::make_shared<DemonstratorJob>(50, std::chrono::milliseconds(100)));
}

void DocumentController::cancelJob(const Id& id)
{
    jobs_->cancel(id);
}

void DocumentController::cancelAllJobs()
{
    for (const JobInfo& info : jobs_->jobs())
        if (!isFinished(info.state))
            jobs_->cancel(info.id);
}
