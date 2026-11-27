#include "OptimizationController.h"

#include "DocumentController.h"

#include "commands/Command.h"
#include "commands/SceneCommands.h"
#include "foundation/Log.h"
#include "geometry/MeshChecks.h"
#include "optimization/SceneBindings.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "scene/Scene.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QThread>

#include <filesystem>
#include <fstream>
#include <mutex>

using namespace resonant;
using namespace resonant::optimization;

namespace {

class TopologyJob final : public resonant::Job {
public:
    explicit TopologyJob(mechanics::TopologySettings settings) : settings_(std::move(settings)) {}
    std::string name() const override { return "Topology optimization (proof of concept)"; }
    JobEnd run(JobContext& context) override
    {
        auto r = mechanics::optimizeTopology(settings_, &context);
        if (!r)
            return context.cancelRequested() ? JobEnd::cancelled() : JobEnd::failed(r.message());
        std::lock_guard lock(mutex_);
        result_ = r.take();
        return JobEnd::completed("compliance " + std::to_string(result_->compliance.empty() ? 0.0 : result_->compliance.back()) + " J");
    }
    std::optional<mechanics::TopologyResult> result() const
    {
        std::lock_guard lock(mutex_);
        return result_;
    }

private:
    mechanics::TopologySettings settings_;
    mutable std::mutex mutex_;
    std::optional<mechanics::TopologyResult> result_;
};

const Candidate* findCandidate(const RunRecord& run, const std::string& id)
{
    for (const Candidate& c : run.candidates)
        if (c.id == id)
            return &c;
    return nullptr;
}

}  // namespace

OptimizationController::OptimizationController(DocumentController* document, QObject* parent)
    : QObject(parent), document_(document), documentAlive_(document)
{
    timer_.setInterval(200);
    connect(&timer_, &QTimer::timeout, this, &OptimizationController::poll);
    connect(document_, &DocumentController::projectReplaced, this, [this] {
        stop();
        checkpoints_.clear();
        emit problemsChanged();
        emit runsChanged();
    });
}

OptimizationController::~OptimizationController()
{
    if (job_ && documentAlive_)
        document_->jobs().cancel(jobId_);
}

OptimizationWorkspace OptimizationController::workspace() const { return workspaceOf(document_->project()); }

std::vector<const ProblemTemplate*> OptimizationController::templates() const
{
    std::vector<const ProblemTemplate*> out;
    for (const ProblemTemplate& t : AdapterRegistry::instance().templates())
        out.push_back(&t);
    return out;
}

Status OptimizationController::storeWorkspace(const OptimizationWorkspace& w, const QString& description)
{
    nlohmann::json value = nlohmann::json::object();
    {
        // Serialize through a scratch project so the stored form is exactly what storeWorkspace writes.
        auto scratch = Project::createNew("scratch");
        optimization::storeWorkspace(*scratch, w);
        value = scratch->extensions.contains(kExtensionKey) ? scratch->extensions[kExtensionKey] : nlohmann::json();
    }
    return document_->pushCommand(std::make_unique<SetProjectExtensionCommand>(document_->project(), kExtensionKey,
                                                                               value.is_null() ? std::string("null") : value.dump(),
                                                                               description.toStdString(), [this] {
                                                                                   emit problemsChanged();
                                                                                   emit runsChanged();
                                                                               }));
}

Result<std::string> OptimizationController::addFromTemplate(const std::string& templateId)
{
    const ProblemTemplate* t = AdapterRegistry::instance().findTemplate(templateId);
    if (!t)
        return Result<std::string>::failure("no template '" + templateId + "'");
    auto created = t->create(document_->project());
    if (!created)
        return Result<std::string>::failure(created.message());
    auto problem = problemFromJson(created.value());
    if (!problem)
        return Result<std::string>::failure(problem.message());
    OptimizationWorkspace w = workspace();
    w.problems.push_back(problem.value());
    if (Status s = storeWorkspace(w, QStringLiteral("Add optimization problem")); !s)
        return Result<std::string>::failure(s.message());
    return problem.value().id;
}

