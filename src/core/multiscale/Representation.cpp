#include "multiscale/Representation.h"

#include "materials/Material.h"
#include "optimization/Evaluator.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace resonant::multiscale {

using nlohmann::json;

namespace {

const std::map<Scale, std::vector<std::string>>& kindsByScale()
{
    static const std::map<Scale, std::vector<std::string>> kinds{
        {Scale::Electronic, {"electronic-structure"}},
        {Scale::Atomistic, {"crystal", "atomic-structure"}},
        {Scale::Molecular, {"molecule", "molecular-system"}},
        {Scale::Microstructure, {"rve.voxel", "microstructure"}},
        {Scale::Effective, {"effective-material"}},
        {Scale::Continuum, {"continuum-material"}},
    };
    return kinds;
}

json stateJson(const State& s)
{
    json j = json::object();
    if (s.temperatureK)
        j["temperatureK"] = *s.temperatureK;
    if (s.pressurePa)
        j["pressurePa"] = *s.pressurePa;
    return j;
}

State stateFrom(const json& j)
{
    State s;
    if (j.is_object()) {
        if (j.contains("temperatureK") && j["temperatureK"].is_number())
            s.temperatureK = j["temperatureK"].get<double>();
        if (j.contains("pressurePa") && j["pressurePa"].is_number())
            s.pressurePa = j["pressurePa"].get<double>();
    }
    return s;
}

}  // namespace

std::string_view toString(Scale s)
{
    switch (s) {
    case Scale::Electronic: return "electronic";
    case Scale::Atomistic: return "atomistic";
    case Scale::Molecular: return "molecular";
    case Scale::Microstructure: return "microstructure";
    case Scale::Effective: return "effective";
    case Scale::Continuum: return "continuum";
    }
    return "continuum";
}

std::optional<Scale> scaleFromString(std::string_view t)
{
    for (Scale s : {Scale::Electronic, Scale::Atomistic, Scale::Molecular, Scale::Microstructure, Scale::Effective, Scale::Continuum})
        if (toString(s) == t)
            return s;
    return std::nullopt;
}

int rank(Scale s)
{
    return static_cast<int>(s);
}

// --- representations ----------------------------------------------------------------------------

std::string Representation::revision() const
{
    return optimization::contentKey({{"kind", kind},
                                     {"scale", std::string(toString(scale))},
                                     {"data", data},
                                     {"constituents", constituents},
                                     {"state", stateJson(state)},
                                     {"boundaryConditions", boundaryConditions}});
}

json Representation::toJson() const
{
    return {{"id", id},
            {"name", name},
            {"description", description},
            {"scale", std::string(toString(scale))},
            {"kind", kind},
            {"materialId", materialId},
            {"data", data},
            {"constituents", constituents},
            {"state", stateJson(state)},
            {"assumptions", assumptions},
            {"boundaryConditions", boundaryConditions},
            {"provenance", provenance},
            {"source", source},
            {"validation", validation},
            {"validationNote", validationNote},
            {"uncertainty", uncertainty},
            {"modelCard", modelCard},
            {"revision", revision()}};
}

Result<Representation> Representation::fromJson(const json& d)
{
    using R = Result<Representation>;
    try {
        if (!d.is_object())
            return R::failure("a representation must be an object");
        Representation r;
        r.id = d.value("id", "");
        r.name = d.value("name", r.id);
        r.description = d.value("description", "");
        const auto scale = scaleFromString(d.value("scale", ""));
        if (!scale)
            return R::failure("the representation '" + r.id + "' has an unknown scale '" + d.value("scale", "") +
                              "' (electronic, atomistic, molecular, microstructure, effective, continuum)");
        r.scale = *scale;
        r.kind = d.value("kind", "");
        r.materialId = d.value("materialId", "");
        r.data = d.value("data", json::object());
        r.constituents = d.value("constituents", std::vector<std::string>{});
        r.state = stateFrom(d.value("state", json::object()));
        r.assumptions = d.value("assumptions", std::vector<std::string>{});
        r.boundaryConditions = d.value("boundaryConditions", json::object());
        r.provenance = d.value("provenance", "user");
        r.source = d.value("source", "");
        r.validation = d.value("validation", "unvalidated");
        r.validationNote = d.value("validationNote", "");
        r.uncertainty = d.value("uncertainty", "");
        r.modelCard = d.value("modelCard", "");
        return r;
    } catch (const std::exception& e) {
        return R::failure(std::string("invalid representation: ") + e.what());
    }
}

