#include "acoustics/Components.h"
#include "acoustics/Focusing.h"
#include "acoustics/Settings.h"
#include "commands/SceneCommands.h"
#include "coupling/WorkflowActions.h"
#include "geometry/Geometry.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "scene/Scene.h"
#include "services/Services.h"

#include <algorithm>
#include <cmath>

namespace resonant::services {

namespace {
using CommandOutcome = Outcome<std::unique_ptr<Command>>;

ServiceError objectNotFound(const Id& id)
{
    return notFound("the project has no object '" + id.str() + "'", "scene-object", id.str());
}

Id parentIdOf(const Scene& scene, const SceneObject& o)
{
    const SceneObject* p = o.parent();
    return p && !scene.isRoot(*p) ? p->id() : Id();
}

Outcome<Unit> checkTransform(const Transform& t)
{
    if (!t.position.allFinite() || !t.rotation.allFinite() || !t.scale.allFinite())
        return invalid("transform values must be finite numbers", "local_transform");
    if ((t.scale.array() <= 0.0).any())
        return invalid("scale factors must be greater than zero", "local_transform.scale");
    return Unit{};
}

// The object a description builds, as one insert command (emitters, arrays).
CommandOutcome insertDescribed(Project& target, nlohmann::json description, std::optional<Transform> transform, const Id& parent)
{
    if (!parent.isNull()) {
        if (!target.scene.find(parent))
            return objectNotFound(parent);
        description["parent"] = parent.str();
    }
    Id parentId;
    auto object = coupling::makeSceneObject(target, description, &parentId);
    if (!object)
        return invalid(object.message());
    if (transform) {
        if (auto ok = checkTransform(*transform); !ok)
            return ok.error();
        object.value()->state.local = *transform;
    }
    std::unique_ptr<Command> command = std::make_unique<InsertObjectCommand>(target.scene, object.take(), parentId, -1, "Add");
    return command;
}

template <class T>
const T* componentAs(const SceneObject& o)
{
    return dynamic_cast<const T*>(o.state.component.get());
}
}  // namespace

// --- scene ------------------------------------------------------------------------------

ObjectView SceneService::view(const SceneObject& o) const
{
    const Scene& scene = s_.project().scene;
    ObjectView v;
    v.id = o.id();
    v.name = o.state.name;
    v.type = objectTypeOf(o);
    v.parent = parentIdOf(scene, o);
    for (const auto& child : o.children())
        v.children.push_back(child->id());
    v.revision = s_.objectRevision(o.id());
    v.local = o.state.local;
    v.worldPosition = transformPoint(scene.worldMatrix(o), Vec3::Zero());
    v.visible = o.state.visible;
    v.material = o.state.materialId;
    if (o.state.geometry) {
        v.geometryKind = o.state.geometry->kindId();
        for (const auto& [key, value] : o.state.geometry->parameters())
            v.parameters[key] = value;
    }
    v.component = o.state.component ? o.state.component->toJson() : nlohmann::json();
    return v;
}

Outcome<SceneSnapshotView> SceneService::snapshot(const CallContext& c)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    SceneSnapshotView v;
    v.projectId = s_.project().id.str();
    v.revision = s_.projectRevision();
    v.eventSequence = s_.events().lastSequence();  // built on the owner thread: no event can slip in between
    s_.project().scene.forEach([&](const SceneObject& o) { v.objects.push_back(view(o)); });
    return v;
}

Outcome<std::vector<ObjectView>> SceneService::list(const CallContext& c, const std::string& type, const std::string& parent)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    std::vector<ObjectView> out;
    const Scene& scene = s_.project().scene;
    scene.forEach([&](const SceneObject& o) {
        if (!type.empty() && objectTypeOf(o) != type)
            return;
        if (!parent.empty()) {
            const Id p = parentIdOf(scene, o);
            if (parent == "root" ? !p.isNull() : p.str() != parent)
                return;
        }
        out.push_back(view(o));
    });
    return out;
}

