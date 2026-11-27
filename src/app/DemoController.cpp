#include "DemoController.h"

#include "AcousticController.h"
#include "AcousticsPanel.h"
#include "DocumentController.h"
#include "EmController.h"
#include "EmPanel.h"
#include "ExperimentsController.h"
#include "ExperimentsPanel.h"
#include "FdtdController.h"
#include "FdtdPanel.h"
#include "FieldAnalysisPanel.h"
#include "HelpWindow.h"
#include "PluginsDialog.h"
#include "ScriptEditor.h"
#include "WorkflowsDialog.h"
#include "MainWindow.h"
#include "MechanicsController.h"
#include "MechanicsPanel.h"
#include "OptimizationController.h"
#include "OptimizationPanel.h"
#include "PythonConsole.h"
#include "PythonSession.h"
#include "ViewportWidget.h"
#include "automation/WorkflowEngine.h"
#include "coupling/WorkflowActions.h"
#include "foundation/Log.h"
#include "foundation/Time.h"
#include "foundation/Version.h"
#include "optimization/Engine.h"
#include "project/ProjectSerializer.h"

#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QStandardPaths>
#include <QStatusBar>

using namespace resonant;
using namespace resonant::demo;
using json = nlohmann::json;

namespace {
constexpr double kPi = 3.14159265358979323846;

QString qs(const std::string& s)
{
    return QString::fromStdString(s);
}

std::string str(const json& p, const char* key, const std::string& fallback = {})
{
    return p.contains(key) && p[key].is_string() ? p[key].get<std::string>() : fallback;
}

// The docks a demonstration may show, by workspace name.
const QHash<QString, QString>& dockNames()
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("scene"), QStringLiteral("sceneDock")},           {QStringLiteral("properties"), QStringLiteral("propertiesDock")},
        {QStringLiteral("analysis"), QStringLiteral("analysisDock")},     {QStringLiteral("acoustics"), QStringLiteral("acousticsDock")},
        {QStringLiteral("mechanics"), QStringLiteral("mechanicsDock")},   {QStringLiteral("em"), QStringLiteral("emDock")},
        {QStringLiteral("experiments"), QStringLiteral("experimentsDock")}, {QStringLiteral("optimization"), QStringLiteral("optimizationDock")},
        {QStringLiteral("full-wave"), QStringLiteral("solverDock")},      {QStringLiteral("python"), QStringLiteral("pythonConsoleDock")},
        {QStringLiteral("demo"), QStringLiteral("demoStudioDock")},       {QStringLiteral("presenter"), QStringLiteral("presenterDock")},
    };
    return names;
}

QDockWidget* dockFor(MainWindow& w, const QString& name)
{
    const QString objectName = dockNames().value(name, name);
    return w.findChild<QDockWidget*>(objectName);
}

std::optional<Id> objectId(const Project& project, const std::string& reference)
{
    if (const SceneObject* o = coupling::findSceneObject(project, reference))
        return o->id();
    return std::nullopt;
}
}  // namespace

DemoController::DemoController(MainWindow& window, QObject* parent) : QObject(parent), window_(window)
{
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        if (!running_ || wait_ == Wait::None || wait_ == Wait::Presenter)
            return;
        for (const Id& id : jobs_)
            window_.controller().cancelJob(id);
        if (pythonRequest_ != 0)
            window_.pythonSession().interrupt();
        const DemoStep& s = sequence_.steps[static_cast<size_t>(index_)];
        finishStep(false, QStringLiteral("timed out after %1 s (the step's limit); what it started was cancelled").arg(s.timeoutSeconds));
    });
    outputDirectory_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/demo-runs");
}

DemoController::~DemoController()
{
    clearWait();
}

void DemoController::setResourceRoot(const QString& root)
{
    root_ = root;
    reloadLibrary();
}

void DemoController::reloadLibrary()
{
    library_ = root_.isEmpty() ? DemoLibrary{{}, {"no resource root: the demonstrations were not found"}} : loadDemoLibrary(root_.toStdString());
    for (const std::string& e : library_.errors)
        logWarning("demo", e);
    emit stateChanged();
}

QString DemoController::resolve(const QString& path) const
{
    return QFileInfo(path).isAbsolute() ? path : QDir(root_).absoluteFilePath(path);
}

bool DemoController::startProfile(const QString& id, bool attended)
{
    const LoadedDemo* d = library_.find(id.toStdString());
    if (!d) {
        logError("demo", "no demonstration profile '" + id.toStdString() + "'");
        return false;
    }
    if (d->profile.status == "planned") {
        logError("demo", "the demonstration '" + id.toStdString() + "' is planned, not implemented");
        return false;
    }
    return start(d->sequence, &d->profile, attended);
}

bool DemoController::start(const DemoSequence& sequence, const DemoProfile* profile, bool attended)
{
    if (running_)
        return false;
    if (Status s = sequence.validate(); !s) {
        logError("demo", s.message());
        return false;
    }
    sequence_ = sequence;
    profile_ = profile ? std::optional<DemoProfile>(*profile) : std::nullopt;
    attended_ = attended;
    stopRequested_ = false;
    paused_ = false;
    running_ = true;
    index_ = -1;
    run_ = DemoRun{};
    run_.profileId = profile ? profile->id : std::string();
    run_.sequenceId = sequence.id;
    run_.sequenceVersion = sequence.version;
    run_.applicationVersion = kApplicationVersion;
    run_.startedUtc = utcNowIso8601();
    const QString stamp = QString::fromStdString(run_.startedUtc).remove(QLatin1Char(':')).remove(QLatin1Char('-'));
    runDirectory_ = QDir(outputDirectory_).absoluteFilePath(qs(profile ? profile->id : sequence.id) + QLatin1Char('-') + stamp);
    QDir().mkpath(runDirectory_);
    manifestPath_.clear();
    logInfo("demo", "started " + (profile ? profile->id : sequence.id) + (attended ? " (presenter)" : " (unattended)"));
    emit runStarted();
    emit stateChanged();
    scheduleStep();
    return true;
}

