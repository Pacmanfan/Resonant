#pragma once

// An optimization problem (RES-OPT-001, RES-OPT-003..007, RES-OPT-016):
// what the user wants (objectives), what Resonant may change (the design
// space), what must be respected (constraints), which physical model
// evaluates candidates (adapter, its configuration and model id), how the
// space is searched (algorithm settings) and, for robust optimization,
// which parameters are uncertain and how (with the user's justification).
//
// The problem is a versioned, serializable definition ("resonant.
// optimization.problem", version 1) stored in the project's extensions and
// exchanged with external tools; it never contains results.
#include "foundation/Result.h"
#include "optimization/Design.h"
#include "optimization/Evaluator.h"
#include "optimization/Objectives.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace resonant::optimization {

inline constexpr const char* kProblemFormat = "resonant.optimization.problem";
inline constexpr int kProblemVersion = 1;

struct AlgorithmSettings {
    std::string algorithm = "nelder-mead";
    int maxEvaluations = 200;        // candidate designs (cached ones included)
    std::uint64_t seed = 1;          // every random choice derives from it
    int populationSize = 0;          // population methods; 0 = automatic
    int pointsPerVariable = 5;       // grid sweep
    int samples = 32;                // random / Latin hypercube / initial design of the Bayesian method
    double tolerance = 1e-6;         // convergence (method specific, documented per method)
    double initialStep = 0.15;       // simplex size or first step, as a fraction of each range
    double gradientStep = 1e-4;      // finite-difference step, as a fraction of each range
    int parallel = 0;                // concurrent evaluations; 0 = automatic (half the hardware threads)
    double maxSeconds = 0.0;         // wall-clock limit, 0 = none
    double memoryBudgetBytes = 0.0;  // per evaluation, passed to the model; 0 = the model's default
    int checkpointEvery = 0;         // candidates between checkpoints, 0 = only at the end
    friend bool operator==(const AlgorithmSettings&, const AlgorithmSettings&) = default;
};

enum class Distribution { Uniform, Normal };
std::string_view toString(Distribution distribution);

// One uncertain parameter of robust optimization: a binding the adapter
// offers (a design variable or not) perturbed around the design value.
struct UncertainParameter {
    std::string binding;
    std::string label;
    Distribution distribution = Distribution::Uniform;
    double width = 0.0;      // Uniform: half-width of the tolerance band; Normal: standard deviation
    bool relative = false;   // width as a fraction of the design value
    double truncate = 3.0;   // Normal: samples beyond this many sigma are clipped
    std::string source;      // where the distribution comes from (data sheet, tolerance, measurement); required
    friend bool operator==(const UncertainParameter&, const UncertainParameter&) = default;
};

enum class RobustMeasure { Mean, MeanPlusSigma, WorstCase };
std::string_view toString(RobustMeasure measure);

struct RobustSettings {
    bool enabled = false;
    std::vector<UncertainParameter> parameters;
    int samples = 16;                 // perturbed evaluations per design (common to every design)
    RobustMeasure measure = RobustMeasure::Mean;
    double k = 1.0;                   // MeanPlusSigma
    double requiredProbability = 1.0; // hard constraints: fraction of samples that must satisfy them
    std::uint64_t seed = 7;
    friend bool operator==(const RobustSettings&, const RobustSettings&) = default;
};

struct OptimizationProblem {
    std::string id;
    std::string name;
    std::string description;
    std::string templateId;
    std::string adapterId;
    nlohmann::json adapterConfig = nlohmann::json::object();
    std::string modelId;            // the model candidates are evaluated with
    std::string validationModelId;  // a higher-fidelity model for re-evaluation; empty = none named
    DesignSpace space;
    std::vector<Objective> objectives;
    std::vector<DesignConstraint> constraints;
    AlgorithmSettings algorithm;
    RobustSettings robust;

    // Structural checks that need no adapter.
    Status validate() const;
    // Checks against what the bound adapter supports: every binding,
    // quantity and model must be offered; objective units are filled in.
    Status validateAgainst(const Capabilities& capabilities, std::vector<std::string>* warnings = nullptr) const;
    // Units of objectives and constraints from the adapter's quantities.
    void fillUnits(const Capabilities& capabilities);
};

nlohmann::json toJson(const AlgorithmSettings& settings);
AlgorithmSettings algorithmSettingsFromJson(const nlohmann::json& value);
nlohmann::json toJson(const RobustSettings& settings);
Result<RobustSettings> robustSettingsFromJson(const nlohmann::json& value);
nlohmann::json toJson(const OptimizationProblem& problem);
Result<OptimizationProblem> problemFromJson(const nlohmann::json& value);

}  // namespace resonant::optimization