Status OptimizationController::setProblem(const OptimizationProblem& problem, const QString& description)
{
    if (Status s = problem.validate(); !s)
        return s;
    OptimizationWorkspace w = workspace();
    OptimizationProblem* existing = w.findProblem(problem.id);
    if (existing)
        *existing = problem;
    else
        w.problems.push_back(problem);
    return storeWorkspace(w, description);
}

Status OptimizationController::removeProblem(const std::string& id)
{
    OptimizationWorkspace w = workspace();
    const auto before = w.problems.size();
    std::erase_if(w.problems, [&](const OptimizationProblem& p) { return p.id == id; });
    if (w.problems.size() == before)
        return Status::failure("no such problem");
    return storeWorkspace(w, QStringLiteral("Remove optimization problem"));
}

Result<std::shared_ptr<const DesignEvaluator>> OptimizationController::evaluator(const OptimizationProblem& problem) const
{
    return AdapterRegistry::instance().bind(problem.adapterId, designSnapshot(document_->project()), problem.adapterConfig);
}

std::string OptimizationController::checkpointFile(const std::string& runId) const
{
    if (document_->filePath().isEmpty())
        return {};
    const QFileInfo file(document_->filePath());
    const QString dir = file.absolutePath() + QLatin1Char('/') + file.completeBaseName() + QStringLiteral(".optimization");
    return (dir + QLatin1Char('/') + QString::fromStdString(runId) + QStringLiteral(".checkpoint.json")).toStdString();
}

const RunRecord* OptimizationController::findRun(const OptimizationWorkspace& w, const std::string& runId) const { return w.findRun(runId); }

Status OptimizationController::launch(OptimizationProblem problem, RunOptions options)
{
    if (job_)
        return Status::failure("an optimization is already running; stop it first (one optimization job at a time)");
    auto ev = evaluator(problem);
    if (!ev)
        return Status::failure(ev.message());
    if (Status s = problem.validateAgainst(ev.value()->capabilities()); !s)
        return s;
    if (options.runId.empty())
        options.runId = newRunId();
    liveRunId_ = options.runId;
    liveProblemId_ = problem.id;
    const std::string runId = options.runId;
    options.checkpoint = [this, runId](const nlohmann::json& j) {
        // Called on the job's thread; kept for resume (the file is written by the job).
        QMetaObject::invokeMethod(this, [this, runId, j] { checkpoints_[runId] = j; }, Qt::QueuedConnection);
    };
    if (problem.algorithm.checkpointEvery <= 0)
        problem.algorithm.checkpointEvery = 10;
    // The checkpoint directory is created only when a run writes into it (a query never creates it).
    const std::string file = checkpointFile(runId);
    if (!file.empty())
        QDir().mkpath(QFileInfo(QString::fromStdString(file)).absolutePath());
    job_ = std::make_shared<OptimizationJob>(problem, ev.take(), options, file);
    jobId_ = document_->submitJob(job_);
    shownCandidates_ = 0;
    progress_ = 0.0;
    status_ = QStringLiteral("Running ") + QString::fromStdString(problem.name) + QStringLiteral("...");
    timer_.start();
    emit candidatesChanged();
    return Status::success();
}

Status OptimizationController::start(const std::string& problemId)
{
    const OptimizationWorkspace w = workspace();
    const OptimizationProblem* p = w.findProblem(problemId);
    if (!p)
        return Status::failure("no optimization problem '" + problemId + "'");
    return launch(*p, {});
}

bool OptimizationController::canResume(const std::string& runId) const
{
    if (checkpoints_.contains(runId))
        return true;
    const std::string file = checkpointFile(runId);
    return !file.empty() && std::filesystem::exists(file);
}

Status OptimizationController::resume(const std::string& runId)
{
    nlohmann::json checkpoint;
    if (const auto it = checkpoints_.find(runId); it != checkpoints_.end())
        checkpoint = it->second;
    else if (const std::string file = checkpointFile(runId); !file.empty() && std::filesystem::exists(file)) {
        try {
            checkpoint = nlohmann::json::parse(std::ifstream(file));
        } catch (const std::exception& e) {
            return Status::failure(std::string("the checkpoint could not be read: ") + e.what());
        }
    }
    if (checkpoint.is_null())
        return Status::failure("there is no checkpoint of run " + runId);
    if (checkpoint.value("complete", false))
        return Status::failure("run " + runId + " is complete; there is nothing to resume");
    auto state = resumeFromCheckpoint(checkpoint);
    if (!state)
        return Status::failure(state.message());
    RunOptions options;
    options.runId = state.value().runId;
    options.cache = state.value().cache;
    options.resumed = true;
    return launch(state.value().problem, options);
}