// --- derivations --------------------------------------------------------------------------------

json Derivation::toJson() const
{
    json props = json::array();
    for (const DerivedProperty& p : properties) {
        json j{{"domain", p.domain}, {"key", p.key}, {"component", p.component}, {"value", p.value}, {"unit", p.unit}};
        if (p.uncertainty)
            j["uncertainty"] = *p.uncertainty;
        props.push_back(std::move(j));
    }
    return {{"id", id},
            {"method", method},
            {"modelCard", modelCard},
            {"solver", solver},
            {"solverVersion", solverVersion},
            {"representation", representation},
            {"representationRevision", representationRevision},
            {"sourceScale", std::string(toString(sourceScale))},
            {"targetScale", std::string(toString(targetScale))},
            {"material", material},
            {"properties", props},
            {"inputs", inputs},
            {"boundaryConditions", boundaryConditions},
            {"convergence", convergence},
            {"resolution", resolution},
            {"sampling", sampling},
            {"conditions", stateJson(conditions)},
            {"constituentKeys", constituentKeys},
            {"created", created},
            {"calculation", calculation},
            {"parent", parent}};
}

Result<Derivation> Derivation::fromJson(const json& d)
{
    using R = Result<Derivation>;
    try {
        Derivation x;
        x.id = d.value("id", "");
        x.method = d.value("method", "");
        x.modelCard = d.value("modelCard", "");
        x.solver = d.value("solver", "");
        x.solverVersion = d.value("solverVersion", "");
        x.representation = d.value("representation", "");
        x.representationRevision = d.value("representationRevision", "");
        const auto from = scaleFromString(d.value("sourceScale", "microstructure"));
        const auto to = scaleFromString(d.value("targetScale", "continuum"));
        if (!from || !to)
            return R::failure("the derivation '" + x.id + "' has an unknown scale");
        x.sourceScale = *from;
        x.targetScale = *to;
        x.material = d.value("material", "");
        for (const json& p : d.value("properties", json::array())) {
            DerivedProperty q;
            q.domain = p.value("domain", "");
            q.key = p.value("key", "");
            q.component = p.value("component", "isotropic");
            q.unit = p.value("unit", "");
            q.value = p.value("value", 0.0);
            if (p.contains("uncertainty") && p["uncertainty"].is_number())
                q.uncertainty = p["uncertainty"].get<double>();
            x.properties.push_back(std::move(q));
        }
        x.inputs = d.value("inputs", json::object());
        x.boundaryConditions = d.value("boundaryConditions", json::object());
        x.convergence = d.value("convergence", json::object());
        x.resolution = d.value("resolution", json::object());
        x.sampling = d.value("sampling", json::object());
        x.conditions = stateFrom(d.value("conditions", json::object()));
        x.constituentKeys = d.value("constituentKeys", std::map<std::string, std::string>{});
        x.created = d.value("created", "");
        x.calculation = d.value("calculation", "");
        x.parent = d.value("parent", "");
        return x;
    } catch (const std::exception& e) {
        return R::failure(std::string("invalid derivation: ") + e.what());
    }
}

// --- the model ----------------------------------------------------------------------------------

const Representation* MultiscaleModel::find(std::string_view id) const
{
    for (const Representation& r : representations)
        if (r.id == id)
            return &r;
    return nullptr;
}

const Derivation* MultiscaleModel::derivation(std::string_view id) const
{
    for (const Derivation& d : derivations)
        if (d.id == id)
            return &d;
    return nullptr;
}

json MultiscaleModel::toJson() const
{
    json reps = json::array(), rels = json::array(), ders = json::array();
    for (const auto& r : representations)
        reps.push_back(r.toJson());
    for (const auto& r : relationships)
        rels.push_back({{"from", r.from}, {"to", r.to}, {"kind", r.kind}});
    for (const auto& d : derivations)
        ders.push_back(d.toJson());
    return {{"format", kFormat}, {"version", 1}, {"representations", reps}, {"relationships", rels}, {"derivations", ders}};
}

