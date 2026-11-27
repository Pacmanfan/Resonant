#pragma once

// Design variables and the design space (RES-OPT-003).
//
// A design variable is one number the optimizer may change. What it changes
// in the project is its `binding`, a key the domain adapter understands
// ("object/<id>/geometry/focalLength"); the optimization core never
// interprets a binding. Every value is in SI units (`unit` names them).
//
//   Continuous   any value in [lower, upper]; `step` > 0 quantizes it
//   Integer      whole numbers in [lower, upper] (counts, activation 0/1)
//   Categorical  an index into `choices` (material, layout, support type)
//
// A locked variable keeps its initial value; an inactive one is left out of
// the design altogether (the project keeps its own value). Derived
// variables are computed from other variables and adapter constants by an
// expression and are never chosen by an algorithm. Variables can be grouped
// into named design parameters ("reflector": focal length, aperture,
// thickness), which the user interface shows together.
//
// Algorithms work in the unit cube of the free (active, unlocked) variables;
// DesignSpace maps between that cube and complete design points.
#include "foundation/Result.h"
#include "optimization/Expression.h"

#include <nlohmann/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace resonant::optimization {

enum class VariableKind { Continuous, Integer, Categorical };
std::string_view toString(VariableKind kind);
std::optional<VariableKind> variableKindFromString(std::string_view text);

// What a variable means physically (for grouping, display and algorithm
// selection; it changes no arithmetic).
enum class VariableRole {
    Position, Dimension, Radius, Thickness, Spacing, MaterialParameter, Frequency, Amplitude, Phase, Delay, Orientation,
    MaterialSelection, ComponentSelection, Count, Activation, Configuration, GeometryType, SolverChoice, Layout, Other
};
std::string_view toString(VariableRole role);
std::optional<VariableRole> variableRoleFromString(std::string_view text);

struct DesignVariable {
    std::string id;       // unique within the problem
    std::string label;
    std::string binding;  // adapter key of what it changes; empty = same as id
    std::string unit;     // SI symbol ("m", "rad", "Hz", "Pa", "" for counts and indices)
    VariableKind kind = VariableKind::Continuous;
    VariableRole role = VariableRole::Other;
    double lower = 0.0;
    double upper = 1.0;
    double initial = 0.0;
    double step = 0.0;                 // > 0: values are lower + k step (Continuous)
    std::vector<std::string> choices;  // Categorical: values are 0 .. choices.size()-1
    std::vector<std::string> choiceLabels;
    bool locked = false;               // fixed at `initial`
    bool active = true;                // false: not part of the design
    std::string group;                 // design parameter this variable belongs to

    const std::string& key() const { return binding.empty() ? id : binding; }
    bool free() const { return active && !locked; }
    // Rounds a value to what the variable can take (integers, steps,
    // category indices) and clamps it into the bounds.
    double quantize(double value) const;
    Status validate() const;
    friend bool operator==(const DesignVariable&, const DesignVariable&) = default;
};

struct DerivedVariable {
    std::string id;
    std::string label;
    std::string binding;  // empty: a value for constraints and reports only, applied to nothing
    std::string unit;
    std::string expression;  // over variable ids and adapter constants
    const std::string& key() const { return binding.empty() ? id : binding; }
    friend bool operator==(const DerivedVariable&, const DerivedVariable&) = default;
};

// A complete design: a value for every active variable (free, locked and
// derived), by variable id, plus perturbations that are not variables
// (robust evaluation) by binding.
struct DesignPoint {
    std::map<std::string, double, std::less<>> values;  // by variable id
    std::map<std::string, double, std::less<>> extra;    // by binding: values applied in addition (perturbations)
    friend bool operator==(const DesignPoint&, const DesignPoint&) = default;
};

class DesignSpace {
public:
    std::vector<DesignVariable> variables;
    std::vector<DerivedVariable> derived;

    // Ids unique, bounds ordered, initial values inside, choices present,
    // derived expressions parse and refer to known names (variables or
    // `constants`), no derived variable depends on itself.
    Status validate(const std::map<std::string, double, std::less<>>& constants = {}) const;

    std::vector<const DesignVariable*> freeVariables() const;
    size_t dimension() const { return freeVariables().size(); }
    const DesignVariable* find(std::string_view id) const;
    bool hasDiscrete() const;

    // The initial design.
    DesignPoint initialPoint(const std::map<std::string, double, std::less<>>& constants = {}) const;
    // The design for a point of the unit cube of the free variables
    // (quantized, locked values and derived values filled in). Fails when a
    // derived expression cannot be evaluated.
    Result<DesignPoint> fromUnit(const std::vector<double>& unit, const std::map<std::string, double, std::less<>>& constants = {}) const;
    // Values of the free variables, mapped into the unit cube.
    std::vector<double> toUnit(const DesignPoint& point) const;
    // Recomputes the derived values of a point whose variable values are set.
    Status completeDerived(DesignPoint& point, const std::map<std::string, double, std::less<>>& constants = {}) const;
    // Values outside the bounds (or not representable), as readable lines.
    std::vector<std::string> boundViolations(const DesignPoint& point) const;
    // Values by binding key (what the adapter applies): every active
    // variable and derived variable with a binding, plus the extras, which
    // are added to the design value or, for a binding that is not part of
    // the design, to its value in `base` (the project's present value).
    std::map<std::string, double, std::less<>> bindings(const DesignPoint& point,
                                                        const std::map<std::string, double, std::less<>>* base = nullptr) const;
    // A stable text form of a point (17 significant digits), for cache keys.
    static std::string canonical(const DesignPoint& point);
};

nlohmann::json toJson(const DesignVariable& variable);
Result<DesignVariable> designVariableFromJson(const nlohmann::json& value);
nlohmann::json toJson(const DesignSpace& space);
Result<DesignSpace> designSpaceFromJson(const nlohmann::json& value);
nlohmann::json toJson(const DesignPoint& point);
Result<DesignPoint> designPointFromJson(const nlohmann::json& value);

}  // namespace resonant::optimization
