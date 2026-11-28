// The functional-structure application services (REQ-CHAT-20261126-017, RES-FUNC-014).
#include "services/Services.h"

#include "commands/SceneCommands.h"
#include "coupling/FunctionalAdapter.h"
#include "functional/EffectiveMaterial.h"
#include "functional/FunctionalStudy.h"
#include "optimization/Engine.h"
#include "optimization/Persistence.h"
#include "project/ProjectSerializer.h"

#include <algorithm>
#include <cmath>
#include <mutex>

namespace resonant::services {

using nlohmann::json;
namespace fn = functional;
namespace opt = optimization;

namespace {

void auditRun(Session& s, const CallContext& c, const std::string& operation, const std::vector<std::string>& resources,
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

Outcome<fn::FunctionalStructure> find(const Project& project, const std::string& id)
{
    auto list = fn::structuresOf(project);
    if (!list)
        return ServiceError::make(ErrorCode::FailedPrecondition, "the project's functional structures cannot be read: " + list.message());
    for (const auto& s : list.value())
        if (s.id == id)
            return s;
    return notFound("there is no functional structure '" + id + "'", "functional-structure", id);
}

std::string issuesText(const std::vector<fn::Issue>& issues)
{
    std::string text;
    for (size_t i = 0; i < issues.size() && i < 6; ++i)
        text += (i ? "; " : "") + issues[i].field + ": " + issues[i].message;
    return text;
}

// A study on a copy of the project: the job owns everything it reads.
class FunctionalStudyJob final : public Job {
public:
    using Work = std::function<Result<json>(const Project&, const json&, JobContext&)>;
    FunctionalStudyJob(std::string name, std::shared_ptr<const Project> project, json structure, Work work)
        : name_(std::move(name)), project_(std::move(project)), structure_(std::move(structure)), work_(std::move(work))
    {
    }
    std::string name() const override { return name_; }
    JobEnd run(JobContext& context) override
    {
        auto r = work_(*project_, structure_, context);
        if (!r)
            return r.message() == "cancelled" ? JobEnd::cancelled() : JobEnd::failed(r.message());
        std::lock_guard lock(mutex_);
        result_ = std::make_shared<const json>(r.take());
        return JobEnd::completed();
    }
    std::shared_ptr<const json> result() const
    {
        std::lock_guard lock(mutex_);
        return result_;
    }

private:
    std::string name_;
    std::shared_ptr<const Project> project_;
    json structure_;
    Work work_;
    mutable std::mutex mutex_;
    std::shared_ptr<const json> result_;
};

opt::Sense senseOf(const std::string& s)
{
    if (s == "minimize")
        return opt::Sense::Minimize;
    if (s == "maximize")
        return opt::Sense::Maximize;
    return opt::Sense::Target;
}

}  // namespace

Outcome<json> FunctionalStructureService::analysisKinds(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    json out = json::array();
    for (const fn::AnalysisKind& k : fn::AnalysisRegistry::instance().kinds())
        out.push_back(fn::toJson(k));
    return out;
}

Outcome<std::vector<json>> FunctionalStructureService::structures(const CallContext& c)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto list = fn::structuresOf(s_.project());
    if (!list)
        return ServiceError::make(ErrorCode::FailedPrecondition, list.message());
    std::vector<json> out;
    for (const auto& s : list.value())
        out.push_back(s.toJson());
    return out;
}

Outcome<json> FunctionalStructureService::structure(const CallContext& c, const std::string& id)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto s = find(s_.project(), id);
    if (!s)
        return s.error();
    return s.value().toJson();
}

Outcome<Mutation> FunctionalStructureService::setStructure(const CallContext& c, const json& document)
{
    auto parsed = fn::FunctionalStructure::fromJson(document);
    if (!parsed)
        return invalid(parsed.message(), "definition");
    std::vector<fn::Issue> issues = fn::validateStructure(parsed.value(), s_.project());
    for (auto& i : fn::validateForAnalysis(parsed.value(), fn::AnalysisRegistry::instance()))
        issues.push_back(std::move(i));
    if (!issues.empty())
        return invalid("the functional structure is not valid: " + issuesText(issues), "definition");
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    Project& changed = *copy.value().project;
    std::vector<fn::FunctionalStructure> list = fn::structuresOf(changed).value();
    bool replaced = false;
    for (auto& s : list)
        if (s.id == parsed.value().id) {
            s = parsed.value();
            replaced = true;
        }
    if (!replaced)
        list.push_back(parsed.value());
    fn::storeStructures(changed, list);
    return s_.applyCopy(c, "FunctionalStructureService.SetStructure", changed,
                        (replaced ? "Change functional structure " : "Add functional structure ") + parsed.value().name);
}

