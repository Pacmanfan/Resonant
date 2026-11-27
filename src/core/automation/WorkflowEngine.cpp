#include "automation/WorkflowEngine.h"

#include "optimization/Engine.h"
#include "optimization/Expression.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace resonant::automation {

// --- registries ----------------------------------------------------------------------------------------

void ActionRegistry::add(ActionInfo info, Run run)
{
    std::string id = info.id;
    actions_[std::move(id)] = {std::move(info), std::move(run)};
}

bool ActionRegistry::knows(std::string_view id) const { return actions_.find(id) != actions_.end(); }

const ActionInfo* ActionRegistry::info(std::string_view id) const
{
    const auto it = actions_.find(id);
    return it == actions_.end() ? nullptr : &it->second.first;
}

std::vector<ActionInfo> ActionRegistry::actions() const
{
    std::vector<ActionInfo> out;
    for (const auto& [id, entry] : actions_)
        out.push_back(entry.first);
    return out;
}

Result<nlohmann::json> ActionRegistry::run(std::string_view id, ActionContext& context, const nlohmann::json& params) const
{
    const auto it = actions_.find(id);
    if (it == actions_.end())
        return Result<nlohmann::json>::failure("no action '" + std::string(id) + "'");
    try {
        return it->second.second(context, params);
    } catch (const std::exception& e) {
        return Result<nlohmann::json>::failure(std::string("action '") + std::string(id) + "' failed: " + e.what());
    }
}

void WorkflowLibrary::add(Workflow workflow, std::string source)
{
    std::lock_guard lock(mutex_);
    for (auto& [w, s] : workflows_)
        if (w.id == workflow.id) {
            w = std::move(workflow);
            s = std::move(source);
            return;
        }
    workflows_.emplace_back(std::move(workflow), std::move(source));
}

std::vector<Workflow> WorkflowLibrary::workflows() const
{
    std::lock_guard lock(mutex_);
    std::vector<Workflow> out;
    for (const auto& [w, s] : workflows_)
        out.push_back(w);
    return out;
}

std::optional<Workflow> WorkflowLibrary::find(std::string_view id) const
{
    std::lock_guard lock(mutex_);
    for (const auto& [w, s] : workflows_)
        if (w.id == id)
            return w;
    return std::nullopt;
}

std::string WorkflowLibrary::sourceOf(std::string_view id) const
{
    std::lock_guard lock(mutex_);
    for (const auto& [w, s] : workflows_)
        if (w.id == id)
            return s;
    return {};
}

// --- parameters and references -----------------------------------------------------------------------------

namespace {

bool typeMatches(const std::string& type, const nlohmann::json& v)
{
    if (type == "number")
        return v.is_number();
    if (type == "integer")
        return v.is_number_integer() || (v.is_number() && std::floor(v.get<double>()) == v.get<double>());
    if (type == "string")
        return v.is_string();
    if (type == "bool")
        return v.is_boolean();
    if (type == "list")
        return v.is_array();
    return true;  // json
}

// Looks up "name" or "steps.<id>.<key>[.<key>...]".
std::optional<nlohmann::json> lookup(const std::string& reference, const nlohmann::json& parameters, const nlohmann::json& outputs)
{
    if (reference.rfind("steps.", 0) == 0) {
        const nlohmann::json* node = &outputs;
        size_t start = 6;
        while (start <= reference.size()) {
            const size_t dot = reference.find('.', start);
            const std::string key = reference.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
            if (node->is_object() && node->contains(key))
                node = &(*node)[key];
            else if (node->is_array() && !key.empty() && std::all_of(key.begin(), key.end(), ::isdigit) && std::stoul(key) < node->size())
                node = &(*node)[std::stoul(key)];
            else
                return std::nullopt;
            if (dot == std::string::npos)
                break;
            start = dot + 1;
        }
        return *node;
    }
    if (parameters.is_object() && parameters.contains(reference))
        return parameters[reference];
    return std::nullopt;
}

void flatten(const std::string& prefix, const nlohmann::json& value, std::map<std::string, double, std::less<>>& out)
{
    if (value.is_number())
        out[prefix] = value.get<double>();
    else if (value.is_boolean())
        out[prefix] = value.get<bool>() ? 1.0 : 0.0;
    else if (value.is_object())
        for (const auto& [k, v] : value.items())
            flatten(prefix.empty() ? k : prefix + "." + k, v, out);
}

}  // namespace