void DemoController::scheduleStep()
{
    // The next step runs from the event loop, so the interface stays live and
    // every event of the previous step has been delivered.
    QTimer::singleShot(0, this, [this] {
        if (!running_)
            return;
        if (stopRequested_) {
            finishRun(QStringLiteral("cancelled"), QStringLiteral("stopped by the user"));
            return;
        }
        if (paused_)
            return;
        ++index_;
        if (index_ >= stepCount()) {
            finishRun(QStringLiteral("completed"), QString());
            return;
        }
        runStep();
    });
}

void DemoController::runStep()
{
    const DemoStep& step = sequence_.steps[static_cast<size_t>(index_)];
    stepStart_ = std::chrono::steady_clock::now();
    StepRun record;
    record.id = step.id;
    record.action = step.action;
    record.startedUtc = utcNowIso8601();
    run_.steps.push_back(record);
    if (!step.notes.empty())
        run_.notes.push_back(step.id + ": " + step.notes);
    emit stepStarted(index_);
    emit stateChanged();
    const json& p = step.params;
    json result = json::object();
    Status status = Status::success();
    const std::string& a = step.action;
    QString error;
    if (a == "open-project")
        status = doOpenProject(p, result);
    else if (a == "workspace")
        status = doWorkspace(p);
    else if (a == "select")
        status = doSelect(p);
    else if (a == "camera")
        status = doCamera(p);
    else if (a == "visualization")
        status = doVisualization(p);
    else if (a == "show")
        status = doShow(p);
    else if (a == "help") {
        window_.showHelp(qs(str(p, "topic")));
        result["topic"] = str(p, "topic");
    } else if (a == "capture")
        status = doCapture(p, step, result);
    else if (a == "project-action")
        status = doProjectAction(p, result);
    else if (a == "promote")
        status = doPromote(p, result);
    else if (a == "experiment-data")
        status = doExperimentData(p, result);
    else if (a == "presenter")
        window_.setPresenterMode(p.value("on", true));
    else if (a == "expect")
        status = checkExpect(p, result);
    else if (a == "pause") {
        const QString text = qs(str(p, "text", step.description));
        if (attended_) {
            wait_ = Wait::Presenter;
            emit waitingForPresenter(index_, text);
            emit stateChanged();
            return;  // next() finishes it
        }
        result["unattended"] = true;
    } else if (a == "run") {
        if (!beginRun(p, error)) {
            finishStep(false, error);
            return;
        }
        return;
    } else if (a == "workflow") {
        if (!beginWorkflow(p, error)) {
            finishStep(false, error);
            return;
        }
        return;
    } else if (a == "python") {
        if (!beginPython(p, error)) {
            finishStep(false, error);
            return;
        }
        return;
    }
    finishStep(status.ok(), status.ok() ? QString() : qs(status.message()), result);
}

void DemoController::finishStep(bool ok, const QString& message, json result)
{
    clearWait();
    if (!running_ || index_ < 0 || index_ >= stepCount())
        return;
    const DemoStep& step = sequence_.steps[static_cast<size_t>(index_)];
    QString text = message;
    if (ok && !step.expect.empty()) {
        if (Status s = checkExpect(step.expect, result); !s) {
            ok = false;
            text = qs(s.message());
        }
    }
    StepRun& record = run_.steps.back();
    record.outcome = ok ? StepOutcome::Done : (stopRequested_ ? StepOutcome::Cancelled : StepOutcome::Failed);
    record.message = text.toStdString();
    record.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - stepStart_).count();
    record.result = std::move(result);
    logInfo("demo", step.id + ": " + std::string(toString(record.outcome)) + (text.isEmpty() ? "" : " - " + text.toStdString()));
    emit stepFinished(index_, ok, text);
    emit stateChanged();
    if (!ok && stopRequested_) {
        finishRun(QStringLiteral("cancelled"), QStringLiteral("stopped by the user"));
        return;
    }
    if (!ok && step.onFailure == FailureBehaviour::Stop) {
        finishRun(QStringLiteral("failed"), QStringLiteral("step '%1' failed: %2").arg(qs(step.id), text));
        return;
    }
    scheduleStep();
}

void DemoController::finishRun(const QString& status, const QString& message)
{
    clearWait();
    running_ = false;
    paused_ = false;
    // Steps never reached are recorded as skipped (a failed run shows where it stopped).
    for (int k = static_cast<int>(run_.steps.size()); k < stepCount(); ++k) {
        StepRun skipped;
        skipped.id = sequence_.steps[static_cast<size_t>(k)].id;
        skipped.action = sequence_.steps[static_cast<size_t>(k)].action;
        skipped.outcome = StepOutcome::Skipped;
        run_.steps.push_back(skipped);
    }
    run_.status = status.toStdString();
    run_.message = message.toStdString();
    run_.finishedUtc = utcNowIso8601();
    manifestPath_ = QDir(runDirectory_).absoluteFilePath(QStringLiteral("run-manifest.json"));
    exportManifest(manifestPath_);
    logInfo("demo", "finished " + run_.sequenceId + ": " + run_.status + (message.isEmpty() ? "" : " - " + message.toStdString()));
    emit runFinished(status, message);
    emit stateChanged();
}