Outcome<Mutation> FunctionalStructureService::deleteStructure(const CallContext& c, const std::string& id)
{
    auto list = fn::structuresOf(s_.project());
    if (!list)
        return ServiceError::make(ErrorCode::FailedPrecondition, list.message());
    std::vector<fn::FunctionalStructure> kept;
    for (const auto& s : list.value())
        if (s.id != id)
            kept.push_back(s);
    if (kept.size() == list.value().size())
        return notFound("there is no functional structure '" + id + "'", "functional-structure", id);
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    fn::storeStructures(*copy.value().project, kept);
    return s_.applyCopy(c, "FunctionalStructureService.DeleteStructure", *copy.value().project, "Delete functional structure " + id);
}

Outcome<json> FunctionalStructureService::parameters(const CallContext& c, const std::string& id)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    auto s = find(s_.project(), id);
    if (!s)
        return s.error();
    auto values = fn::resolveParameters(s.value(), s_.project());
    if (!values)
        return ServiceError::make(ErrorCode::FailedPrecondition, values.message());
    json out = json::array();
    for (const auto& p : values.value())
        out.push_back({{"id", p.id},
                       {"name", p.name},
                       {"kind", std::string(fn::toString(p.kind))},
                       {"value", p.value},
                       {"unit", p.unit},
                       {"min", std::isfinite(p.minimum) ? json(p.minimum) : json(nullptr)},
                       {"max", std::isfinite(p.maximum) ? json(p.maximum) : json(nullptr)},
                       {"editable", p.editable},
                       {"binding", p.binding},
                       {"object", p.objectName},
                       {"group", p.group}});
    return out;
}

Outcome<Mutation> FunctionalStructureService::setParameters(const CallContext& c, const std::string& id, const std::map<std::string, double, std::less<>>& values,
                                                            std::optional<std::uint64_t> expectedRevision)
{
    if (expectedRevision && *expectedRevision != s_.projectRevision())
        return ServiceError::make(ErrorCode::Conflict, "the project changed (revision " + std::to_string(s_.projectRevision()) + ", expected " +
                                                           std::to_string(*expectedRevision) + ")");
    auto s = find(s_.project(), id);
    if (!s)
        return s.error();
    if (values.empty())
        return invalid("no parameter values were given", "values");
    auto built = fn::withParameters(s.value(), s_.project(), values);
    if (!built)
        return invalid(built.message(), "values");
    Project& changed = *built.value().first;
    std::vector<fn::FunctionalStructure> list = fn::structuresOf(changed).value();
    for (auto& x : list)
        if (x.id == id)
            x = built.value().second;
    fn::storeStructures(changed, list);
    std::string names;
    for (const auto& [key, value] : values)
        names += (names.empty() ? "" : ", ") + key;
    return s_.applyCopy(c, "FunctionalStructureService.SetParameters", changed, "Set " + names + " of " + s.value().name);
}

Outcome<ValidationView> FunctionalStructureService::validate(const CallContext& c, const std::string& id, const json& document)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    fn::FunctionalStructure s;
    if (document.is_null() || document.empty()) {
        auto found = find(s_.project(), id);
        if (!found)
            return found.error();
        s = found.value();
    } else {
        auto parsed = fn::FunctionalStructure::fromJson(document);
        if (!parsed)
            return invalid(parsed.message(), "definition");
        s = parsed.take();
    }
    ValidationView v;
    std::vector<fn::Issue> issues = fn::validateStructure(s, s_.project());
    for (auto& i : fn::validateForAnalysis(s, fn::AnalysisRegistry::instance()))
        issues.push_back(std::move(i));
    for (const auto& i : issues)
        v.issues.push_back({3, i.field, i.message, s.id});
    if (issues.empty())
        if (auto p = fn::resolveParameters(s, s_.project()); !p)
            v.issues.push_back({3, "parameters", p.message(), s.id});
    v.valid = v.issues.empty();
    return v;
}