Outcome<ObjectView> SceneService::object(const CallContext& c, const Id& id)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    const SceneObject* o = s_.project().scene.find(id);
    if (!o)
        return objectNotFound(id);
    return view(*o);
}

Outcome<Mutation> SceneService::update(const CallContext& c, const Id& id, std::optional<std::uint64_t> expectedRevision, const Update& u)
{
    return s_.mutate(c, "SceneService.UpdateObject", {id}, expectedRevision, [id, u](Project& target) -> CommandOutcome {
        const SceneObject* o = target.scene.find(id);
        if (!o)
            return objectNotFound(id);
        ObjectState state = o->state;
        if (u.name) {
            std::string name = *u.name;
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            if (name.empty())
                return invalid("an object name cannot be empty", "name");
            state.name = name;
        }
        if (u.transform) {
            if (auto ok = checkTransform(*u.transform); !ok)
                return ok.error();
            state.local = *u.transform;
        }
        if (u.visible)
            state.visible = *u.visible;
        auto composite = std::make_unique<CompositeCommand>(u.description ? *u.description
                                                            : u.name      ? "Rename " + o->state.name
                                                                          : "Edit " + o->state.name);
        if (!(state == o->state))
            composite->add(std::make_unique<SetObjectStateCommand>(target.scene, id, state, composite->description()));
        if (u.parent) {
            const Id newParent = (u.parent->empty() || *u.parent == "root") ? Id() : Id(*u.parent);
            if (!newParent.isNull() && !target.scene.find(newParent))
                return objectNotFound(newParent);
            if (newParent != parentIdOf(target.scene, *o))
                composite->add(std::make_unique<MoveObjectCommand>(target.scene, id, newParent));
        }
        if (composite->empty())
            return std::unique_ptr<Command>();  // nothing to change
        std::unique_ptr<Command> command = std::move(composite);
        return command;
    });
}

Outcome<Mutation> SceneService::remove(const CallContext& c, const std::vector<Id>& ids, const std::map<Id, std::uint64_t>& expected)
{
    for (const auto& [id, revision] : expected)
        if (s_.project().scene.find(id) && s_.objectRevision(id) != revision) {
            ServiceError e = ServiceError::make(ErrorCode::Conflict, "the object changed since revision " + std::to_string(revision));
            e.resourceType = "scene-object";
            e.resourceId = id.str();
            e.currentRevision = s_.objectRevision(id);
            e.requestId = c.requestId;
            return e;
        }
    return s_.mutate(c, "SceneService.DeleteObjects", ids, std::nullopt, [ids](Project& target) -> CommandOutcome {
        if (ids.empty())
            return invalid("no object to delete", "object_ids");
        std::vector<Id> roots;
        for (const Id& id : ids) {
            const SceneObject* o = target.scene.find(id);
            if (!o)
                return objectNotFound(id);
            const bool covered = std::any_of(ids.begin(), ids.end(), [&](const Id& other) {
                const SceneObject* p = target.scene.find(other);
                return p && p != o && p->isAncestorOf(*o);
            });
            if (!covered && std::find(roots.begin(), roots.end(), id) == roots.end())
                roots.push_back(id);
        }
        auto composite = std::make_unique<CompositeCommand>(roots.size() == 1 ? "Delete " + target.scene.find(roots.front())->state.name
                                                                               : "Delete " + std::to_string(roots.size()) + " objects");
        for (const Id& id : roots)
            composite->add(std::make_unique<RemoveObjectCommand>(target.scene, id));
        std::unique_ptr<Command> command = std::move(composite);
        return command;
    });
}