void OptimizationController::stop()
{
    if (job_ && documentAlive_)
        document_->jobs().cancel(jobId_);
}

std::vector<Candidate> OptimizationController::liveCandidates() const { return job_ ? job_->candidates() : std::vector<Candidate>{}; }

void OptimizationController::poll()
{
    if (topologyJob_) {
        const auto info = document_->jobs().info(topologyJobId_);
        if (info && isFinished(info->state)) {
            topology_ = std::static_pointer_cast<TopologyJob>(topologyJob_)->result();
            topologyJob_.reset();
            emit topologyFinished(QString::fromStdString(info->message));
        }
    }
    if (job_) {
        const auto info = document_->jobs().info(jobId_);
        if (info) {
            progress_ = info->progress;
            status_ = QString::fromStdString(info->message);
        }
        const size_t n = job_->candidates().size();
        if (n != shownCandidates_) {
            shownCandidates_ = n;
            emit candidatesChanged();
        }
        if (info && isFinished(info->state))
            finish();
    }
    if (!job_ && !topologyJob_)
        timer_.stop();
}

void OptimizationController::finish()
{
    const auto info = document_->jobs().info(jobId_);
    auto record = job_->result();
    if (!job_->checkpoint().is_null())
        checkpoints_[liveRunId_] = job_->checkpoint();
    job_.reset();
    QString message;
    if (record) {
        OptimizationWorkspace w = workspace();
        std::erase_if(w.runs, [&](const RunRecord& r) { return r.runId == record->runId; });
        w.runs.push_back(*record);
        message = QString::fromStdString(record->runId + ": " + std::to_string(record->candidates.size()) + " candidates, " + record->termination.reason);
        [[maybe_unused]] Status s = storeWorkspace(w, QStringLiteral("Record optimization run"));
    } else {
        message = QStringLiteral("The optimization did not run: ") + (info ? QString::fromStdString(info->message) : QString());
    }
    status_ = message;
    emit candidatesChanged();
    emit runsChanged();
    emit runFinished(message);
}

bool OptimizationController::waitUntilIdle(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while ((job_ || topologyJob_) && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        poll();
        QThread::msleep(5);
    }
    return !job_ && !topologyJob_;
}

// --- results ---------------------------------------------------------------------------------------------

Result<MultiFidelityReport> OptimizationController::reevaluate(const std::string& runId, const std::vector<std::string>& candidateIds,
                                                                const std::string& modelId)
{
    using R = Result<MultiFidelityReport>;
    OptimizationWorkspace w = workspace();
    const RunRecord* run = findRun(w, runId);
    if (!run)
        return R::failure("no run " + runId);
    auto problem = problemFromJson(run->problem);
    if (!problem)
        return R::failure(problem.message());
    auto ev = evaluator(problem.value());
    if (!ev)
        return R::failure(ev.message());
    std::vector<Candidate> chosen;
    for (const std::string& id : candidateIds)
        if (const Candidate* c = findCandidate(*run, id))
            chosen.push_back(*c);
    if (chosen.empty())
        return R::failure("no candidate selected");
    problem.value().fillUnits(ev.value()->capabilities());
    ServiceOptions options;
    options.runId = newRunId();
    options.parallel = std::max(1, QThread::idealThreadCount() / 2);
    EvaluationService service(problem.value(), ev.value(), options);
    MultiFidelityReport report = optimization::reevaluate(service, chosen, modelId);
    RunRecord record;
    record.runId = options.runId;
    record.problem = run->problem;
    record.algorithm = "re-evaluation";
    record.settings = problem.value().algorithm;
    record.snapshotKey = ev.value()->snapshotKey();
    record.startedUtc = record.finishedUtc = utcNow();
    record.complete = true;
    record.termination.reason = "re-evaluated " + std::to_string(report.validated.size()) + " candidate(s) of " + runId + " with " + modelId;
    record.candidates = report.validated;
    record.notes = report.notes;
    record.physicsEvaluations = service.physicsEvaluations();
    w.runs.push_back(record);
    if (Status s = storeWorkspace(w, QStringLiteral("Re-evaluate candidates")); !s)
        return R::failure(s.message());
    return report;
}