Outcome<JobView> FunctionalStructureService::startStudy(const CallContext& c, const std::string& operation, const std::string& id, const std::string& kind,
                                                        std::function<Result<json>(const Project&, const json&, JobContext&)> work)
{
    auto a = s_.authorize(c, Permission::Execute);
    if (!a)
        return a.error();
    auto s = find(s_.project(), id);
    if (!s) {
        auditRun(s_, a.value(), operation, {id}, s.error());
        return s.error();
    }
    std::vector<fn::Issue> issues = fn::validateStructure(s.value(), s_.project());
    for (auto& i : fn::validateForAnalysis(s.value(), fn::AnalysisRegistry::instance()))
        issues.push_back(std::move(i));
    if (!issues.empty()) {
        ServiceError e = invalid("the functional structure is not valid: " + issuesText(issues), "definition");
        auditRun(s_, a.value(), operation, {id}, e);
        return e;
    }
    auto copy = deserializeProject(serializeProject(s_.project()));
    if (!copy)
        return ServiceError::make(ErrorCode::Internal, "the project could not be copied: " + copy.message());
    std::shared_ptr<const Project> snapshot(std::move(copy.value().project));
    auto job = std::make_shared<FunctionalStudyJob>(kind + ": " + s.value().name, snapshot, s.value().toJson(), std::move(work));
    const Id jobId = s_.host().jobs().submit(job);
    const std::uint64_t revision = s_.projectRevision();
    JobRecord record;
    record.jobId = jobId.str();
    record.type = "functional." + kind;
    record.sessionId = a.value().sessionId;
    const std::string name = s.value().name;
    record.onFinished = [job, revision, name, kind](Session& session, const JobInfo& info) {
        const auto doc = job->result();
        if (!doc)
            return;
        json d = *doc;
        d["jobId"] = info.id.str();
        d["projectRevision"] = revision;
        // The project may have changed while the job ran: the result says so.
        const auto stale = fn::resultStaleness(d, session.project(), fn::AnalysisRegistry::instance());
        d["staleOnCompletion"] = !stale.empty();
        if (!stale.empty())
            d["staleReasons"] = fn::toJson(stale);
        ResultEntry entry;
        entry.kind = kResultKind;
        entry.name = name + " (" + kind + ")";
        entry.jobId = info.id.str();
        entry.projectRevision = revision;
        entry.document = std::make_shared<const json>(std::move(d));
        const std::string resultId = session.addResult(std::move(entry));
        if (const JobRecord* r = session.jobRecord(info.id.str()))
            const_cast<JobRecord*>(r)->resultIds.push_back(resultId);
    };
    s_.trackJob(std::move(record));
    auditRun(s_, a.value(), operation, {id, jobId.str()});
    if (const auto info = s_.host().jobs().info(jobId))
        return s_.services().jobs.view(*info);
    JobView v;
    v.id = jobId.str();
    return v;
}

Outcome<JobView> FunctionalStructureService::analyze(const CallContext& c, const std::string& id, const std::string& model)
{
    return startStudy(c, "FunctionalStructureService.Analyze", id, "analysis", [model](const Project& p, const json& doc, JobContext& ctx) -> Result<json> {
        auto s = fn::FunctionalStructure::fromJson(doc);
        if (!s)
            return Result<json>::failure(s.message());
        return fn::analyzeDocument(s.value(), p, fn::AnalysisRegistry::instance(), model, &ctx);
    });
}

Outcome<JobView> FunctionalStructureService::sweep(const CallContext& c, const std::string& id, const json& spec)
{
    std::vector<fn::SweepAxis> axes;
    try {
        for (const json& a : spec.value("axes", json::array())) {
            fn::SweepAxis axis;
            axis.parameter = a.value("parameter", "");
            if (a.contains("values")) {
                axis.values = a["values"].get<std::vector<double>>();
            } else {
                const double from = a.at("from").get<double>(), to = a.at("to").get<double>();
                const int points = a.value("points", 11);
                if (points < 2 || points > 400)
                    return invalid("between 2 and 400 points per axis", "spec.axes");
                for (int i = 0; i < points; ++i)
                    axis.values.push_back(from + (to - from) * i / (points - 1));
            }
            axes.push_back(std::move(axis));
        }
    } catch (const std::exception& e) {
        return invalid(std::string("invalid sweep: ") + e.what(), "spec");
    }
    if (axes.empty())
        return invalid("a sweep needs at least one axis", "spec.axes");
    const std::string model = spec.value("model", "");
    return startStudy(c, "FunctionalStructureService.Sweep", id, "sweep", [axes, model](const Project& p, const json& doc, JobContext& ctx) -> Result<json> {
        auto s = fn::FunctionalStructure::fromJson(doc);
        if (!s)
            return Result<json>::failure(s.message());
        return fn::sweepDocument(s.value(), p, fn::AnalysisRegistry::instance(), axes, model, &ctx);
    });
}

