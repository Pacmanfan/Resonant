#pragma once

// Workflow definitions (RES-AUTO-001..004): a versioned, serializable
// description of a repeatable engineering procedure ("resonant.workflow",
// version 1) that the workflow engine executes with the same application
// functions an interactive user reaches (actions registered by the domains).
//
//   id, name, description, version     identity
//   requires                           capabilities (action ids, Python, ...) it needs
//   parameters                         typed inputs with defaults and units
//   steps                              actions with parameters, conditions,
//                                      repetition, dependencies and error policy
//   outputs                            what the run reports
//   expectedSeconds, docs              expected runtime and help-topic ids
//
// Step parameters may refer to workflow parameters and to the outputs of
// earlier steps: a string "${name}" is replaced by the value (keeping its
// type when the whole string is the reference), "${steps.<id>.<key>}" by an
// output. Conditions ("when") are arithmetic expressions over numeric
// parameters and outputs (optimization/Expression); a step runs when the
// expression is non-zero.
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant::automation {

inline constexpr const char* kWorkflowFormat = "resonant.workflow";
inline constexpr int kWorkflowFormatVersion = 1;

struct WorkflowParameter {
    std::string id;
    std::string label;
    std::string type = "number";  // number, integer, string, bool, list, json
    nlohmann::json defaultValue;
    std::string unit;
    std::string description;
};

struct WorkflowStep {
    std::string id;
    std::string label;
    std::string action;                        // registered action id
    nlohmann::json params = nlohmann::json::object();
    std::string when;                          // condition; empty = always
    std::string repeatParameter;               // run once per value of this parameter ...
    nlohmann::json repeatValues;               // ... taken from this list (or the parameter's list value)
    std::vector<std::string> dependsOn;        // steps that must have succeeded
    bool continueOnError = false;              // false: a failure stops the workflow
    std::string description;
};

struct Workflow {
    std::string id;
    std::string name;
    std::string description;
    std::string version = "1";
    std::vector<std::string> requirements;
    std::vector<WorkflowParameter> parameters;
    std::vector<WorkflowStep> steps;
    std::vector<std::string> outputs;          // "steps.<id>.<key>" or step ids
    double expectedSeconds = 0.0;
    std::vector<std::string> docs;             // help-topic ids
    std::string category;                      // acoustic, mechanical, electromagnetic, optimization, ...

    // Structure only: ids, references to earlier steps, parseable conditions.
    Status validate() const;
};

nlohmann::json toJson(const Workflow& workflow);
Result<Workflow> workflowFromJson(const nlohmann::json& value);

// Records of a run.
enum class StepStatus { Done, Skipped, Failed, Cancelled };
std::string_view toString(StepStatus status);

struct StepRecord {
    std::string stepId;
    std::string action;
    StepStatus status = StepStatus::Done;
    int iteration = -1;                        // repetition index, -1 when not repeated
    nlohmann::json repeatValue;
    std::string message;
    nlohmann::json outputs = nlohmann::json::object();
    double seconds = 0.0;
};

enum class RunStatus { Completed, Failed, Cancelled };
std::string_view toString(RunStatus status);

struct WorkflowRun {
    std::string runId;
    std::string workflowId;
    std::string workflowVersion;
    nlohmann::json parameters = nlohmann::json::object();  // the resolved values
    std::string startedUtc, finishedUtc;
    RunStatus status = RunStatus::Completed;
    std::string message;
    std::vector<StepRecord> steps;
    nlohmann::json outputs = nlohmann::json::object();     // by step id (lists for repeated steps)
    double seconds = 0.0;
};

nlohmann::json toJson(const WorkflowRun& run);
WorkflowRun workflowRunFromJson(const nlohmann::json& value);

}  // namespace resonant::automation
