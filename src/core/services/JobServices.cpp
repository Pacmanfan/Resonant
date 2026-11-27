#include "acoustics/Components.h"
#include "acoustics/Propagation.h"
#include "acoustics/Settings.h"
#include "automation/WorkflowEngine.h"
#include "manipulators/AcousticFieldRun.h"
#include "optimization/Algorithms.h"
#include "optimization/Engine.h"
#include "optimization/Evaluator.h"
#include "optimization/Persistence.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "services/Services.h"

#include <cmath>
#include <filesystem>

namespace resonant::services {

namespace {
void auditStart(Session& s, const CallContext& c, const std::string& operation, const std::vector<std::string>& resources,
                const std::optional<ServiceError>& error = {})
{
    AuditRecord r;
    r.requestId = c.requestId;
    r.sessionId = c.sessionId;
    r.clientName = c.clientName;
    r.source = c.source;
    r.operation = operation;
    r.resourceIds = resources;
    r.revisionBefore = r.revisionAfter = s.projectRevision();
    r.succeeded = !error;
    r.error = error;
    s.record(std::move(r));
}
}  // namespace

// --- simulation --------------------------------------------------------------------

Outcome<std::vector<SimulationService::Solver>> SimulationService::solvers(const CallContext& c)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    const acoustics::AcousticSettings settings = acoustics::settingsOf(s_.project());
    std::vector<Solver> out;
    out.push_back({acoustics::kFreeFieldModelId, "Acoustic free field (analytical)", "acoustic", "superposition of point sources", true, true,
                   acoustics::FreeFieldModel(settings.propagation).assumptions()});
    out.push_back({acoustics::kReflectorModelId, "Acoustic reflectors (Kirchhoff)", "acoustic", "Kirchhoff physical optics over reflector surfaces", true, true,
                   {"single reflection of each surface element", "surfaces large compared to the wavelength", "selected by the project's acoustic model setting"}});
    out.push_back({"acoustic.fdtd", "Acoustic full-wave (FDTD)", "acoustic", "finite-difference time domain", true, false,
                   {"available in the Full-Wave Solver panel; through this API: planned"}});
    out.push_back({"mechanics.fe", "Structural finite elements", "mechanical", "beams, plates, membranes, solids: static, modal, harmonic, transient", true,
                   false, {"available in the Mechanics panel; through this API: planned"}});
    out.push_back({"em.analytical", "Electromagnetic analytical models", "electromagnetic", "thin-wire antennas, array factors, exact dipole fields", true,
                   false, {"available in the Electromagnetics panel; through this API: planned"}});
    out.push_back({"em.yee", "Electromagnetic full-wave (Yee FDTD)", "electromagnetic", "Yee finite-difference time domain", true, false,
                   {"available in the Electromagnetics panel; through this API: planned"}});
    return out;
}

Outcome<SimulationService::Plan> SimulationService::estimateAcousticField(const CallContext& c, const Id& region)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    if (!s_.project().scene.find(region))
        return notFound("the project has no object '" + region.str() + "'", "scene-object", region.str());
    const acoustics::AcousticSettings settings = acoustics::settingsOf(s_.project());
    auto estimate = acoustics::estimateFieldRun(s_.project(), region, settings);
    if (!estimate)
        return invalid(estimate.message(), "region_id");
    Plan plan;
    plan.estimate = estimate.take();
    plan.modelId = settings.modelId;
    plan.frequencyHz = settings.frequencyHz;
    for (const auto& source : acoustics::collectSources(s_.project().scene))
        if (std::abs(source.frequencyHz - settings.frequencyHz) <= 1e-9 * settings.frequencyHz)
            ++plan.activeSources;
    return plan;
}