bool DemoController::exportManifest(const QString& path) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(QByteArray::fromStdString(toJson(run_).dump(2)));
    return true;
}

void DemoController::next()
{
    if (wait_ == Wait::Presenter) {
        wait_ = Wait::None;
        json result = {{"presenter", "continued"}};
        finishStep(true, QString(), result);
    } else if (paused_)
        resume();
}

void DemoController::pause()
{
    if (running_ && !paused_) {
        paused_ = true;
        emit stateChanged();
    }
}

void DemoController::resume()
{
    if (running_ && paused_) {
        paused_ = false;
        emit stateChanged();
        if (wait_ == Wait::None)
            scheduleStep();
    }
}

void DemoController::stop()
{
    if (!running_)
        return;
    stopRequested_ = true;
    if (wait_ == Wait::Jobs) {
        for (const Id& id : jobs_)
            window_.controller().cancelJob(id);
        return;  // the cancelled job ends the step
    }
    if (wait_ == Wait::Python) {
        window_.pythonSession().interrupt();
        return;
    }
    if (wait_ == Wait::Presenter) {
        wait_ = Wait::None;
        finishStep(false, QStringLiteral("stopped by the user"));
        return;
    }
    if (paused_) {
        paused_ = false;
        finishRun(QStringLiteral("cancelled"), QStringLiteral("stopped by the user"));
    }
    // Otherwise the scheduled next step sees the request.
}

bool DemoController::restart()
{
    if (running_)
        return false;
    if (sequence_.steps.empty())
        return false;
    const DemoSequence s = sequence_;
    const std::optional<DemoProfile> p = profile_;
    return start(s, p ? &*p : nullptr, attended_);
}

void DemoController::clearWait()
{
    for (const auto& c : connections_)
        disconnect(c);
    connections_.clear();
    timeout_.stop();
    jobs_.clear();
    jobFailure_.clear();
    needSignal_ = false;
    signalSeen_ = false;
    collect_ = nullptr;
    failureText_ = nullptr;
    pythonRequest_ = 0;
    if (wait_ != Wait::Presenter)
        wait_ = Wait::None;
}

// --- immediate actions -------------------------------------------------------

Status DemoController::doOpenProject(const json& p, json& result)
{
    QString path = resolve(qs(str(p, "path")));
    if (!QFileInfo::exists(path))
        return Status::failure("the project " + str(p, "path") + " does not exist");
    run_.projectSha256 = sha256OfFile(path.toStdString());
    if (p.value("copyToRunDirectory", false)) {
        // Work on a copy in the run's folder: data a step stores next to the
        // project (measurements) never lands among the bundled examples.
        const QString copy = QDir(runDirectory_).absoluteFilePath(QFileInfo(path).fileName());
        QFile::remove(copy);
        if (!QFile::copy(path, copy))
            return Status::failure("the project could not be copied into the run folder");
        QFile::setPermissions(copy, QFile::permissions(copy) | QFileDevice::WriteOwner);
        path = copy;
    }
    if (Status s = window_.openProjectFile(path); !s)
        return s;
    window_.statusBar()->clearMessage();
    run_.projectFile = str(p, "path");  // as given (relative to the resource root): no private paths
    run_.projectId = window_.controller().project().id.str();
    const json& ext = window_.controller().project().extensions;
    for (const char* key : {"acoustics", "mechanics", "electromagnetics", "fdtd"})
        if (ext.contains(key))
            run_.simulation[key] = ext[key];
    result = {{"project", run_.projectFile}, {"sha256", run_.projectSha256}, {"objects", window_.controller().scene().objectCount()}};
    return Status::success();
}

Status DemoController::doWorkspace(const json& p)
{
    if (p.contains("windowSize") && p["windowSize"].is_array() && p["windowSize"].size() == 2) {
        window_.showNormal();
        window_.resize(p["windowSize"][0].get<int>(), p["windowSize"][1].get<int>());
    }
    const QString panel = qs(str(p, "panel"));
    if (p.value("only", false) || p.contains("docks")) {
        // A consistent arrangement: only the listed docks (and the panel) are shown.
        QSet<QString> keep;
        for (const auto& d : p.value("docks", json::array()))
            keep.insert(dockNames().value(qs(d.get<std::string>()), qs(d.get<std::string>())));
        keep.insert(dockNames().value(panel, panel));
        if (window_.presenterMode())
            keep.insert(QStringLiteral("presenterDock"));
        for (QDockWidget* d : window_.findChildren<QDockWidget*>())
            if (d->parentWidget() == &window_ || d->isFloating())
                d->setVisible(keep.contains(d->objectName()));
    }
    // Dock sizes for a consistent arrangement: {"python": 380} (height of a
    // bottom dock, width of a side dock), applied after the docks are shown.
    for (const char* key : {"dockHeights", "dockWidths"})
        if (p.contains(key) && p[key].is_object()) {
            const bool heights = std::string(key) == "dockHeights";
            QTimer::singleShot(0, this, [this, sizes = p[key], heights] {
                for (const auto& [name, size] : sizes.items())
                    if (QDockWidget* d = dockFor(window_, qs(name)))
                        window_.resizeDocks({d}, {size.get<int>()}, heights ? Qt::Vertical : Qt::Horizontal);
            });
        }
    if (panel.isEmpty() || panel == QLatin1String("viewport"))
        return Status::success();
    QDockWidget* dock = dockFor(window_, panel);
    if (!dock)
        return Status::failure("no workspace panel '" + panel.toStdString() + "'");
    dock->show();
    dock->raise();
    return doShow(p);
}

