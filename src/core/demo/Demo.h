// Demonstrations (RES-DEMO-001..010, ADR-000215): profiles, sequences of
// application-level steps, and the manifest of a run. This is data only: the
// application's DemoController executes the steps with the application's own
// commands and waits for its events. Nothing here depends on a physics module,
// and nothing here produces a result: every result a demonstration shows is
// calculated by Resonant when the step runs.
#pragma once

#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace resonant::demo {

inline constexpr const char* kProfileFormat = "resonant.demo-profile";
inline constexpr const char* kSequenceFormat = "resonant.demo-sequence";
inline constexpr const char* kRunFormat = "resonant.demo-run";
inline constexpr int kFormatVersion = 1;

// The step actions (stable names; see docs/demos/DEMO_SEQUENCE_FORMAT.md).
const std::vector<std::string>& stepActions();

enum class FailureBehaviour { Stop, Continue };

struct DemoStep {
    std::string id;
    std::string action;
    std::string description;
    nlohmann::json params = nlohmann::json::object();
    nlohmann::json expect = nlohmann::json::object();  // expected state after the step (checked)
    double timeoutSeconds = 60.0;                      // a bound for failure, never a reason for success
    FailureBehaviour onFailure = FailureBehaviour::Stop;
    std::string notes;       // presenter notes
    std::string screenshot;  // the screenshot this step illustrates (or captures)
};

struct DemoSequence {
    std::string id;
    std::string version = "1";
    std::string name;
    std::vector<DemoStep> steps;
    Status validate() const;
};

// The seven questions every demonstration answers (request section 31).
struct DemoStory {
    std::string problem, configure, calculate, meaning, inspect, value, assumptions;
};

struct DemoProfile {
    std::string id, name, description, audience, domain;
    std::string status = "available";  // available | partial | planned
    std::string project;               // example project, relative to the resource root
    std::string workspace;             // initial workspace (panel)
    std::vector<std::string> requires_;  // capabilities ("python", workflow ids, actions)
    std::vector<std::string> data;       // datasets used (labelled synthetic or measured)
    std::string sequence;                // sequence file, relative to the resource root
    std::vector<std::string> expected;   // expected results, in words
    nlohmann::json camera = nlohmann::json::object();
    nlohmann::json visualization = nlohmann::json::object();
    std::vector<std::string> screenshots;
    std::string notes;                   // presenter notes file
    double durationMinutes = 0.0;
    std::vector<std::string> completion; // completion criteria, in words
    std::vector<std::string> limitations;
    std::vector<std::string> remaining;  // what a partial demonstration still lacks
    DemoStory story;
    Status validate() const;
};

Result<DemoSequence> sequenceFromJson(const nlohmann::json& j);
nlohmann::json toJson(const DemoSequence& sequence);
Result<DemoProfile> profileFromJson(const nlohmann::json& j);
nlohmann::json toJson(const DemoProfile& profile);

// The demonstrations under a resource root (<root>/demos/profiles/*.json);
// each profile with its sequence. A broken file is reported, not skipped.
struct LoadedDemo {
    DemoProfile profile;
    DemoSequence sequence;
};
struct DemoLibrary {
    std::vector<LoadedDemo> demos;
    std::vector<std::string> errors;
    const LoadedDemo* find(const std::string& id) const;
};
DemoLibrary loadDemoLibrary(const std::filesystem::path& resourceRoot);
Result<DemoSequence> loadSequence(const std::filesystem::path& file);

// The manifest of one run (request section 33).
enum class StepOutcome { Done, Failed, Skipped, Cancelled };
std::string_view toString(StepOutcome outcome);

struct StepRun {
    std::string id, action;
    StepOutcome outcome = StepOutcome::Done;
    std::string message;
    std::string startedUtc;
    double seconds = 0.0;
    nlohmann::json result = nlohmann::json::object();  // what the step calculated or checked
};

struct DemoRun {
    std::string profileId, sequenceId, sequenceVersion;
    std::string applicationVersion;
    std::string projectFile, projectId, projectSha256;
    nlohmann::json simulation = nlohmann::json::object();  // the project's solver settings when the run started
    std::vector<std::string> screenshots;
    std::vector<std::string> notes;
    std::string status = "running";  // running | completed | failed | cancelled
    std::string message;
    std::string startedUtc, finishedUtc;
    std::vector<StepRun> steps;
};
nlohmann::json toJson(const DemoRun& run);

std::string sha256OfFile(const std::filesystem::path& file);  // hex; empty when unreadable

}  // namespace resonant::demo