Outcome<SimulationService::Started> SimulationService::startAcousticField(const CallContext& c, const Id& region, int runNumber)
{
    auto a = s_.authorize(c, Permission::Execute);
    if (!a)
        return a.error();
    auto plan = estimateAcousticField(a.value(), region);
    if (!plan) {
        auditStart(s_, a.value(), "SimulationService.StartAcousticFieldSimulation", {region.str()}, plan.error());
        return plan.error();
    }
    if (!plan.value().estimate.allowed) {
        ServiceError e = ServiceError::make(ErrorCode::ResourceExhausted, plan.value().estimate.refusal);
        auditStart(s_, a.value(), "SimulationService.StartAcousticFieldSimulation", {region.str()}, e);
        return e;
    }
    const int number = runNumber > 0 ? runNumber : static_cast<int>(s_.results().size()) + 1;
    auto run = acoustics::prepareFieldRun(s_.project(), region, number);
    if (!run) {
        ServiceError e = ServiceError::make(ErrorCode::FailedPrecondition, run.message());
        auditStart(s_, a.value(), "SimulationService.StartAcousticFieldSimulation", {region.str()}, e);
        return e;
    }
    std::shared_ptr<acoustics::FieldJob> job = run.value().job;
    const Id jobId = s_.host().jobs().submit(job);
    const std::uint64_t revision = s_.projectRevision();
    JobRecord record;
    record.jobId = jobId.str();
    record.type = "acoustic.field";
    record.sessionId = a.value().sessionId;
    record.onFinished = [job, revision](Session& session, const JobInfo& info) {
        if (info.state != JobState::Completed || !job->result())
            return;
        ResultEntry entry;
        entry.kind = "acoustic.field";
        entry.name = job->result()->name;
        entry.jobId = info.id.str();
        entry.projectRevision = revision;
        entry.field = job->result();
        const std::string id = session.addResult(std::move(entry));
        if (const JobRecord* r = session.jobRecord(info.id.str()))
            const_cast<JobRecord*>(r)->resultIds.push_back(id);
    };
    s_.trackJob(std::move(record));
    auditStart(s_, a.value(), "SimulationService.StartAcousticFieldSimulation", {region.str(), jobId.str()});
    Started started;
    started.plan = plan.take();
    started.fieldJob = job;
    if (const auto info = s_.host().jobs().info(jobId))
        started.job = s_.services().jobs.view(*info);
    else
        started.job.id = jobId.str();
    return started;
}

// --- jobs ---------------------------------------------------------------------------

JobView JobService::view(const JobInfo& info) const
{
    JobView v;
    v.id = info.id.str();
    v.name = info.name;
    v.phase = info.message;
    v.progress = info.progress;
    v.elapsedSeconds = info.elapsedSeconds;
    const bool cancelling = s_.cancelRequested().contains(v.id);
    switch (info.state) {
    case JobState::Queued: v.state = cancelling ? "cancelling" : "queued"; break;
    case JobState::Running: v.state = cancelling ? "cancelling" : "running"; break;
    case JobState::Completed: v.state = "completed"; break;
    case JobState::Failed:
        v.state = "failed";
        v.error = ServiceError::make(ErrorCode::Internal, info.message);
        break;
    case JobState::Cancelled:
        v.state = "cancelled";
        v.error = ServiceError::make(ErrorCode::Cancelled, info.message.empty() ? "cancelled" : info.message);
        break;
    }
    v.cancellable = !isFinished(info.state);
    v.type = "application";
    if (const JobRecord* r = s_.jobRecord(v.id)) {
        v.type = r->type;
        v.sessionId = r->sessionId;
        v.resultIds = r->resultIds;
    }
    return v;
}

Outcome<JobView> JobService::get(const CallContext& c, const std::string& id)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    const auto info = s_.host().jobs().info(Id(id));
    if (!info)
        return notFound("there is no job '" + id + "'", "job", id);
    return view(*info);
}

Outcome<std::vector<JobView>> JobService::list(const CallContext& c, bool activeOnly)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    std::vector<JobView> out;
    for (const JobInfo& info : s_.host().jobs().jobs())
        if (!activeOnly || !isFinished(info.state))
            out.push_back(view(info));
    return out;
}