Outcome<JobView> FunctionalStructureService::sensitivity(const CallContext& c, const std::string& id, const json& spec)
{
    std::vector<std::string> parameters;
    try {
        parameters = spec.value("parameters", std::vector<std::string>{});
    } catch (const std::exception& e) {
        return invalid(std::string("invalid sensitivity request: ") + e.what(), "spec");
    }
    const double step = spec.value("relativeStep", 1e-3);
    const std::string model = spec.value("model", "");
    return startStudy(c, "FunctionalStructureService.ComputeSensitivity", id, "sensitivity",
                      [parameters, step, model](const Project& p, const json& doc, JobContext& ctx) -> Result<json> {
                          auto s = fn::FunctionalStructure::fromJson(doc);
                          if (!s)
                              return Result<json>::failure(s.message());
                          return fn::sensitivityDocument(s.value(), p, fn::AnalysisRegistry::instance(), parameters, step, model, &ctx);
                      });
}

Outcome<json> FunctionalStructureService::result(const CallContext& c, const std::string& resultId)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    const ResultEntry* entry = s_.result(resultId);
    if (!entry || entry->kind != kResultKind || !entry->document)
        return notFound("there is no functional result '" + resultId + "'", "result", resultId);
    json d = *entry->document;
    const auto stale = fn::resultStaleness(d, s_.project(), fn::AnalysisRegistry::instance());
    d["stale"] = !stale.empty();
    d["currentStaleness"] = fn::toJson(stale);
    d["resultId"] = resultId;
    return d;
}