Status DemoController::doShow(const json& p)
{
    const QString panel = qs(str(p, "panel"));
    const QString tab = qs(str(p, "tab"));
    if (!panel.isEmpty())
        if (QDockWidget* dock = dockFor(window_, panel)) {
            dock->show();
            dock->raise();
        }
    if (!tab.isEmpty()) {
        if (panel == QLatin1String("acoustics"))
            window_.acousticsPanel().showTab(tab);
        else if (panel == QLatin1String("mechanics"))
            window_.mechanicsPanel().showTab(tab);
        else if (panel == QLatin1String("em"))
            window_.emPanel().showTab(tab);
        else if (panel == QLatin1String("optimization"))
            window_.optimizationPanel().showTab(tab);
        else
            return Status::failure("the panel '" + panel.toStdString() + "' has no tabs to show");
    }
    if (p.contains("result")) {
        const int index = p["result"].get<int>();
        const int item = p.value("item", 0);
        const std::string domain = str(p, "domain", panel.toStdString());
        if (domain == "mechanics") {
            if (index >= static_cast<int>(window_.mechanics().results().size()))
                return Status::failure("there is no mechanical result " + std::to_string(index));
            window_.mechanics().display(index, item);
        } else if (domain == "em") {
            if (index >= static_cast<int>(window_.electromagnetics().results().size()))
                return Status::failure("there is no electromagnetic field result " + std::to_string(index));
            window_.electromagnetics().display(index);
        } else
            return Status::failure("results of '" + domain + "' cannot be chosen by index");
    }
    if (p.contains("pattern3d") && p["pattern3d"].get<bool>()) {
        if (!window_.emPanel().calculatePattern() || !window_.emPanel().showPattern3D())
            return Status::failure("the 3D pattern could not be shown: " + window_.emPanel().patternText().toStdString());
    }
    return Status::success();
}

Status DemoController::doSelect(const json& p)
{
    const auto id = objectId(window_.controller().project(), str(p, "object"));
    if (!id)
        return Status::failure("the project has no object '" + str(p, "object") + "'");
    window_.controller().select(*id);
    return Status::success();
}

Status DemoController::doCamera(const json& p)
{
    ViewportWidget& v = window_.viewport();
    const std::string preset = str(p, "preset");
    if (preset == "front")
        v.viewFront();
    else if (preset == "top")
        v.viewTop();
    else if (preset == "right")
        v.viewRight();
    else if (preset == "reset" || preset == "iso")
        v.resetCamera();
    else if (preset == "frame-all")
        v.frameAll();
    else if (preset == "frame-selection")
        v.frameSelection();
    else if (!preset.empty())
        return Status::failure("unknown camera preset '" + preset + "'");
    auto& cam = v.camera();
    if (p.contains("yaw"))
        cam.yaw = p["yaw"].get<double>() * kPi / 180.0;
    if (p.contains("pitch"))
        cam.pitch = p["pitch"].get<double>() * kPi / 180.0;
    if (p.contains("distance"))
        cam.distance = p["distance"].get<double>();
    if (p.contains("zoom"))
        cam.distance *= p["zoom"].get<double>();
    if (p.contains("target") && p["target"].is_array() && p["target"].size() == 3)
        cam.target = Vec3(p["target"][0].get<double>(), p["target"][1].get<double>(), p["target"][2].get<double>());
    if (p.contains("grid"))
        v.setGridVisible(p["grid"].get<bool>());
    if (p.contains("axes"))
        v.setAxesVisible(p["axes"].get<bool>());
    v.refresh();
    return Status::success();
}

Status DemoController::doVisualization(const json& p)
{
    const std::string domain = str(p, "domain", "acoustics");
    const std::string q = str(p, "quantity");
    if (domain == "acoustics") {
        using Q = render::FieldQuantity;
        static const std::map<std::string, Q> quantities = {{"magnitude", Q::Magnitude}, {"phase", Q::Phase}, {"instantaneous", Q::Instantaneous},
                                                            {"intensity", Q::RelativeIntensity}, {"decibels", Q::Decibels}};
        if (!q.empty()) {
            const auto it = quantities.find(q);
            if (it == quantities.end())
                return Status::failure("unknown acoustic quantity '" + q + "'");
            window_.acoustics().setQuantity(it->second);
        }
        if (p.contains("animate"))
            window_.acoustics().setAnimating(p["animate"].get<bool>());
    } else if (domain == "mechanics") {
        using Q = MechanicsController::Quantity;
        static const std::map<std::string, Q> quantities = {{"displacement", Q::DisplacementMagnitude}, {"normal", Q::NormalComponent},
                                                            {"x", Q::X}, {"y", Q::Y}, {"z", Q::Z}, {"velocity", Q::VelocityMagnitude},
                                                            {"acceleration", Q::AccelerationMagnitude}, {"von-mises", Q::VonMises}};
        if (!q.empty()) {
            const auto it = quantities.find(q);
            if (it == quantities.end())
                return Status::failure("unknown mechanical quantity '" + q + "'");
            window_.mechanics().setQuantity(it->second);
        }
        if (p.contains("animate"))
            window_.mechanics().setAnimating(p["animate"].get<bool>());
    } else if (domain == "em") {
        using Q = EmController::Quantity;
        static const std::map<std::string, Q> quantities = {{"magnitude", Q::Magnitude}, {"phase", Q::Phase}, {"instantaneous", Q::Instantaneous},
                                                            {"decibels", Q::Decibels}};
        if (!q.empty()) {
            const auto it = quantities.find(q);
            if (it == quantities.end())
                return Status::failure("unknown electromagnetic quantity '" + q + "'");
            window_.electromagnetics().setQuantity(it->second);
        }
    } else
        return Status::failure("unknown visualization domain '" + domain + "'");
    window_.viewport().refresh();
    return Status::success();
}

