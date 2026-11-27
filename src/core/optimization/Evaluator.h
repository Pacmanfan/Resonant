#pragma once

// The boundary between the physics-independent optimization core and the
// physics domains (RES-OPT-001, RES-OPT-002, RES-OPT-020).
//
// A domain ADAPTER is bound to one design snapshot (a serialized project
// and the adapter's own configuration) and becomes a DesignEvaluator. It
// states explicitly what it supports - which variables (bindings) it can
// change, which quantities it calculates (with unit, definition,
// normalization and the models they apply to), which models it can
// evaluate with and at which fidelity - and evaluates a design point by
// reconstructing the design from the snapshot, applying the point, checking
// the design, running the physical model and reporting quantities. It never
// changes the project it was bound to.
//
// The core contains no equation of any domain. It only knows quantities by
// key and unit, and never assumes that two domains expose the same ones.
#include "foundation/Result.h"
#include "optimization/Design.h"

#include <nlohmann/json.hpp>

#include <Eigen/Core>

#include <complex>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace resonant {
class JobContext;
class Project;
}  // namespace resonant

namespace resonant::optimization {

// Model levels (RES-OPT-014):
//   Analytical    closed form, idealized (free-field point sources, array
//                 factor, Euler-Bernoulli formulas)
//   Reduced       approximate numerical or more complete analytical (Kirchhoff
//                 surface integration, coarse finite elements, exact element
//                 fields without coupling)
//   HighFidelity  full-wave or refined numerical (FDTD, refined finite elements)
//   Experimental  calibrated measurement
enum class Fidelity { Analytical = 1, Reduced = 2, HighFidelity = 3, Experimental = 4 };
std::string_view toString(Fidelity fidelity);
std::optional<Fidelity> fidelityFromString(std::string_view text);

struct QuantityInfo {
    std::string key;
    std::string label;
    std::string unit;        // SI symbol, "" for dimensionless, "dB" for levels
    std::string definition;  // the mathematical definition
    std::string normalization;  // what the value is relative to, if anything
    std::vector<std::string> models;  // model ids that calculate it (empty: every model of the adapter)
    bool linearInSources = false;     // a field value that is linear in the source weights
};

struct ModelInfo {
    std::string id;
    std::string name;
    std::string version;
    Fidelity fidelity = Fidelity::Analytical;
    std::vector<std::string> assumptions;
    bool available = true;
    std::string unavailableReason;
    double typicalSeconds = 0.0;  // rough cost of one evaluation, for algorithm selection
};

struct Capabilities {
    std::string adapterId;
    std::string domain;     // "acoustic", "mechanical", "electromagnetic"
    std::string version;
    std::vector<DesignVariable> variables;  // offered for this snapshot, with suggested bounds
    std::vector<QuantityInfo> quantities;
    std::vector<ModelInfo> models;
    std::map<std::string, double, std::less<>> constants;  // names usable in expressions ("wavelength")
    std::vector<std::string> limitations;
    bool threadSafe = true;        // evaluate() may run concurrently
    bool providesGradients = false;  // exact gradients; none of the current adapters does
    bool supportsLinearInverse = false;

