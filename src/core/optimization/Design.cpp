#include "optimization/Design.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <set>

namespace resonant::optimization {

namespace {

constexpr std::pair<VariableKind, std::string_view> kKinds[] = {
    {VariableKind::Continuous, "continuous"}, {VariableKind::Integer, "integer"}, {VariableKind::Categorical, "categorical"}};

constexpr std::pair<VariableRole, std::string_view> kRoles[] = {
    {VariableRole::Position, "position"},
    {VariableRole::Dimension, "dimension"},
    {VariableRole::Radius, "radius"},
    {VariableRole::Thickness, "thickness"},
    {VariableRole::Spacing, "spacing"},
    {VariableRole::MaterialParameter, "material-parameter"},
    {VariableRole::Frequency, "frequency"},
    {VariableRole::Amplitude, "amplitude"},
    {VariableRole::Phase, "phase"},
    {VariableRole::Delay, "delay"},
    {VariableRole::Orientation, "orientation"},
    {VariableRole::MaterialSelection, "material-selection"},
    {VariableRole::ComponentSelection, "component-selection"},
    {VariableRole::Count, "count"},
    {VariableRole::Activation, "activation"},
    {VariableRole::Configuration, "configuration"},
    {VariableRole::GeometryType, "geometry-type"},
    {VariableRole::SolverChoice, "solver-choice"},
    {VariableRole::Layout, "layout"},
    {VariableRole::Other, "other"},
};

}  // namespace

std::string_view toString(VariableKind kind)
{
    for (const auto& [k, s] : kKinds)
        if (k == kind)
            return s;
    return "continuous";
}

std::optional<VariableKind> variableKindFromString(std::string_view text)
{
    for (const auto& [k, s] : kKinds)
        if (s == text)
            return k;
    return std::nullopt;
}

std::string_view toString(VariableRole role)
{
    for (const auto& [r, s] : kRoles)
        if (r == role)
            return s;
    return "other";
}

std::optional<VariableRole> variableRoleFromString(std::string_view text)
{
    for (const auto& [r, s] : kRoles)
        if (s == text)
            return r;
    return std::nullopt;
}

double DesignVariable::quantize(double value) const
{
    if (!std::isfinite(value))
        value = initial;
    double lo = lower, hi = upper;
    if (kind == VariableKind::Categorical) {
        lo = 0.0;
        hi = choices.empty() ? 0.0 : static_cast<double>(choices.size() - 1);
    }
    value = std::clamp(value, lo, hi);
    switch (kind) {
    case VariableKind::Continuous:
        if (step > 0.0) {
            value = lower + std::round((value - lower) / step) * step;
            if (value > hi + 1e-12 * std::max(1.0, std::abs(hi)))
                value -= step;
            value = std::clamp(value, lo, hi);
        }
        return value;
    case VariableKind::Integer: {
        double v = std::round(value);
        if (v < lo)
            v = std::ceil(lo);
        if (v > hi)
            v = std::floor(hi);
        return v;
    }
    case VariableKind::Categorical: return std::round(value);
    }
    return value;
}

Status DesignVariable::validate() const
{
    const std::string name = "variable '" + id + "'";
    if (id.empty())
        return Status::failure("a design variable has no id");
    if (kind == VariableKind::Categorical) {
        if (choices.empty())
            return Status::failure(name + " is categorical but has no choices");
        if (!choiceLabels.empty() && choiceLabels.size() != choices.size())
            return Status::failure(name + ": one label per choice");
        if (initial < 0.0 || initial > static_cast<double>(choices.size() - 1) || std::round(initial) != initial)
            return Status::failure(name + ": the initial value must be the index of a choice");
        return Status::success();
    }
    if (!std::isfinite(lower) || !std::isfinite(upper) || !std::isfinite(initial))
        return Status::failure(name + ": bounds and initial value must be finite");
    if (!(lower <= upper))
        return Status::failure(name + ": the lower bound is above the upper bound");
    if (lower == upper && !locked)
        return Status::failure(name + ": equal bounds; lock the variable instead");
    if (initial < lower || initial > upper)
        return Status::failure(name + ": the initial value " + std::to_string(initial) + " is outside [" + std::to_string(lower) + ", " +
                               std::to_string(upper) + "]");
    if (step < 0.0 || !std::isfinite(step))
        return Status::failure(name + ": the step must be zero or positive");
    if (kind == VariableKind::Integer && (std::round(lower) > upper || std::floor(upper) < lower))
        return Status::failure(name + ": no whole number between the bounds");
    return Status::success();
}

// --- design space ---------------------------------------------------------------------------

Status DesignSpace::validate(const std::map<std::string, double, std::less<>>& constants) const
{
    std::set<std::string> ids;
    for (const DesignVariable& v : variables) {
        if (Status s = v.validate(); !s)
            return s;
        if (!ids.insert(v.id).second)
            return Status::failure("two design variables have the id '" + v.id + "'");
    }
    std::map<std::string, std::vector<std::string>> deps;
    for (const DerivedVariable& d : derived) {
        if (d.id.empty())
            return Status::failure("a derived variable has no id");
        if (!ids.insert(d.id).second)
            return Status::failure("the id '" + d.id + "' is used twice");
        auto e = Expression::parse(d.expression);
        if (!e)
            return Status::failure("derived variable '" + d.id + "': " + e.message());
        deps[d.id] = e.value().names();
    }
    for (const auto& [id, names] : deps)
        for (const std::string& n : names)
            if (!ids.contains(n) && !constants.contains(n))
                return Status::failure("derived variable '" + id + "' refers to '" + n + "', which is neither a variable nor a constant of the adapter");
    // No cycles among derived variables.
    std::map<std::string, int> state;  // 0 new, 1 visiting, 2 done
    std::function<bool(const std::string&)> visit = [&](const std::string& id) {
        const auto it = deps.find(id);
        if (it == deps.end())
            return true;
        if (state[id] == 1)
            return false;
        if (state[id] == 2)
            return true;
        state[id] = 1;
        for (const std::string& n : it->second)
            if (!visit(n))
                return false;
        state[id] = 2;
        return true;
    };
    for (const auto& [id, names] : deps)
        if (!visit(id))
            return Status::failure("derived variable '" + id + "' depends on itself");
    return Status::success();
}

std::vector<const DesignVariable*> DesignSpace::freeVariables() const
{
    std::vector<const DesignVariable*> out;
    for (const DesignVariable& v : variables)
        if (v.free())
            out.push_back(&v);
    return out;
}

const DesignVariable* DesignSpace::find(std::string_view id) const
{
    for (const DesignVariable& v : variables)
        if (v.id == id)
            return &v;
    return nullptr;
}

bool DesignSpace::hasDiscrete() const
{
    for (const DesignVariable* v : freeVariables())
        if (v->kind != VariableKind::Continuous || v->step > 0.0)
            return true;
    return false;
}

Status DesignSpace::completeDerived(DesignPoint& point, const std::map<std::string, double, std::less<>>& constants) const
{
    // Evaluate in dependency order: repeat passes until nothing changes.
    std::set<std::string> pending;
    for (const DerivedVariable& d : derived)
        pending.insert(d.id);
    for (size_t pass = 0; pass <= derived.size() && !pending.empty(); ++pass) {
        for (const DerivedVariable& d : derived) {
            if (!pending.contains(d.id))
                continue;
            auto e = Expression::parse(d.expression);
            if (!e)
                return Status::failure(e.message());
            std::map<std::string, double, std::less<>> values = constants;
            for (const auto& [k, v] : point.values)
                values[k] = v;
            bool ready = true;
            for (const std::string& n : e.value().names())
                if (!values.contains(n))
                    ready = false;
            if (!ready)
                continue;
            auto v = e.value().evaluate(values);
            if (!v)
                return Status::failure("derived variable '" + d.id + "': " + v.message());
            point.values[d.id] = v.value();
            pending.erase(d.id);
        }
    }
    if (!pending.empty())
        return Status::failure("derived variable '" + *pending.begin() + "' could not be evaluated");
    return Status::success();
}

DesignPoint DesignSpace::initialPoint(const std::map<std::string, double, std::less<>>& constants) const
{
    DesignPoint p;
    for (const DesignVariable& v : variables)
        if (v.active)
            p.values[v.id] = v.quantize(v.initial);
    [[maybe_unused]] const Status s = completeDerived(p, constants);
    return p;
}

Result<DesignPoint> DesignSpace::fromUnit(const std::vector<double>& unit, const std::map<std::string, double, std::less<>>& constants) const
{
    DesignPoint p;
    size_t i = 0;
    for (const DesignVariable& v : variables) {
        if (!v.active)
            continue;
        if (v.locked) {
            p.values[v.id] = v.quantize(v.initial);
            continue;
        }
        const double u = i < unit.size() ? std::clamp(unit[i], 0.0, 1.0) : 0.5;
        ++i;
        double value;
        if (v.kind == VariableKind::Categorical) {
            const double n = static_cast<double>(v.choices.size());
            value = std::min(std::floor(u * n), n - 1.0);  // equal share of the cube per choice
        } else if (v.kind == VariableKind::Integer) {
            const double lo = std::ceil(v.lower), hi = std::floor(v.upper);
            value = std::min(lo + std::floor(u * (hi - lo + 1.0)), hi);
        } else {
            value = v.lower + u * (v.upper - v.lower);
        }
        p.values[v.id] = v.quantize(value);
    }
    if (Status s = completeDerived(p, constants); !s)
        return Result<DesignPoint>::failure(s.message());
    return p;
}

std::vector<double> DesignSpace::toUnit(const DesignPoint& point) const
{
    std::vector<double> out;
    for (const DesignVariable* v : freeVariables()) {
        const auto it = point.values.find(v->id);
        const double value = it == point.values.end() ? v->initial : it->second;
        double u;
        if (v->kind == VariableKind::Categorical) {
            const double n = static_cast<double>(v->choices.size());
            u = (value + 0.5) / n;
        } else if (v->kind == VariableKind::Integer) {
            const double lo = std::ceil(v->lower), hi = std::floor(v->upper);
            u = (value - lo + 0.5) / (hi - lo + 1.0);
        } else {
            u = v->upper > v->lower ? (value - v->lower) / (v->upper - v->lower) : 0.5;
        }
        out.push_back(std::clamp(u, 0.0, 1.0));
    }
    return out;
}

std::vector<std::string> DesignSpace::boundViolations(const DesignPoint& point) const
{
    std::vector<std::string> out;
    for (const DesignVariable& v : variables) {
        if (!v.active)
            continue;
        const auto it = point.values.find(v.id);
        if (it == point.values.end()) {
            out.push_back("'" + v.id + "' has no value");
            continue;
        }
        if (std::abs(v.quantize(it->second) - it->second) > 1e-12 * std::max(1.0, std::abs(it->second)))
            out.push_back("'" + v.id + "' = " + std::to_string(it->second) + " is outside its bounds or not a value it can take");
        if (v.locked && std::abs(it->second - v.quantize(v.initial)) > 1e-12 * std::max(1.0, std::abs(v.initial)))
            out.push_back("'" + v.id + "' is locked at " + std::to_string(v.initial));
    }
    return out;
}

std::map<std::string, double, std::less<>> DesignSpace::bindings(const DesignPoint& point,
                                                               const std::map<std::string, double, std::less<>>* base) const
{
    std::map<std::string, double, std::less<>> out;
    for (const DesignVariable& v : variables) {
        if (!v.active)
            continue;
        const auto it = point.values.find(v.id);
        if (it != point.values.end())
            out[v.key()] = it->second;
    }
    for (const DerivedVariable& d : derived) {
        if (d.binding.empty())
            continue;
        const auto it = point.values.find(d.id);
        if (it != point.values.end())
            out[d.binding] = it->second;
    }
    for (const auto& [k, v] : point.extra) {
        double value = 0.0;
        if (const auto it = out.find(k); it != out.end())
            value = it->second;
        else if (base)
            if (const auto b = base->find(k); b != base->end())
                value = b->second;
        out[k] = value + v;  // a perturbation adds to the design value
    }
    return out;
}

std::string DesignSpace::canonical(const DesignPoint& point)
{
    std::string out;
    char buffer[64];
    for (const auto& [k, v] : point.values) {
        std::snprintf(buffer, sizeof buffer, "%.17g", v);
        out += k + "=" + buffer + ";";
    }
    for (const auto& [k, v] : point.extra) {
        std::snprintf(buffer, sizeof buffer, "%.17g", v);
        out += "+" + k + "=" + buffer + ";";
    }
    return out;
}

// --- JSON ------------------------------------------------------------------------------------

nlohmann::json toJson(const DesignVariable& v)
{
    nlohmann::json j{{"id", v.id},       {"label", v.label},       {"unit", v.unit},     {"kind", toString(v.kind)},
                     {"role", toString(v.role)}, {"lower", v.lower}, {"upper", v.upper}, {"initial", v.initial},
                     {"locked", v.locked}, {"active", v.active}};
    if (!v.binding.empty())
        j["binding"] = v.binding;
    if (v.step > 0.0)
        j["step"] = v.step;
    if (!v.choices.empty())
        j["choices"] = v.choices;
    if (!v.choiceLabels.empty())
        j["choiceLabels"] = v.choiceLabels;
    if (!v.group.empty())
        j["group"] = v.group;
    return j;
}

Result<DesignVariable> designVariableFromJson(const nlohmann::json& j)
{
    using R = Result<DesignVariable>;
    if (!j.is_object())
        return R::failure("a design variable must be an object");
    try {
        DesignVariable v;
        v.id = j.at("id").get<std::string>();
        v.label = j.value("label", v.id);
        v.binding = j.value("binding", std::string());
        v.unit = j.value("unit", std::string());
        const auto kind = variableKindFromString(j.value("kind", std::string("continuous")));
        if (!kind)
            return R::failure("variable '" + v.id + "': unknown kind");
        v.kind = *kind;
        v.role = variableRoleFromString(j.value("role", std::string("other"))).value_or(VariableRole::Other);
        v.lower = j.value("lower", 0.0);
        v.upper = j.value("upper", 1.0);
        v.initial = j.value("initial", v.lower);
        v.step = j.value("step", 0.0);
        v.locked = j.value("locked", false);
        v.active = j.value("active", true);
        v.group = j.value("group", std::string());
        if (j.contains("choices"))
            v.choices = j.at("choices").get<std::vector<std::string>>();
        if (j.contains("choiceLabels"))
            v.choiceLabels = j.at("choiceLabels").get<std::vector<std::string>>();
        return v;
    } catch (const std::exception& e) {
        return R::failure(std::string("design variable: ") + e.what());
    }
}

nlohmann::json toJson(const DesignSpace& space)
{
    nlohmann::json vars = nlohmann::json::array();
    for (const DesignVariable& v : space.variables)
        vars.push_back(toJson(v));
    nlohmann::json derived = nlohmann::json::array();
    for (const DerivedVariable& d : space.derived) {
        nlohmann::json j{{"id", d.id}, {"label", d.label}, {"unit", d.unit}, {"expression", d.expression}};
        if (!d.binding.empty())
            j["binding"] = d.binding;
        derived.push_back(std::move(j));
    }
    return {{"variables", vars}, {"derived", derived}};
}

Result<DesignSpace> designSpaceFromJson(const nlohmann::json& j)
{
    using R = Result<DesignSpace>;
    DesignSpace space;
    if (!j.is_object())
        return R::failure("the design space must be an object");
    const nlohmann::json vars = j.value("variables", nlohmann::json::array());
    for (const auto& item : vars) {
        auto v = designVariableFromJson(item);
        if (!v)
            return R::failure(v.message());
        space.variables.push_back(v.take());
    }
    const nlohmann::json derived = j.value("derived", nlohmann::json::array());
    for (const auto& item : derived) {
        DerivedVariable d;
        d.id = item.value("id", std::string());
        d.label = item.value("label", d.id);
        d.binding = item.value("binding", std::string());
        d.unit = item.value("unit", std::string());
        d.expression = item.value("expression", std::string());
        space.derived.push_back(std::move(d));
    }
    return space;
}

nlohmann::json toJson(const DesignPoint& point)
{
    nlohmann::json j{{"values", point.values}};
    if (!point.extra.empty())
        j["extra"] = point.extra;
    return j;
}

Result<DesignPoint> designPointFromJson(const nlohmann::json& j)
{
    try {
        DesignPoint p;
        const nlohmann::json values = j.value("values", nlohmann::json::object());
        for (const auto& [k, v] : values.items())
            p.values[k] = v.get<double>();
        if (j.contains("extra"))
            for (const auto& [k, v] : j.at("extra").items())
                p.extra[k] = v.get<double>();
        return p;
    } catch (const std::exception& e) {
        return Result<DesignPoint>::failure(std::string("design point: ") + e.what());
    }
}

}  // namespace resonant::optimization