Status DemoController::doProjectAction(const json& p, json& result)
{
    const std::string id = str(p, "id");
    if (!automation::ActionRegistry::instance().knows(id))
        return Status::failure("unknown action '" + id + "'");
    auto copy = deserializeProject(serializeProject(window_.controller().project()));
    if (!copy)
        return Status::failure(copy.message());
    const json outputs = json::object();
    automation::ActionContext context{*copy.value().project, nullptr, outputs, runDirectory_.toStdString()};
    auto out = automation::ActionRegistry::instance().run(id, context, p.value("params", json::object()));
    if (!out)
        return Status::failure(id + ": " + out.message());
    result = out.value();
    return window_.applyProjectCopy(*copy.value().project, QStringLiteral("Demo: ") + qs(id));
}

Status DemoController::doPromote(const json& p, json& result)
{
    const auto w = window_.optimization().workspace();
    if (w.runs.empty())
        return Status::failure("there is no optimization run to promote from");
    const auto& run = w.runs.back();
    std::string candidate = str(p, "candidate", "best");
    if (candidate == "best") {
        if (!run.best)
            return Status::failure("the last run has no best feasible candidate");
        candidate = run.candidates[*run.best].id;
    }
    if (Status s = window_.optimization().promote(run.runId, candidate); !s)
        return s;
    result = {{"run", run.runId}, {"candidate", candidate}};
    return Status::success();
}

Status DemoController::doExperimentData(const json& p, json& result)
{
    const std::string op = str(p, "op");
    const std::string experiment = str(p, "experiment");
    ExperimentsController& x = window_.experiments();
    if (op == "import-touchstone") {
        const QString file = resolve(qs(str(p, "file")));
        if (Status s = x.importTouchstone(file, experiment); !s)
            return s;
        const auto records = x.measurements(experiment);
        if (records.empty())
            return Status::failure("the import produced no measurement");
        result = {{"measurement", records.back().id}, {"file", str(p, "file")}};
        window_.experimentsPanel().selectExperiment(experiment);
        return Status::success();
    }
    if (op == "compare") {
        std::string measurement = str(p, "measurement");
        if (measurement.empty()) {
            const auto records = x.measurements(experiment);
            if (records.empty())
                return Status::failure("the experiment has no measurement to compare");
            measurement = records.back().id;
        }
        auto c = x.compareWithSimulation(experiment, measurement);
        if (!c)
            return Status::failure(c.message());
        result = {{"kind", c.value().kind}, {"simulated", c.value().simulatedLabel}, {"metrics", c.value().metrics}, {"note", c.value().note}};
        return Status::success();
    }
    return Status::failure("unknown experiment-data operation '" + op + "'");
}

Status DemoController::doCapture(const json& p, const DemoStep& step, json& result)
{
    const QString name = qs(str(p, "name"));
    const QString target = qs(str(p, "target", "window"));
    QWidget* widget = nullptr;
    QWidget* temporary = nullptr;
    if (target == QLatin1String("window"))
        widget = &window_;
    else if (target == QLatin1String("viewport"))
        widget = &window_.viewport();
    else if (target == QLatin1String("help"))
        widget = &window_.helpWindow();
    else if (target == QLatin1String("script-editor")) {
        widget = &window_.scriptEditor();
        if (p.contains("open") && !window_.scriptEditor().openFile(resolve(qs(str(p, "open")))))
            return Status::failure("the script " + str(p, "open") + " could not be opened");
    }
    else if (target == QLatin1String("plugins"))
        widget = temporary = window_.showPluginsDialog();
    else if (target == QLatin1String("workflows"))
        widget = window_.showWorkflowsDialog();
    else if (target == QLatin1String("materials"))
        widget = temporary = window_.showMaterialsDialog();
    else if (QDockWidget* dock = dockFor(window_, target)) {
        dock->show();
        dock->raise();
        widget = dock;
    }
    if (!widget)
        return Status::failure("nothing to capture: '" + target.toStdString() + "'");
    if (p.contains("size") && p["size"].is_array() && p["size"].size() == 2 && widget->isWindow())
        widget->resize(p["size"][0].get<int>(), p["size"][1].get<int>());
    window_.statusBar()->clearMessage();
    widget->show();
    widget->raise();
    // Lay out and draw everything that is pending (the viewport renders on grab).
    for (int k = 0; k < 3; ++k)
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    window_.viewport().refresh();
    const QImage image = widget->grab().toImage();
    if (temporary)
        temporary->close();
    if (image.isNull())
        return Status::failure("the capture of '" + target.toStdString() + "' is empty");
    const QString directory = p.contains("directory") ? resolve(qs(str(p, "directory"))) : runDirectory_;
    QDir().mkpath(directory);
    const QString file = QDir(directory).absoluteFilePath(name);
    if (!image.save(file, "PNG"))
        return Status::failure("could not write " + name.toStdString());
    run_.screenshots.push_back(name.toStdString());
    result = {{"file", name.toStdString()}, {"width", image.width()}, {"height", image.height()}, {"target", target.toStdString()}};
    if (!step.screenshot.empty())
        result["page"] = step.screenshot;
    if (p.contains("page"))
        result["page"] = p["page"];
    // An annotated copy with numbered callouts; the original stays unannotated.
    if (p.contains("annotations") && p["annotations"].is_array() && !p["annotations"].empty()) {
        QImage annotated = image.convertToFormat(QImage::Format_ARGB32);
        QPainter painter(&annotated);
        painter.setRenderHint(QPainter::Antialiasing);
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(15);
        painter.setFont(font);
        int number = 0;
        for (const json& a : p["annotations"]) {
            ++number;
            const QPointF at(a.value("x", 0.5) * annotated.width(), a.value("y", 0.5) * annotated.height());
            painter.setPen(QPen(QColor(255, 255, 255), 2));
            painter.setBrush(QColor(0xc8, 0x3a, 0x1e));
            painter.drawEllipse(at, 14, 14);
            painter.drawText(QRectF(at.x() - 14, at.y() - 14, 28, 28), Qt::AlignCenter, QString::number(number));
            const QString label = qs(str(a, "text"));
            if (!label.isEmpty()) {
                const QRectF box(at.x() + 18, at.y() - 12, painter.fontMetrics().horizontalAdvance(label) + 12, 24);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0x13, 0x40, 0x6b, 225));
                painter.drawRoundedRect(box, 4, 4);
                painter.setPen(QColor(255, 255, 255));
                painter.drawText(box, Qt::AlignCenter, label);
            }
        }
        painter.end();
        QString annotatedName = name;
        annotatedName.replace(QLatin1String(".png"), QLatin1String("-annotated.png"));
        if (!annotated.save(QDir(directory).absoluteFilePath(annotatedName), "PNG"))
            return Status::failure("could not write " + annotatedName.toStdString());
        run_.screenshots.push_back(annotatedName.toStdString());
        result["annotated"] = annotatedName.toStdString();
    }
    return Status::success();
}