Result<nlohmann::json> resolveParameters(const Workflow& workflow, const nlohmann::json& values)
{
    using R = Result<nlohmann::json>;
    nlohmann::json out = nlohmann::json::object();
    for (const WorkflowParameter& p : workflow.parameters)
        out[p.id] = p.defaultValue;
    if (!values.is_null() && !values.is_object())
        return R::failure("workflow parameters must be given as an object");
    if (values.is_object())
        for (const auto& [k, v] : values.items()) {
            const auto it = std::find_if(workflow.parameters.begin(), workflow.parameters.end(), [&](const WorkflowParameter& p) { return p.id == k; });
            if (it == workflow.parameters.end())
                return R::failure("workflow '" + workflow.id + "' has no parameter '" + k + "'");
            if (!typeMatches(it->type, v))
                return R::failure("parameter '" + k + "' must be of type " + it->type);
            out[k] = v;
        }
    for (const WorkflowParameter& p : workflow.parameters)
        if (out[p.id].is_null())
            return R::failure("parameter '" + p.id + "' has no value and no default");
    return out;
}

Result<nlohmann::json> substitute(const nlohmann::json& params, const nlohmann::json& parameters, const nlohmann::json& outputs)
{
    using R = Result<nlohmann::json>;
    if (params.is_string()) {
        const std::string s = params.get<std::string>();
        // The whole string is one reference: keep the value's type.
        if (s.size() > 3 && s.rfind("${", 0) == 0 && s.back() == '}' && s.find("${", 2) == std::string::npos) {
            const std::string ref = s.substr(2, s.size() - 3);
            auto v = lookup(ref, parameters, outputs);
            if (!v)
                return R::failure("unknown reference '${" + ref + "}'");
            return *v;
        }
        std::string out;
        size_t pos = 0;
        while (true) {
            const size_t open = s.find("${", pos);
            if (open == std::string::npos) {
                out += s.substr(pos);
                break;
            }
            const size_t close = s.find('}', open);
            if (close == std::string::npos)
                return R::failure("unterminated reference in '" + s + "'");
            out += s.substr(pos, open - pos);
            const std::string ref = s.substr(open + 2, close - open - 2);
            auto v = lookup(ref, parameters, outputs);
            if (!v)
                return R::failure("unknown reference '${" + ref + "}'");
            out += v->is_string() ? v->get<std::string>() : v->dump();
            pos = close + 1;
        }
        return nlohmann::json(out);
    }
    if (params.is_object()) {
        nlohmann::json out = nlohmann::json::object();
        for (const auto& [k, v] : params.items()) {
            auto r = substitute(v, parameters, outputs);
            if (!r)
                return r;
            out[k] = r.take();
        }
        return out;
    }
    if (params.is_array()) {
        nlohmann::json out = nlohmann::json::array();
        for (const auto& v : params) {
            auto r = substitute(v, parameters, outputs);
            if (!r)
                return r;
            out.push_back(r.take());
        }
        return out;
    }
    return params;
}

Status checkRequirements(const Workflow& workflow, const std::vector<std::string>& capabilities)
{
    const ActionRegistry& registry = ActionRegistry::instance();
    for (const WorkflowStep& s : workflow.steps)
        if (!registry.knows(s.action))
            return Status::failure("workflow '" + workflow.id + "' uses the action '" + s.action + "', which this build does not have");
    for (const std::string& r : workflow.requirements)
        if (!registry.knows(r) && std::find(capabilities.begin(), capabilities.end(), r) == capabilities.end())
            return Status::failure("workflow '" + workflow.id + "' requires '" + r + "', which is not available");
    return Status::success();
}

// --- execution ----------------------------------------------------------------------------------------------