Outcome<Mutation> SceneService::duplicate(const CallContext& c, const std::vector<Id>& ids)
{
    return s_.mutate(c, "SceneService.DuplicateObjects", ids, std::nullopt, [ids](Project& target) -> CommandOutcome {
        if (ids.empty())
            return invalid("no object to duplicate", "object_ids");
        auto composite = std::make_unique<CompositeCommand>(ids.size() == 1 ? "Duplicate " + (target.scene.find(ids.front()) ? target.scene.find(ids.front())->state.name : ids.front().str())
                                                                              : "Duplicate " + std::to_string(ids.size()) + " objects");
        for (const Id& id : ids) {
            const SceneObject* o = target.scene.find(id);
            if (!o)
                return objectNotFound(id);
            auto copy = o->cloneSubtree(true);
            copy->state.name = target.scene.uniqueName(o->state.name + " copy");
            composite->add(std::make_unique<InsertObjectCommand>(target.scene, std::move(copy), parentIdOf(target.scene, *o), o->indexInParent() + 1, "Duplicate"));
        }
        std::unique_ptr<Command> command = std::move(composite);
        return command;
    });
}

Outcome<Mutation> SceneService::createGroup(const CallContext& c, const std::string& name, const Id& parent, const std::vector<Id>& members)
{
    return s_.mutate(c, "SceneService.CreateGroup", members, std::nullopt, [name, parent, members](Project& target) -> CommandOutcome {
        if (!parent.isNull() && !target.scene.find(parent))
            return objectNotFound(parent);
        for (const Id& m : members)
            if (!target.scene.find(m))
                return objectNotFound(m);
        auto group = makeGroup(name.empty() ? target.scene.uniqueName("Group") : name);
        const Id groupId = group->id();
        auto composite = std::make_unique<CompositeCommand>("Group " + std::to_string(members.size()) + " object(s)");
        composite->add(std::make_unique<InsertObjectCommand>(target.scene, std::move(group), parent, -1, "Add"));
        for (const Id& m : members)
            composite->add(std::make_unique<MoveObjectCommand>(target.scene, m, groupId));
        std::unique_ptr<Command> command = std::move(composite);
        return command;
    });
}

Outcome<ValidationView> SceneService::validate(const CallContext& c, const Id& id)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    const SceneObject* o = s_.project().scene.find(id);
    if (!o)
        return objectNotFound(id);
    ValidationView v;
    if (o->state.geometry)
        if (Status st = o->state.geometry->validate(); !st)
            v.issues.push_back({3, "geometry", st.message(), id.str()});
    if (!o->state.materialId.isNull() && !s_.project().materials.find(o->state.materialId))
        v.issues.push_back({3, "material_id", "the assigned material is not in the library", id.str()});
    if (o->state.component) {
        auto again = ComponentRegistry::instance().create(o->state.component->typeId(), o->state.component->toJson());
        if (!again)
            v.issues.push_back({3, "component", again.message(), id.str()});
    }
    v.valid = v.issues.empty();
    return v;
}

// --- geometry ----------------------------------------------------------------------------

Outcome<std::vector<GeometryService::Kind>> GeometryService::kinds(const CallContext& c)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    std::vector<Kind> out;
    for (const GeometryKind* k : GeometryRegistry::instance().kinds()) {
        Kind kind{k->id, k->displayName, k->category, {}};
        for (const ParameterDef& p : k->parameters)
            kind.parameters.push_back({p.key, p.label, p.unit, p.defaultValue, p.minValue, p.maxValue, p.integer});
        out.push_back(std::move(kind));
    }
    return out;
}

namespace {
Outcome<Geometry> geometryWith(const std::string& kind, const std::map<std::string, double>& parameters, const Geometry* base = nullptr)
{
    std::optional<Geometry> made;
    if (base)
        made = *base;
    else {
        auto created = Geometry::create(kind);
        if (!created)
            return invalid(created.message(), "kind_id");
        made = created.take();
    }
    Geometry& g = *made;
    ServiceError error = ServiceError::make(ErrorCode::InvalidArgument, "invalid geometry parameters");
    for (const auto& [key, value] : parameters)
        if (Status st = g.setParameter(key, value); !st)
            error.violations.push_back({"parameters." + key, st.message()});
    if (error.violations.empty())
        if (Status st = g.validate(); !st)
            error.violations.push_back({"parameters", st.message()});
    if (!error.violations.empty()) {
        error.message = error.violations.front().description;
        return error;
    }
    return std::move(*made);
}
}  // namespace