Result<MultiscaleModel> MultiscaleModel::fromJson(const json& d)
{
    using R = Result<MultiscaleModel>;
    if (d.is_null())
        return MultiscaleModel{};
    if (!d.is_object() || d.value("format", "") != kFormat)
        return R::failure(std::string("the multiscale extension is not a ") + kFormat + " document");
    if (d.value("version", 0) != 1)
        return R::failure("unsupported multiscale version " + std::to_string(d.value("version", 0)));
    MultiscaleModel m;
    for (const json& r : d.value("representations", json::array())) {
        auto x = Representation::fromJson(r);
        if (!x)
            return R::failure(x.message());
        m.representations.push_back(x.take());
    }
    for (const json& r : d.value("relationships", json::array()))
        m.relationships.push_back({r.value("from", ""), r.value("to", ""), r.value("kind", "")});
    for (const json& r : d.value("derivations", json::array())) {
        auto x = Derivation::fromJson(r);
        if (!x)
            return R::failure(x.message());
        m.derivations.push_back(x.take());
    }
    return m;
}

std::vector<std::string> validateModel(const MultiscaleModel& m, const Project& project)
{
    std::vector<std::string> issues;
    std::set<std::string> ids;
    for (const Representation& r : m.representations) {
        if (r.id.empty() || r.id.find_first_of(" \t\n:/") != std::string::npos)
            issues.push_back("a representation id must be non-empty, without spaces, colons or slashes ('" + r.id + "')");
        else if (!ids.insert(r.id).second)
            issues.push_back("the representation id '" + r.id + "' is used twice");
        const auto& kinds = kindsByScale().at(r.scale);
        if (std::find(kinds.begin(), kinds.end(), r.kind) == kinds.end())
            issues.push_back("'" + r.id + "': the kind '" + r.kind + "' is not a " + std::string(toString(r.scale)) + " representation");
        if (!r.materialId.empty() && !project.materials.find(Id(r.materialId)))
            issues.push_back("'" + r.id + "' describes the material '" + r.materialId + "', which is not in the library");
        for (const std::string& c : r.constituents)
            if (!project.materials.find(Id(c)))
                issues.push_back("'" + r.id + "': the constituent '" + c + "' is not in the library");
        if (r.provenance != "synthetic" && r.provenance != "imported" && r.provenance != "computed" && r.provenance != "user")
            issues.push_back("'" + r.id + "': provenance must be synthetic, imported, computed or user");
        if (r.validation != "unvalidated" && r.validation != "verified" && r.validation != "validated")
            issues.push_back("'" + r.id + "': validation must be unvalidated, verified or validated");
    }
    const auto exists = [&](const std::string& ref) {
        const auto colon = ref.find(':');
        if (colon == std::string::npos)
            return false;
        const std::string type = ref.substr(0, colon), id = ref.substr(colon + 1);
        if (type == "representation")
            return m.find(id) != nullptr;
        if (type == "material")
            return project.materials.find(Id(id)) != nullptr;
        if (type == "derivation")
            return m.derivation(id) != nullptr;
        return false;
    };
    for (const Relationship& r : m.relationships)
        if (!exists(r.from) || !exists(r.to))
            issues.push_back("the relationship " + r.from + " -" + r.kind + "-> " + r.to + " names something that does not exist");
    std::set<std::string> derivationIds;
    for (const Derivation& d : m.derivations) {
        if (d.id.empty() || !derivationIds.insert(d.id).second)
            issues.push_back("every derivation needs an id of its own ('" + d.id + "')");
        if (!d.representation.empty() && !m.find(d.representation))
            issues.push_back("the derivation '" + d.id + "' comes from the representation '" + d.representation + "', which does not exist");
    }
    return issues;
}

Result<MultiscaleModel> modelOf(const Project& project)
{
    const auto it = project.extensions.find(kExtension);
    if (it == project.extensions.end())
        return MultiscaleModel{};
    return MultiscaleModel::fromJson(*it);
}

void storeModel(Project& project, const MultiscaleModel& model)
{
    if (model.representations.empty() && model.relationships.empty() && model.derivations.empty()) {
        project.extensions.erase(kExtension);
        return;
    }
    project.extensions[kExtension] = model.toJson();
}

std::string materialContentKey(const Material& material)
{
    json j = toJson(material);
    j.erase("name");
    j.erase("description");
    return optimization::contentKey(j);
}

// --- staleness and lineage -------------------------------------------------------------------------