Result<WorkflowRun> runWorkflow(Project& project, const Workflow& workflow, const nlohmann::json& parameterValues, JobContext* context,
                                const std::filesystem::path& outputDirectory)
{
    using R = Result<WorkflowRun>;
    if (Status s = workflow.validate(); !s)
        return R::failure(s.message());
    if (Status s = checkRequirements(workflow); !s)
        return R::failure(s.message());
    auto parameters = resolveParameters(workflow, parameterValues);
    if (!parameters)
        return R::failure(parameters.message());

    WorkflowRun run;
    run.runId = "WF-" + optimization::newRunId().substr(4);
    run.workflowId = workflow.id;
    run.workflowVersion = workflow.version;
    run.parameters = parameters.value();
    run.startedUtc = optimization::utcNow();
    const auto t0 = std::chrono::steady_clock::now();
    std::map<std::string, bool> succeeded;
    const ActionRegistry& registry = ActionRegistry::instance();
    bool stopped = false;
    for (size_t index = 0; index < workflow.steps.size() && !stopped; ++index) {
        const WorkflowStep& step = workflow.steps[index];
        if (context) {
            if (context->cancelRequested()) {
                run.status = RunStatus::Cancelled;
                run.message = "cancelled before step '" + step.id + "'";
                break;
            }
            context->reportProgress(static_cast<double>(index) / workflow.steps.size(), step.label.empty() ? step.id : step.label);
        }
        StepRecord base;
        base.stepId = step.id;
        base.action = step.action;
        // Dependencies.
        bool blocked = false;
        for (const std::string& d : step.dependsOn)
            if (!succeeded[d]) {
                base.status = StepStatus::Skipped;
                base.message = "skipped: the step it depends on ('" + d + "') did not succeed";
                blocked = true;
            }
        // Condition.
        if (!blocked && !step.when.empty()) {
            std::map<std::string, double, std::less<>> values;
            flatten("", run.parameters, values);
            flatten("steps", run.outputs, values);
            auto e = optimization::Expression::parse(step.when);
            auto v = e ? e.value().evaluate(values) : Result<double>::failure(e.message());
            if (!v) {
                base.status = StepStatus::Failed;
                base.message = "the condition '" + step.when + "' could not be evaluated: " + v.message();
                run.steps.push_back(base);
                succeeded[step.id] = false;
                if (!step.continueOnError) {
                    run.status = RunStatus::Failed;
                    run.message = "step '" + step.id + "': " + base.message;
                    stopped = true;
                }
                continue;
            }
            if (v.value() == 0.0) {
                base.status = StepStatus::Skipped;
                base.message = "skipped: the condition '" + step.when + "' is false";
                blocked = true;
            }
        }
        if (blocked) {
            run.steps.push_back(base);
            succeeded[step.id] = false;
            continue;
        }
        // Repetition.
        std::vector<nlohmann::json> values;
        bool repeated = !step.repeatParameter.empty();
        if (repeated) {
            const nlohmann::json list = step.repeatValues.is_array() ? step.repeatValues : run.parameters[step.repeatParameter];
            if (!list.is_array() || list.empty()) {
                base.status = StepStatus::Failed;
                base.message = "nothing to repeat over: '" + step.repeatParameter + "' has no list of values";
                run.steps.push_back(base);
                succeeded[step.id] = false;
                if (!step.continueOnError) {
                    run.status = RunStatus::Failed;
                    run.message = "step '" + step.id + "': " + base.message;
                    stopped = true;
                }
                continue;
            }
            values.assign(list.begin(), list.end());
        } else {
            values.push_back(nlohmann::json());
        }
        nlohmann::json collected = repeated ? nlohmann::json::array() : nlohmann::json::object();
        bool ok = true;
        for (size_t k = 0; k < values.size(); ++k) {
            if (context && context->cancelRequested()) {
                StepRecord r = base;
                r.status = StepStatus::Cancelled;
                r.message = "cancelled";
                run.steps.push_back(r);
                run.status = RunStatus::Cancelled;
                run.message = "cancelled in step '" + step.id + "'";
                stopped = true;
                ok = false;
                break;
            }
            nlohmann::json localParameters = run.parameters;
            if (repeated)
                localParameters[step.repeatParameter] = values[k];
            StepRecord r = base;
            r.iteration = repeated ? static_cast<int>(k) : -1;
            r.repeatValue = repeated ? values[k] : nlohmann::json();
            const auto ts = std::chrono::steady_clock::now();
            auto params = substitute(step.params, localParameters, run.outputs);
            Result<nlohmann::json> out = params ? Result<nlohmann::json>(nlohmann::json()) : Result<nlohmann::json>::failure(params.message());
            if (params) {
                ActionContext ac{project, context, run.outputs, outputDirectory};
                out = registry.run(step.action, ac, params.value());
            }
            r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - ts).count();
            if (!out) {
                const bool cancelled = context && context->cancelRequested();
                r.status = cancelled ? StepStatus::Cancelled : StepStatus::Failed;
                r.message = out.message();
                run.steps.push_back(r);
                ok = false;
                if (cancelled) {
                    run.status = RunStatus::Cancelled;
                    run.message = "cancelled in step '" + step.id + "'";
                    stopped = true;
                }
                break;
            }
            r.status = StepStatus::Done;
            r.outputs = out.value().is_object() ? out.value() : nlohmann::json{{"value", out.value()}};
            run.steps.push_back(r);
            if (repeated)
                collected.push_back(r.outputs);
            else
                collected = r.outputs;
        }
        succeeded[step.id] = ok;
        if (ok)
            run.outputs[step.id] = collected;
        else if (!stopped && !step.continueOnError) {
            run.status = RunStatus::Failed;
            run.message = "step '" + step.id + "' failed: " + run.steps.back().message;
            stopped = true;
        }
    }
    run.finishedUtc = optimization::utcNow();
    run.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (context && run.status == RunStatus::Completed)
        context->reportProgress(1.0, "completed");
    return run;
}