Outcome<Mutation> GeometryService::createPrimitive(const CallContext& c, const Create& create)
{
    return s_.mutate(c, "GeometryService.CreatePrimitive", {}, std::nullopt, [create](Project& target) -> CommandOutcome {
        auto g = geometryWith(create.kind, create.parameters);
        if (!g)
            return g.error();
        if (!create.parent.isNull() && !target.scene.find(create.parent))
            return objectNotFound(create.parent);
        if (!create.material.isNull() && !target.materials.find(create.material))
            return notFound("the project has no material '" + create.material.str() + "'", "material", create.material.str());
        const std::string name = create.name.empty() ? target.scene.uniqueName(g.value().kind().displayName) : create.name;
        auto object = makeGeometryObject(name, g.take());
        if (create.transform) {
            if (auto ok = checkTransform(*create.transform); !ok)
                return ok.error();
            object->state.local = *create.transform;
        }
        object->state.materialId = create.material;
        std::unique_ptr<Command> command = std::make_unique<InsertObjectCommand>(target.scene, std::move(object), create.parent, -1, "Add");
        return command;
    });
}

Outcome<Mutation> GeometryService::updateParameters(const CallContext& c, const Id& id, std::optional<std::uint64_t> expectedRevision,
                                                    const std::map<std::string, double>& parameters)
{
    return s_.mutate(c, "GeometryService.UpdateGeometryParameters", {id}, expectedRevision, [id, parameters](Project& target) -> CommandOutcome {
        const SceneObject* o = target.scene.find(id);
        if (!o)
            return objectNotFound(id);
        if (!o->state.geometry)
            return invalid("'" + o->state.name + "' has no geometry", "object_id");
        auto g = geometryWith({}, parameters, &*o->state.geometry);
        if (!g)
            return g.error();
        ObjectState state = o->state;
        state.geometry = g.take();
        if (state == o->state)
            return std::unique_ptr<Command>();
        std::unique_ptr<Command> command = std::make_unique<SetObjectStateCommand>(target.scene, id, std::move(state), "Edit " + o->state.name + " geometry");
        return command;
    });
}

Outcome<ValidationView> GeometryService::validate(const CallContext& c, const std::string& kind, const std::map<std::string, double>& parameters)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    ValidationView v;
    auto g = geometryWith(kind, parameters);
    if (!g) {
        std::vector<FieldViolation> violations = g.error().violations;
        if (violations.empty())
            violations.push_back({std::string(), g.error().message});
        for (const FieldViolation& f : violations)
            v.issues.push_back({3, f.field, f.description, std::string()});
    }
    v.valid = v.issues.empty();
    return v;
}

// --- materials ---------------------------------------------------------------------------------

Outcome<std::vector<nlohmann::json>> MaterialService::list(const CallContext& c, const std::string& domain)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    std::vector<nlohmann::json> out;
    for (const Material& m : s_.project().materials.materials()) {
        if (!domain.empty()) {
            const auto domains = m.domains();
            if (std::none_of(domains.begin(), domains.end(), [&](PhysicsDomain d) { return toString(d) == domain; }))
                continue;
        }
        out.push_back(toJson(m));
    }
    return out;
}

Outcome<nlohmann::json> MaterialService::get(const CallContext& c, const Id& id)
{
    auto a = s_.authorize(c, Permission::Read);
    if (!a)
        return a.error();
    const Material* m = s_.project().materials.find(id);
    if (!m)
        return notFound("the project has no material '" + id.str() + "'", "material", id.str());
    return toJson(*m);
}