Status DemoController::checkExpect(const json& e, json& result)
{
    const Project& project = window_.controller().project();
    if (e.contains("object") && !objectId(project, e["object"].get<std::string>()))
        return Status::failure("expected the object '" + e["object"].get<std::string>() + "'");
    if (e.contains("minModes") && window_.mechanicsPanel().modeRowCount() < e["minModes"].get<int>())
        return Status::failure("expected at least " + std::to_string(e["minModes"].get<int>()) + " modes, found " +
                               std::to_string(window_.mechanicsPanel().modeRowCount()));
    if (e.contains("minCandidates") && window_.optimizationPanel().candidateRows() < e["minCandidates"].get<int>())
        return Status::failure("expected at least " + std::to_string(e["minCandidates"].get<int>()) + " candidates");
    if (e.contains("acousticResults") && static_cast<int>(window_.acoustics().results().size()) < e["acousticResults"].get<int>())
        return Status::failure("expected an acoustic field result");
    if (e.contains("consoleContains") && !window_.pythonConsole().outputText().contains(qs(e["consoleContains"].get<std::string>())))
        return Status::failure("expected the console to show '" + e["consoleContains"].get<std::string>() + "'");
    if (e.contains("patternContains") && !window_.emPanel().patternText().contains(qs(e["patternContains"].get<std::string>())))
        return Status::failure("expected the pattern summary to contain '" + e["patternContains"].get<std::string>() + "'");
    if (e.contains("undoAvailable") && window_.controller().commands().canUndo() != e["undoAvailable"].get<bool>())
        return Status::failure(e["undoAvailable"].get<bool>() ? "expected an undoable change" : "expected no undoable change");
    if (e.contains("revisions")) {
        const auto w = window_.optimization().workspace();
        if (static_cast<int>(w.revisions.size()) < e["revisions"].get<int>())
            return Status::failure("expected a promoted design revision");
    }
    result["checked"] = e;
    return Status::success();
}

// --- waiting actions ---------------------------------------------------------

void DemoController::startJobWait(size_t jobsBefore, const std::function<json()>& collect, const std::function<QString()>& failureText)
{
    const auto all = window_.controller().jobs().jobs();
    for (size_t k = jobsBefore; k < all.size(); ++k)
        jobs_.insert(all[k].id);
    collect_ = collect;
    failureText_ = failureText;
    wait_ = Wait::Jobs;
    const DemoStep& step = sequence_.steps[static_cast<size_t>(index_)];
    timeout_.start(static_cast<int>(step.timeoutSeconds * 1000.0));
    connections_.append(connect(&window_.controller(), &DocumentController::jobChanged, this, [this](const JobInfo&) { checkJobs(); }));
    // Fast jobs may have finished already; check from the event loop.
    QTimer::singleShot(0, this, &DemoController::checkJobs);
}

void DemoController::checkJobs()
{
    if (wait_ != Wait::Jobs)
        return;
    JobManager& jobs = window_.controller().jobs();
    for (const Id& id : jobs_) {
        const auto info = jobs.info(id);
        if (!info)
            continue;
        if (!isFinished(info->state))
            return;  // still running: wait for its event
        if (info->state == JobState::Failed)
            jobFailure_ = qs(info->name + " failed: " + info->message);
        else if (info->state == JobState::Cancelled)
            jobFailure_ = qs(info->name + " was cancelled");
    }
    // Every job finished; a controller that records its result on its own
    // schedule must also have said so (unless a job failed).
    if (needSignal_ && !signalSeen_ && jobFailure_.isEmpty())
        return;
    wait_ = Wait::None;
    QTimer::singleShot(0, this, [this, collect = collect_, failure = failureText_, error = jobFailure_] {
        if (!running_)
            return;
        if (!error.isEmpty()) {
            finishStep(false, error);
            return;
        }
        json result = collect ? collect() : json::object();
        const QString problem = failure ? failure() : QString();
        finishStep(problem.isEmpty(), problem, result);
    });
}