std::vector<StaleProperty> staleProperties(const MultiscaleModel& m, const Project& project)
{
    std::vector<StaleProperty> out;
    for (const Material& material : project.materials.materials())
        for (PhysicsDomain domain : material.domains()) {
            const PropertySet* set = material.properties(domain);
            if (!set)
                continue;
            for (const auto& [key, value] : *set) {
                if (value.derivation.empty())
                    continue;
                StaleProperty s{material.id().str(), std::string(toString(domain)), key, value.derivation, {}};
                const Derivation* d = m.derivation(value.derivation);
                if (!d) {
                    s.reasons.push_back("its derivation '" + value.derivation + "' is not in the project");
                    out.push_back(std::move(s));
                    continue;
                }
                const Representation* r = m.find(d->representation);
                if (!r)
                    s.reasons.push_back("the source representation '" + d->representation + "' was removed");
                else if (r->revision() != d->representationRevision)
                    s.reasons.push_back("the source representation '" + r->name + "' changed since the derivation");
                for (const auto& [id, keyThen] : d->constituentKeys) {
                    const Material* c = project.materials.find(Id(id));
                    if (!c)
                        s.reasons.push_back("the constituent '" + id + "' was removed");
                    else if (materialContentKey(*c) != keyThen)
                        s.reasons.push_back("the constituent '" + c->name() + "' changed since the derivation");
                }
                bool matched = false;
                for (const DerivedProperty& p : d->properties)
                    if (p.domain == s.domain && p.key == key && std::abs(p.value - value.value) <= 1e-12 * std::max(1.0, std::abs(p.value)))
                        matched = true;
                if (!matched)
                    s.reasons.push_back("the value was changed after the derivation");
                if (!s.reasons.empty())
                    out.push_back(std::move(s));
            }
        }
    return out;
}

namespace {

json representationSummary(const Representation& r)
{
    return {{"id", r.id},           {"name", r.name},       {"scale", std::string(toString(r.scale))}, {"kind", r.kind},
            {"revision", r.revision()}, {"provenance", r.provenance}, {"validation", r.validation},     {"modelCard", r.modelCard}};
}

// The representations below a representation: its constituents' derivations, recursively.
json chainBelow(const MultiscaleModel& m, const Project& project, const Representation& r, std::set<std::string>& seen, int depth)
{
    json steps = json::array();
    if (depth > 8 || !seen.insert(r.id).second)
        return steps;
    for (const std::string& c : r.constituents) {
        const Material* mat = project.materials.find(Id(c));
        json step{{"material", c}, {"name", mat ? mat->name() : std::string("(missing)")}, {"derivedFrom", json::array()}};
        if (mat)
            for (PhysicsDomain domain : mat->domains())
                if (const PropertySet* set = mat->properties(domain))
                    for (const auto& [key, value] : *set) {
                        if (value.derivation.empty())
                            continue;
                        const Derivation* d = m.derivation(value.derivation);
                        const Representation* below = d ? m.find(d->representation) : nullptr;
                        json link{{"property", std::string(toString(domain)) + "." + key}, {"derivation", value.derivation}};
                        if (below) {
                            link["representation"] = representationSummary(*below);
                            link["below"] = chainBelow(m, project, *below, seen, depth + 1);
                        }
                        step["derivedFrom"].push_back(std::move(link));
                    }
        steps.push_back(std::move(step));
    }
    return steps;
}

}  // namespace

json lineageOf(const MultiscaleModel& m, const Project& project, const std::string& materialId)
{
    const Material* material = project.materials.find(Id(materialId));
    if (!material)
        return json();
    const auto stale = staleProperties(m, project);
    json props = json::array();
    for (PhysicsDomain domain : material->domains()) {
        const PropertySet* set = material->properties(domain);
        if (!set)
            continue;
        for (const auto& [key, value] : *set) {
            json p{{"domain", std::string(toString(domain))},
                   {"key", key},
                   {"value", value.value},
                   {"provenance", std::string(toString(value.provenance))},
                   {"source", value.source},
                   {"derivation", value.derivation},
                   {"stale", false},
                   {"staleReasons", json::array()}};
            if (value.uncertainty)
                p["uncertainty"] = *value.uncertainty;
            for (const StaleProperty& s : stale)
                if (s.material == materialId && s.domain == std::string(toString(domain)) && s.key == key) {
                    p["stale"] = true;
                    p["staleReasons"] = s.reasons;
                }
            if (const Derivation* d = m.derivation(value.derivation)) {
                p["derivationRecord"] = d->toJson();
                if (const Representation* r = m.find(d->representation)) {
                    std::set<std::string> seen;
                    p["representation"] = representationSummary(*r);
                    p["chain"] = chainBelow(m, project, *r, seen, 0);
                }
            }
            props.push_back(std::move(p));
        }
    }
    json described = json::array();
    for (const Representation& r : m.representations)
        if (r.materialId == materialId)
            described.push_back(representationSummary(r));
    return {{"material", materialId}, {"name", material->name()}, {"properties", props}, {"representations", described}};
}

}  // namespace resonant::multiscale