Outcome<JobView> JobService::cancel(const CallContext& c, const std::string& id)
{
    auto a = s_.authorize(c, Permission::Execute);
    if (!a)
        return a.error();
    const auto info = s_.host().jobs().info(Id(id));
    if (!info)
        return notFound("there is no job '" + id + "'", "job", id);
    if (isFinished(info->state))
        return ServiceError::make(ErrorCode::FailedPrecondition, "the job already ended (" + std::string(toString(info->state)) + ")");
    s_.markCancelRequested(id);
    s_.host().jobs().cancel(Id(id));
    auditStart(s_, a.value(), "JobService.CancelJob", {id});
    return view(s_.host().jobs().info(Id(id)).value_or(*info));
}

// --- results ----------------------------------------------------------------------------

Outcome<std::vector<const ResultEntry*>> ResultService::list(const CallContext& c)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    return s_.results();
}

Outcome<const ResultEntry*> ResultService::get(const CallContext& c, const std::string& id)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    const ResultEntry* r = s_.result(id);
    if (!r)
        return notFound("there is no result '" + id + "'", "result", id);
    return r;
}

Outcome<FieldSlice> ResultService::slice(const CallContext& c, const std::string& id, int fixedAxis, int fixedIndex, int stride, std::size_t maxSamples)
{
    auto r = get(c, id);
    if (!r)
        return r.error();
    if (!r.value()->field)
        return invalid("the result '" + id + "' is not a sampled field", "result_id");
    const acoustics::FieldResult& f = *r.value()->field;
    const int n[3] = {f.frame.nu, f.frame.nv, f.frame.nw};
    if (fixedAxis < 0 || fixedAxis > 2)
        return invalid("the fixed axis must be u, v or w", "fixed_axis");
    if (fixedIndex < 0 || fixedIndex >= n[fixedAxis])
        return invalid("the index " + std::to_string(fixedIndex) + " is outside 0.." + std::to_string(n[fixedAxis] - 1), "fixed_index");
    stride = std::max(1, stride);
    const int a1 = fixedAxis == 0 ? 1 : 0;
    const int a2 = fixedAxis == 2 ? 1 : 2;
    FieldSlice out;
    out.rows = (n[a1] + stride - 1) / stride;
    out.columns = (n[a2] + stride - 1) / stride;
    const std::size_t count = static_cast<std::size_t>(out.rows) * static_cast<std::size_t>(out.columns);
    if (count > maxSamples) {
        ServiceError e = ServiceError::make(ErrorCode::ResourceExhausted, "the slice has " + std::to_string(count) + " samples (at most " +
                                                                             std::to_string(maxSamples) + "): use a larger stride");
        e.violations.push_back({"stride", e.message});
        return e;
    }
    out.samples.reserve(count);
    int index[3] = {0, 0, 0};
    index[fixedAxis] = fixedIndex;
    for (int i = 0; i < n[a1]; i += stride)
        for (int j = 0; j < n[a2]; j += stride) {
            index[a1] = i;
            index[a2] = j;
            out.samples.push_back(f.pressure[f.frame.index(index[0], index[1], index[2])]);
        }
    return out;
}

Outcome<std::vector<FieldPointSample>> ResultService::samples(const CallContext& c, const std::string& id, const std::vector<Vec3>& points)
{
    auto r = get(c, id);
    if (!r)
        return r.error();
    if (!r.value()->field)
        return invalid("the result '" + id + "' is not a sampled field", "result_id");
    if (points.size() > 100000)
        return ServiceError::make(ErrorCode::ResourceExhausted, "at most 100 000 points per request");
    const acoustics::FieldResult& f = *r.value()->field;
    // GridFrame axes span the whole region: one step is axis / (count - 1).
    const int n[3] = {f.frame.nu, f.frame.nv, f.frame.nw};
    const auto step = [](const Vec3& axis, int count) { return count > 1 ? Vec3(axis / (count - 1)) : Vec3(Vec3::Zero()); };
    const Vec3 axes[3] = {step(f.frame.axisU, n[0]), step(f.frame.axisV, n[1]), step(f.frame.axisW, n[2])};
    double largest = 0.0;
    for (const Vec3& axis : axes)
        largest = std::max(largest, axis.norm());
    std::vector<FieldPointSample> out;
    for (const Vec3& p : points) {
        FieldPointSample s;
        s.point = p;
        const Vec3 d = p - f.frame.origin;
        int index[3] = {0, 0, 0};
        bool inside = p.allFinite();
        Vec3 reconstructed = f.frame.origin;
        for (int k = 0; k < 3 && inside; ++k) {
            const double len2 = axes[k].squaredNorm();
            if (len2 <= 0.0 || n[k] <= 1)
                continue;
            const long i = std::lround(d.dot(axes[k]) / len2);
            inside = i >= 0 && i < n[k];
            index[k] = static_cast<int>(i);
            reconstructed += axes[k] * static_cast<double>(i);
        }
        // Nearest sample, and no farther than half the largest step (not extrapolated).
        inside = inside && (p - reconstructed).norm() <= 0.5 * std::max(largest, 1e-12) * 1.7320508;
        s.inside = inside;
        if (inside)
            s.value = f.pressure[f.frame.index(index[0], index[1], index[2])];
        out.push_back(s);
    }
    return out;
}

