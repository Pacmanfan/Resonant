#include "functional/Dependencies.h"

#include "geometry/Geometry.h"
#include "optimization/Evaluator.h"
#include "optimization/SceneBindings.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"

#include <algorithm>

namespace resonant::functional {

using nlohmann::json;

json DependencyRecord::toJson() const
{
    return {{"geometry", geometry},   {"materials", materials}, {"definition", definition}, {"discretization", discretization},
            {"objects", objects},     {"materialKeys", materialKeys}};
}

DependencyRecord DependencyRecord::fromJson(const json& v)
{
    DependencyRecord r;
    r.geometry = v.value("geometry", "");
    r.materials = v.value("materials", "");
    r.definition = v.value("definition", "");
    r.discretization = v.value("discretization", "");
    r.objects = v.value("objects", std::map<std::string, std::string>{});
    r.materialKeys = v.value("materialKeys", std::map<std::string, std::string>{});
    return r;
}

std::set<std::string> referencedObjects(const FunctionalStructure& s)
{
    std::set<std::string> ids;
    for (const Body& b : s.bodies)
        ids.insert(b.objectId);
    for (const Region& r : s.regions)
        if (!r.objectId.empty())
            ids.insert(r.objectId);
    for (const Boundary& b : s.boundaries)
        if (!b.objectId.empty())
            ids.insert(b.objectId);
    for (const Parameter& p : s.parameters)
        if (p.kind == ParameterKind::Bound)
            if (const auto b = optimization::parseSceneBinding(p.binding))
                ids.insert(b->objectId);
    ids.erase("");
    return ids;
}

std::set<std::string> referencedMaterials(const FunctionalStructure& s, const Project& project)
{
    std::set<std::string> ids;
    for (const Region& r : s.regions)
        ids.insert(r.materialId);
    for (const std::string& o : referencedObjects(s))
        if (const SceneObject* object = project.scene.find(Id(o)); object && !object->state.materialId.isNull())
            ids.insert(object->state.materialId.str());
    ids.erase("");
    return ids;
}

namespace {

json objectDocument(const Project& project, const std::string& id)
{
    const SceneObject* o = project.scene.find(Id(id));
    if (!o)
        return {{"missing", id}};
    json d{{"type", o->type()}, {"material", o->state.materialId.str()}, {"visible", o->state.visible}};
    if (o->state.geometry) {
        d["kind"] = o->state.geometry->kindId();
        json parameters = json::object();
        for (const auto& [key, value] : o->state.geometry->parameters())
            parameters[key] = value;
        d["parameters"] = std::move(parameters);
    }
    if (o->state.component)
        d["component"] = o->state.component->toJson();
    const Mat4 m = project.scene.worldMatrix(*o);
    json placement = json::array();
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            placement.push_back(m(r, c));
    d["placement"] = std::move(placement);
    return d;
}

}  // namespace

DependencyRecord dependenciesOf(const FunctionalStructure& s, const Project& project, const json& discretization)
{
    DependencyRecord r;
    json all = json::object();
    for (const std::string& id : referencedObjects(s)) {
        const json d = objectDocument(project, id);
        r.objects[id] = optimization::contentKey(d);
        all[id] = d;
    }
    r.geometry = optimization::contentKey(all);
    json materials = json::object();
    for (const std::string& id : referencedMaterials(s, project)) {
        const Material* m = project.materials.find(Id(id));
        const json d = m ? resonant::toJson(*m) : json{{"missing", id}};
        r.materialKeys[id] = optimization::contentKey(d);
        materials[id] = d;
    }
    r.materials = optimization::contentKey(materials);
    r.definition = optimization::contentKey(s.toJson());
    r.discretization = optimization::contentKey(discretization);
    return r;
}

std::vector<StaleReason> staleness(const DependencyRecord& recorded, const DependencyRecord& current, const Project& project)
{
    std::vector<StaleReason> out;
    const auto nameOf = [&](const std::string& id) {
        const SceneObject* o = project.scene.find(Id(id));
        return o ? "'" + o->state.name + "'" : "'" + id + "' (no longer in the scene)";
    };
    if (recorded.geometry != current.geometry) {
        bool named = false;
        for (const auto& [id, key] : recorded.objects) {
            const auto it = current.objects.find(id);
            if (it == current.objects.end() || it->second != key) {
                out.push_back({"geometry", "the geometry, placement or setup of " + nameOf(id) + " changed"});
                named = true;
            }
        }
        for (const auto& [id, key] : current.objects)
            if (!recorded.objects.count(id)) {
                out.push_back({"geometry", "the structure now also refers to " + nameOf(id)});
                named = true;
            }
        if (!named)
            out.push_back({"geometry", "the referenced geometry changed"});
    }
    if (recorded.materials != current.materials) {
        bool named = false;
        for (const auto& [id, key] : recorded.materialKeys) {
            const auto it = current.materialKeys.find(id);
            if (it == current.materialKeys.end() || it->second != key) {
                const Material* m = project.materials.find(Id(id));
                out.push_back({"material", "the material " + (m ? "'" + m->name() + "'" : "'" + id + "'") + " changed"});
                named = true;
            }
        }
        if (!named)
            out.push_back({"material", "the set of referenced materials changed"});
    }
    if (recorded.definition != current.definition)
        out.push_back({"definition", "the structure's definition (parameters, analysis, objectives or constraints) changed"});
    if (recorded.discretization != current.discretization)
        out.push_back({"discretization", "the model or its settings changed"});
    return out;
}

json toJson(const std::vector<StaleReason>& reasons)
{
    json a = json::array();
    for (const StaleReason& r : reasons)
        a.push_back({{"what", r.what}, {"detail", r.detail}});
    return a;
}

SimulationGeometryCache& SimulationGeometryCache::shared()
{
    static SimulationGeometryCache cache;
    return cache;
}

std::shared_ptr<const void> SimulationGeometryCache::find(const std::string& key)
{
    std::lock_guard lock(mutex_);
    for (auto it = entries_.begin(); it != entries_.end(); ++it)
        if (it->first == key) {
            auto value = it->second;
            entries_.erase(it);
            entries_.emplace_back(key, value);
            ++hits_;
            return value;
        }
    ++misses_;
    return nullptr;
}

void SimulationGeometryCache::store(const std::string& key, std::shared_ptr<const void> value)
{
    std::lock_guard lock(mutex_);
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const auto& e) { return e.first == key; }), entries_.end());
    entries_.emplace_back(key, std::move(value));
    if (entries_.size() > kCapacity)
        entries_.erase(entries_.begin());
}

void SimulationGeometryCache::clear()
{
    std::lock_guard lock(mutex_);
    entries_.clear();
    hits_ = misses_ = 0;
}

size_t SimulationGeometryCache::hits() const
{
    std::lock_guard lock(mutex_);
    return hits_;
}

size_t SimulationGeometryCache::misses() const
{
    std::lock_guard lock(mutex_);
    return misses_;
}

}  // namespace resonant::functional
