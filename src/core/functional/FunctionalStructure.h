#pragma once

// Functional structures (RES-FUNC-001): an engineered object described by the
// scene objects it consists of (by role), its material regions, boundaries,
// domains, excitation, environment, frequency range, the analysis that
// computes its behavior, objectives and constraints. The document holds no
// physics: every metric comes from the analysis it names (Analysis.h). This
// module includes no physics domain and no coupling (layering rule).

#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace resonant {
class Project;
}

namespace resonant::functional {

inline constexpr const char* kFormat = "resonant.functional-structure";
inline constexpr int kVersion = 1;
inline constexpr const char* kExtension = "functional";
inline constexpr const char* kWorkspaceFormat = "resonant.functional";

struct Body {
    std::string objectId;
    std::string role;  // e.g. "cavity", "mirror1", "horn", "resonator", "layer1"
};

enum class ParameterKind { Bound, Free, Derived };
std::string_view toString(ParameterKind kind);

struct Parameter {
    std::string id;           // a name usable in expressions (letters, digits, _)
    std::string name;
    ParameterKind kind = ParameterKind::Free;
    std::string binding;      // Bound: object/<id>/geometry/<key> or object/<id>/position/<x|y|z>
    std::string expression;   // Derived: of other parameter ids
    std::string unit;         // SI symbol
    double value = 0.0;       // Free: the value; others: ignored (read from the project / computed)
    double minimum = -std::numeric_limits<double>::infinity();
    double maximum = std::numeric_limits<double>::infinity();
    double defaultValue = std::numeric_limits<double>::quiet_NaN();
    std::string group;
    std::string description;
};

struct Region {
    std::string id;
    std::string objectId;
    std::string role;       // e.g. "walls", "fill", "layer1"
    std::string materialId;
};

struct Boundary {
    std::string id;
    std::string objectId;
    std::string role;       // e.g. "port", "mouth", "support"
    std::string condition;  // e.g. "pec", "baffled-piston", "clamped"
    nlohmann::json settings = nlohmann::json::object();
};

struct Objective {
    std::string id;
    std::string metric;     // a metric key of the analysis
    std::string sense = "target";  // minimize, maximize, target, within
    double target = 0.0;
    double low = 0.0, high = 0.0;  // within
    double weight = 1.0;
    std::string description;
};

struct Constraint {
    std::string id;
    std::string expression;   // of parameter ids and [metric keys]
    std::string relation = "<=";  // "<=" or ">="
    double limit = 0.0;
    std::string unit;
    std::string category = "manufacturing";  // manufacturing, physical, design
    std::string process;      // e.g. "cnc", "sheet", "additive", "injection-molding", "lamination", "composite"
    bool hard = true;
    std::string description;
};

struct AnalysisConfig {
    std::string kind;
    std::string model;  // empty: the kind's first model
    nlohmann::json settings = nlohmann::json::object();
};

struct FunctionalStructure {
    std::string id;
    std::string name;
    std::string description;
    std::vector<Body> bodies;
    std::vector<Parameter> parameters;
    std::vector<Region> regions;
    std::vector<Boundary> boundaries;
    std::vector<std::string> domains;
    nlohmann::json excitation = nlohmann::json::object();
    nlohmann::json environment = nlohmann::json::object();
    double frequencyLow = 0.0, frequencyHigh = 0.0;  // Hz; 0: none
    AnalysisConfig analysis;
    std::vector<Objective> objectives;
    std::vector<Constraint> constraints;
    std::string physicalSystem;  // id of a coupled-engine system, or empty
    std::string notes;

    static Result<FunctionalStructure> fromJson(const nlohmann::json& document);
    nlohmann::json toJson() const;

    const Parameter* parameter(const std::string& id) const;
    Parameter* parameter(const std::string& id);
    const Body* body(const std::string& role) const;
    const Region* region(const std::string& role) const;
    const Boundary* boundary(const std::string& role) const;
};

struct Issue {
    std::string field;
    std::string message;
};

// References against the project (objects, materials, bindings), parameter
// rules (unique ids, ranges, expressions, cycles), constraints and
// objectives (syntax). Analysis-specific checks are added by the analysis.
std::vector<Issue> validateStructure(const FunctionalStructure& structure, const Project& project);

// The structures stored in a project (extension `functional`).
Result<std::vector<FunctionalStructure>> structuresOf(const Project& project);
// Writes the list into the project's extension.
void storeStructures(Project& project, const std::vector<FunctionalStructure>& structures);

}  // namespace resonant::functional