// --- optimization ---------------------------------------------------------------------------

Outcome<nlohmann::json> OptimizationService::problems(const CallContext& c)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    nlohmann::json out = nlohmann::json::array();
    for (const auto& p : optimization::workspaceOf(s_.project()).problems)
        out.push_back({{"id", p.id},
                       {"name", p.name},
                       {"algorithm", p.algorithm.algorithm},
                       {"variables", p.space.variables.size()},
                       {"objectives", p.objectives.size()},
                       {"constraints", p.constraints.size()}});
    return out;
}

Outcome<nlohmann::json> OptimizationService::runs(const CallContext& c)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    nlohmann::json out = nlohmann::json::array();
    for (const auto& r : optimization::workspaceOf(s_.project()).runs) {
        nlohmann::json j = {{"id", r.runId},
                            {"problem", r.problem.value("id", std::string())},
                            {"algorithm", r.algorithm},
                            {"candidates", r.candidates.size()},
                            {"termination", r.termination.reason}};
        if (r.best) {
            j["best"] = r.candidates[*r.best].id;
            j["bestValues"] = r.candidates[*r.best].objectiveValues;
        }
        out.push_back(j);
    }
    return out;
}

Outcome<JobView> OptimizationService::start(const CallContext& c, const std::string& problemId, int maxEvaluations)
{
    auto a = s_.authorize(c, Permission::Execute);
    if (!a)
        return a.error();
    const optimization::OptimizationWorkspace w = optimization::workspaceOf(s_.project());
    const optimization::OptimizationProblem* found = w.findProblem(problemId);
    if (!found)
        return notFound("the project has no optimization problem '" + problemId + "'", "optimization-problem", problemId);
    optimization::OptimizationProblem problem = *found;
    if (maxEvaluations > 0)
        problem.algorithm.maxEvaluations = maxEvaluations;
    auto evaluator = optimization::AdapterRegistry::instance().bind(problem.adapterId, optimization::designSnapshot(s_.project()), problem.adapterConfig);
    if (!evaluator) {
        ServiceError e = ServiceError::make(ErrorCode::FailedPrecondition, evaluator.message());
        auditStart(s_, a.value(), "OptimizationService.StartOptimization", {problemId}, e);
        return e;
    }
    auto job = std::make_shared<optimization::OptimizationJob>(problem, evaluator.take());
    const Id jobId = s_.host().jobs().submit(job);
    const CallContext context = a.value();
    JobRecord record;
    record.jobId = jobId.str();
    record.type = "optimization";
    record.sessionId = context.sessionId;
    record.onFinished = [job, problem](Session& session, const JobInfo&) {
        const auto run = job->result();
        if (!run)
            return;
        // The run is recorded in the project (the design itself is not changed: promotion is separate).
        optimization::OptimizationWorkspace ws = optimization::workspaceOf(session.project());
        ws.runs.push_back(*run);
        optimization::storeWorkspace(session.project(), ws);
    };
    s_.trackJob(std::move(record));
    auditStart(s_, context, "OptimizationService.StartOptimization", {problemId, jobId.str()});
    if (const auto info = s_.host().jobs().info(jobId))
        return s_.services().jobs.view(*info);
    JobView v;
    v.id = jobId.str();
    return v;
}

