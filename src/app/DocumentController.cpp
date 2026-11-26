#include "DocumentController.h"

#include "commands/SceneCommands.h"
#include "foundation/Log.h"
#include "project/ProjectSerializer.h"

#include <QFileInfo>

using namespace resonant;

DocumentController::DocumentController(QObject* parent)
    : QObject(parent), project_(Project::createNew()), jobs_(std::make_unique<JobManager>(1))
{
    attachSceneListener();
    commands_.addListener([this] { emit documentStateChanged(); });
    // Job events arrive on worker threads; hand them to this object's thread.
    jobListener_ = jobs_->addListener([this](const JobInfo& info) {
        QMetaObject::invokeMethod(this, [this, info] { emit jobChanged(info); }, Qt::QueuedConnection);
    });
}

DocumentController::~DocumentController()
{
    jobs_->removeListener(jobListener_);
    // Stop the workers before the project they may be reading goes away.
    jobs_.reset();
}

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

void DocumentController::installProject(std::unique_ptr<Project> project, const QString& path)
{
    selection_.clear();
    project_ = std::move(project);
    filePath_ = path;
    commands_.clear();
    attachSceneListener();
    emit projectReplaced();
    emit selectionChanged();
    emit materialsChanged();
    emit documentStateChanged();
}

void DocumentController::newProject()
{
    installProject(Project::createNew(), QString());
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
    const std::string previousName = project_->name;
    if (filePath_ != path)
        project_->name = QFileInfo(path).completeBaseName().toStdString();
    if (Status status = saveProjectFile(*project_, path.toStdString()); !status) {
        project_->name = previousName;
        return status;
    }
    filePath_ = path;
    commands_.markClean();  // also emits documentStateChanged
    return Status::success();
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
    auto geometry = Geometry::create(kindId.toStdString());
    if (!geometry)
        return Result<Id>::failure(geometry.message());
    const std::string name = project_->scene.uniqueName(geometry.value().kind().displayName);
    return insertObject(makeGeometryObject(name, geometry.take()));
}

Result<Id> DocumentController::createGroup()
{
    return insertObject(makeGroup(project_->scene.uniqueName("Group")));
}

Status DocumentController::deleteSelection()
{
    if (selection_.isEmpty())
        return Status::failure("nothing is selected");
    // Deleting an object deletes its descendants: skip selected objects
    // whose ancestor is selected too.
    QList<Id> roots;
    for (const Id& id : selection_) {
        const SceneObject* object = project_->scene.find(id);
        bool covered = false;
        for (const Id& other : selection_) {
            const SceneObject* candidate = project_->scene.find(other);
            if (candidate && object && candidate != object && candidate->isAncestorOf(*object))
                covered = true;
        }
        if (object && !covered)
            roots.append(id);
    }
    auto composite = std::make_unique<CompositeCommand>(
        roots.size() == 1 ? "Delete " + project_->scene.find(roots.first())->state.name
                          : "Delete " + std::to_string(roots.size()) + " objects");
    for (const Id& id : roots)
        composite->add(std::make_unique<RemoveObjectCommand>(project_->scene, id));
    const std::string description = composite->description();
    Status status = pushCommand(std::move(composite));
    if (status)
        logInfo("scene", description);
    return status;
}

Status DocumentController::duplicateSelection()
{
    if (selection_.isEmpty())
        return Status::failure("nothing is selected");
    auto composite = std::make_unique<CompositeCommand>(
        selection_.size() == 1 ? "Duplicate " + project_->scene.find(selection_.first())->state.name
                               : "Duplicate " + std::to_string(selection_.size()) + " objects");
    QList<Id> copies;
    for (const Id& id : selection_) {
        const SceneObject* object = project_->scene.find(id);
        if (!object)
            continue;
        auto copy = object->cloneSubtree(true);
        copy->state.name = project_->scene.uniqueName(object->state.name + " copy");
        copies.append(copy->id());
        const SceneObject* parent = object->parent();
        const Id parentId = (parent && !project_->scene.isRoot(*parent)) ? parent->id() : Id();
        composite->add(std::make_unique<InsertObjectCommand>(project_->scene, std::move(copy), parentId,
                                                             object->indexInParent() + 1, "Duplicate"));
    }
    const std::string description = composite->description();
    Status status = pushCommand(std::move(composite));
    if (status) {
        logInfo("scene", description);
        setSelection(copies);
    }
    return status;
}