Result<SensitivityReport> OptimizationController::sensitivity(const std::string& runId, const std::string& candidateId, double step)
{
    using R = Result<SensitivityReport>;
    const OptimizationWorkspace w = workspace();
    const RunRecord* run = findRun(w, runId);
    if (!run)
        return R::failure("no run " + runId);
    const Candidate* c = findCandidate(*run, candidateId);
    if (!c)
        return R::failure("no candidate " + candidateId);
    auto problem = problemFromJson(run->problem);
    if (!problem)
        return R::failure(problem.message());
    auto ev = evaluator(problem.value());
    if (!ev)
        return R::failure(ev.message());
    problem.value().fillUnits(ev.value()->capabilities());
    ServiceOptions options;
    options.parallel = std::max(1, QThread::idealThreadCount() / 2);
    EvaluationService service(problem.value(), ev.value(), options);
    return localSensitivity(service, c->point, step);
}

Result<LinearInverseResult> OptimizationController::solveInverse(const std::string& problemId, const LinearInverseOptions& options)
{
    using R = Result<LinearInverseResult>;
    OptimizationWorkspace w = workspace();
    const OptimizationProblem* p = w.findProblem(problemId);
    if (!p)
        return R::failure("no optimization problem '" + problemId + "'");
    OptimizationProblem problem = *p;
    auto ev = evaluator(problem);
    if (!ev)
        return R::failure(ev.message());
    const DesignPoint start = problem.space.initialPoint(ev.value()->capabilities().constants);
    auto system = ev.value()->linearSystem(problem.space.bindings(start), problem.modelId);
    if (!system)
        return R::failure(system.message());
    auto solved = solveLinearInverse(system.value(), options);
    if (!solved)
        return solved;
    // The solution as a candidate, evaluated with the physical model like any other.
    const auto bindings = weightBindings(system.value(), solved.value().weights);
    DesignPoint point = start;
    for (const auto& [binding, value] : bindings) {
        bool found = false;
        for (const DesignVariable& v : problem.space.variables)
            if (v.active && v.key() == binding) {
                point.values[v.id] = value;
                found = true;
            }
        if (!found)
            point.extra[binding] = value - (ev.value()->capabilities().variable(binding) ? ev.value()->capabilities().variable(binding)->initial : 0.0);
    }
    problem.fillUnits(ev.value()->capabilities());
    ServiceOptions so;
    so.runId = newRunId();
    EvaluationService service(problem, ev.value(), so);
    std::vector<Candidate> candidates = service.evaluatePoints({point}, "linear inverse: " + solved.value().classification, 0, {}, {}, true);
    RunRecord record;
    record.runId = so.runId;
    record.problem = toJson(problem);
    record.algorithm = "linear-inverse";
    record.settings = problem.algorithm;
    record.snapshotKey = ev.value()->snapshotKey();
    record.startedUtc = record.finishedUtc = utcNow();
    record.complete = true;
    record.termination.reason = solved.value().classification;
    record.termination.converged = solved.value().accurate;
    record.candidates = candidates;
    record.notes = solved.value().notes;
    if (!candidates.empty() && candidates.front().feasible)
        record.best = 0;
    w.runs.push_back(record);
    if (Status s = storeWorkspace(w, QStringLiteral("Record inverse design")); !s)
        return R::failure(s.message());
    return solved;
}

Result<SurrogateInfo> OptimizationController::fitSurrogate(const std::string& runId, size_t objectiveIndex, const std::string& kind)
{
    const OptimizationWorkspace w = workspace();
    const RunRecord* run = findRun(w, runId);
    if (!run)
        return Result<SurrogateInfo>::failure("no run " + runId);
    auto problem = problemFromJson(run->problem);
    if (!problem)
        return Result<SurrogateInfo>::failure(problem.message());
    auto study = optimization::fitSurrogate(problem.value(), run->candidates, objectiveIndex, kind);
    if (!study)
        return Result<SurrogateInfo>::failure(study.message());
    return study.value().info;
}

