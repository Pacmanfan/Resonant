#include "materialstate/StateModel.h"

#include "project/Project.h"

#include <algorithm>
#include <cmath>

namespace resonant::materialstate {

using nlohmann::json;

std::string_view toString(VariableClass c)
{
    switch (c) {
    case VariableClass::Reversible: return "reversible";
    case VariableClass::Irreversible: return "irreversible";
    case VariableClass::History: return "history";
    }
    return "reversible";
}

json StateVariable::toJson() const
{
    return {{"key", key}, {"label", label}, {"unit", unit}, {"class", std::string(toString(kind))}, {"description", description}};
}

json ModelCard::toJson() const
{
    json vars = json::array(), params = json::array();
    for (const auto& v : variables)
        vars.push_back(v.toJson());
    for (const auto& p : parameters)
        params.push_back({{"key", p.key}, {"label", p.label}, {"unit", p.unit}, {"value", p.value}, {"description", p.description}});
    return {{"id", id},           {"title", title},         {"domain", domain},     {"status", status},       {"variables", vars},
            {"parameters", params}, {"assumptions", assumptions}, {"excluded", excluded}, {"validity", validity}, {"method", method},
            {"reference", reference}, {"loadings", loadings}};
}

StateField::Summary StateField::summary(std::string_view key, size_t component) const
{
    Summary s;
    bool first = true;
    double sum = 0.0;
    size_t n = 0;
    for (const PointState& p : points) {
        const auto it = p.find(key);
        if (it == p.end() || component >= it->second.size())
            continue;
        const double v = it->second[component];
        s.min = first ? v : std::min(s.min, v);
        s.max = first ? v : std::max(s.max, v);
        first = false;
        sum += v;
        ++n;
    }
    s.mean = n ? sum / static_cast<double>(n) : 0.0;
    return s;
}

json StepConvergence::toJson() const
{
    return {{"converged", converged}, {"iterations", iterations}, {"residual", residual}, {"message", message}};
}

json StateRecord::toJson() const
{
    return {{"id", id},           {"name", name},         {"model", model},     {"kind", kind},       {"material", material},
            {"region", region},   {"created", created},   {"settings", settings}, {"history", history}, {"final", final},
            {"summary", summary}, {"committable", committable}};
}

Result<StateRecord> StateRecord::fromJson(const json& d)
{
    using R = Result<StateRecord>;
    try {
        StateRecord r;
        r.id = d.value("id", "");
        if (r.id.empty())
            return R::failure("a state record needs an id");
        r.name = d.value("name", r.id);
        r.model = d.value("model", "");
        r.kind = d.value("kind", "");
        r.material = d.value("material", "");
        r.region = d.value("region", "");
        r.created = d.value("created", "");
        r.settings = d.value("settings", json::object());
        r.history = d.value("history", json::array());
        r.final = d.value("final", json::object());
        r.summary = d.value("summary", json::object());
        r.committable = d.value("committable", json::array());
        return r;
    } catch (const std::exception& e) {
        return R::failure(std::string("invalid state record: ") + e.what());
    }
}

const StateRecord* Workspace::record(std::string_view id) const
{
    for (const auto& r : records)
        if (r.id == id)
            return &r;
    return nullptr;
}

json Workspace::toJson() const
{
    json recs = json::array();
    for (const auto& r : records)
        recs.push_back(r.toJson());
    return {{"format", kFormat}, {"version", 1}, {"studies", studies}, {"records", recs}};
}

Result<Workspace> Workspace::fromJson(const json& d)
{
    using R = Result<Workspace>;
    if (d.is_null())
        return Workspace{};
    if (!d.is_object() || d.value("format", "") != kFormat)
        return R::failure(std::string("the material-state extension is not a ") + kFormat + " document");
    if (d.value("version", 0) != 1)
        return R::failure("unsupported material-state version");
    Workspace w;
    w.studies = d.value("studies", json::array());
    for (const json& r : d.value("records", json::array())) {
        auto x = StateRecord::fromJson(r);
        if (!x)
            return R::failure(x.message());
        w.records.push_back(x.take());
    }
    return w;
}

Result<Workspace> workspaceOf(const Project& project)
{
    const auto it = project.extensions.find(kExtension);
    if (it == project.extensions.end())
        return Workspace{};
    return Workspace::fromJson(*it);
}

void storeWorkspace(Project& project, const Workspace& w)
{
    if (w.studies.empty() && w.records.empty()) {
        project.extensions.erase(kExtension);
        return;
    }
    project.extensions[kExtension] = w.toJson();
}

}  // namespace resonant::materialstate