Outcome<Mutation> MaterialService::create(const CallContext& c, const nlohmann::json& description)
{
    nlohmann::json j = description;
    if (!j.contains("id"))
        j["id"] = Id::generate().str();
    auto material = materialFromJson(j);
    if (!material)
        return invalid(material.message(), "material_json");
    const Material m = material.take();
    return s_.mutate(c, "MaterialService.CreateMaterial", {m.id()}, std::nullopt, [m](Project& target) -> CommandOutcome {
        if (target.materials.find(m.id()))
            return ServiceError::make(ErrorCode::AlreadyExists, "a material with the id '" + m.id().str() + "' exists");
        MaterialLibrary library = target.materials;
        if (Status st = library.add(m); !st)
            return invalid(st.message(), "material_json");
        std::unique_ptr<Command> command = std::make_unique<SetMaterialLibraryCommand>(target.materials, std::move(library), "Add material " + m.name());
        return command;
    });
}

namespace {
// A new library, with the references to removed materials cleared in the same step.
std::unique_ptr<Command> libraryCommand(Project& target, MaterialLibrary library, const std::string& description)
{
    auto composite = std::make_unique<CompositeCommand>(description);
    target.scene.forEach([&](const SceneObject& o) {
        if (!o.state.materialId.isNull() && !library.find(o.state.materialId)) {
            ObjectState state = o.state;
            state.materialId = Id();
            composite->add(std::make_unique<SetObjectStateCommand>(target.scene, o.id(), std::move(state), "Remove material"));
        }
    });
    composite->add(std::make_unique<SetMaterialLibraryCommand>(target.materials, std::move(library), description));
    return composite;
}
}  // namespace

Outcome<Mutation> MaterialService::remove(const CallContext& c, const Id& id)
{
    return s_.mutate(c, "MaterialService.DeleteMaterial", {id}, std::nullopt, [id](Project& target) -> CommandOutcome {
        const Material* m = target.materials.find(id);
        if (!m)
            return notFound("the project has no material '" + id.str() + "'", "material", id.str());
        MaterialLibrary library = target.materials;
        library.remove(id);
        return libraryCommand(target, std::move(library), "Delete material " + m->name());
    });
}

Outcome<Mutation> MaterialService::replaceLibrary(const CallContext& c, const MaterialLibrary& library, const std::string& description)
{
    return s_.mutate(c, "MaterialService.ReplaceLibrary", {}, std::nullopt, [library, description](Project& target) -> CommandOutcome {
        if (library == target.materials)
            return std::unique_ptr<Command>();
        return libraryCommand(target, library, description);
    });
}

Outcome<Mutation> MaterialService::assign(const CallContext& c, const Id& material, const std::vector<Id>& objects)
{
    return s_.mutate(c, material.isNull() ? "MaterialService.RemoveMaterialAssignment" : "MaterialService.AssignMaterial", objects, std::nullopt,
                     [material, objects](Project& target) -> CommandOutcome {
                         if (objects.empty())
                             return invalid("no object given", "object_ids");
                         if (!material.isNull() && !target.materials.find(material))
                             return notFound("the project has no material '" + material.str() + "'", "material", material.str());
                         auto composite = std::make_unique<CompositeCommand>(material.isNull() ? "Remove material assignment"
                                                                                               : "Assign material " + target.materials.find(material)->name());
                         for (const Id& id : objects) {
                             const SceneObject* o = target.scene.find(id);
                             if (!o)
                                 return objectNotFound(id);
                             if (o->state.materialId == material)
                                 continue;
                             ObjectState state = o->state;
                             state.materialId = material;
                             composite->add(std::make_unique<SetObjectStateCommand>(target.scene, id, std::move(state), composite->description()));
                         }
                         if (composite->empty())
                             return std::unique_ptr<Command>();
                         std::unique_ptr<Command> command = std::move(composite);
                         return command;
                     });
}

Outcome<Mutation> MaterialService::unassign(const CallContext& c, const std::vector<Id>& objects)
{
    return assign(c, Id(), objects);
}

// --- emitters and arrays ---------------------------------------------------------------------------

Outcome<Mutation> EmitterService::create(const CallContext& c, const std::string& name, std::optional<Transform> transform, const Id& parent,
                                         const nlohmann::json& settings)
{
    return s_.mutate(c, "EmitterService.CreateEmitter", {}, std::nullopt, [=](Project& target) -> CommandOutcome {
        nlohmann::json d = {{"type", "emitter"}, {"name", name.empty() ? target.scene.uniqueName("Emitter") : name}};
        if (!settings.is_null() && !settings.empty())
            d["component"] = settings;
        return insertDescribed(target, d, transform, parent);
    });
}