// --- job ---------------------------------------------------------------------------------------------------

WorkflowJob::WorkflowJob(nlohmann::json projectSnapshot, Workflow workflow, nlohmann::json parameters, std::filesystem::path outputDirectory)
    : snapshot_(std::move(projectSnapshot)), workflow_(std::move(workflow)), parameters_(std::move(parameters)),
      outputDirectory_(std::move(outputDirectory))
{
}

JobEnd WorkflowJob::run(JobContext& context)
{
    auto loaded = deserializeProject(snapshot_);
    if (!loaded)
        return JobEnd::failed("the project snapshot could not be read: " + loaded.message());
    Project& project = *loaded.value().project;
    auto run = runWorkflow(project, workflow_, parameters_, &context, outputDirectory_);
    if (!run)
        return JobEnd::failed(run.message());
    const RunStatus status = run.value().status;
    const std::string message = run.value().message;
    {
        std::lock_guard lock(mutex_);
        result_ = run.take();
        resultingProject_ = serializeProject(project);
    }
    if (status == RunStatus::Cancelled)
        return JobEnd::cancelled();
    if (status == RunStatus::Failed)
        return JobEnd::failed(message);
    return JobEnd::completed("workflow completed");
}

std::optional<WorkflowRun> WorkflowJob::result() const
{
    std::lock_guard lock(mutex_);
    return result_;
}

nlohmann::json WorkflowJob::resultingProject() const
{
    std::lock_guard lock(mutex_);
    return resultingProject_;
}

std::vector<WorkflowRun> workflowHistory(const Project& project)
{
    std::vector<WorkflowRun> out;
    if (!project.extensions.contains(kAutomationExtensionKey))
        return out;
    const nlohmann::json& a = project.extensions[kAutomationExtensionKey];
    if (!a.is_object() || !a.contains("runs") || !a["runs"].is_array())
        return out;
    for (const auto& r : a["runs"])
        out.push_back(workflowRunFromJson(r));
    return out;
}

nlohmann::json appendHistory(const Project& project, const WorkflowRun& run, size_t keep)
{
    nlohmann::json runs = nlohmann::json::array();
    for (const WorkflowRun& r : workflowHistory(project))
        runs.push_back(toJson(r));
    runs.push_back(toJson(run));
    while (runs.size() > keep)
        runs.erase(runs.begin());
    return {{"version", 1}, {"runs", runs}};
}

}  // namespace resonant::automation