Outcome<JobView> FunctionalStructureService::optimize(const CallContext& c, const std::string& id, const json& spec)
{
    auto a = s_.authorize(c, Permission::Execute);
    if (!a)
        return a.error();
    auto s = find(s_.project(), id);
    if (!s)
        return s.error();
    if (s.value().objectives.empty())
        return invalid("the structure has no objectives to optimize", "objectives");
    const fn::AnalysisKind* kind = fn::AnalysisRegistry::instance().find(s.value().analysis.kind);
    if (!kind)
        return invalid("no analysis '" + s.value().analysis.kind + "'", "analysis.kind");
    const std::string modelId = spec.value("model", s.value().analysis.model);
    const fn::ModelInfo* model = kind->model(modelId);
    if (!model)
        return invalid("the analysis has no model '" + modelId + "'", "spec.model");
    const json config{{"structure", id}};
    auto evaluator = opt::AdapterRegistry::instance().bind(coupling::kFunctionalAdapterId, opt::designSnapshot(s_.project()), config);
    if (!evaluator)
        return ServiceError::make(ErrorCode::FailedPrecondition, evaluator.message());
    const opt::Capabilities& caps = evaluator.value()->capabilities();
    // The baseline scales the objectives (minimize / maximize) and checks that the design evaluates.
    std::map<std::string, double, std::less<>> initial;
    for (const auto& v : caps.variables)
        initial[v.key()] = v.initial;
    const opt::Evaluation baseline = evaluator.value()->evaluate(initial, model->id, nullptr);
    if (!baseline.ok())
        return ServiceError::make(ErrorCode::FailedPrecondition, "the present design cannot be evaluated: " + baseline.message);
    opt::OptimizationProblem problem;
    problem.id = "fs-opt-" + Id::generate().str().substr(0, 8);
    problem.name = "Optimize " + s.value().name;
    problem.description = "Generated from the objectives and constraints of the functional structure '" + id + "'.";
    problem.adapterId = coupling::kFunctionalAdapterId;
    problem.adapterConfig = config;
    problem.modelId = model->id;
    problem.space.variables = caps.variables;
    for (const fn::Objective& o : s.value().objectives) {
        opt::Objective x;
        x.id = o.id;
        x.label = o.description.empty() ? o.metric : o.description;
        x.quantity = o.metric;
        x.sense = senseOf(o.sense);
        x.weight = o.weight;
        if (o.sense == "within") {
            x.target = 0.5 * (o.low + o.high);
            x.scale = 0.5 * (o.high - o.low);
        } else if (o.sense == "target") {
            x.target = o.target;
            x.scale = std::max(std::abs(o.target), 1e-300);
        } else {
            const auto it = baseline.quantities.find(o.metric);
            x.scale = it != baseline.quantities.end() && std::abs(it->second) > 0.0 ? std::abs(it->second) : 1.0;
        }
        problem.objectives.push_back(x);
    }
    for (const fn::Constraint& k : s.value().constraints) {
        opt::DesignConstraint x;
        x.id = k.id;
        x.label = k.description.empty() ? k.expression : k.description;
        x.quantity = "constraint." + k.id;
        x.relation = k.relation == ">=" ? opt::Relation::AtLeast : opt::Relation::AtMost;
        x.limit = k.limit;
        x.hard = k.hard;
        x.scale = std::max(std::abs(k.limit), 1e-300);
        problem.constraints.push_back(x);
    }
    problem.algorithm.algorithm = spec.value("algorithm", problem.objectives.size() > 1 ? std::string("nsga2") : std::string("nelder-mead"));
    problem.algorithm.maxEvaluations = std::clamp(spec.value("maxEvaluations", 40), 2, 5000);
    problem.algorithm.seed = spec.value("seed", 1ull);
    problem.fillUnits(caps);
    std::vector<std::string> warnings;
    if (Status st = problem.validateAgainst(caps, &warnings); !st)
        return invalid("the optimization problem is not valid: " + st.message(), "spec");
    auto job = std::make_shared<opt::OptimizationJob>(problem, evaluator.take());
    const Id jobId = s_.host().jobs().submit(job);
    JobRecord record;
    record.jobId = jobId.str();
    record.type = "functional.optimization";
    record.sessionId = a.value().sessionId;
    record.onFinished = [job, problem](Session& session, const JobInfo&) {
        const auto run = job->result();
        if (!run)
            return;
        // The run and its problem are recorded in the project's optimization workspace (the design is not changed).
        opt::OptimizationWorkspace ws = opt::workspaceOf(session.project());
        if (!ws.findProblem(problem.id))
            ws.problems.push_back(problem);
        ws.runs.push_back(*run);
        opt::storeWorkspace(session.project(), ws);
    };
    s_.trackJob(std::move(record));
    auditRun(s_, a.value(), "FunctionalStructureService.Optimize", {id, jobId.str()});
    if (const auto info = s_.host().jobs().info(jobId))
        return s_.services().jobs.view(*info);
    JobView v;
    v.id = jobId.str();
    return v;
}

Outcome<json> FunctionalStructureService::explain(const CallContext& c, const std::string& runId, int count)
{
    if (auto a = s_.authorize(c, Permission::Read); !a)
        return a.error();
    const opt::OptimizationWorkspace ws = opt::workspaceOf(s_.project());
    const opt::RunRecord* run = ws.findRun(runId);
    if (!run)
        return notFound("there is no optimization run '" + runId + "'", "optimization-run", runId);
    auto problem = opt::problemFromJson(run->problem);
    if (!problem)
        return ServiceError::make(ErrorCode::FailedPrecondition, "the run's problem cannot be read: " + problem.message());
    const std::string id = problem.value().adapterConfig.value("structure", "");
    if (problem.value().adapterId != coupling::kFunctionalAdapterId || id.empty())
        return invalid("the run did not optimize a functional structure", "runId");
    auto s = find(s_.project(), id);
    if (!s)
        return s.error();
    const fn::AnalysisKind* kind = fn::AnalysisRegistry::instance().find(s.value().analysis.kind);
    if (!kind)
        return invalid("no analysis '" + s.value().analysis.kind + "'", "analysis.kind");
    auto parameters = fn::resolveParameters(s.value(), s_.project());
    if (!parameters)
        return ServiceError::make(ErrorCode::FailedPrecondition, parameters.message());
    const fn::Values baseP = fn::valuesOf(parameters.value());
    auto baseline = fn::evaluateStructure(s.value(), s_.project(), fn::AnalysisRegistry::instance(), problem.value().modelId, nullptr);
    if (!baseline)
        return ServiceError::make(ErrorCode::FailedPrecondition, "the present design cannot be evaluated: " + baseline.message());
    std::vector<size_t> order;
    for (size_t i = 0; i < run->candidates.size(); ++i)
        if (run->candidates[i].feasible && run->candidates[i].evaluation.ok())
            order.push_back(i);
    std::sort(order.begin(), order.end(), [&](size_t x, size_t y) { return run->candidates[x].weightedScore < run->candidates[y].weightedScore; });
    json candidates = json::array();
    for (size_t n = 0; n < order.size() && static_cast<int>(n) < std::max(1, count); ++n) {
        const opt::Candidate& k = run->candidates[order[n]];
        fn::Values candP = baseP;
        for (const auto& [variable, value] : k.point.values)
            candP[variable] = value;
        fn::Values candM;
        for (const auto& [key, value] : k.evaluation.quantities)
            if (key.rfind("constraint.", 0) != 0)
                candM[key] = value;
        candidates.push_back({{"candidate", k.id},
                              {"values", k.point.values},
                              {"weightedScore", k.weightedScore},
                              {"feasible", k.feasible},
                              {"metrics", candM},
                              {"model", k.evaluation.model.modelId},
                              {"explanation", fn::explainCandidate(s.value(), s_.project(), *kind, baseP, baseline.value().metrics, candP, candM, nullptr)}});
    }
    return json{{"run", runId}, {"structure", id}, {"evaluations", run->candidates.size()}, {"candidates", candidates},
                {"note", "each candidate's values were computed by the named model for that candidate's own design"}};
}

