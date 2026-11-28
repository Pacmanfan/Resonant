#pragma once

// Studies of functional structures (RES-FUNC-011, RES-FUNC-012): one
// analysis, parameter sweeps (every point on its own copy of the design),
// local sensitivities, mode tracking across sweeps, objectives, constraints,
// ranking and explanations. Results are JSON documents
// (`resonant.functional-result`) with the dependency record and provenance.

#include "foundation/Result.h"
#include "functional/Analysis.h"
#include "functional/Dependencies.h"
#include "functional/Parameters.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant {
class JobContext;
}

namespace resonant::functional {

inline constexpr const char* kResultFormat = "resonant.functional-result";

// Evaluates the structure's analysis on the project as it is.
Result<AnalysisOutcome> evaluateStructure(const FunctionalStructure& structure, const Project& project, const AnalysisRegistry& registry,
                                          const std::string& modelId, JobContext* context);

struct ObjectiveResult {
    std::string id, metric, sense;
    double value = 0.0;
    bool available = false;
    bool met = false;        // target within 1 %, inside the range; minimize / maximize: not applicable (false)
    double deviation = 0.0;  // target: |v - t| / |t|; within: distance outside the range / its width; others: the value
    double weight = 1.0;
};
std::vector<ObjectiveResult> evaluateObjectives(const FunctionalStructure& structure, const Values& metrics);

struct ConstraintResult {
    std::string id, expression, relation, category, process;
    double value = 0.0, limit = 0.0;
    bool evaluated = false;
    bool satisfied = false;
    bool hard = true;
    std::string message;
};
// Expressions of parameter ids and metric keys ([cavity.f1] for keys with dots).
std::vector<ConstraintResult> evaluateConstraints(const FunctionalStructure& structure, const Values& parameters, const Values& metrics);

// The analysis result document.
Result<nlohmann::json> analyzeDocument(const FunctionalStructure& structure, const Project& project, const AnalysisRegistry& registry,
                                       const std::string& modelId, JobContext* context);

struct SweepAxis {
    std::string parameter;
    std::vector<double> values;
};
// A full grid over the axes (at most 400 points).
Result<nlohmann::json> sweepDocument(const FunctionalStructure& structure, const Project& project, const AnalysisRegistry& registry,
                                     const std::vector<SweepAxis>& axes, const std::string& modelId, JobContext* context);

// Central differences of every metric with respect to each parameter (default: all that can be set).
Result<nlohmann::json> sensitivityDocument(const FunctionalStructure& structure, const Project& project, const AnalysisRegistry& registry,
                                           const std::vector<std::string>& parameters, double relativeStep, const std::string& modelId,
                                           JobContext* context);

// Tracks modes across consecutive points: by label where the model labels its
// modes, otherwise by nearest frequency; a correspondence within `tolerance`
// (relative) of a second candidate is flagged ambiguous.
nlohmann::json trackModes(const std::vector<std::vector<Mode>>& points, double tolerance = 0.02);

// Why a candidate differs from the baseline: parameters changed, models,
// objectives improved and worsened, active constraints, mechanism, limits.
nlohmann::json explainCandidate(const FunctionalStructure& structure, const Project& project, const AnalysisKind& kind, const Values& baselineParameters,
                                const Values& baselineMetrics, const Values& candidateParameters, const Values& candidateMetrics,
                                const AnalysisOutcome* candidateOutcome);

// The staleness of a result document against a project: the structure (by
// id) and the model it names, compared with the dependencies it recorded.
std::vector<StaleReason> resultStaleness(const nlohmann::json& result, const Project& project, const AnalysisRegistry& registry);

// A copy of the project (and the structure) with the parameter values applied.
Result<std::pair<std::unique_ptr<Project>, FunctionalStructure>> withParameters(const FunctionalStructure& structure, const Project& project,
                                                                                 const Values& values);

}  // namespace resonant::functional