Outcome<Mutation> SimulationService::createFieldRegion(const CallContext& c, const std::string& name, std::optional<Transform> transform,
                                                      const Id& parent, const nlohmann::json& settings)
{
    return s_.mutate(c, "SimulationService.CreateFieldRegion", {}, std::nullopt, [=](Project& target) -> CommandOutcome {
        nlohmann::json d = {{"type", "region"}, {"name", name.empty() ? target.scene.uniqueName("Field region") : name}};
        if (!settings.is_null() && !settings.empty())
            d["component"] = settings;
        return insertDescribed(target, d, transform, parent);
    });
}

Outcome<ObjectView> EmitterService::get(const CallContext& c, const Id& id)
{
    auto v = s_.services().scene.object(c, id);
    if (v && v.value().type != acoustics::object_types::Emitter)
        return invalid("'" + v.value().name + "' is not an emitter", "object_id");
    return v;
}

Outcome<std::vector<ObjectView>> EmitterService::list(const CallContext& c)
{
    return s_.services().scene.list(c, acoustics::object_types::Emitter, {});
}

Outcome<Mutation> EmitterService::update(const CallContext& c, const Id& id, std::optional<std::uint64_t> expectedRevision, const nlohmann::json& patch,
                                         std::optional<Transform> transform)
{
    return s_.mutate(c, "EmitterService.UpdateEmitter", {id}, expectedRevision, [=](Project& target) -> CommandOutcome {
        const SceneObject* o = target.scene.find(id);
        if (!o)
            return objectNotFound(id);
        if (objectTypeOf(*o) != acoustics::object_types::Emitter)
            return invalid("'" + o->state.name + "' is not an emitter", "object_id");
        ObjectState state = o->state;
        if (!patch.is_null() && !patch.empty()) {
            auto changed = coupling::changedObjectState(target, *o, {{"component", patch}});
            if (!changed)
                return invalid(changed.message(), "source");
            state = changed.take();
        }
        if (transform) {
            if (auto ok = checkTransform(*transform); !ok)
                return ok.error();
            state.local = *transform;
        }
        if (state == o->state)
            return std::unique_ptr<Command>();
        std::unique_ptr<Command> command = std::make_unique<SetObjectStateCommand>(target.scene, id, std::move(state), "Edit emitter " + o->state.name);
        return command;
    });
}

Outcome<Mutation> ArrayService::create(const CallContext& c, const std::string& name, std::optional<Transform> transform, const Id& parent,
                                       const nlohmann::json& settings)
{
    return s_.mutate(c, "ArrayService.CreateArray", {}, std::nullopt, [=](Project& target) -> CommandOutcome {
        nlohmann::json d = {{"type", "array"}, {"name", name.empty() ? target.scene.uniqueName("Array") : name}};
        if (!settings.is_null() && !settings.empty())
            d["component"] = settings;
        return insertDescribed(target, d, transform, parent);
    });
}

Outcome<ObjectView> ArrayService::get(const CallContext& c, const Id& id)
{
    auto v = s_.services().scene.object(c, id);
    if (v && v.value().type != acoustics::object_types::Array)
        return invalid("'" + v.value().name + "' is not an acoustic array", "object_id");
    return v;
}

std::vector<std::pair<Vec3, nlohmann::json>> ArrayService::elements(const SceneObject& array) const
{
    std::vector<std::pair<Vec3, nlohmann::json>> out;
    if (const auto* a = componentAs<acoustics::ArrayComponent>(array))
        for (const acoustics::ArrayElement& e : acoustics::generateElements(a->settings))
            out.emplace_back(e.localPosition, acoustics::EmitterComponent(e.settings).toJson());
    return out;
}

