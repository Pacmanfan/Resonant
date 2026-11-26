// The application's document: one project, its undo history, the selection
// and the background jobs. Every panel and the viewport talk to the project
// through this controller; it is the only place that turns user actions
// into core commands, and the only source of change notifications.
#pragma once

#include "commands/Command.h"
#include "project/Project.h"
#include "simulation/Job.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class DocumentController : public QObject {
    Q_OBJECT
public:
    explicit DocumentController(QObject* parent = nullptr);
    ~DocumentController() override;

    resonant::Project& project() { return *project_; }
    const resonant::Project& project() const { return *project_; }
    const resonant::Scene& scene() const { return project_->scene; }
    resonant::CommandStack& commands() { return commands_; }
    resonant::JobManager& jobs() { return *jobs_; }

    // --- document lifecycle -------------------------------------------------
    QString filePath() const { return filePath_; }
    bool isModified() const { return !commands_.isClean(); }
    // "Untitled" or the file name, for the window title.
    QString displayName() const;

    void newProject();
    // On failure the current project is left untouched.
    resonant::Status openProject(const QString& path, QStringList* warnings = nullptr);
    resonant::Status saveProject();  // to filePath(); fails when there is none
    resonant::Status saveProjectAs(const QString& path);

    // --- selection ----------------------------------------------------------
    const QList<resonant::Id>& selection() const { return selection_; }
    resonant::Id primarySelection() const { return selection_.isEmpty() ? resonant::Id() : selection_.last(); }
    bool isSelected(const resonant::Id& id) const { return selection_.contains(id); }
    void setSelection(const QList<resonant::Id>& ids);
    void select(const resonant::Id& id, bool toggle = false);
    void clearSelection() { setSelection({}); }
    void selectAll();

    // --- undoable operations -------------------------------------------------
    // Each returns a failed Status with a message for the user when the
    // operation cannot be done; nothing is changed in that case.
    resonant::Result<resonant::Id> createPrimitive(const QString& kindId);
    resonant::Result<resonant::Id> createGroup();
    // Inserts a prepared object (used by domain modules for their objects).
    resonant::Result<resonant::Id> insertObject(std::unique_ptr<resonant::SceneObject> object,
                                                const resonant::Id& parentId = resonant::Id(),
                                                const QString& verb = QStringLiteral("Add"));
    resonant::Status deleteSelection();
    resonant::Status duplicateSelection();
    resonant::Status setObjectState(const resonant::Id& id, resonant::ObjectState state, const QString& description);
    resonant::Status renameObject(const resonant::Id& id, const QString& name);
    resonant::Status setTransform(const resonant::Id& id, const resonant::Transform& transform, const QString& verb);
    resonant::Status setGeometryParameter(const resonant::Id& id, const std::string& key, double value);
    resonant::Status setMaterial(const resonant::Id& id, const resonant::Id& materialId);
    resonant::Status setVisible(const resonant::Id& id, bool visible);
    resonant::Status moveObject(const resonant::Id& id, const resonant::Id& newParentId);
    resonant::Status setMaterialLibrary(resonant::MaterialLibrary library, const QString& description);
    resonant::Status pushCommand(std::unique_ptr<resonant::Command> command);
    void undo();
    void redo();

    // --- jobs -----------------------------------------------------------------
    resonant::Id submitJob(std::shared_ptr<resonant::Job> job);
    resonant::Id startDemonstratorJob();
    void cancelJob(const resonant::Id& id);
    void cancelAllJobs();

signals:
    void projectReplaced();                                   // new / open: everything changed
    void sceneChanged(const resonant::SceneEvent& event);     // one scene edit
    void selectionChanged();
    void materialsChanged();
    void documentStateChanged();                              // modified flag, title, undo / redo availability
    void jobChanged(const resonant::JobInfo& info);           // delivered on the user-interface thread
    void operationFailed(const QString& message);             // an undo / redo or command that failed

private:
    void installProject(std::unique_ptr<resonant::Project> project, const QString& path);
    void attachSceneListener();
    void pruneSelection();

    std::unique_ptr<resonant::Project> project_;
    resonant::CommandStack commands_;
    std::unique_ptr<resonant::JobManager> jobs_;
    QList<resonant::Id> selection_;
    QString filePath_;
    int sceneListener_ = 0;
    int jobListener_ = 0;
};