bool DemoController::beginRun(const json& p, QString& error)
{
    const std::string what = str(p, "what");
    // The controller's own completion signal, connected before the run starts.
    const auto awaitSignal = [this](QMetaObject::Connection c) {
        needSignal_ = true;
        signalSeen_ = false;
        connections_.append(c);
    };
    const auto seen = [this] {
        signalSeen_ = true;
        QTimer::singleShot(0, this, &DemoController::checkJobs);
    };
    if (what == "acoustics.field")
        awaitSignal(connect(&window_.acoustics(), &AcousticController::resultsChanged, this, seen));
    else if (what == "full-wave")
        awaitSignal(connect(&window_.fullWave(), &FdtdController::resultsChanged, this, seen));
    else if (what.rfind("mechanics.", 0) == 0)
        awaitSignal(connect(&window_.mechanics(), &MechanicsController::resultsChanged, this, seen));
    else if (what == "optimization")
        awaitSignal(connect(&window_.optimization(), &OptimizationController::runFinished, this, seen));
    else if (what == "experiment")
        awaitSignal(connect(&window_.experiments(), &ExperimentsController::runFinished, this, seen));
    const size_t before = window_.controller().jobs().jobs().size();
    const Project& project = window_.controller().project();
    const auto idOf = [&](const char* key) { return objectId(project, str(p, key)); };
    bool started = false;
    std::function<json()> collect;
    std::function<QString()> failed;
    if (what == "acoustics.field") {
        if (auto r = idOf("region"))
            window_.acousticsPanel().selectRegion(*r);
        const size_t results = window_.acoustics().results().size();
        started = window_.acousticsPanel().runSimulation();
        if (!started)
            error = QStringLiteral("the field could not be calculated: ") + window_.acousticsPanel().estimateText();
        collect = [this] { return json{{"summary", [this] {
                                           json s = json::array();
                                           for (const QString& item : window_.fieldAnalysis().summaryItems())
                                               s.push_back(item.toStdString());
                                           return s;
                                       }()}}; };
        failed = [this, results] {
            return window_.acoustics().results().size() > results || !window_.acoustics().results().empty()
                       ? QString()
                       : QStringLiteral("the field calculation produced no result");
        };
    } else if (what == "acoustics.focus") {
        if (auto a = idOf("array"))
            window_.acousticsPanel().selectFocusArray(*a);
        if (auto t = idOf("target"))
            window_.acousticsPanel().selectFocusTarget(*t);
        started = window_.acousticsPanel().calculateFocus() && window_.acousticsPanel().applyFocus();
        if (!started)
            error = QStringLiteral("the array could not be focused (select an array and a target)");
    } else if (what == "full-wave") {
        if (auto r = idOf("region"))
            window_.fullWavePanel().selectRegion(*r);
        started = window_.fullWavePanel().run();
        if (!started)
            error = QStringLiteral("the full-wave solver refused: ") + window_.fullWavePanel().statusText();
        collect = [this] { return json{{"status", window_.fullWavePanel().statusText().toStdString()}}; };
        failed = [this] { return window_.fullWave().results().empty() ? QStringLiteral("the full-wave run produced no result") : QString(); };
    } else if (what.rfind("mechanics.", 0) == 0) {
        if (auto s = idOf("structure"))
            window_.mechanicsPanel().selectStructure(*s);
        MechanicsPanel& m = window_.mechanicsPanel();
        const std::string kind = what.substr(10);
        started = kind == "modal" ? m.runModal() : kind == "harmonic" ? m.runHarmonic() : kind == "transient" ? m.runTransient() : kind == "static" ? m.runStatic() : false;
        if (!started)
            error = QStringLiteral("the mechanical analysis could not run: ") + m.statusText();
        collect = [this] {
            json modes = json::array();
            MechanicsPanel& panel = window_.mechanicsPanel();
            for (int r = 0; r < std::min(6, panel.modeRowCount()); ++r)
                modes.push_back(panel.modeCell(r, 1).toStdString());
            return json{{"status", panel.statusText().toStdString()}, {"modes", modes}};
        };
        failed = [this] { return window_.mechanics().results().empty() ? QStringLiteral("the mechanical analysis produced no result") : QString(); };
    } else if (what == "em.analyse" || what == "em.pattern") {
        if (auto o = idOf("object"))
            window_.emPanel().selectObject(*o);
        EmPanel& e = window_.emPanel();
        if (what == "em.analyse") {
            e.showTab(QStringLiteral("impedanceTab"));
            started = e.analyse();
            collect = [this] { return json{{"report", window_.emPanel().reportText().toStdString()}}; };
        } else {
            e.showTab(QStringLiteral("patternTab"));
            started = e.calculatePattern();
            if (started && p.value("show3d", true))
                started = e.showPattern3D();
            collect = [this] { return json{{"pattern", window_.emPanel().patternText().toStdString()}}; };
        }
        if (!started)
            error = QStringLiteral("the antenna analysis could not run: ") + e.reportText() + e.patternText();
    } else if (what == "em.array") {
        if (auto o = idOf("object"))
            window_.emPanel().selectObject(*o);
        window_.emPanel().showTab(QStringLiteral("arrayTab"));
        started = window_.emPanel().analyseArray();
        if (!started)
            error = QStringLiteral("the array could not be analysed: ") + window_.emPanel().arrayText();
        collect = [this] { return json{{"array", window_.emPanel().arrayText().toStdString()}}; };
    } else if (what == "optimization") {
        OptimizationPanel& o = window_.optimizationPanel();
        auto w = window_.optimization().workspace();
        std::string id = str(p, "problem", "first");
        if (id == "first" && !w.problems.empty())
            id = w.problems.front().id;
        o.selectProblem(id);
        if (o.selectedProblem() != id) {
            error = QStringLiteral("the project has no optimization problem '%1'").arg(qs(id));
            return false;
        }
        o.showTab(QStringLiteral("runTab"));
        started = o.start();
        if (!started)
            error = QStringLiteral("the optimization could not start: ") + o.statusText();
        collect = [this] {
            window_.optimizationPanel().showTab(QStringLiteral("candidatesTab"));
            const auto ws = window_.optimization().workspace();
            json r = {{"status", window_.optimizationPanel().statusText().toStdString()}, {"candidates", window_.optimizationPanel().candidateRows()}};
            if (!ws.runs.empty() && ws.runs.back().best)
                r["bestObjective"] = ws.runs.back().candidates[*ws.runs.back().best].objectiveValues;
            return r;
        };
    } else if (what == "experiment") {
        ExperimentsPanel& x = window_.experimentsPanel();
        auto list = window_.experiments().workspace().experiments;
        std::string id = str(p, "experiment", "first");
        if (id == "first" && !list.empty())
            id = list.front().id;
        x.selectExperiment(id);
        if (x.selectedExperiment() != id) {
            error = QStringLiteral("the project has no experiment '%1'").arg(qs(id));
            return false;
        }
        // Never authorized here: an experiment with physical outputs refuses, as it must.
        started = x.runSelected();
        if (!started)
            error = QStringLiteral("the experiment did not start: ") + x.statusText();
        collect = [this] { return json{{"status", window_.experimentsPanel().statusText().toStdString()},
                                       {"measurements", window_.experimentsPanel().measurementCount()}}; };
    } else {
        error = QStringLiteral("unknown run '%1'").arg(qs(what));
        return false;
    }
    if (!started) {
        clearWait();
        return false;
    }
    // A run that started no job finished inside the call (and said so already, or never will).
    if (window_.controller().jobs().jobs().size() == before)
        needSignal_ = false;
    startJobWait(before, collect, failed);
    return true;
}

