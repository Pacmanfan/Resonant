#include "functional/Parameters.h"

#include "geometry/Geometry.h"
#include "optimization/Expression.h"
#include "optimization/SceneBindings.h"
#include "project/Project.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace resonant::functional {

namespace {

const ParameterDef* definitionOf(const Geometry& g, const std::string& key)
{
    for (const ParameterDef& def : g.kind().parameters)
        if (def.key == key)
            return &def;
    return nullptr;
}

std::string format(double v)
{
    char text[32];
    std::snprintf(text, sizeof text, "%.6g", v);
    return text;
}

}  // namespace

Result<std::vector<ParameterValue>> resolveParameters(const FunctionalStructure& s, const Project& project)
{
    using R = Result<std::vector<ParameterValue>>;
    std::vector<ParameterValue> out;
    Values known;
    for (const Parameter& p : s.parameters) {
        ParameterValue v;
        v.id = p.id;
        v.name = p.name;
        v.unit = p.unit;
        v.binding = p.binding;
        v.group = p.group;
        v.description = p.description;
        v.kind = p.kind;
        v.minimum = p.minimum;
        v.maximum = p.maximum;
        v.editable = p.kind != ParameterKind::Derived;
        if (p.kind == ParameterKind::Free) {
            v.value = p.value;
        } else if (p.kind == ParameterKind::Bound) {
            const auto b = optimization::parseSceneBinding(p.binding);
            const SceneObject* o = b ? project.scene.find(Id(b->objectId)) : nullptr;
            if (!o)
                return R::failure("parameter '" + p.id + "': the bound object is not in the scene");
            v.objectName = o->state.name;
            if (b->kind == optimization::SceneBinding::Kind::Geometry) {
                if (!o->state.geometry)
                    return R::failure("parameter '" + p.id + "': '" + o->state.name + "' has no geometry");
                const ParameterDef* def = definitionOf(*o->state.geometry, b->field);
                if (!def)
                    return R::failure("parameter '" + p.id + "': '" + o->state.name + "' has no geometry parameter '" + b->field + "'");
                v.value = o->state.geometry->parameter(b->field);
                v.minimum = std::max(v.minimum, def->minValue);
                v.maximum = std::min(v.maximum, def->maxValue);
                if (v.unit.empty())
                    v.unit = def->unit;
            } else if (b->kind == optimization::SceneBinding::Kind::Position) {
                const int axis = b->field == "x" ? 0 : b->field == "y" ? 1 : 2;
                v.value = o->state.local.position[axis];
                if (v.unit.empty())
                    v.unit = "m";
            } else {
                return R::failure("parameter '" + p.id + "': only geometry and position bindings are supported");
            }
        }
        if (p.kind != ParameterKind::Derived)
            known[p.id] = v.value;
        out.push_back(std::move(v));
    }
    // Derived parameters, repeatedly until every one is known (validation refuses cycles).
    for (size_t pass = 0; pass <= s.parameters.size(); ++pass) {
        bool missing = false;
        for (size_t i = 0; i < s.parameters.size(); ++i) {
            const Parameter& p = s.parameters[i];
            if (p.kind != ParameterKind::Derived || known.count(p.id))
                continue;
            auto e = optimization::Expression::parse(p.expression);
            if (!e)
                return R::failure("parameter '" + p.id + "': " + e.message());
            bool ready = true;
            for (const std::string& n : e.value().names())
                ready = ready && known.count(n);
            if (!ready) {
                missing = true;
                continue;
            }
            auto value = e.value().evaluate(known);
            if (!value)
                return R::failure("parameter '" + p.id + "': " + value.message());
            out[i].value = value.value();
            known[p.id] = value.value();
        }
        if (!missing)
            return out;
    }
    return R::failure("derived parameters could not be evaluated (unknown names or a cycle)");
}

Values valuesOf(const std::vector<ParameterValue>& parameters)
{
    Values v;
    for (const ParameterValue& p : parameters)
        v[p.id] = p.value;
    return v;
}

Status applyParameters(FunctionalStructure& s, Project& project, const Values& values)
{
    auto current = resolveParameters(s, project);
    if (!current)
        return Status::failure(current.message());
    std::map<std::string, std::vector<std::pair<std::string, double>>> geometry;
    for (const auto& [id, value] : values) {
        const auto it = std::find_if(current.value().begin(), current.value().end(), [&](const ParameterValue& p) { return p.id == id; });
        if (it == current.value().end())
            return Status::failure("the structure has no parameter '" + id + "'");
        if (it->kind == ParameterKind::Derived)
            return Status::failure("'" + id + "' is derived (" + s.parameter(id)->expression + ") and cannot be set");
        if (!std::isfinite(value) || value < it->minimum || value > it->maximum)
            return Status::failure("'" + it->name + "' = " + format(value) + " lies outside its range [" + format(it->minimum) + ", " +
                                   format(it->maximum) + "]" + (it->unit.empty() ? "" : " " + it->unit));
        if (it->kind == ParameterKind::Free) {
            s.parameter(id)->value = value;
            continue;
        }
        const auto b = optimization::parseSceneBinding(it->binding);
        SceneObject* o = project.scene.find(Id(b->objectId));
        if (b->kind == optimization::SceneBinding::Kind::Geometry) {
            geometry[b->objectId].emplace_back(b->field, value);
            continue;
        }
        ObjectState state = o->state;
        state.local.position[b->field == "x" ? 0 : b->field == "y" ? 1 : 2] = value;
        if (Status st = project.scene.setState(o->id(), std::move(state)); !st)
            return st;
    }
    for (auto& [objectId, list] : geometry) {
        SceneObject* o = project.scene.find(Id(objectId));
        Geometry g = *o->state.geometry;
        // Several passes: a joint rule may reject one value until another is set.
        std::vector<std::pair<std::string, double>> pending = list;
        std::string lastError;
        for (size_t pass = 0; pass <= list.size() && !pending.empty(); ++pass) {
            std::vector<std::pair<std::string, double>> failed;
            for (const auto& [key, value] : pending)
                if (Status st = g.setParameter(key, value); !st) {
                    failed.emplace_back(key, value);
                    lastError = st.message();
                }
            if (failed.size() == pending.size())
                break;
            pending = std::move(failed);
        }
        if (!pending.empty())
            return Status::failure("invalid geometry of '" + o->state.name + "': " + lastError);
        if (Status st = g.validate(); !st)
            return Status::failure("invalid geometry of '" + o->state.name + "': " + st.message());
        ObjectState state = o->state;
        state.geometry = g;
        if (Status st = project.scene.setState(o->id(), std::move(state)); !st)
            return st;
    }
    return Status::success();
}

}  // namespace resonant::functional
