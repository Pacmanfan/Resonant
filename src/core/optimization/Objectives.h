#pragma once

// Objectives and constraints (RES-OPT-004..006).
//
// An objective names a QUANTITY an adapter calculates ("focus.pressure",
// "modal.f1", "pattern.sidelobeDb") and what is wanted of it. Its score is
// a dimensionless number to minimize:
//
//   Minimize   s = v / scale
//   Maximize   s = -v / scale
//   Target     s = |v - target| / scale
//   AtMost     s = max(0, v - target) / scale      (a threshold to satisfy)
//   AtLeast    s = max(0, target - v) / scale
//
// `scale` (in the quantity's unit) states what a change of one unit of
// score means; it is the user's normalization, never guessed. Scores of
// different objectives are only combined when the user asks for a weighted
// sum; Pareto methods compare them separately.
//
// A constraint bounds a quantity (or an expression over design values):
// value <= limit, >= limit or = limit (within a tolerance). Its violation is
// the amount by which it is missed, in the quantity's unit. Hard
// constraints decide feasibility: a candidate that misses one is infeasible
// and says by how much. Soft constraints add penalty * (violation/scale)^2
// to the weighted score instead.
#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace resonant::optimization {

enum class Sense { Minimize, Maximize, Target, AtMost, AtLeast };
std::string_view toString(Sense sense);
std::optional<Sense> senseFromString(std::string_view text);

struct Objective {
    std::string id;
    std::string label;
    std::string quantity;  // adapter quantity key
    Sense sense = Sense::Minimize;
    double target = 0.0;   // Target / AtMost / AtLeast, in the quantity's unit
    double weight = 1.0;   // weighted-sum scalarization only
    double scale = 1.0;    // normalization, in the quantity's unit, > 0
    std::string unit;      // filled from the adapter's quantity description

    double score(double value) const;
    Status validate() const;
    friend bool operator==(const Objective&, const Objective&) = default;
};

enum class Relation { AtMost, AtLeast, Equal };
std::string_view toString(Relation relation);
std::optional<Relation> relationFromString(std::string_view text);

enum class ConstraintCategory { Geometric, Physical, Manufacturing, Computational };
std::string_view toString(ConstraintCategory category);
std::optional<ConstraintCategory> constraintCategoryFromString(std::string_view text);

struct DesignConstraint {
    std::string id;
    std::string label;
    // An adapter quantity key, or "expr:<expression>" over design-variable
    // ids and adapter constants (a relation between parameters, checked
    // before anything is simulated).
    std::string quantity;
    Relation relation = Relation::AtMost;
    double limit = 0.0;
    double tolerance = 0.0;   // Equal: |v - limit| <= tolerance counts as met
    bool hard = true;
    double penalty = 1.0;     // soft constraints
    double scale = 1.0;       // soft constraints: normalization of the violation
    ConstraintCategory category = ConstraintCategory::Physical;
    std::string unit;

    bool isExpression() const { return quantity.rfind("expr:", 0) == 0; }
    std::string expression() const { return isExpression() ? quantity.substr(5) : std::string(); }
    // >= 0: how far the value misses the constraint, in its unit.
    double violation(double value) const;
    Status validate() const;
    friend bool operator==(const DesignConstraint&, const DesignConstraint&) = default;
};

struct ConstraintResult {
    std::string id;
    double value = 0.0;
    double limit = 0.0;
    double violation = 0.0;
    bool satisfied = true;
    bool hard = true;
    bool evaluated = true;   // false: the quantity was not available (counts as violated for hard constraints)
    std::string message;
};

nlohmann::json toJson(const Objective& objective);
Result<Objective> objectiveFromJson(const nlohmann::json& value);
nlohmann::json toJson(const DesignConstraint& constraint);
Result<DesignConstraint> constraintFromJson(const nlohmann::json& value);
nlohmann::json toJson(const ConstraintResult& result);
ConstraintResult constraintResultFromJson(const nlohmann::json& value);

}  // namespace resonant::optimization