bool DemoController::beginWorkflow(const json& p, QString& error)
{
    const auto w = automation::WorkflowLibrary::instance().find(str(p, "id"));
    if (!w) {
        error = QStringLiteral("no workflow '%1'").arg(qs(str(p, "id")));
        return false;
    }
    const json params = p.value("parameters", json::object());
    if (auto r = automation::resolveParameters(*w, params); !r) {
        error = qs(r.message());
        return false;
    }
    auto job = std::make_shared<automation::WorkflowJob>(serializeProject(window_.controller().project()), *w, params,
                                                          QDir(runDirectory_).absoluteFilePath(QStringLiteral("workflow")).toStdString());
    const size_t before = window_.controller().jobs().jobs().size();
    window_.controller().submitJob(job);
    startJobWait(
        before,
        [this, job] {
            json r = json::object();
            if (const auto run = job->result()) {
                r = {{"status", std::string(automation::toString(run->status))}, {"steps", run->steps.size()}, {"message", run->message}};
                if (run->status == automation::RunStatus::Completed) {
                    auto loaded = deserializeProject(job->resultingProject());
                    if (loaded) {
                        loaded.value().project->extensions[automation::kAutomationExtensionKey] =
                            automation::appendHistory(window_.controller().project(), *run);
                        if (Status s = window_.applyProjectCopy(*loaded.value().project, QStringLiteral("Workflow: ") + qs(run->workflowId)); !s)
                            r["applyError"] = s.message();
                    }
                }
            }
            return r;
        },
        [job] {
            const auto run = job->result();
            return run && run->status == automation::RunStatus::Completed ? QString() : QStringLiteral("the workflow did not complete");
        });
    return true;
}

bool DemoController::beginPython(const json& p, QString& error)
{
    PythonSession& session = window_.pythonSession();
    window_.showPythonConsole();
    PythonConsole& console = window_.pythonConsole();
    if (!session.isRunning() && !window_.startPython()) {
        error = QStringLiteral("Python is not available: ") + session.startupError();
        return false;
    }
    consoleMark_ = static_cast<size_t>(console.outputText().size());
    // Queued before the code: the worker handles requests in order, so the
    // script always sees the project as it is now (also in a worker just started).
    if (p.value("sendProject", true))
        console.sendProject(false);
    QString code = qs(str(p, "code"));
    code.replace(QStringLiteral("${runDirectory}"), runDirectory_);
    const int id = console.runCode(code, qs(str(p, "label", "demo")));
    if (id == 0) {
        error = QStringLiteral("the console is busy");
        return false;
    }
    pythonRequest_ = id;
    wait_ = Wait::Python;
    timeout_.start(static_cast<int>(sequence_.steps[static_cast<size_t>(index_)].timeoutSeconds * 1000.0));
    connections_.append(connect(&session, &PythonSession::finished, this, [this](int rid, bool ok) {
        if (rid != pythonRequest_)
            return;
        // Let the console show the last output and the application apply any change first.
        QTimer::singleShot(0, this, [this, ok] {
            const QString output = window_.pythonConsole().outputText().mid(static_cast<int>(consoleMark_));
            json result = {{"output", output.right(4000).toStdString()}};
            finishStep(ok, ok ? QString() : QStringLiteral("the script raised an error: ") + output.right(400).trimmed(), result);
        });
    }));
    return true;
}