    const QuantityInfo* quantity(std::string_view key) const;
    const ModelInfo* model(std::string_view id) const;
    const DesignVariable* variable(std::string_view binding) const;
};

// Which model evaluated a candidate, exactly (RES-OPT-011).
struct ModelRecord {
    std::string modelId;
    std::string modelName;
    std::string version;
    Fidelity fidelity = Fidelity::Analytical;
    std::vector<std::string> assumptions;
    nlohmann::json settings = nlohmann::json::object();  // the solver settings that matter
    std::string adapterId;
    std::string adapterVersion;
    std::string snapshotKey;  // the dependency key of the design snapshot
};
nlohmann::json toJson(const ModelRecord& record);
ModelRecord modelRecordFromJson(const nlohmann::json& value);

struct Evaluation {
    enum class Status {
        Ok,
        InvalidDesign,  // the design could not be built (invalid geometry, overlapping elements, ...)
        Refused,        // the model refused it (resolution, memory, unsupported)
        Failed,         // the solver failed
        Cancelled
    };
    Status status = Status::Ok;
    std::string message;
    std::map<std::string, double, std::less<>> quantities;
    ModelRecord model;
    std::vector<std::string> notes;     // what the user should know about this evaluation
    std::vector<std::string> warnings;
    nlohmann::json details = nlohmann::json::object();  // compact result data (cuts, achieved values)
    double seconds = 0.0;
    bool ok() const { return status == Status::Ok; }
};
std::string_view toString(Evaluation::Status status);
nlohmann::json toJson(const Evaluation& evaluation);
Evaluation evaluationFromJson(const nlohmann::json& value);

// A complex linear model of a source configuration: the quantity vector at
// the targets is A w for complex source weights w (RES-OPT-009).
struct LinearSystem {
    Eigen::MatrixXcd response;               // targets x sources
    Eigen::VectorXcd desired;                // wanted values at the targets
    Eigen::VectorXd rowWeights;              // importance of each target (>= 0)
    Eigen::VectorXcd current;                // the sources' present weights
    std::vector<std::string> sourceLabels;
    std::vector<std::string> targetLabels;
    std::vector<std::string> amplitudeBindings;  // per source: binding of its amplitude (magnitude)
    std::vector<std::string> phaseBindings;      // per source: binding of its phase (rad)
    std::vector<std::string> zeroBindings;       // set to zero with the weights (time delays folded into the phase)
    std::string weightUnit;                  // unit of a weight ("Pa at 1 m", "N", "relative")
    std::string targetUnit;                  // unit of a target value ("Pa", "m", "relative field")
    std::string model;                       // the model that produced the response
    std::vector<std::string> notes;
};

class DesignEvaluator {
public:
    virtual ~DesignEvaluator() = default;
    virtual const Capabilities& capabilities() const = 0;
    // Identifies the full dependency state of the snapshot (project content
    // and adapter configuration). Two evaluators with the same key evaluate
    // the same point identically, which is what makes caching safe.
    virtual const std::string& snapshotKey() const = 0;
    // Evaluates a design given as values by binding key. Must not modify
    // anything shared; with capabilities().threadSafe it may run concurrently.
    virtual Evaluation evaluate(const std::map<std::string, double, std::less<>>& bindings, const std::string& modelId,
                                JobContext* context = nullptr) const = 0;
    // Writes a design into a project (promotion and export). The project
    // must be the snapshot or a copy of it.
    virtual Status apply(const std::map<std::string, double, std::less<>>& bindings, Project& project) const = 0;
    // The source-to-target linear model at a design, when the adapter has
    // one for its configuration.
    virtual Result<LinearSystem> linearSystem(const std::map<std::string, double, std::less<>>& bindings, const std::string& modelId) const
    {
        (void)bindings;
        (void)modelId;
        return Result<LinearSystem>::failure("this adapter has no linear source model for its configuration");
    }
};

// A starting point for a common task ("focus a linear array on a point").
struct ProblemTemplate {
    std::string id;
    std::string title;
    std::string description;
    std::string adapterId;
    std::function<Result<nlohmann::json>(const Project& project)> create;  // an OptimizationProblem as JSON
};

class AdapterRegistry {
public:
    using Factory = std::function<Result<std::shared_ptr<const DesignEvaluator>>(const nlohmann::json& projectSnapshot,
                                                                                 const nlohmann::json& configuration)>;
    // The registry with every domain adapter (defined where the domains are
    // known: src/core/coupling/OptimizationRegistry.cpp).
    static AdapterRegistry& instance();

    void add(std::string adapterId, std::string title, Factory factory);
    void addTemplate(ProblemTemplate problemTemplate);
    bool knows(std::string_view adapterId) const;
    std::vector<std::pair<std::string, std::string>> adapters() const;  // id, title
    const std::vector<ProblemTemplate>& templates() const { return templates_; }
    const ProblemTemplate* findTemplate(std::string_view id) const;
    Result<std::shared_ptr<const DesignEvaluator>> bind(std::string_view adapterId, const nlohmann::json& projectSnapshot,
                                                        const nlohmann::json& configuration) const;

private:
    struct Entry {
        std::string id, title;
        Factory factory;
    };
    std::vector<Entry> entries_;
    std::vector<ProblemTemplate> templates_;
};

// Helper for adapters: a stable key of a JSON document (FNV-1a 64 of its
// compact dump, as 16 hex digits). Not cryptographic; a dependency key.
std::string contentKey(const nlohmann::json& document);

}  // namespace resonant::optimization