Outcome<Mutation> ArrayService::setExcitation(const CallContext& c, const Id& id, std::optional<std::uint64_t> expectedRevision,
                                              const std::vector<Element>& elements, bool clearOthers)
{
    return s_.mutate(c, "ArrayService.SetArrayExcitation", {id}, expectedRevision, [=](Project& target) -> CommandOutcome {
        const SceneObject* o = target.scene.find(id);
        if (!o)
            return objectNotFound(id);
        const auto* array = componentAs<acoustics::ArrayComponent>(*o);
        if (!array)
            return invalid("'" + o->state.name + "' is not an acoustic array", "object_id");
        auto copy = std::make_shared<acoustics::ArrayComponent>(*array);
        const int count = copy->settings.elementCount();
        if (clearOthers)
            copy->settings.overrides.clear();
        ServiceError error = ServiceError::make(ErrorCode::InvalidArgument, "invalid element excitation");
        for (const Element& e : elements) {
            if (e.index < 0 || e.index >= count) {
                error.violations.push_back({"elements.index", "element " + std::to_string(e.index) + " does not exist (the array has " + std::to_string(count) + ")"});
                continue;
            }
            acoustics::ElementOverride& ov = copy->settings.overrides[e.index];
            if (e.phaseRad)
                ov.phaseRad = *e.phaseRad;
            if (e.amplitudePa) {
                if (!(*e.amplitudePa >= 0.0) || !std::isfinite(*e.amplitudePa))
                    error.violations.push_back({"elements.amplitude_peak_pa_at_1m", "an amplitude must be a finite number >= 0"});
                ov.amplitudePa = *e.amplitudePa;
            }
            if (e.delaySec)
                ov.delaySec = *e.delaySec;
            if (e.active)
                ov.active = *e.active;
        }
        if (!error.violations.empty()) {
            error.message = error.violations.front().description;
            return error;
        }
        if (Status st = copy->settings.validate(); !st)
            return invalid(st.message(), "elements");
        ObjectState state = o->state;
        state.component = copy;
        if (state == o->state)
            return std::unique_ptr<Command>();
        std::unique_ptr<Command> command = std::make_unique<SetObjectStateCommand>(target.scene, id, std::move(state), "Set excitation of " + o->state.name);
        return command;
    });
}

Outcome<Mutation> ArrayService::focus(const CallContext& c, const Id& id, std::optional<std::uint64_t> expectedRevision, const Id& targetId,
                                      std::optional<Vec3> point, bool timeDelay)
{
    return s_.mutate(c, "ArrayService.FocusArray", {id}, expectedRevision, [=](Project& target) -> CommandOutcome {
        const SceneObject* o = target.scene.find(id);
        if (!o)
            return objectNotFound(id);
        const auto* array = componentAs<acoustics::ArrayComponent>(*o);
        if (!array)
            return invalid("'" + o->state.name + "' is not an acoustic array", "object_id");
        Vec3 focus;
        if (point)
            focus = *point;
        else {
            const SceneObject* t = target.scene.find(targetId);
            if (!t)
                return objectNotFound(targetId);
            focus = transformPoint(target.scene.worldMatrix(*t), Vec3::Zero());
        }
        if (!focus.allFinite())
            return invalid("the focus point must be finite", "point_world_meters");
        const acoustics::AcousticSettings s = acoustics::settingsOf(target);
        const auto control = timeDelay ? acoustics::FocusControl::TimeDelay : acoustics::FocusControl::Phase;
        const auto solution =
            acoustics::computeFocus(acoustics::generateElements(array->settings), target.scene.worldMatrix(*o), focus, s.medium, s.frequencyHz, control);
        auto copy = std::make_shared<acoustics::ArrayComponent>(*array);
        copy->settings = acoustics::applyFocus(array->settings, solution, acoustics::Taper::Uniform);
        ObjectState state = o->state;
        state.component = copy;
        std::unique_ptr<Command> command = std::make_unique<SetObjectStateCommand>(target.scene, id, std::move(state), "Focus " + o->state.name);
        return command;
    });
}

}  // namespace resonant::services
