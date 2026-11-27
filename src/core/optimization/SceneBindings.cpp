#include "optimization/SceneBindings.h"

#include "geometry/Geometry.h"
#include "materials/Material.h"
#include "optimization/Persistence.h"
#include "project/Project.h"
#include "scene/Scene.h"

#include <algorithm>
#include <cmath>

namespace resonant::optimization {

std::string objectBinding(const std::string& objectId, const std::string& what) { return "object/" + objectId + "/" + what; }

std::optional<SceneBinding> parseSceneBinding(std::string_view key)
{
    if (key.rfind("object/", 0) != 0)
        return std::nullopt;
    const std::string_view rest = key.substr(7);
    const size_t slash = rest.find('/');
    if (slash == std::string_view::npos)
        return std::nullopt;
    SceneBinding b;
    b.objectId = std::string(rest.substr(0, slash));
    const std::string_view what = rest.substr(slash + 1);
    const auto axis = [](std::string_view a) { return a == "x" || a == "y" || a == "z"; };
    if (what.rfind("position/", 0) == 0 && axis(what.substr(9))) {
        b.kind = SceneBinding::Kind::Position;
        b.field = std::string(what.substr(9));
        return b;
    }
    if (what.rfind("rotation/", 0) == 0 && axis(what.substr(9))) {
        b.kind = SceneBinding::Kind::Rotation;
        b.field = std::string(what.substr(9));
        return b;
    }
    if (what.rfind("geometry/", 0) == 0 && what.size() > 9) {
        b.kind = SceneBinding::Kind::Geometry;
        b.field = std::string(what.substr(9));
        return b;
    }
    if (what == "material") {
        b.kind = SceneBinding::Kind::Material;
        return b;
    }
    return std::nullopt;
}

void appendSceneVariables(const Project& project, const SceneObject& object, std::vector<DesignVariable>& out, bool geometry, bool position,
                          std::optional<PhysicsDomain> materialDomain)
{
    const std::string id = object.id().str();
    const std::string name = object.state.name;
    if (geometry && object.state.geometry) {
        const Geometry& g = *object.state.geometry;
        for (const ParameterDef& def : g.kind().parameters) {
            DesignVariable v;
            v.binding = objectBinding(id, "geometry/" + def.key);
            v.id = v.binding;
            v.label = name + ": " + def.label;
            v.unit = def.unit;
            v.group = name;
            const double value = g.parameter(def.key);
            v.initial = value;
            if (def.integer) {
                v.kind = VariableKind::Integer;
                v.role = VariableRole::Count;
                v.lower = std::max(def.minValue + (def.exclusiveMin ? 1.0 : 0.0), std::floor(0.25 * value));
                v.upper = std::min(def.maxValue, std::ceil(4.0 * value));
            } else {
                const std::string& k = def.key;
                const auto has = [&k](const char* s) {
                    std::string lower = k;
                    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    return lower.find(s) != std::string::npos;
                };
                v.role = has("thickness") ? VariableRole::Thickness
                         : has("radius")  ? VariableRole::Radius
                         : has("spacing") ? VariableRole::Spacing
                         : def.unit == "rad" ? VariableRole::Orientation
                                             : VariableRole::Dimension;
                double lo = value > 0.0 ? 0.25 * value : value < 0.0 ? 4.0 * value : -1.0;
                double hi = value > 0.0 ? 4.0 * value : value < 0.0 ? 0.25 * value : 1.0;
                const double minimum = def.exclusiveMin ? def.minValue + 1e-9 * std::max(1.0, std::abs(def.minValue)) : def.minValue;
                lo = std::max(lo, minimum);
                hi = std::min(hi, def.maxValue);
                if (!(lo < hi))
                    continue;
                v.lower = lo;
                v.upper = hi;
            }
            v.initial = std::clamp(value, v.lower, v.upper);
            out.push_back(std::move(v));
        }
    }
    if (position) {
        static const char* axes[] = {"x", "y", "z"};
        for (int a = 0; a < 3; ++a) {
            DesignVariable v;
            v.binding = objectBinding(id, std::string("position/") + axes[a]);
            v.id = v.binding;
            v.label = name + ": position " + axes[a];
            v.unit = "m";
            v.role = VariableRole::Position;
            v.group = name;
            v.initial = object.state.local.position[a];
            v.lower = v.initial - 0.1;
            v.upper = v.initial + 0.1;
            out.push_back(std::move(v));
        }
    }
    if (materialDomain) {
        DesignVariable v;
        v.binding = objectBinding(id, "material");
        v.id = v.binding;
        v.label = name + ": material";
        v.kind = VariableKind::Categorical;
        v.role = VariableRole::MaterialSelection;
        v.group = name;
        for (const Material& m : project.materials.materials())
            if (m.appliesTo(*materialDomain)) {
                v.choices.push_back(m.id().str());
                v.choiceLabels.push_back(m.name());
            }
        const auto it = std::find(v.choices.begin(), v.choices.end(), object.state.materialId.str());
        if (v.choices.size() >= 2 && it != v.choices.end()) {
            v.initial = static_cast<double>(it - v.choices.begin());
            v.upper = static_cast<double>(v.choices.size() - 1);
            out.push_back(std::move(v));
        }
    }
}

SnapshotAdapter::SnapshotAdapter(nlohmann::json snapshot, nlohmann::json configuration)
    : snapshot_(std::move(snapshot)), configuration_(std::move(configuration))
{
}

void SnapshotAdapter::finishBinding()
{
    key_ = contentKey({{"design", dependencyDocument(snapshot_)},
                       {"configuration", configuration_},
                       {"adapter", capabilities_.adapterId},
                       {"version", capabilities_.version}});
}

Result<bool> SnapshotAdapter::applyDomainBinding(Project&, std::string_view, double) const { return false; }

Status SnapshotAdapter::checkDesign(const Project&) const { return Status::success(); }

Status SnapshotAdapter::apply(const std::map<std::string, double, std::less<>>& bindings, Project& project) const
{
    // Geometry parameters per object, applied together.
    std::map<std::string, std::vector<std::pair<std::string, double>>> geometry;
    for (const auto& [key, value] : bindings) {
        if (key == "memoryBudgetBytes")
            continue;
        if (!std::isfinite(value))
            return Status::failure("'" + key + "' is not a finite value");
        const auto b = parseSceneBinding(key);
        if (!b) {
            auto handled = applyDomainBinding(project, key, value);
            if (!handled)
                return Status::failure(handled.message());
            if (!handled.value())
                return Status::failure("the adapter cannot change '" + key + "'");
            continue;
        }
        SceneObject* object = project.scene.find(Id(b->objectId));
        if (!object)
            return Status::failure("the design refers to an object that is not in the project (" + b->objectId + ")");
        if (b->kind == SceneBinding::Kind::Geometry) {
            geometry[b->objectId].emplace_back(b->field, value);
            continue;
        }
        ObjectState state = object->state;
        const int axis = b->field == "x" ? 0 : b->field == "y" ? 1 : 2;
        if (b->kind == SceneBinding::Kind::Position)
            state.local.position[axis] = value;
        else if (b->kind == SceneBinding::Kind::Rotation)
            state.local.rotation[axis] = value;
        else {
            const DesignVariable* v = capabilities_.variable(key);
            const long index = std::lround(value);
            if (!v || index < 0 || static_cast<size_t>(index) >= v->choices.size())
                return Status::failure("material choice " + std::to_string(index) + " of '" + object->state.name + "' does not exist");
            const Id material(v->choices[static_cast<size_t>(index)]);
            if (!project.materials.find(material))
                return Status::failure("the material '" + material.str() + "' is not in the library");
            state.materialId = material;
        }
        if (Status s = project.scene.setState(object->id(), std::move(state)); !s)
            return s;
    }
    for (auto& [objectId, parameters] : geometry) {
        SceneObject* object = project.scene.find(Id(objectId));
        if (!object->state.geometry)
            return Status::failure("'" + object->state.name + "' has no geometry");
        Geometry g = *object->state.geometry;
        // Several passes: a joint rule may reject one value until another is set.
        std::vector<std::pair<std::string, double>> pending = parameters;
        std::string lastError;
        for (size_t pass = 0; pass <= parameters.size() && !pending.empty(); ++pass) {
            std::vector<std::pair<std::string, double>> failed;
            for (const auto& [key, value] : pending)
                if (Status s = g.setParameter(key, value); !s) {
                    failed.emplace_back(key, value);
                    lastError = s.message();
                }
            if (failed.size() == pending.size())
                break;
            pending = std::move(failed);
        }
        if (!pending.empty())
            return Status::failure("invalid geometry of '" + object->state.name + "': " + lastError);
        if (Status s = g.validate(); !s)
            return Status::failure("invalid geometry of '" + object->state.name + "': " + s.message());
        ObjectState state = object->state;
        state.geometry = g;
        if (Status s = project.scene.setState(object->id(), std::move(state)); !s)
            return s;
    }
    return checkDesign(project);
}

Result<std::unique_ptr<Project>> SnapshotAdapter::build(const std::map<std::string, double, std::less<>>& bindings) const
{
    auto project = projectFromSnapshot(snapshot_);
    if (!project)
        return project;
    if (Status s = apply(bindings, *project.value()); !s)
        return Result<std::unique_ptr<Project>>::failure(s.message());
    return project;
}

}  // namespace resonant::optimization
