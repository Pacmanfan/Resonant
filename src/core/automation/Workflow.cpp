#include "automation/Workflow.h"

#include "optimization/Expression.h"

#include <set>

namespace resonant::automation {

std::string_view toString(StepStatus status)
{
    switch (status) {
    case StepStatus::Done: return "done";
    case StepStatus::Skipped: return "skipped";
    case StepStatus::Failed: return "failed";
    case StepStatus::Cancelled: return "cancelled";
    }
    return "done";
}

std::string_view toString(RunStatus status)
{
    switch (status) {
    case RunStatus::Completed: return "completed";
    case RunStatus::Failed: return "failed";
    case RunStatus::Cancelled: return "cancelled";
    }
    return "completed";
}

Status Workflow::validate() const
{
    if (id.empty())
        return Status::failure("the workflow has no id");
    if (steps.empty())
        return Status::failure("workflow '" + id + "' has no step");
    std::set<std::string> parameterIds;
    for (const WorkflowParameter& p : parameters) {
        if (p.id.empty() || !parameterIds.insert(p.id).second)
            return Status::failure("workflow '" + id + "': parameter ids must be unique and not empty");
        if (p.id.find('.') != std::string::npos)
            return Status::failure("workflow '" + id + "': parameter '" + p.id + "' must not contain '.'");
    }
    std::set<std::string> seen;
    for (const WorkflowStep& s : steps) {
        if (s.id.empty() || !seen.insert(s.id).second)
            return Status::failure("workflow '" + id + "': step ids must be unique and not empty");
        if (s.action.empty())
            return Status::failure("workflow '" + id + "': step '" + s.id + "' names no action");
        for (const std::string& d : s.dependsOn)
            if (!seen.contains(d) || d == s.id)
                return Status::failure("workflow '" + id + "': step '" + s.id + "' depends on '" + d + "', which is not an earlier step");
        if (!s.when.empty())
            if (auto e = optimization::Expression::parse(s.when); !e)
                return Status::failure("workflow '" + id + "': step '" + s.id + "': " + e.message());
        if (!s.repeatParameter.empty() && !parameterIds.contains(s.repeatParameter))
            return Status::failure("workflow '" + id + "': step '" + s.id + "' repeats over the unknown parameter '" + s.repeatParameter + "'");
    }
    return Status::success();
}

nlohmann::json toJson(const Workflow& w)
{
    nlohmann::json params = nlohmann::json::array();
    for (const WorkflowParameter& p : w.parameters)
        params.push_back({{"id", p.id}, {"label", p.label}, {"type", p.type}, {"default", p.defaultValue}, {"unit", p.unit}, {"description", p.description}});
    nlohmann::json steps = nlohmann::json::array();
    for (const WorkflowStep& s : w.steps) {
        nlohmann::json j{{"id", s.id}, {"action", s.action}, {"params", s.params}};
        if (!s.label.empty())
            j["label"] = s.label;
        if (!s.when.empty())
            j["when"] = s.when;
        if (!s.repeatParameter.empty())
            j["repeat"] = {{"parameter", s.repeatParameter}, {"values", s.repeatValues}};
        if (!s.dependsOn.empty())
            j["dependsOn"] = s.dependsOn;
        if (s.continueOnError)
            j["continueOnError"] = true;
        if (!s.description.empty())
            j["description"] = s.description;
        steps.push_back(std::move(j));
    }
    return {{"format", kWorkflowFormat}, {"version", kWorkflowFormatVersion}, {"id", w.id}, {"name", w.name},
            {"description", w.description}, {"workflowVersion", w.version}, {"category", w.category}, {"requires", w.requirements},
            {"parameters", params}, {"steps", steps}, {"outputs", w.outputs}, {"expectedSeconds", w.expectedSeconds}, {"docs", w.docs}};
}

Result<Workflow> workflowFromJson(const nlohmann::json& j)
{
    using R = Result<Workflow>;
    if (!j.is_object())
        return R::failure("a workflow must be a JSON object");
    if (j.value("format", std::string(kWorkflowFormat)) != kWorkflowFormat)
        return R::failure("not a workflow (format '" + j.value("format", std::string()) + "')");
    if (j.value("version", kWorkflowFormatVersion) > kWorkflowFormatVersion)
        return R::failure("the workflow format version " + std::to_string(j.value("version", 0)) + " is newer than this build reads (" +
                          std::to_string(kWorkflowFormatVersion) + ")");
    try {
        Workflow w;
        w.id = j.at("id").get<std::string>();
        w.name = j.value("name", w.id);
        w.description = j.value("description", std::string());
        w.version = j.value("workflowVersion", std::string("1"));
        w.category = j.value("category", std::string());
        w.requirements = j.value("requires", std::vector<std::string>());
        const nlohmann::json params = j.value("parameters", nlohmann::json::array());
        for (const auto& p : params) {
            WorkflowParameter x;
            x.id = p.at("id").get<std::string>();
            x.label = p.value("label", x.id);
            x.type = p.value("type", std::string("number"));
            x.defaultValue = p.value("default", nlohmann::json());
            x.unit = p.value("unit", std::string());
            x.description = p.value("description", std::string());
            w.parameters.push_back(std::move(x));
        }
        const nlohmann::json steps = j.value("steps", nlohmann::json::array());
        for (const auto& s : steps) {
            WorkflowStep x;
            x.id = s.at("id").get<std::string>();
            x.label = s.value("label", x.id);
            x.action = s.at("action").get<std::string>();
            x.params = s.value("params", nlohmann::json::object());
            x.when = s.value("when", std::string());
            if (s.contains("repeat")) {
                const auto& r = s["repeat"];
                x.repeatParameter = r.value("parameter", std::string());
                x.repeatValues = r.value("values", nlohmann::json());
            }
            x.dependsOn = s.value("dependsOn", std::vector<std::string>());
            x.continueOnError = s.value("continueOnError", false);
            x.description = s.value("description", std::string());
            w.steps.push_back(std::move(x));
        }
        w.outputs = j.value("outputs", std::vector<std::string>());
        w.expectedSeconds = j.value("expectedSeconds", 0.0);
        w.docs = j.value("docs", std::vector<std::string>());
        if (Status s = w.validate(); !s)
            return R::failure(s.message());
        return w;
    } catch (const std::exception& e) {
        return R::failure(std::string("workflow: ") + e.what());
    }
}

nlohmann::json toJson(const WorkflowRun& r)
{
    nlohmann::json steps = nlohmann::json::array();
    for (const StepRecord& s : r.steps) {
        nlohmann::json j{{"step", s.stepId}, {"action", s.action}, {"status", toString(s.status)}, {"outputs", s.outputs}, {"seconds", s.seconds}};
        if (s.iteration >= 0) {
            j["iteration"] = s.iteration;
            j["repeatValue"] = s.repeatValue;
        }
        if (!s.message.empty())
            j["message"] = s.message;
        steps.push_back(std::move(j));
    }
    return {{"format", "resonant.workflow.run"}, {"version", 1}, {"runId", r.runId}, {"workflow", r.workflowId},
            {"workflowVersion", r.workflowVersion}, {"parameters", r.parameters}, {"startedUtc", r.startedUtc}, {"finishedUtc", r.finishedUtc},
            {"status", toString(r.status)}, {"message", r.message}, {"steps", steps}, {"outputs", r.outputs}, {"seconds", r.seconds}};
}

WorkflowRun workflowRunFromJson(const nlohmann::json& j)
{
    WorkflowRun r;
    r.runId = j.value("runId", std::string());
    r.workflowId = j.value("workflow", std::string());
    r.workflowVersion = j.value("workflowVersion", std::string());
    r.parameters = j.value("parameters", nlohmann::json::object());
    r.startedUtc = j.value("startedUtc", std::string());
    r.finishedUtc = j.value("finishedUtc", std::string());
    const std::string status = j.value("status", std::string("completed"));
    r.status = status == "failed" ? RunStatus::Failed : status == "cancelled" ? RunStatus::Cancelled : RunStatus::Completed;
    r.message = j.value("message", std::string());
    const nlohmann::json steps = j.value("steps", nlohmann::json::array());
    for (const auto& s : steps) {
        StepRecord x;
        x.stepId = s.value("step", std::string());
        x.action = s.value("action", std::string());
        const std::string st = s.value("status", std::string("done"));
        x.status = st == "skipped" ? StepStatus::Skipped : st == "failed" ? StepStatus::Failed : st == "cancelled" ? StepStatus::Cancelled : StepStatus::Done;
        x.iteration = s.value("iteration", -1);
        x.repeatValue = s.value("repeatValue", nlohmann::json());
        x.message = s.value("message", std::string());
        x.outputs = s.value("outputs", nlohmann::json::object());
        x.seconds = s.value("seconds", 0.0);
        r.steps.push_back(std::move(x));
    }
    r.outputs = j.value("outputs", nlohmann::json::object());
    r.seconds = j.value("seconds", 0.0);
    return r;
}

}  // namespace resonant::automation