Status DocumentController::setObjectState(const Id& id, ObjectState state, const QString& description)
{
    const SceneObject* object = project_->scene.find(id);
    if (!object)
        return Status::failure("the object no longer exists");
    if (object->state == state)
        return Status::success();  // nothing to do, and nothing to undo
    return pushCommand(
        std::make_unique<SetObjectStateCommand>(project_->scene, id, std::move(state), description.toStdString()));
}

Status DocumentController::renameObject(const Id& id, const QString& name)
{
    const SceneObject* object = project_->scene.find(id);
    if (!object)
        return Status::failure("the object no longer exists");
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return Status::failure("an object name cannot be empty");
    ObjectState state = object->state;
    state.name = trimmed.toStdString();
    return setObjectState(id, std::move(state), QStringLiteral("Rename ") + QString::fromStdString(object->state.name));
}

Status DocumentController::setTransform(const Id& id, const Transform& transform, const QString& verb)
{
    const SceneObject* object = project_->scene.find(id);
    if (!object)
        return Status::failure("the object no longer exists");
    if (!transform.position.allFinite() || !transform.rotation.allFinite() || !transform.scale.allFinite())
        return Status::failure("transform values must be finite numbers");
    if ((transform.scale.array() <= 0.0).any())
        return Status::failure("scale factors must be greater than zero");
    ObjectState state = object->state;
    state.local = transform;
    return setObjectState(id, std::move(state), verb + QLatin1Char(' ') + QString::fromStdString(object->state.name));
}

Status DocumentController::setGeometryParameter(const Id& id, const std::string& key, double value)
{
    const SceneObject* object = project_->scene.find(id);
    if (!object || !object->state.geometry)
        return Status::failure("the object has no geometry");
    ObjectState state = object->state;
    if (Status status = state.geometry->setParameter(key, value); !status)
        return status;
    return setObjectState(id, std::move(state),
                          QStringLiteral("Edit ") + QString::fromStdString(object->state.name) + QStringLiteral(" geometry"));
}

Status DocumentController::setMaterial(const Id& id, const Id& materialId)
{
    const SceneObject* object = project_->scene.find(id);
    if (!object)
        return Status::failure("the object no longer exists");
    if (!materialId.isNull() && !project_->materials.find(materialId))
        return Status::failure("the material is not in the project's library");
    ObjectState state = object->state;
    state.materialId = materialId;
    return setObjectState(id, std::move(state),
                          QStringLiteral("Assign material to ") + QString::fromStdString(object->state.name));
}

Status DocumentController::setVisible(const Id& id, bool visible)
{
    const SceneObject* object = project_->scene.find(id);
    if (!object)
        return Status::failure("the object no longer exists");
    ObjectState state = object->state;
    state.visible = visible;
    return setObjectState(id, std::move(state),
                          (visible ? QStringLiteral("Show ") : QStringLiteral("Hide "))
                              + QString::fromStdString(object->state.name));
}

Status DocumentController::moveObject(const Id& id, const Id& newParentId)
{
    return pushCommand(std::make_unique<MoveObjectCommand>(project_->scene, id, newParentId));
}

Status DocumentController::setMaterialLibrary(MaterialLibrary library, const QString& description)
{
    if (library == project_->materials)
        return Status::success();
    // Objects may refer to a material that the edit removes: clear those
    // references in the same undo step.
    auto composite = std::make_unique<CompositeCommand>(description.toStdString());
    project_->scene.forEach([&](const SceneObject& object) {
        if (!object.state.materialId.isNull() && !library.find(object.state.materialId)) {
            ObjectState state = object.state;
            state.materialId = Id();
            composite->add(std::make_unique<SetObjectStateCommand>(project_->scene, object.id(), std::move(state),
                                                                   "Remove material"));
        }
    });
    composite->add(std::make_unique<SetMaterialLibraryCommand>(project_->materials, std::move(library),
                                                               description.toStdString(),
                                                               [this] { emit materialsChanged(); }));
    return pushCommand(std::move(composite));
}

void DocumentController::undo()
{
    if (!commands_.canUndo())
        return;
    const std::string text = commands_.undoText();
    if (Status status = commands_.undo(); !status) {
        logError("scene", "undo failed: " + status.message());
        emit operationFailed(QStringLiteral("Undo failed: ") + QString::fromStdString(status.message()));
        return;
    }
    logInfo("scene", "undo: " + text);
}

void DocumentController::redo()
{
    if (!commands_.canRedo())
        return;
    const std::string text = commands_.redoText();
    if (Status status = commands_.redo(); !status) {
        logError("scene", "redo failed: " + status.message());
        emit operationFailed(QStringLiteral("Redo failed: ") + QString::fromStdString(status.message()));
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
