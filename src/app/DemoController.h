// Demo Studio's engine (RES-DEMO-001..010, ADR-000215). It runs a
// demonstration sequence step by step with the application's own commands
// (open a project, show a panel, select an object, set the camera, run a
// simulation through its panel, apply a workflow action, run Python, open a
// help topic, capture a screenshot, pause for narration) and waits for the
// application's events, never for a fixed time: a run step waits for the jobs
// it started to finish, a Python step for the worker's reply, a narration
// pause for the presenter. A failed simulation fails its step (the previous
// result is never shown as if it were new). Every run is recorded in a
// manifest (resonant.demo-run).
//
// Demonstration infrastructure only: no physics here, and no result that the
// application did not calculate in the step.
#pragma once

#include "demo/Demo.h"
#include "foundation/Id.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <chrono>
#include <optional>

class MainWindow;

class DemoController : public QObject {
    Q_OBJECT
public:
    explicit DemoController(MainWindow& window, QObject* parent = nullptr);
    ~DemoController() override;

    QString resourceRoot() const { return root_; }
    void setResourceRoot(const QString& root);
    const resonant::demo::DemoLibrary& library() const { return library_; }
    void reloadLibrary();
    // Screenshots and the run manifest go here (default: the user's data directory).
    QString outputDirectory() const { return outputDirectory_; }
    void setOutputDirectory(const QString& directory) { outputDirectory_ = directory; }

    // attended: narration pauses wait for next(); unattended runs pass them.
    bool startProfile(const QString& id, bool attended = false);
    bool start(const resonant::demo::DemoSequence& sequence, const resonant::demo::DemoProfile* profile = nullptr, bool attended = false);
    void next();
    void pause();   // stops after the running step
    void resume();
    void stop();    // cancels: the running step's jobs are cancelled, the run is recorded as cancelled
    bool restart();

    bool isRunning() const { return running_; }
    bool isPaused() const { return paused_; }
    bool isWaitingForPresenter() const { return wait_ == Wait::Presenter; }
    int currentStep() const { return index_; }
    int stepCount() const { return static_cast<int>(sequence_.steps.size()); }
    const resonant::demo::DemoSequence& sequence() const { return sequence_; }
    const resonant::demo::DemoProfile* profile() const { return profile_ ? &*profile_ : nullptr; }
    const resonant::demo::DemoRun& lastRun() const { return run_; }
    QString manifestPath() const { return manifestPath_; }
    bool exportManifest(const QString& path) const;

    // Where a project path of a step resolves (relative to the resource root).
    QString resolve(const QString& path) const;

signals:
    void runStarted();
    void stepStarted(int index);
    void stepFinished(int index, bool ok, const QString& message);
    void waitingForPresenter(int index, const QString& text);
    void runFinished(const QString& status, const QString& message);
    void stateChanged();

private:
    enum class Wait { None, Jobs, Python, Presenter };
    void scheduleStep();
    void runStep();
    void finishStep(bool ok, const QString& message, nlohmann::json result = nlohmann::json::object());
    void finishRun(const QString& status, const QString& message);
    void clearWait();
    void startJobWait(size_t jobsBefore, const std::function<nlohmann::json()>& collect, const std::function<QString()>& failureText);
    void checkJobs();

    resonant::Status doOpenProject(const nlohmann::json& p, nlohmann::json& result);
    resonant::Status doWorkspace(const nlohmann::json& p);
    resonant::Status doSelect(const nlohmann::json& p);
    resonant::Status doCamera(const nlohmann::json& p);
    resonant::Status doVisualization(const nlohmann::json& p);
    resonant::Status doShow(const nlohmann::json& p);
    resonant::Status doCapture(const nlohmann::json& p, const resonant::demo::DemoStep& step, nlohmann::json& result);
    resonant::Status doProjectAction(const nlohmann::json& p, nlohmann::json& result);
    resonant::Status doPromote(const nlohmann::json& p, nlohmann::json& result);
    resonant::Status doExperimentData(const nlohmann::json& p, nlohmann::json& result);
    bool beginRun(const nlohmann::json& p, QString& error);
    bool beginWorkflow(const nlohmann::json& p, QString& error);
    bool beginPython(const nlohmann::json& p, QString& error);
    resonant::Status checkExpect(const nlohmann::json& expect, nlohmann::json& result);

    MainWindow& window_;
    QString root_;
    QString outputDirectory_;
    resonant::demo::DemoLibrary library_;
    resonant::demo::DemoSequence sequence_;
    std::optional<resonant::demo::DemoProfile> profile_;
    resonant::demo::DemoRun run_;
    QString manifestPath_;
    QString runDirectory_;
    bool running_ = false;
    bool paused_ = false;
    bool attended_ = false;
    bool stopRequested_ = false;
    int index_ = -1;
    Wait wait_ = Wait::None;
    std::chrono::steady_clock::time_point stepStart_;
    QTimer timeout_;
    QList<QMetaObject::Connection> connections_;
    QSet<resonant::Id> jobs_;
    QString jobFailure_;
    bool needSignal_ = false;   // the run's controller must also report completion
    bool signalSeen_ = false;
    std::function<nlohmann::json()> collect_;
    std::function<QString()> failureText_;
    int pythonRequest_ = 0;
    size_t consoleMark_ = 0;
};