Outcome<Mutation> FunctionalStructureService::createEffectiveMaterial(const CallContext& c, const std::string& resultId, const json& options)
{
    if (auto a = s_.authorize(c, Permission::Modify); !a)
        return a.error();
    if (!options.is_null() && !options.is_object())
        return invalid("options must be an object", "options");
    const ResultEntry* entry = s_.result(resultId);
    if (!entry || entry->kind != kResultKind || !entry->document)
        return notFound("there is no functional result '" + resultId + "'", "result", resultId);
    const json& d = *entry->document;
    if (d.value("kind", "") != "analysis")
        return invalid("an effective material comes from an analysis result, not from a " + d.value("kind", std::string("?")), "resultId");
    const json* record = nullptr;
    if (const auto details = d.find("details"); details != d.end() && details->is_object())
        if (const auto e = details->find("effectiveMaterial"); e != details->end())
            record = &*e;
    if (!record)
        return ServiceError::make(ErrorCode::FailedPrecondition,
                                  "the analysis reported no effective material: it is not an effective-medium analysis, or the structure lies "
                                  "outside the validity of its effective description (see the result's warnings)");
    if (const auto stale = fn::resultStaleness(d, s_.project(), fn::AnalysisRegistry::instance()); !stale.empty())
        return ServiceError::make(ErrorCode::FailedPrecondition, "the result is stale (" + stale.front().what + ": " + stale.front().detail +
                                                                     "): run the analysis again before saving its effective material");
    auto effective = fn::EffectiveMaterial::fromJson(*record);
    if (!effective)
        return invalid(effective.message(), "details.effectiveMaterial");
    const json o = options.is_object() ? options : json::object();
    const Id id = o.contains("id") && o["id"].is_string() && !o["id"].get<std::string>().empty() ? Id(o["id"].get<std::string>()) : Id::generate();
    const std::string reference = "result " + resultId + " (" + d.value("analysis", json::object()).value("kind", std::string()) + ", project revision " +
                                  std::to_string(entry->projectRevision) + ")";
    auto material = fn::toLibraryMaterial(effective.value(), id, o.value("name", std::string()), o.value("components", json()), reference);
    if (!material)
        return invalid(material.message(), "options");
    const Material m = material.take();
    return s_.mutate(c, "FunctionalStructureService.CreateEffectiveMaterial", {m.id()}, std::nullopt, [m](Project& target) -> Outcome<std::unique_ptr<Command>> {
        if (target.materials.find(m.id()))
            return ServiceError::make(ErrorCode::AlreadyExists, "a material with the id '" + m.id().str() + "' exists");
        MaterialLibrary library = target.materials;
        if (Status st = library.add(m); !st)
            return invalid(st.message(), "options");
        std::unique_ptr<Command> command =
            std::make_unique<SetMaterialLibraryCommand>(target.materials, std::move(library), "Add effective material " + m.name());
        return command;
    });
}

}  // namespace resonant::services
