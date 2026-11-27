#include "optimization/Objectives.h"

#include "optimization/Expression.h"

#include <algorithm>
#include <algorithm>
#include <cmath>
#include <limits>

namespace resonant::optimization {

namespace {

constexpr std::pair<Sense, std::string_view> kSenses[] = {
    {Sense::Minimize, "minimize"}, {Sense::Maximize, "maximize"}, {Sense::Target, "target"}, {Sense::AtMost, "at-most"}, {Sense::AtLeast, "at-least"}};
constexpr std::pair<Relation, std::string_view> kRelations[] = {{Relation::AtMost, "<="}, {Relation::AtLeast, ">="}, {Relation::Equal, "="}};
constexpr std::pair<ConstraintCategory, std::string_view> kCategories[] = {{ConstraintCategory::Geometric, "geometric"},
                                                                           {ConstraintCategory::Physical, "physical"},
                                                                           {ConstraintCategory::Manufacturing, "manufacturing"},
                                                                           {ConstraintCategory::Computational, "computational"}};

template <typename E, size_t N>
std::string_view name(const std::pair<E, std::string_view> (&table)[N], E value)
{
    for (const auto& [k, s] : table)
        if (k == value)
            return s;
    return table[0].second;
}

template <typename E, size_t N>
std::optional<E> parse(const std::pair<E, std::string_view> (&table)[N], std::string_view text)
{
    for (const auto& [k, s] : table)
        if (s == text)
            return k;
    return std::nullopt;
}

}  // namespace

std::string_view toString(Sense sense) { return name(kSenses, sense); }
std::optional<Sense> senseFromString(std::string_view text) { return parse(kSenses, text); }
std::string_view toString(Relation relation) { return name(kRelations, relation); }
std::optional<Relation> relationFromString(std::string_view text) { return parse(kRelations, text); }
std::string_view toString(ConstraintCategory category) { return name(kCategories, category); }
std::optional<ConstraintCategory> constraintCategoryFromString(std::string_view text) { return parse(kCategories, text); }

double Objective::score(double v) const
{
    const double s = scale > 0.0 ? scale : 1.0;
    switch (sense) {
    case Sense::Minimize: return v / s;
    case Sense::Maximize: return -v / s;
    case Sense::Target: return std::abs(v - target) / s;
    case Sense::AtMost: return std::max(0.0, v - target) / s;
    case Sense::AtLeast: return std::max(0.0, target - v) / s;
    }
    return v / s;
}

Status Objective::validate() const
{
    if (id.empty())
        return Status::failure("an objective has no id");
    if (quantity.empty())
        return Status::failure("objective '" + id + "' names no quantity");
    if (!(scale > 0.0) || !std::isfinite(scale))
        return Status::failure("objective '" + id + "': the normalization scale must be positive");
    if (!(weight >= 0.0) || !std::isfinite(weight))
        return Status::failure("objective '" + id + "': the weight must be zero or positive");
    if (!std::isfinite(target))
        return Status::failure("objective '" + id + "': the target must be finite");
    return Status::success();
}

double DesignConstraint::violation(double v) const
{
    if (!std::isfinite(v))
        return std::numeric_limits<double>::infinity();
    switch (relation) {
    case Relation::AtMost: return std::max(0.0, v - limit);
    case Relation::AtLeast: return std::max(0.0, limit - v);
    case Relation::Equal: return std::max(0.0, std::abs(v - limit) - tolerance);
    }
    return 0.0;
}

Status DesignConstraint::validate() const
{
    if (id.empty())
        return Status::failure("a constraint has no id");
    if (quantity.empty())
        return Status::failure("constraint '" + id + "' names no quantity");
    if (isExpression()) {
        auto e = Expression::parse(expression());
        if (!e)
            return Status::failure("constraint '" + id + "': " + e.message());
    }
    if (!std::isfinite(limit) || tolerance < 0.0 || !(scale > 0.0) || penalty < 0.0)
        return Status::failure("constraint '" + id + "': limit, tolerance, scale and penalty must be finite and non-negative (scale positive)");
    return Status::success();
}

nlohmann::json toJson(const Objective& o)
{
    return {{"id", o.id},         {"label", o.label}, {"quantity", o.quantity}, {"sense", toString(o.sense)}, {"target", o.target},
            {"weight", o.weight}, {"scale", o.scale}, {"unit", o.unit}};
}

Result<Objective> objectiveFromJson(const nlohmann::json& j)
{
    try {
        Objective o;
        o.id = j.at("id").get<std::string>();
        o.label = j.value("label", o.id);
        o.quantity = j.at("quantity").get<std::string>();
        const auto sense = senseFromString(j.value("sense", std::string("minimize")));
        if (!sense)
            return Result<Objective>::failure("objective '" + o.id + "': unknown sense");
        o.sense = *sense;
        o.target = j.value("target", 0.0);
        o.weight = j.value("weight", 1.0);
        o.scale = j.value("scale", 1.0);
        o.unit = j.value("unit", std::string());
        return o;
    } catch (const std::exception& e) {
        return Result<Objective>::failure(std::string("objective: ") + e.what());
    }
}

nlohmann::json toJson(const DesignConstraint& c)
{
    return {{"id", c.id},       {"label", c.label}, {"quantity", c.quantity}, {"relation", toString(c.relation)},
            {"limit", c.limit}, {"tolerance", c.tolerance}, {"hard", c.hard}, {"penalty", c.penalty},
            {"scale", c.scale}, {"category", toString(c.category)}, {"unit", c.unit}};
}

Result<DesignConstraint> constraintFromJson(const nlohmann::json& j)
{
    try {
        DesignConstraint c;
        c.id = j.at("id").get<std::string>();
        c.label = j.value("label", c.id);
        c.quantity = j.at("quantity").get<std::string>();
        const auto relation = relationFromString(j.value("relation", std::string("<=")));
        if (!relation)
            return Result<DesignConstraint>::failure("constraint '" + c.id + "': unknown relation");
        c.relation = *relation;
        c.limit = j.value("limit", 0.0);
        c.tolerance = j.value("tolerance", 0.0);
        c.hard = j.value("hard", true);
        c.penalty = j.value("penalty", 1.0);
        c.scale = j.value("scale", 1.0);
        c.category = constraintCategoryFromString(j.value("category", std::string("physical"))).value_or(ConstraintCategory::Physical);
        c.unit = j.value("unit", std::string());
        return c;
    } catch (const std::exception& e) {
        return Result<DesignConstraint>::failure(std::string("constraint: ") + e.what());
    }
}

nlohmann::json toJson(const ConstraintResult& r)
{
    nlohmann::json j{{"id", r.id}, {"value", std::isfinite(r.value) ? nlohmann::json(r.value) : nlohmann::json(nullptr)}, {"limit", r.limit},
                     {"violation", std::isfinite(r.violation) ? nlohmann::json(r.violation) : nlohmann::json(nullptr)},
                     {"satisfied", r.satisfied}, {"hard", r.hard}, {"evaluated", r.evaluated}};
    if (!r.message.empty())
        j["message"] = r.message;
    return j;
}

ConstraintResult constraintResultFromJson(const nlohmann::json& j)
{
    ConstraintResult r;
    r.id = j.value("id", std::string());
    r.value = j.contains("value") && j["value"].is_number() ? j["value"].get<double>() : std::numeric_limits<double>::quiet_NaN();
    r.limit = j.value("limit", 0.0);
    r.violation = j.contains("violation") && j["violation"].is_number() ? j["violation"].get<double>() : std::numeric_limits<double>::infinity();
    r.satisfied = j.value("satisfied", false);
    r.hard = j.value("hard", true);
    r.evaluated = j.value("evaluated", true);
    r.message = j.value("message", std::string());
    return r;
}

}  // namespace resonant::optimization