// --- workflows ----------------------------------------------------------------------------

Outcome<nlohmann::json> WorkflowService::templates(const CallContext& c)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    nlohmann::json out = nlohmann::json::array();
    for (const automation::Workflow& w : automation::WorkflowLibrary::instance().workflows())
        out.push_back({{"id", w.id},
                       {"name", w.name},
                       {"description", w.description},
                       {"category", w.category},
                       {"source", automation::WorkflowLibrary::instance().sourceOf(w.id)},
                       {"steps", w.steps.size()},
                       {"definition", automation::toJson(w)}});
    return out;
}

Outcome<JobView> WorkflowService::execute(const CallContext& c, const std::string& workflowId, const nlohmann::json& definition,
                                          const nlohmann::json& parameters, bool applyChanges)
{
    auto a = s_.authorize(c, Permission::Execute);
    if (!a)
        return a.error();
    if (applyChanges && !has(a.value().permissions, Permission::Modify))
        return ServiceError::make(ErrorCode::PermissionDenied, "applying a workflow's changes needs the modify permission");
    std::optional<automation::Workflow> workflow;
    if (!definition.is_null() && !definition.empty()) {
        auto parsed = automation::workflowFromJson(definition);
        if (!parsed)
            return invalid(parsed.message(), "definition");
        workflow = parsed.take();
    } else {
        workflow = automation::WorkflowLibrary::instance().find(workflowId);
        if (!workflow)
            return notFound("there is no workflow '" + workflowId + "'", "workflow", workflowId);
    }
    if (auto resolved = automation::resolveParameters(*workflow, parameters.is_null() ? nlohmann::json::object() : parameters); !resolved)
        return invalid(resolved.message(), "parameters");
    // Files a workflow writes stay on the server, in a folder of the run (never a client's path).
    const std::filesystem::path out = std::filesystem::temp_directory_path() / "resonant-workflows" / Id::generate().str();
    auto job = std::make_shared<automation::WorkflowJob>(serializeProject(s_.project()), *workflow,
                                                         parameters.is_null() ? nlohmann::json::object() : parameters, out);
    const Id jobId = s_.host().jobs().submit(job);
    const CallContext context = a.value();
    JobRecord record;
    record.jobId = jobId.str();
    record.type = "workflow";
    record.sessionId = context.sessionId;
    record.onFinished = [job, applyChanges, context](Session& session, const JobInfo& info) {
        const auto run = job->result();
        if (!run)
            return;
        nlohmann::json outcome = automation::toJson(*run);
        if (run->status == automation::RunStatus::Completed && applyChanges) {
            auto loaded = deserializeProject(job->resultingProject());
            if (loaded) {
                loaded.value().project->extensions[automation::kAutomationExtensionKey] = automation::appendHistory(session.project(), *run);
                auto applied = session.applyCopy(context, "WorkflowService.ExecuteWorkflow", *loaded.value().project, "Workflow: " + run->workflowId);
                if (!applied)
                    outcome["applyError"] = applied.error().message;
            }
        }
        if (const JobRecord* r = session.jobRecord(info.id.str()))
            const_cast<JobRecord*>(r)->outcome = outcome;
    };
    s_.trackJob(std::move(record));
    auditStart(s_, context, "WorkflowService.ExecuteWorkflow", {workflow->id, jobId.str()});
    if (const auto info = s_.host().jobs().info(jobId))
        return s_.services().jobs.view(*info);
    JobView v;
    v.id = jobId.str();
    return v;
}

Outcome<std::pair<nlohmann::json, bool>> WorkflowService::run(const CallContext& c, const std::string& jobId)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    const JobRecord* r = s_.jobRecord(jobId);
    if (!r || r->type != "workflow")
        return notFound("there is no workflow job '" + jobId + "'", "job", jobId);
    return std::make_pair(r->outcome, r->handled);
}

}  // namespace resonant::services
