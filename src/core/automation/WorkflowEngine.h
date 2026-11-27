#pragma once

// The workflow engine (RES-AUTO-005..008).
//
// Actions are named operations registered by the domains (in
// src/core/coupling: scene construction, settings, acoustic fields and
// focusing, modal and forced response, antenna and array patterns,
// optimization, export and reports). They call the same functions the
// application's panels call; there is no automation-only physics.
//
// runWorkflow() executes the steps of a workflow in order on a project:
// it resolves parameter references, evaluates conditions, repeats steps over
// lists, checks dependencies, stops at the first failed step unless that step
// says otherwise (a failure is never silently passed over), honours
// cancellation between steps and inside long actions, reports progress and
// returns a run record. The project it is given is changed by the steps that
// build or modify the design; callers that must keep their design pass a copy.
#include "automation/Workflow.h"
#include "simulation/Job.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace resonant {
class Project;
}

namespace resonant::automation {

struct ActionContext {
    Project& project;
    JobContext* job = nullptr;                   // cancellation and progress of long actions
    const nlohmann::json& outputs;               // outputs of the earlier steps, by step id
    std::filesystem::path outputDirectory;       // where files are written (relative paths resolve here)
    bool cancelled() const { return job && job->cancelRequested(); }
};

struct ActionInfo {
    std::string id;
    std::string category;
    std::string description;
    nlohmann::json parameters = nlohmann::json::object();  // name -> description
    std::vector<std::string> outputs;
};

class ActionRegistry {
public:
    using Run = std::function<Result<nlohmann::json>(ActionContext& context, const nlohmann::json& params)>;
    // Every domain action (defined in src/core/coupling/WorkflowActions.cpp).
    static ActionRegistry& instance();

    void add(ActionInfo info, Run run);
    bool knows(std::string_view id) const;
    const ActionInfo* info(std::string_view id) const;
    std::vector<ActionInfo> actions() const;
    Result<nlohmann::json> run(std::string_view id, ActionContext& context, const nlohmann::json& params) const;

private:
    std::map<std::string, std::pair<ActionInfo, Run>, std::less<>> actions_;
};

// Workflow templates: built in (src/core/coupling/WorkflowTemplates.cpp) and
// added by workflow plugins.
class WorkflowLibrary {
public:
    static WorkflowLibrary& instance();
    void add(Workflow workflow, std::string source);   // replaces a workflow with the same id
    std::vector<Workflow> workflows() const;
    std::optional<Workflow> find(std::string_view id) const;
    std::string sourceOf(std::string_view id) const;

private:
    mutable std::mutex mutex_;
    std::vector<std::pair<Workflow, std::string>> workflows_;
};

// The workflow's parameter values: defaults overridden by `values`; an
// unknown parameter name or a value of the wrong type is refused.
Result<nlohmann::json> resolveParameters(const Workflow& workflow, const nlohmann::json& values);
// Replaces "${...}" references in a parameter document.
Result<nlohmann::json> substitute(const nlohmann::json& params, const nlohmann::json& parameters, const nlohmann::json& outputs);
// Whether every action and capability the workflow requires is present
// (`capabilities`: extra capability names such as "python").
Status checkRequirements(const Workflow& workflow, const std::vector<std::string>& capabilities = {});

Result<WorkflowRun> runWorkflow(Project& project, const Workflow& workflow, const nlohmann::json& parameterValues = nlohmann::json::object(),
                                JobContext* context = nullptr, const std::filesystem::path& outputDirectory = {});

// A workflow as a background job on its own copy of a project (a serialized
// snapshot); the changed project is available afterwards, never applied to
// the document by the job.
class WorkflowJob final : public resonant::Job {
public:
    WorkflowJob(nlohmann::json projectSnapshot, Workflow workflow, nlohmann::json parameters, std::filesystem::path outputDirectory = {});
    std::string name() const override { return "Workflow: " + workflow_.name; }
    JobEnd run(JobContext& context) override;
    std::optional<WorkflowRun> result() const;
    nlohmann::json resultingProject() const;  // the snapshot after the run (null before)

private:
    nlohmann::json snapshot_;
    Workflow workflow_;
    nlohmann::json parameters_;
    std::filesystem::path outputDirectory_;
    mutable std::mutex mutex_;
    std::optional<WorkflowRun> result_;
    nlohmann::json resultingProject_;
};

// Run history in the project's extensions ("automation": {version 1, runs}),
// newest last, at most `keep` runs.
inline constexpr const char* kAutomationExtensionKey = "automation";
std::vector<WorkflowRun> workflowHistory(const Project& project);
nlohmann::json appendHistory(const Project& project, const WorkflowRun& run, size_t keep = 50);  // the new extension value

}  // namespace resonant::automation
