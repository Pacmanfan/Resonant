#include "functional/FunctionalStructure.h"

#include "geometry/Geometry.h"
#include "optimization/Expression.h"
#include "optimization/SceneBindings.h"
#include "project/Project.h"

#include <cmath>
#include <functional>
#include <set>

namespace resonant::functional {

using nlohmann::json;

std::string_view toString(ParameterKind kind)
{
    switch (kind) {
    case ParameterKind::Bound: return "bound";
    case ParameterKind::Free: return "free";
    case ParameterKind::Derived: return "derived";
    }
    return "free";
}

namespace {

double number(const json& j, const char* key, double fallback)
{
    const auto it = j.find(key);
    if (it == j.end() || it->is_null())
        return fallback;
    if (!it->is_number())
        throw std::runtime_error(std::string("'") + key + "' must be a number");
    return it->get<double>();
}

json finite(double v)
{
    return std::isfinite(v) ? json(v) : json(nullptr);
}

bool validId(const std::string& id)
{
    if (id.empty() || std::isdigit(static_cast<unsigned char>(id[0])))
        return false;
    for (char c : id)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            return false;
    return true;
}

}  // namespace

Result<FunctionalStructure> FunctionalStructure::fromJson(const json& d)
{
    using R = Result<FunctionalStructure>;
    try {
        if (!d.is_object() || d.value("format", "") != kFormat)
            return R::failure(std::string("not a functional structure (format must be '") + kFormat + "')");
        if (d.value("version", 0) != kVersion)
            return R::failure("unsupported functional structure version " + std::to_string(d.value("version", 0)));
        FunctionalStructure s;
        s.id = d.value("id", "");
        s.name = d.value("name", s.id);
        s.description = d.value("description", "");
        if (s.id.empty())
            return R::failure("the structure has no id");
        for (const json& b : d.value("bodies", json::array()))
            s.bodies.push_back({b.value("objectId", ""), b.value("role", "")});
        for (const json& p : d.value("parameters", json::array())) {
            Parameter x;
            x.id = p.value("id", "");
            x.name = p.value("name", x.id);
            const std::string kind = p.value("kind", p.contains("expression") ? "derived" : p.contains("binding") ? "bound" : "free");
            if (kind == "bound")
                x.kind = ParameterKind::Bound;
            else if (kind == "derived")
                x.kind = ParameterKind::Derived;
            else if (kind == "free")
                x.kind = ParameterKind::Free;
            else
                return R::failure("parameter '" + x.id + "': unknown kind '" + kind + "'");
            x.binding = p.value("binding", "");
            x.expression = p.value("expression", "");
            x.unit = p.value("unit", "");
            x.value = number(p, "value", 0.0);
            x.minimum = number(p, "min", -std::numeric_limits<double>::infinity());
            x.maximum = number(p, "max", std::numeric_limits<double>::infinity());
            x.defaultValue = number(p, "default", std::numeric_limits<double>::quiet_NaN());
            x.group = p.value("group", "");
            x.description = p.value("description", "");
            s.parameters.push_back(std::move(x));
        }
        for (const json& r : d.value("regions", json::array()))
            s.regions.push_back({r.value("id", ""), r.value("objectId", ""), r.value("role", ""), r.value("materialId", "")});
        for (const json& b : d.value("boundaries", json::array()))
            s.boundaries.push_back({b.value("id", ""), b.value("objectId", ""), b.value("role", ""), b.value("condition", ""),
                                    b.value("settings", json::object())});
        for (const json& x : d.value("domains", json::array()))
            s.domains.push_back(x.get<std::string>());
        s.excitation = d.value("excitation", json::object());
        s.environment = d.value("environment", json::object());
        if (const auto it = d.find("frequencyRange"); it != d.end() && it->is_array() && it->size() == 2) {
            s.frequencyLow = (*it)[0].get<double>();
            s.frequencyHigh = (*it)[1].get<double>();
        }
        const json a = d.value("analysis", json::object());
        s.analysis.kind = a.value("kind", "");
        s.analysis.model = a.value("model", "");
        s.analysis.settings = a.value("settings", json::object());
        for (const json& o : d.value("objectives", json::array())) {
            Objective x;
            x.id = o.value("id", "");
            x.metric = o.value("metric", "");
            x.sense = o.value("sense", "target");
            x.target = number(o, "target", 0.0);
            x.low = number(o, "low", 0.0);
            x.high = number(o, "high", 0.0);
            x.weight = number(o, "weight", 1.0);
            x.description = o.value("description", "");
            s.objectives.push_back(std::move(x));
        }
        for (const json& c : d.value("constraints", json::array())) {
            Constraint x;
            x.id = c.value("id", "");
            x.expression = c.value("expression", "");
            x.relation = c.value("relation", "<=");
            x.limit = number(c, "limit", 0.0);
            x.unit = c.value("unit", "");
            x.category = c.value("category", "manufacturing");
            x.process = c.value("process", "");
            x.hard = c.value("hard", true);
            x.description = c.value("description", "");
            s.constraints.push_back(std::move(x));
        }
        s.physicalSystem = d.value("physicalSystem", "");
        s.notes = d.value("notes", "");
        return s;
    } catch (const std::exception& e) {
        return R::failure(std::string("invalid functional structure: ") + e.what());
    }
}

json FunctionalStructure::toJson() const
{
    json d{{"format", kFormat}, {"version", kVersion}, {"id", id}, {"name", name}};
    if (!description.empty())
        d["description"] = description;
    d["bodies"] = json::array();
    for (const Body& b : bodies)
        d["bodies"].push_back({{"objectId", b.objectId}, {"role", b.role}});
    d["parameters"] = json::array();
    for (const Parameter& p : parameters) {
        json x{{"id", p.id}, {"name", p.name}, {"kind", toString(p.kind)}, {"unit", p.unit}};
        if (p.kind == ParameterKind::Bound)
            x["binding"] = p.binding;
        if (p.kind == ParameterKind::Derived)
            x["expression"] = p.expression;
        if (p.kind == ParameterKind::Free)
            x["value"] = p.value;
        if (std::isfinite(p.minimum))
            x["min"] = p.minimum;
        if (std::isfinite(p.maximum))
            x["max"] = p.maximum;
        if (std::isfinite(p.defaultValue))
            x["default"] = p.defaultValue;
        if (!p.group.empty())
            x["group"] = p.group;
        if (!p.description.empty())
            x["description"] = p.description;
        d["parameters"].push_back(std::move(x));
    }
    d["regions"] = json::array();
    for (const Region& r : regions)
        d["regions"].push_back({{"id", r.id}, {"objectId", r.objectId}, {"role", r.role}, {"materialId", r.materialId}});
    d["boundaries"] = json::array();
    for (const Boundary& b : boundaries)
        d["boundaries"].push_back({{"id", b.id}, {"objectId", b.objectId}, {"role", b.role}, {"condition", b.condition}, {"settings", b.settings}});
    d["domains"] = domains;
    d["excitation"] = excitation;
    d["environment"] = environment;
    if (frequencyHigh > 0.0)
        d["frequencyRange"] = {frequencyLow, frequencyHigh};
    d["analysis"] = {{"kind", analysis.kind}, {"model", analysis.model}, {"settings", analysis.settings}};
    d["objectives"] = json::array();
    for (const Objective& o : objectives) {
        json x{{"id", o.id}, {"metric", o.metric}, {"sense", o.sense}, {"weight", o.weight}};
        if (o.sense == "target")
            x["target"] = o.target;
        if (o.sense == "within") {
            x["low"] = o.low;
            x["high"] = o.high;
        }
        if (!o.description.empty())
            x["description"] = o.description;
        d["objectives"].push_back(std::move(x));
    }
    d["constraints"] = json::array();
    for (const Constraint& c : constraints) {
        json x{{"id", c.id}, {"expression", c.expression}, {"relation", c.relation}, {"limit", finite(c.limit)}, {"unit", c.unit},
               {"category", c.category}, {"hard", c.hard}};
        if (!c.process.empty())
            x["process"] = c.process;
        if (!c.description.empty())
            x["description"] = c.description;
        d["constraints"].push_back(std::move(x));
    }
    if (!physicalSystem.empty())
        d["physicalSystem"] = physicalSystem;
    if (!notes.empty())
        d["notes"] = notes;
    return d;
}

const Parameter* FunctionalStructure::parameter(const std::string& pid) const
{
    for (const Parameter& p : parameters)
        if (p.id == pid)
            return &p;
    return nullptr;
}

Parameter* FunctionalStructure::parameter(const std::string& pid)
{
    for (Parameter& p : parameters)
        if (p.id == pid)
            return &p;
    return nullptr;
}

const Body* FunctionalStructure::body(const std::string& role) const
{
    for (const Body& b : bodies)
        if (b.role == role)
            return &b;
    return nullptr;
}

const Region* FunctionalStructure::region(const std::string& role) const
{
    for (const Region& r : regions)
        if (r.role == role)
            return &r;
    return nullptr;
}

const Boundary* FunctionalStructure::boundary(const std::string& role) const
{
    for (const Boundary& b : boundaries)
        if (b.role == role)
            return &b;
    return nullptr;
}

std::vector<Issue> validateStructure(const FunctionalStructure& s, const Project& project)
{
    std::vector<Issue> issues;
    const auto add = [&](std::string field, std::string message) { issues.push_back({std::move(field), std::move(message)}); };
    if (s.id.empty() || s.id.find_first_of(" \t\n/") != std::string::npos)
        add("id", "the structure id must be non-empty, without spaces or slashes");
    const auto objectExists = [&](const std::string& id) { return !id.empty() && project.scene.find(Id(id)) != nullptr; };
    std::set<std::string> roles;
    for (size_t i = 0; i < s.bodies.size(); ++i) {
        const Body& b = s.bodies[i];
        const std::string field = "bodies[" + std::to_string(i) + "]";
        if (!objectExists(b.objectId))
            add(field + ".objectId", "the object '" + b.objectId + "' is not in the scene");
        if (b.role.empty() || !roles.insert(b.role).second)
            add(field + ".role", "every body needs a role of its own");
    }
    for (size_t i = 0; i < s.regions.size(); ++i) {
        const Region& r = s.regions[i];
        const std::string field = "regions[" + std::to_string(i) + "]";
        if (!r.objectId.empty() && !objectExists(r.objectId))
            add(field + ".objectId", "the object '" + r.objectId + "' is not in the scene");
        if (r.materialId.empty() || !project.materials.find(Id(r.materialId)))
            add(field + ".materialId", "the material '" + r.materialId + "' is not in the project's library");
    }
    for (size_t i = 0; i < s.boundaries.size(); ++i)
        if (!s.boundaries[i].objectId.empty() && !objectExists(s.boundaries[i].objectId))
            add("boundaries[" + std::to_string(i) + "].objectId", "the object '" + s.boundaries[i].objectId + "' is not in the scene");
    // Parameters.
    std::set<std::string> ids;
    for (const Parameter& p : s.parameters) {
        const std::string field = "parameters." + p.id;
        if (!validId(p.id))
            add(field, "a parameter id must be an identifier (letters, digits, _), not '" + p.id + "'");
        else if (!ids.insert(p.id).second)
            add(field, "the parameter id is used twice");
        if (p.minimum > p.maximum)
            add(field, "the minimum is larger than the maximum");
        if (p.kind == ParameterKind::Free && !(std::isfinite(p.value) && p.value >= p.minimum && p.value <= p.maximum))
            add(field + ".value", "the value lies outside the parameter's range");
        if (p.kind == ParameterKind::Bound) {
            const auto b = optimization::parseSceneBinding(p.binding);
            if (!b || (b->kind != optimization::SceneBinding::Kind::Geometry && b->kind != optimization::SceneBinding::Kind::Position)) {
                add(field + ".binding", "'" + p.binding + "' is not a geometry or position binding (object/<id>/geometry/<key>)");
                continue;
            }
            const SceneObject* o = project.scene.find(Id(b->objectId));
            if (!o) {
                add(field + ".binding", "the object '" + b->objectId + "' is not in the scene");
                continue;
            }
            if (b->kind == optimization::SceneBinding::Kind::Geometry) {
                bool known = false;
                if (o->state.geometry)
                    for (const ParameterDef& def : o->state.geometry->kind().parameters)
                        known = known || def.key == b->field;
                if (!known)
                    add(field + ".binding", "'" + o->state.name + "' has no geometry parameter '" + b->field + "'");
            }
        }
    }
    // Derived expressions: of known parameters, without cycles.
    std::map<std::string, std::vector<std::string>> uses;
    for (const Parameter& p : s.parameters) {
        if (p.kind != ParameterKind::Derived)
            continue;
        auto e = optimization::Expression::parse(p.expression);
        if (!e) {
            add("parameters." + p.id + ".expression", e.message());
            continue;
        }
        for (const std::string& n : e.value().names()) {
            if (!s.parameter(n))
                add("parameters." + p.id + ".expression", "unknown parameter '" + n + "'");
            uses[p.id].push_back(n);
        }
    }
    std::set<std::string> done, active;
    std::function<bool(const std::string&)> cyclic = [&](const std::string& id) {
        if (active.count(id))
            return true;
        if (done.count(id))
            return false;
        active.insert(id);
        for (const std::string& n : uses[id])
            if (cyclic(n))
                return true;
        active.erase(id);
        done.insert(id);
        return false;
    };
    for (const auto& [id, list] : uses)
        if (cyclic(id)) {
            add("parameters." + id + ".expression", "derived parameters depend on each other in a cycle");
            break;
        }
    for (const Constraint& c : s.constraints) {
        if (c.relation != "<=" && c.relation != ">=")
            add("constraints." + c.id + ".relation", "the relation must be '<=' or '>='");
        auto e = optimization::Expression::parse(c.expression);
        if (!e)
            add("constraints." + c.id + ".expression", e.message());
    }
    for (const Objective& o : s.objectives) {
        if (o.metric.empty())
            add("objectives." + o.id + ".metric", "the objective names no metric");
        if (o.sense != "minimize" && o.sense != "maximize" && o.sense != "target" && o.sense != "within")
            add("objectives." + o.id + ".sense", "the sense must be minimize, maximize, target or within");
        if (o.sense == "within" && !(o.low < o.high))
            add("objectives." + o.id, "a range objective needs low < high");
        if (!(o.weight >= 0.0))
            add("objectives." + o.id + ".weight", "weights cannot be negative");
    }
    if (s.frequencyHigh > 0.0 && !(s.frequencyLow >= 0.0 && s.frequencyLow < s.frequencyHigh))
        add("frequencyRange", "the frequency range must be ascending and non-negative");
    if (s.analysis.kind.empty())
        add("analysis.kind", "the structure names no analysis");
    return issues;
}

Result<std::vector<FunctionalStructure>> structuresOf(const Project& project)
{
    using R = Result<std::vector<FunctionalStructure>>;
    std::vector<FunctionalStructure> out;
    const auto it = project.extensions.find(kExtension);
    if (it == project.extensions.end())
        return out;
    for (const json& d : it->value("structures", json::array())) {
        auto s = FunctionalStructure::fromJson(d);
        if (!s)
            return R::failure(s.message());
        out.push_back(s.take());
    }
    return out;
}

void storeStructures(Project& project, const std::vector<FunctionalStructure>& structures)
{
    json list = json::array();
    for (const FunctionalStructure& s : structures)
        list.push_back(s.toJson());
    if (list.empty()) {
        project.extensions.erase(kExtension);
        return;
    }
    project.extensions[kExtension] = {{"format", kWorkspaceFormat}, {"version", 1}, {"structures", std::move(list)}};
}

}  // namespace resonant::functional