Status OptimizationController::promote(const std::string& runId, const std::string& candidateId, bool acceptChangedDesign)
{
    OptimizationWorkspace w = workspace();
    const RunRecord* run = findRun(w, runId);
    if (!run)
        return Status::failure("no run " + runId);
    const Candidate* c = findCandidate(*run, candidateId);
    if (!c)
        return Status::failure("no candidate " + candidateId);
    if (!c->evaluated())
        return Status::failure("the candidate was not evaluated (" + c->evaluation.message + "); it cannot be promoted");
    auto problem = problemFromJson(run->problem);
    if (!problem)
        return Status::failure(problem.message());
    auto ev = evaluator(problem.value());
    if (!ev)
        return Status::failure(ev.message());
    if (!acceptChangedDesign && c->evaluation.model.snapshotKey != ev.value()->snapshotKey())
        return Status::failure("the design has changed since this candidate was evaluated; re-evaluate it on the present design, or promote "
                               "it anyway (its values are applied to the present design)");
    const auto bindings = problem.value().space.bindings(c->point);
    auto copy = projectFromSnapshot(designSnapshot(document_->project()));
    if (!copy)
        return Status::failure(copy.message());
    if (Status s = ev.value()->apply(bindings, *copy.value()); !s)
        return Status::failure("the candidate cannot be applied to the present design: " + s.message());
    Project& live = document_->project();
    const QString what = QStringLiteral("Promote candidate ") + QString::fromStdString(c->id);
    auto composite = std::make_unique<CompositeCommand>(what.toStdString());
    if (!(copy.value()->materials == live.materials))
        composite->add(std::make_unique<SetMaterialLibraryCommand>(live.materials, copy.value()->materials, "materials of the promoted design"));
    copy.value()->scene.forEach([&](const SceneObject& o) {
        const SceneObject* existing = live.scene.find(o.id());
        if (existing && !(existing->state == o.state))
            composite->add(std::make_unique<SetObjectStateCommand>(live.scene, o.id(), o.state, what.toStdString()));
    });
    for (const auto& [key, value] : copy.value()->extensions.items()) {
        if (key == kExtensionKey)
            continue;
        if (!live.extensions.contains(key) || live.extensions[key] != value)
            composite->add(std::make_unique<SetProjectExtensionCommand>(live, key, value.dump(), what.toStdString()));
    }
    DesignRevision revision;
    revision.id = "REV-" + std::to_string(w.revisions.size() + 1);
    revision.candidateId = c->id;
    revision.runId = runId;
    revision.problemId = problem.value().id;
    revision.promotedUtc = utcNow();
    revision.model = c->evaluation.model.modelId;
    revision.fidelity = std::string(toString(c->evaluation.model.fidelity));
    revision.validated = isValidated(problem.value(), *c);
    revision.values = bindings;
    for (const auto& [binding, value] : bindings)
        if (const DesignVariable* v = ev.value()->capabilities().variable(binding))
            revision.previous[binding] = v->initial;
    revision.parents = c->parents;
    revision.note = revision.validated ? "evaluated with a validation (high-fidelity) model"
                                       : "NOT validated: evaluated with '" + c->evaluation.model.modelName + "' only";
    if (acceptChangedDesign && c->evaluation.model.snapshotKey != ev.value()->snapshotKey())
        revision.note += "; promoted onto a design that changed after the evaluation";
    w.revisions.push_back(revision);
    auto scratch = Project::createNew("scratch");
    optimization::storeWorkspace(*scratch, w);
    composite->add(std::make_unique<SetProjectExtensionCommand>(live, kExtensionKey, scratch->extensions[kExtensionKey].dump(), what.toStdString(),
                                                                [this] {
                                                                    emit problemsChanged();
                                                                    emit runsChanged();
                                                                }));
    return document_->pushCommand(std::move(composite));
}

