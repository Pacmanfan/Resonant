#pragma once

// Optimization data in a project (RES-OPT-019, RES-OPT-022).
//
// Problems, run records (with every candidate, without the cache) and design
// revisions are stored in the project's extensions under "optimization"
// (format version 1). Nothing here changes the scene: a run never touches
// the design, and only an explicit promotion writes a candidate into it, as
// an undoable change that is recorded as a revision with its lineage and the
// values it replaced.
#include "foundation/Result.h"
#include "optimization/Engine.h"
#include "optimization/Problem.h"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace resonant {
class Project;
}

namespace resonant::optimization {

inline constexpr const char* kExtensionKey = "optimization";
inline constexpr int kWorkspaceVersion = 1;

struct DesignRevision {
    std::string id;
    std::string candidateId, runId, problemId;
    std::string promotedUtc;
    std::string model, fidelity;
    bool validated = false;           // evaluated with a validation / high-fidelity model
    std::map<std::string, double, std::less<>> values;    // by binding: what was applied
    std::map<std::string, double, std::less<>> previous;  // by binding: what the design had before
    std::vector<std::string> parents;                     // the candidate's lineage
    std::string note;
};

struct OptimizationWorkspace {
    std::vector<OptimizationProblem> problems;
    std::vector<RunRecord> runs;
    std::vector<DesignRevision> revisions;
    const OptimizationProblem* findProblem(std::string_view id) const;
    OptimizationProblem* findProblem(std::string_view id);
    const RunRecord* findRun(std::string_view id) const;
};

OptimizationWorkspace workspaceOf(const Project& project);
void storeWorkspace(Project& project, const OptimizationWorkspace& workspace);

// The design snapshot an adapter is bound to: the serialized project
// without its optimization data.
nlohmann::json designSnapshot(const Project& project);
// The part of a snapshot that determines physics: without volatile fields
// (modification time) and without optimization data.
nlohmann::json dependencyDocument(const nlohmann::json& snapshot);
// Reconstructs an independent project from a snapshot.
Result<std::unique_ptr<Project>> projectFromSnapshot(const nlohmann::json& snapshot);

nlohmann::json toJson(const DesignRevision& revision);
DesignRevision revisionFromJson(const nlohmann::json& value);

// Candidates of a run as CSV (one row per candidate: id, origin, model,
// fidelity, feasibility, variables, objectives, constraint violations), for
// spreadsheets and scientific tools.
std::string candidatesCsv(const OptimizationProblem& problem, const std::vector<Candidate>& candidates);

}  // namespace resonant::optimization
