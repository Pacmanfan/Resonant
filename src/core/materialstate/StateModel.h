#pragma once

// Dynamic material state (RES-DMS-001..004, ADR-000225): what a constitutive model's
// state is made of, how it is stored over a region and over time, and the records of
// runs kept in the project (extension "material-state"). No physics here: the models
// are in the domains (mechanics, thermal, electromagnetics), their catalogue and runners
// in coupling/MaterialStateModels.
//
// The update contract every model follows: from the committed state and an increment
// (strain, time step, field), compute a trial state with its energies and convergence;
// the caller commits it only when the step converged, otherwise keeps the committed
// state and reports the failure.

#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace resonant {
class Project;
}

namespace resonant::materialstate {

inline constexpr const char* kExtension = "material-state";
inline constexpr const char* kFormat = "resonant.material-state";

// Reversible: returns with the load (elastic strain, temperature); irreversible: does
// not (plastic strain, damage); history: a memory the model keeps (maximum strain reached,
// hysteron states, the time since loading).
enum class VariableClass { Reversible, Irreversible, History };
std::string_view toString(VariableClass c);

struct StateVariable {
    std::string key, label, unit;
    VariableClass kind = VariableClass::Reversible;
    std::string description;
    nlohmann::json toJson() const;
};

struct Parameter {
    std::string key, label, unit;
    double value = 0.0;  // the default
    std::string description;
};

// What a constitutive model or study represents (RES-DMS-013).
struct ModelCard {
    std::string id, title, domain;
    std::string status = "implemented";  // implemented, not implemented
    std::vector<StateVariable> variables;
    std::vector<Parameter> parameters;
    std::vector<std::string> assumptions, excluded;
    std::string validity, method, reference;
    nlohmann::json loadings = nlohmann::json::array();  // named loading presets: [{id, title, loading}]
    nlohmann::json toJson() const;
};

// The state of one material point: values by variable key (a scalar or a small vector).
using PointState = std::map<std::string, std::vector<double>, std::less<>>;

// A state over a region: one material point per cell or element.
struct StateField {
    std::string model, material, region;
    std::vector<double> positions;  // m, one coordinate per point (1D regions), may be empty
    std::vector<PointState> points;
    double time = 0.0;              // s (or the load parameter of a quasi-static run)
    int step = 0;
    // min / mean / max of a scalar variable over the points.
    struct Summary {
        double min = 0.0, mean = 0.0, max = 0.0;
    };
    Summary summary(std::string_view key, size_t component = 0) const;
};

struct StepConvergence {
    bool converged = true;
    int iterations = 0;
    double residual = 0.0;
    std::string message;
    nlohmann::json toJson() const;
};

// A run's record: settings, the history (snapshots), the final state, energies and what
// can be committed to a library material.
struct StateRecord {
    std::string id, name, model, kind;  // kind: material-point, structure, evolution
    std::string material, region, created;
    nlohmann::json settings = nlohmann::json::object();
    nlohmann::json history = nlohmann::json::array();   // [{step, time, values..., energy, convergence}]
    nlohmann::json final = nlohmann::json::object();
    nlohmann::json summary = nlohmann::json::object();
    nlohmann::json committable = nlohmann::json::array();  // [{domain, key, value, unit, note}]
    nlohmann::json toJson() const;
    static Result<StateRecord> fromJson(const nlohmann::json& document);
};

struct Workspace {
    nlohmann::json studies = nlohmann::json::array();  // [{id, name, model, parameters, loading}]
    std::vector<StateRecord> records;
    const StateRecord* record(std::string_view id) const;
    nlohmann::json toJson() const;
    static Result<Workspace> fromJson(const nlohmann::json& document);
};

Result<Workspace> workspaceOf(const Project& project);  // empty when the project has none
void storeWorkspace(Project& project, const Workspace& workspace);

}  // namespace resonant::materialstate