Status OptimizationController::exportCandidate(const std::string& runId, const std::string& candidateId, const QString& directory)
{
    const OptimizationWorkspace w = workspace();
    const RunRecord* run = findRun(w, runId);
    if (!run)
        return Status::failure("no run " + runId);
    const Candidate* c = findCandidate(*run, candidateId);
    if (!c)
        return Status::failure("no candidate " + candidateId);
    auto problem = problemFromJson(run->problem);
    if (!problem)
        return Status::failure(problem.message());
    auto ev = evaluator(problem.value());
    if (!ev)
        return Status::failure(ev.message());
    auto copy = projectFromSnapshot(designSnapshot(document_->project()));
    if (!copy)
        return Status::failure(copy.message());
    if (Status s = ev.value()->apply(problem.value().space.bindings(c->point), *copy.value()); !s)
        return s;
    const std::filesystem::path dir(directory.toStdString());
    std::filesystem::create_directories(dir);
    nlohmann::json meshes = nlohmann::json::array();
    Status failure = Status::success();
    copy.value()->scene.forEach([&](const SceneObject& o) {
        const SceneObject* original = document_->project().scene.find(o.id());
        if (!o.state.geometry || (original && original->state.geometry == o.state.geometry))
            return;
        TriangleMesh mesh = o.state.geometry->tessellate();
        // World coordinates.
        const Mat4 world = copy.value()->scene.worldMatrix(o);
        for (auto& p : mesh.positions)
            p = transformPoint(world, p.cast<double>()).cast<float>();
        for (auto& n : mesh.normals)
            n = transformDirection(world, n.cast<double>()).normalized().cast<float>();
        std::string base = c->id + "-" + o.state.name;
        for (char& ch : base)
            if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-')
                ch = '_';
        if (Status s = writeStl(mesh, dir / (base + ".stl"), base); !s)
            failure = s;
        if (Status s = writeObj(mesh, dir / (base + ".obj"), "candidate " + c->id + " of run " + runId); !s)
            failure = s;
        PrintabilityLimits limits;
        const PrintabilityReport r = checkPrintability(mesh, limits);
        meshes.push_back({{"object", o.state.name},
                          {"stl", base + ".stl"},
                          {"obj", base + ".obj"},
                          {"closed", r.integrity.closed},
                          {"volume", r.integrity.volume},
                          {"boundaryEdges", r.integrity.boundaryEdges},
                          {"overhangFraction", r.overhang},
                          {"failures", r.failures},
                          {"warnings", r.warnings}});
    });
    if (!failure)
        return failure;
    const nlohmann::json record{{"format", "resonant.optimization.candidate"},
                                {"version", 1},
                                {"note", "a simulated design: these checks are not a manufacturing approval; detail and verify it before fabrication"},
                                {"run", runId},
                                {"problem", run->problem},
                                {"candidate", toJson(*c)},
                                {"bindings", problem.value().space.bindings(c->point)},
                                {"validated", isValidated(problem.value(), *c)},
                                {"meshes", meshes}};
    std::ofstream(dir / (c->id + ".json")) << record.dump(2) << "\n";
    return Status::success();
}

Status OptimizationController::exportRun(const std::string& runId, const QString& path)
{
    const OptimizationWorkspace w = workspace();
    const RunRecord* run = findRun(w, runId);
    if (!run)
        return Status::failure("no run " + runId);
    std::ofstream out(path.toStdString());
    if (!out)
        return Status::failure("cannot write " + path.toStdString());
    if (path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
        auto problem = problemFromJson(run->problem);
        if (!problem)
            return Status::failure(problem.message());
        out << candidatesCsv(problem.value(), run->candidates);
    } else {
        out << toJson(*run).dump(2) << "\n";
    }
    return out ? Status::success() : Status::failure("writing " + path.toStdString() + " failed");
}

Status OptimizationController::startTopology(const mechanics::TopologySettings& settings)
{
    if (topologyJob_)
        return Status::failure("a topology optimization is already running");
    if (Status s = settings.validate(); !s)
        return s;
    topologyJob_ = std::make_shared<TopologyJob>(settings);
    topologyJobId_ = document_->submitJob(topologyJob_);
    timer_.start();
    return Status::success();
}

Status OptimizationController::exportTopology(const QString& path)
{
    if (!topology_)
        return Status::failure("run the topology optimization first");
    const TriangleMesh mesh = mechanics::extrudeTopology(*topology_);
    return writeStl(mesh, path.toStdString(), "topology-proof-of-concept");
}
