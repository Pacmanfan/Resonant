#include "coupling/WorkflowActions.h"

#include "acoustics/Components.h"
#include "acoustics/FieldEvaluator.h"
#include "acoustics/FieldGrid.h"
#include "acoustics/Focusing.h"
#include "acoustics/Propagation.h"
#include "acoustics/Settings.h"
#include "electromagnetics/ArrayFactor.h"
#include "electromagnetics/EmComponents.h"
#include "electromagnetics/EmStudy.h"
#include "foundation/Transform.h"
#include "geometry/Geometry.h"
#include "manipulators/Manipulator.h"
#include "manipulators/ReflectorModel.h"
#include "mechanics/MechanicalComponents.h"
#include "mechanics/Study.h"
#include "optimization/Engine.h"
#include "optimization/Persistence.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "scene/Component.h"
#include "scene/Scene.h"

#include <cmath>
#include <fstream>
#include <numbers>
#include <sstream>

namespace resonant::coupling {

using nlohmann::json;
using automation::ActionContext;
using automation::ActionInfo;

namespace {

constexpr double kPi = std::numbers::pi;

struct TypeInfo {
    const char* name;
    const char* objectType;
    bool needsGeometry;
    std::function<json()> defaults;  // the default component data, or null
};

const std::vector<TypeInfo>& types()
{
    static const std::vector<TypeInfo> t{
        {"geometry", resonant::object_types::Geometry, true, nullptr},
        {"emitter", acoustics::object_types::Emitter, false, [] { return acoustics::EmitterComponent{}.toJson(); }},
        {"array", acoustics::object_types::Array, false, [] { return acoustics::ArrayComponent{}.toJson(); }},
        {"target", acoustics::object_types::Target, false, [] { return acoustics::TargetComponent{}.toJson(); }},
        {"probe", acoustics::object_types::Probe, false, [] { return acoustics::ProbeComponent{}.toJson(); }},
        {"region", acoustics::object_types::Region, false, [] { return acoustics::RegionComponent{}.toJson(); }},
        {"manipulator", "", true, nullptr},
        {"structure", resonant::object_types::Geometry, true, [] { return mechanics::StructureComponent{}.toJson(); }},
        {"actuator", mechanics::object_types::Actuator, false, [] { return mechanics::ActuatorComponent{}.toJson(); }},
        {"sensor", mechanics::object_types::Sensor, false, [] { return mechanics::SensorComponent{}.toJson(); }},
        {"vibration-target", mechanics::object_types::VibrationTarget, false, [] { return mechanics::VibrationTargetComponent{}.toJson(); }},
        {"antenna", em::object_types::Antenna, true, [] { return em::AntennaComponent{}.toJson(); }},
        {"em-array", em::object_types::Array, false, [] { return em::EmArrayComponent{}.toJson(); }},
        {"em-source", em::object_types::Source, false, [] { return em::SourceComponent{}.toJson(); }},
    };
    return t;
}

std::string componentTypeOf(const std::string& name)
{
    static const std::map<std::string, std::string> ids{
        {"emitter", acoustics::object_types::Emitter},       {"array", acoustics::object_types::Array},
        {"target", acoustics::object_types::Target},         {"probe", acoustics::object_types::Probe},
        {"region", acoustics::object_types::Region},         {"structure", mechanics::kStructureComponentType},
        {"actuator", mechanics::object_types::Actuator},     {"sensor", mechanics::object_types::Sensor},
        {"vibration-target", mechanics::object_types::VibrationTarget},
        {"antenna", em::object_types::Antenna},              {"em-array", em::object_types::Array},
        {"em-source", em::object_types::Source}};
    const auto it = ids.find(name);
    return it == ids.end() ? std::string() : it->second;
}

Result<Vec3> vec3(const json& v, const std::string& what)
{
    if (!v.is_array() || v.size() != 3 || !v[0].is_number() || !v[1].is_number() || !v[2].is_number())
        return Result<Vec3>::failure(what + " must be [x, y, z] numbers");
    return Vec3(v[0].get<double>(), v[1].get<double>(), v[2].get<double>());
}

Status setGeometryParameters(Geometry& g, const json& parameters, const std::string& objectName)
{
    if (!parameters.is_object())
        return Status::failure("geometry parameters must be an object");
    std::vector<std::pair<std::string, double>> pending;
    for (const auto& [k, v] : parameters.items()) {
        if (!v.is_number())
            return Status::failure("geometry parameter '" + k + "' must be a number");
        pending.emplace_back(k, v.get<double>());
    }
    std::string error;
    for (size_t pass = 0; pass <= pending.size() && !pending.empty(); ++pass) {
        std::vector<std::pair<std::string, double>> failed;
        for (const auto& [k, v] : pending)
            if (Status s = g.setParameter(k, v); !s) {
                failed.emplace_back(k, v);
                error = s.message();
            }
        if (failed.size() == pending.size())
            break;
        pending = std::move(failed);
    }
    if (!pending.empty())
        return Status::failure("'" + objectName + "': " + error);
    return g.validate();
}

Status writeText(const std::filesystem::path& path, const std::string& text)
{
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out)
        return Status::failure("cannot write " + path.string());
    out << text;
    return out ? Status::success() : Status::failure("writing " + path.string() + " failed");
}

std::filesystem::path resolvePath(const ActionContext& c, const std::string& path)
{
    const std::filesystem::path p(path);
    return p.is_absolute() || c.outputDirectory.empty() ? p : c.outputDirectory / p;
}

Result<const SceneObject*> requireObject(const Project& project, const json& params, const char* key)
{
    const std::string ref = params.value(key, std::string());
    const SceneObject* o = findSceneObject(project, ref);
    if (!o)
        return Result<const SceneObject*>::failure("no object '" + ref + "' (parameter '" + key + "')");
    return o;
}

template <typename T>
const T* as(const SceneObject& o)
{
    return dynamic_cast<const T*>(o.state.component.get());
}

const SceneObject* firstWith(const Scene& scene, const std::function<bool(const SceneObject&)>& test)
{
    const SceneObject* found = nullptr;
    scene.forEach([&](const SceneObject& o) {
        if (!found && test(o))
            found = &o;
    });
    return found;
}

std::string csvNumber(double v)
{
    std::ostringstream s;
    s.precision(10);
    s << v;
    return s.str();
}

// "set" parameter of analysis actions: changes applied before the analysis
// (design variants) to set.object, or to the action's own object.
Status applySet(Project& project, const std::string& reference, const json& params)
{
    if (!params.contains("set"))
        return Status::success();
    json changes = params["set"];
    std::string target = reference;
    if (changes.is_object() && changes.contains("object")) {
        target = changes["object"].get<std::string>();
        changes.erase("object");
    }
    return changeSceneObject(project, target, changes);
}

json vectorJson(const Vec3& v) { return json::array({v.x(), v.y(), v.z()}); }

std::string htmlEscape(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if (c == '<')
            out += "&lt;";
        else if (c == '>')
            out += "&gt;";
        else if (c == '&')
            out += "&amp;";
        else
            out += c;
    }
    return out;
}

void htmlValue(std::ostringstream& h, const json& v)
{
    if (v.is_object()) {
        h << "<table>";
        for (const auto& [k, x] : v.items()) {
            h << "<tr><th>" << htmlEscape(k) << "</th><td>";
            htmlValue(h, x);
            h << "</td></tr>";
        }
        h << "</table>";
    } else if (v.is_array()) {
        if (v.size() > 40) {
            h << v.size() << " values";
            return;
        }
        h << "<ol>";
        for (const auto& x : v) {
            h << "<li>";
            htmlValue(h, x);
            h << "</li>";
        }
        h << "</ol>";
    } else if (v.is_string()) {
        h << htmlEscape(v.get<std::string>());
    } else {
        h << htmlEscape(v.dump());
    }
}

}  // namespace

std::vector<std::string> sceneObjectTypes()
{
    std::vector<std::string> out;
    for (const TypeInfo& t : types())
        out.push_back(t.name);
    return out;
}

const SceneObject* findSceneObject(const Project& project, const std::string& reference)
{
    if (reference.empty())
        return nullptr;
    if (const SceneObject* o = project.scene.find(Id(reference)))
        return o;
    return firstWith(project.scene, [&](const SceneObject& o) { return o.state.name == reference; });
}

Result<std::string> addSceneObject(Project& project, const json& d)
{
    using R = Result<std::string>;
    if (!d.is_object())
        return R::failure("an object description must be a JSON object");
    const std::string type = d.value("type", std::string("geometry"));
    const auto it = std::find_if(types().begin(), types().end(), [&](const TypeInfo& t) { return type == t.name; });
    if (it == types().end())
        return R::failure("unknown object type '" + type + "'");
    const std::string name = d.value("name", std::string(it->name));
    std::unique_ptr<SceneObject> object;
    if (type == "manipulator") {
        auto made = manipulators::makeManipulator(name, d.value("kind", std::string("reflector.parabolic")));
        if (!made)
            return R::failure(made.message());
        object = made.take();
    } else {
        object = std::make_unique<SceneObject>(d.contains("id") ? Id(d["id"].get<std::string>()) : Id::generate(), it->objectType);
        object->state.name = name;
        if (it->needsGeometry) {
            const std::string kind = d.value("kind", std::string(type == "antenna" ? "antenna.dipole" : "box"));
            auto g = Geometry::create(kind);
            if (!g)
                return R::failure(g.message());
            object->state.geometry = g.take();
        }
    }
    if (type == "manipulator" && d.contains("id")) {
        auto fixed = std::make_unique<SceneObject>(Id(d["id"].get<std::string>()), object->type());
        fixed->state = object->state;
        object = std::move(fixed);
    }
    if (d.contains("parameters")) {
        if (!object->state.geometry)
            return R::failure("a " + type + " has no geometry parameters");
        Geometry g = *object->state.geometry;
        if (Status s = setGeometryParameters(g, d["parameters"], name); !s)
            return R::failure(s.message());
        object->state.geometry = g;
    }
    if (it->defaults) {
        json data = it->defaults();
        if (d.contains("component"))
            data.merge_patch(d["component"]);
        auto c = ComponentRegistry::instance().create(componentTypeOf(type), data);
        if (!c)
            return R::failure("'" + name + "': " + c.message());
        object->state.component = c.take();
    } else if (d.contains("component") && type != "manipulator") {
        return R::failure("a " + type + " object has no component");
    }
    if (d.contains("position")) {
        auto p = vec3(d["position"], "position");
        if (!p)
            return R::failure(p.message());
        object->state.local.position = p.value();
    }
    if (d.contains("rotation")) {
        auto r = vec3(d["rotation"], "rotation");
        if (!r)
            return R::failure(r.message());
        object->state.local.rotation = r.value();
    }
    if (d.contains("material")) {
        const std::string m = d["material"].get<std::string>();
        const Material* material = project.materials.find(Id(m));
        if (!material)
            material = project.materials.findByName(m);
        if (!material)
            return R::failure("no material '" + m + "'");
        object->state.materialId = material->id();
    }
    Id parent;
    if (d.contains("parent")) {
        const SceneObject* p = findSceneObject(project, d["parent"].get<std::string>());
        if (!p)
            return R::failure("no parent object '" + d["parent"].get<std::string>() + "'");
        parent = p->id();
    }
    const std::string id = object->id().str();
    auto inserted = project.scene.insert(std::move(object), parent);
    if (!inserted)
        return R::failure(inserted.message());
    return id;
}

Status changeSceneObject(Project& project, const std::string& reference, const json& changes)
{
    const SceneObject* o = findSceneObject(project, reference);
    if (!o)
        return Status::failure("no object '" + reference + "'");
    if (!changes.is_object())
        return Status::failure("changes must be a JSON object");
    ObjectState state = o->state;
    if (changes.contains("name"))
        state.name = changes["name"].get<std::string>();
    if (changes.contains("visible"))
        state.visible = changes["visible"].get<bool>();
    if (changes.contains("position")) {
        auto p = vec3(changes["position"], "position");
        if (!p)
            return Status::failure(p.message());
        state.local.position = p.value();
    }
    if (changes.contains("rotation")) {
        auto r = vec3(changes["rotation"], "rotation");
        if (!r)
            return Status::failure(r.message());
        state.local.rotation = r.value();
    }
    if (changes.contains("parameters")) {
        if (!state.geometry)
            return Status::failure("'" + state.name + "' has no geometry");
        Geometry g = *state.geometry;
        if (Status s = setGeometryParameters(g, changes["parameters"], state.name); !s)
            return s;
        state.geometry = g;
    }
    if (changes.contains("component")) {
        if (!state.component)
            return Status::failure("'" + state.name + "' has no component");
        json data = state.component->toJson();
        data.merge_patch(changes["component"]);
        auto c = ComponentRegistry::instance().create(state.component->typeId(), data);
        if (!c)
            return Status::failure("'" + state.name + "': " + c.message());
        state.component = c.take();
    }
    if (changes.contains("material")) {
        if (changes["material"].is_null())
            state.materialId = Id();
        else {
            const std::string m = changes["material"].get<std::string>();
            const Material* material = project.materials.find(Id(m));
            if (!material)
                material = project.materials.findByName(m);
            if (!material)
                return Status::failure("no material '" + m + "'");
            state.materialId = material->id();
        }
    }
    return project.scene.setState(o->id(), std::move(state));
}

void registerWorkflowActions(automation::ActionRegistry& registry)
{
    // --- project and scene ---
    registry.add({"project.save", "project", "Save the project file", {{"path", "file path (relative to the output directory)"}}, {"path"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     const auto path = resolvePath(c, p.value("path", std::string("workflow.resonant")));
                     if (Status s = saveProjectFile(c.project, path); !s)
                         return Result<json>::failure(s.message());
                     return json{{"path", path.string()}};
                 });
    registry.add({"scene.add", "scene", "Add an object (geometry, emitter, array, target, region, manipulator, structure, actuator, sensor, "
                                        "vibration-target, antenna, em-array, em-source)",
                  {{"type", "object type"}, {"id", "optional stable id"}, {"name", "name"}, {"kind", "geometry kind"},
                   {"parameters", "geometry parameters"}, {"component", "component data merged into the defaults"},
                   {"position", "[x, y, z] m"}, {"rotation", "[x, y, z] rad"}, {"material", "material id or name"}},
                  {"id"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     auto id = addSceneObject(c.project, p);
                     if (!id)
                         return Result<json>::failure(id.message());
                     return json{{"id", id.value()}};
                 });
    registry.add({"scene.set", "scene", "Change an object", {{"object", "id or name"}, {"name", ""}, {"position", ""}, {"rotation", ""},
                                                             {"parameters", "geometry parameters"}, {"component", "merge patch"}, {"material", ""}},
                  {"id"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     const std::string ref = p.value("object", std::string());
                     json changes = p;
                     changes.erase("object");
                     if (Status s = changeSceneObject(c.project, ref, changes); !s)
                         return Result<json>::failure(s.message());
                     return json{{"id", findSceneObject(c.project, ref)->id().str()}};
                 });
    registry.add({"scene.remove", "scene", "Remove an object and its children", {{"object", "id or name"}}, {}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     auto o = requireObject(c.project, p, "object");
                     if (!o)
                         return Result<json>::failure(o.message());
                     const Id id = o.value()->id();
                     c.project.scene.detach(id);
                     return json{{"removed", id.str()}};
                 });
    registry.add({"settings.set", "project", "Change project settings of a domain (acoustics, mechanics, electromagnetics, fdtd)",
                  {{"key", "settings key"}, {"values", "merge patch"}}, {}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     const std::string key = p.value("key", std::string());
                     json current;
                     if (key == "acoustics")
                         current = acoustics::toJson(acoustics::settingsOf(c.project));
                     else if (key == "mechanics")
                         current = mechanics::toJson(mechanics::mechanicsSettingsOf(c.project));
                     else if (key == "electromagnetics")
                         current = em::toJson(em::emSettingsOf(c.project));
                     else if (key == "fdtd")
                         current = c.project.extensions.value("fdtd", json::object());
                     else
                         return Result<json>::failure("unknown settings key '" + key + "'");
                     current.merge_patch(p.value("values", json::object()));
                     Status valid = Status::success();
                     if (key == "acoustics") {
                         auto s = acoustics::settingsFromJson(current);
                         valid = s ? Status::success() : Status::failure(s.message());
                         if (s)
                             acoustics::storeSettings(c.project, s.value());
                     } else if (key == "mechanics") {
                         auto s = mechanics::mechanicsSettingsFromJson(current);
                         valid = s ? Status::success() : Status::failure(s.message());
                         if (s)
                             mechanics::storeMechanicsSettings(c.project, s.value());
                     } else if (key == "electromagnetics") {
                         auto s = em::emSettingsFromJson(current);
                         valid = s ? Status::success() : Status::failure(s.message());
                         if (s)
                             em::storeEmSettings(c.project, s.value());
                     } else {
                         c.project.extensions[key] = current;
                     }
                     if (!valid)
                         return Result<json>::failure("settings '" + key + "': " + valid.message());
                     return json{{"key", key}};
                 });

    // --- acoustics ---
    registry.add({"acoustics.field", "acoustic", "Calculate the acoustic field on a region with the project's field model",
                  {{"region", "region object"}, {"csv", "optional CSV file of the samples"}, {"set", "optional changes before the calculation"}},
                  {"maxPressure", "maxPoint", "meanPressure", "frequencyHz", "samples", "model"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     using R = Result<json>;
                     auto ro = requireObject(c.project, p, "region");
                     if (!ro)
                         return R::failure(ro.message());
                     if (Status st = applySet(c.project, ro.value()->id().str(), p); !st)
                         return R::failure(st.message());
                     ro = requireObject(c.project, p, "region");
                     const auto* region = as<acoustics::RegionComponent>(*ro.value());
                     if (!region)
                         return R::failure("'" + ro.value()->state.name + "' is not a field region");
                     const acoustics::AcousticSettings s = acoustics::settingsOf(c.project);
                     std::vector<acoustics::PointSource> sources;
                     for (const auto& src : acoustics::collectSources(c.project.scene))
                         if (std::abs(src.frequencyHz - s.frequencyHz) <= 1e-9 * s.frequencyHz)
                             sources.push_back(src);
                     if (sources.empty())
                         return R::failure("no active source at the analysis frequency");
                     const acoustics::GridFrame frame = acoustics::gridOf(region->settings, c.project.scene.worldMatrix(*ro.value()));
                     const auto estimate = acoustics::estimateField(frame, sources.size(), s.medium, s.frequencyHz, s.memoryLimitBytes);
                     if (!estimate.allowed)
                         return R::failure(estimate.refusal);
                     std::shared_ptr<const acoustics::FieldEvaluator> evaluator;
                     std::string model = s.modelId;
                     if (s.modelId == acoustics::kReflectorModelId) {
                         manipulators::ReflectorOptions o;
                         o.elementsPerWavelength = s.reflectorElementsPerWavelength;
                         auto m = std::make_shared<const manipulators::ReflectorModel>(
                             s.propagation, manipulators::collectReflectors(c.project.scene, c.project.materials, s.medium.wavelength(s.frequencyHz), o).surfaces);
                         evaluator = std::make_shared<const manipulators::ReflectorFieldEvaluator>(sources, s.frequencyHz, s.medium, m);
                     } else {
                         evaluator = std::make_shared<const acoustics::FieldEvaluator>(sources, s.frequencyHz, s.medium,
                                                                                      std::make_shared<const acoustics::FreeFieldModel>(s.propagation));
                         model = acoustics::kFreeFieldModelId;
                     }
                     const auto result = acoustics::computeField("workflow field", *evaluator, frame);
                     if (!result)
                         return R::failure("the field could not be computed");
                     if (p.contains("csv")) {
                         std::ostringstream csv;
                         csv << "x,y,z,re,im,magnitude\n";
                         for (int iw = 0; iw < frame.nw; ++iw)
                             for (int iv = 0; iv < frame.nv; ++iv)
                                 for (int iu = 0; iu < frame.nu; ++iu) {
                                     const Vec3 x = frame.point(iu, iv, iw);
                                     const auto v = result->pressure[frame.index(iu, iv, iw)];
                                     csv << csvNumber(x.x()) << "," << csvNumber(x.y()) << "," << csvNumber(x.z()) << "," << csvNumber(v.real()) << ","
                                         << csvNumber(v.imag()) << "," << csvNumber(std::abs(v)) << "\n";
                                 }
                         if (Status st = writeText(resolvePath(c, p["csv"].get<std::string>()), csv.str()); !st)
                             return R::failure(st.message());
                     }
                     return json{{"maxPressure", result->statistics.maxMagnitude},
                                 {"maxPoint", vectorJson(result->statistics.maxPoint)},
                                 {"meanPressure", result->statistics.meanMagnitude},
                                 {"frequencyHz", s.frequencyHz},
                                 {"samples", frame.pointCount()},
                                 {"model", model}};
                 });
    registry.add({"acoustics.pressure", "acoustic", "Complex pressure at points (free field or reflector model)", {{"points", "[[x, y, z], ...]"}},
                  {"values"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     const acoustics::AcousticSettings s = acoustics::settingsOf(c.project);
                     std::vector<acoustics::PointSource> sources;
                     for (const auto& src : acoustics::collectSources(c.project.scene))
                         if (std::abs(src.frequencyHz - s.frequencyHz) <= 1e-9 * s.frequencyHz)
                             sources.push_back(src);
                     const acoustics::FieldEvaluator evaluator(sources, s.frequencyHz, s.medium, std::make_shared<const acoustics::FreeFieldModel>(s.propagation));
                     json values = json::array();
                     for (const auto& pt : p.value("points", json::array())) {
                         auto x = vec3(pt, "point");
                         if (!x)
                             return Result<json>::failure(x.message());
                         const Complex v = evaluator.pressure(x.value());
                         values.push_back({{"magnitude", std::abs(v)}, {"phase", std::arg(v)}});
                     }
                     return json{{"values", values}, {"model", acoustics::kFreeFieldModelId}};
                 });
    registry.add({"acoustics.focus", "acoustic", "Focus an array on a target (phase or time-delay control)",
                  {{"array", "array object"}, {"target", "target object"}, {"point", "[x, y, z] instead of a target"}, {"control", "phase | delay"}},
                  {"elements", "frequencyHz"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     using R = Result<json>;
                     auto ao = requireObject(c.project, p, "array");
                     if (!ao)
                         return R::failure(ao.message());
                     const auto* array = as<acoustics::ArrayComponent>(*ao.value());
                     if (!array)
                         return R::failure("'" + ao.value()->state.name + "' is not an acoustic array");
                     Vec3 target;
                     if (p.contains("point")) {
                         auto x = vec3(p["point"], "point");
                         if (!x)
                             return R::failure(x.message());
                         target = x.value();
                     } else {
                         auto to = requireObject(c.project, p, "target");
                         if (!to)
                             return R::failure(to.message());
                         target = transformPoint(c.project.scene.worldMatrix(*to.value()), Vec3::Zero());
                     }
                     const acoustics::AcousticSettings s = acoustics::settingsOf(c.project);
                     const auto control = p.value("control", std::string("phase")) == "delay" ? acoustics::FocusControl::TimeDelay : acoustics::FocusControl::Phase;
                     const auto solution = acoustics::computeFocus(acoustics::generateElements(array->settings), c.project.scene.worldMatrix(*ao.value()), target,
                                                                   s.medium, s.frequencyHz, control);
                     auto copy = std::make_shared<acoustics::ArrayComponent>(*array);
                     copy->settings = acoustics::applyFocus(array->settings, solution, acoustics::Taper::Uniform);
                     ObjectState state = ao.value()->state;
                     state.component = copy;
                     if (Status st = c.project.scene.setState(ao.value()->id(), std::move(state)); !st)
                         return R::failure(st.message());
                     return json{{"elements", solution.elements.size()}, {"frequencyHz", s.frequencyHz}, {"target", vectorJson(target)},
                                 {"control", std::string(acoustics::toString(control))}};
                 });

    // --- mechanics ---
    const auto structureOf = [](const Project& project, const json& p) -> const SceneObject* {
        if (p.contains("structure"))
            return findSceneObject(project, p["structure"].get<std::string>());
        return firstWith(project.scene, [](const SceneObject& o) { return as<mechanics::StructureComponent>(o) != nullptr; });
    };
    registry.add({"mechanics.modal", "mechanical", "Natural frequencies of a structure (finite elements)",
                  {{"structure", "structure object (default: the first)"}, {"modes", "number of modes"}, {"set", "changes before the analysis"}},
                  {"frequencies", "method"}},
                 [structureOf](ActionContext& c, const json& p) -> Result<json> {
                     using R = Result<json>;
                     const SceneObject* s = structureOf(c.project, p);
                     if (!s)
                         return R::failure("no structure");
                     if (Status st = applySet(c.project, s->id().str(), p); !st)
                         return R::failure(st.message());
                     mechanics::MechanicsSettings ms = mechanics::mechanicsSettingsOf(c.project);
                     ms.modeCount = p.value("modes", ms.modeCount);
                     auto study = std::make_shared<const mechanics::StructureStudy>(mechanics::prepareStudy(c.project, s->id(), ms.memoryLimitBytes));
                     if (!study->allowed)
                         return R::failure(study->refusal);
                     auto r = mechanics::runModal(study, ms, c.job);
                     if (!r)
                         return R::failure(r.message());
                     json f = json::array();
                     for (const auto& m : r.value()->modal->modes)
                         f.push_back(m.frequencyHz);
                     return json{{"frequencies", f}, {"method", r.value()->modal->method}, {"elementSize", study->elementSize}};
                 });
    registry.add({"mechanics.harmonic", "mechanical", "Forced response of a structure at its sensors and targets",
                  {{"structure", ""}, {"fMin", "Hz"}, {"fMax", "Hz"}, {"points", "frequencies"}, {"set", "changes before the analysis"}},
                  {"frequencies", "peaks"}},
                 [structureOf](ActionContext& c, const json& p) -> Result<json> {
                     using R = Result<json>;
                     const SceneObject* s = structureOf(c.project, p);
                     if (!s)
                         return R::failure("no structure");
                     if (Status st = applySet(c.project, s->id().str(), p); !st)
                         return R::failure(st.message());
                     mechanics::MechanicsSettings ms = mechanics::mechanicsSettingsOf(c.project);
                     ms.frequencyMin = p.value("fMin", ms.frequencyMin);
                     ms.frequencyMax = p.value("fMax", ms.frequencyMax);
                     ms.frequencyPoints = p.value("points", ms.frequencyPoints);
                     auto study = std::make_shared<const mechanics::StructureStudy>(mechanics::prepareStudy(c.project, s->id(), ms.memoryLimitBytes));
                     if (!study->allowed)
                         return R::failure(study->refusal);
                     if (study->sensors.empty() && study->targets.empty())
                         return R::failure("the structure has no sensor or vibration target to report");
                     auto r = mechanics::runHarmonic(study, ms, c.job);
                     if (!r)
                         return R::failure(r.message());
                     const auto& h = *r.value()->harmonic;
                     std::vector<std::string> labels;
                     for (const auto& x : study->sensors)
                         labels.push_back(x.label);
                     for (const auto& x : study->targets)
                         labels.push_back(x.label);
                     json peaks = json::object();
                     for (size_t i = 0; i < h.responses.size() && i < labels.size(); ++i) {
                         size_t best = 0;
                         for (size_t k = 0; k < h.responses[i].size(); ++k)
                             if (std::abs(h.responses[i][k]) > std::abs(h.responses[i][best]))
                                 best = k;
                         peaks[labels[i]] = {{"peakFrequency", h.frequenciesHz[best]}, {"peakDisplacement", std::abs(h.responses[i][best])}};
                     }
                     return json{{"frequencies", h.frequenciesHz.size()}, {"fMin", ms.frequencyMin}, {"fMax", ms.frequencyMax}, {"peaks", peaks},
                                 {"method", h.method}};
                 });

    // --- electromagnetics ---
    registry.add({"em.antenna", "electromagnetic", "Analytical antenna characteristics", {{"object", "antenna"}, {"set", "changes before the analysis"}},
                  {"inputResistance", "inputReactance", "returnLossDb", "directivityDbi", "gainDbi"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     auto o = requireObject(c.project, p, "object");
                     if (!o)
                         return Result<json>::failure(o.message());
                     if (Status st = applySet(c.project, o.value()->id().str(), p); !st)
                         return Result<json>::failure(st.message());
                     const em::AntennaReport r = em::analyzeAntenna(c.project, o.value()->id());
                     if (!r.analytical || !r.impedance)
                         return Result<json>::failure("no analytical model for this antenna" + (r.notes.empty() ? std::string() : ": " + r.notes.front()));
                     const auto dbi = [](double v) { return v > 0.0 ? 10.0 * std::log10(v) : -200.0; };
                     json j{{"frequencyHz", r.frequencyHz},
                            {"inputResistance", r.impedance->input.real()},
                            {"inputReactance", r.impedance->input.imag()},
                            {"directivityDbi", dbi(r.directivity)},
                            {"gainDbi", dbi(r.gain)},
                            {"model", r.model}};
                     if (r.matching)
                         j["returnLossDb"] = r.matching->returnLossDb;
                     return j;
                 });
    registry.add({"em.pattern", "electromagnetic", "Radiation pattern of an antenna or array", {{"object", ""}, {"resolutionDeg", ""}, {"csv", "optional pattern file"}},
                  {"directivityDbi", "mainThetaDeg", "mainPhiDeg", "sidelobeDb"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     using R = Result<json>;
                     auto o = requireObject(c.project, p, "object");
                     if (!o)
                         return R::failure(o.message());
                     if (Status st = applySet(c.project, o.value()->id().str(), p); !st)
                         return R::failure(st.message());
                     o = requireObject(c.project, p, "object");
                     const double res = p.value("resolutionDeg", 1.0);
                     json out;
                     if (const auto* a = as<em::EmArrayComponent>(*o.value())) {
                         const em::PatternAnalysis pa = em::analyzePattern(em::arrayModelOf(a->settings), a->settings.frequencyHz, res);
                         out = {{"directivityDbi", pa.directivityDbi}, {"mainThetaDeg", pa.mainTheta * 180.0 / kPi}, {"mainPhiDeg", pa.mainPhi * 180.0 / kPi},
                                {"sidelobeDb", pa.sidelobeDb}, {"beamwidthThetaDeg", pa.beamwidthTheta * 180.0 / kPi}, {"model", "array factor"}};
                     }
                     auto grid = em::patternOf(c.project, o.value()->id(), std::max(res, 1.0));
                     if (!grid)
                         return R::failure(grid.message());
                     if (out.is_null())
                         out = {{"directivityDbi", grid.value().directivity > 0.0 ? 10.0 * std::log10(grid.value().directivity) : -200.0},
                                {"model", grid.value().model}};
                     if (p.contains("csv")) {
                         std::ostringstream csv;
                         csv << "theta_deg,phi_deg,normalized_magnitude\n";
                         const auto& g = grid.value();
                         for (int i = 0; i < g.thetaCount; ++i)
                             for (int j = 0; j < g.phiCount; ++j)
                                 csv << csvNumber(i * g.stepRad * 180.0 / kPi) << "," << csvNumber(j * g.stepRad * 180.0 / kPi) << "," << csvNumber(g.value(i, j))
                                     << "\n";
                         if (Status st = writeText(resolvePath(c, p["csv"].get<std::string>()), csv.str()); !st)
                             return R::failure(st.message());
                         out["csv"] = p["csv"];
                     }
                     return out;
                 });
    registry.add({"em.steer", "electromagnetic", "Steer an array (phase or true time delay)", {{"array", ""}, {"thetaDeg", ""}, {"phiDeg", ""}, {"method", "phase | delay"}},
                  {"thetaDeg", "phiDeg"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     auto o = requireObject(c.project, p, "array");
                     if (!o)
                         return Result<json>::failure(o.message());
                     const auto* a = as<em::EmArrayComponent>(*o.value());
                     if (!a)
                         return Result<json>::failure("not an electromagnetic array");
                     em::ArrayModel model = em::arrayModelOf(a->settings);
                     const double t = p.value("thetaDeg", 0.0), ph = p.value("phiDeg", 0.0);
                     em::steer(model, em::directionOf(t * kPi / 180.0, ph * kPi / 180.0), a->settings.frequencyHz,
                               p.value("method", std::string("phase")) == "delay" ? em::SteeringMethod::TimeDelay : em::SteeringMethod::Phase);
                     auto copy = std::make_shared<em::EmArrayComponent>(*a);
                     copy->settings = em::withExcitation(a->settings, model);
                     ObjectState state = o.value()->state;
                     state.component = copy;
                     if (Status st = c.project.scene.setState(o.value()->id(), std::move(state)); !st)
                         return Result<json>::failure(st.message());
                     return json{{"thetaDeg", t}, {"phiDeg", ph}};
                 });
    registry.add({"em.taper", "electromagnetic", "Dolph-Chebyshev amplitude taper of a linear array (phases kept)", {{"array", ""}, {"sidelobeDb", "positive dB"}},
                  {"weights"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     auto o = requireObject(c.project, p, "array");
                     if (!o)
                         return Result<json>::failure(o.message());
                     const auto* a = as<em::EmArrayComponent>(*o.value());
                     if (!a || a->settings.layout != em::EmArrayLayout::Linear)
                         return Result<json>::failure("a linear electromagnetic array is needed");
                     const auto w = em::chebyshevWeights(a->settings.countX, p.value("sidelobeDb", 30.0));
                     auto copy = std::make_shared<em::EmArrayComponent>(*a);
                     for (size_t n = 0; n < w.size(); ++n)
                         copy->settings.overrides[static_cast<int>(n)].amplitude = w[n] * a->settings.amplitude;
                     ObjectState state = o.value()->state;
                     state.component = copy;
                     if (Status st = c.project.scene.setState(o.value()->id(), std::move(state)); !st)
                         return Result<json>::failure(st.message());
                     return json{{"weights", w}};
                 });

    // --- optimization ---
    registry.add({"optimization.run", "optimization", "Run an optimization problem (from a template or given) and record it in the project",
                  {{"template", "template id"}, {"problem", "a problem document"}, {"maxEvaluations", "budget override"}, {"algorithm", "method override"}},
                  {"runId", "candidates", "best", "pareto"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     using R = Result<json>;
                     json document;
                     if (p.contains("problem"))
                         document = p["problem"];
                     else {
                         const optimization::ProblemTemplate* t = optimization::AdapterRegistry::instance().findTemplate(p.value("template", std::string()));
                         if (!t)
                             return R::failure("no optimization template '" + p.value("template", std::string()) + "'");
                         auto made = t->create(c.project);
                         if (!made)
                             return R::failure(made.message());
                         document = made.take();
                     }
                     auto problem = optimization::problemFromJson(document);
                     if (!problem)
                         return R::failure(problem.message());
                     optimization::OptimizationProblem pr = problem.take();
                     pr.algorithm.maxEvaluations = p.value("maxEvaluations", pr.algorithm.maxEvaluations);
                     pr.algorithm.algorithm = p.value("algorithm", pr.algorithm.algorithm);
                     auto ev = optimization::AdapterRegistry::instance().bind(pr.adapterId, optimization::designSnapshot(c.project), pr.adapterConfig);
                     if (!ev)
                         return R::failure(ev.message());
                     auto run = optimization::runOptimization(pr, ev.take(), {}, c.job);
                     if (!run)
                         return R::failure(run.message());
                     optimization::OptimizationWorkspace w = optimization::workspaceOf(c.project);
                     if (!w.findProblem(pr.id))
                         w.problems.push_back(pr);
                     w.runs.push_back(run.value());
                     optimization::storeWorkspace(c.project, w);
                     json out{{"runId", run.value().runId}, {"candidates", run.value().candidates.size()}, {"pareto", run.value().pareto.size()},
                              {"termination", run.value().termination.reason}, {"problem", pr.id}};
                     if (run.value().best) {
                         const auto& b = run.value().candidates[*run.value().best];
                         out["best"] = {{"id", b.id}, {"values", b.point.values}, {"objectives", b.objectiveValues}};
                     }
                     return out;
                 });

    // --- files and checks ---
    registry.add({"io.write-json", "io", "Write data (usually outputs of earlier steps) as JSON", {{"path", ""}, {"data", ""}}, {"path"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     const auto path = resolvePath(c, p.value("path", std::string("outputs.json")));
                     if (Status s = writeText(path, p.value("data", json()).dump(2) + "\n"); !s)
                         return Result<json>::failure(s.message());
                     return json{{"path", path.string()}};
                 });
    registry.add({"io.write-csv", "io", "Write rows (objects with the same keys) as CSV", {{"path", ""}, {"rows", "[{...}, ...]"}}, {"path", "rows"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     const json rows = p.value("rows", json::array());
                     if (!rows.is_array() || rows.empty() || !rows[0].is_object())
                         return Result<json>::failure("rows must be a list of objects");
                     std::ostringstream csv;
                     std::vector<std::string> keys;
                     for (const auto& [k, v] : rows[0].items())
                         keys.push_back(k);
                     for (size_t i = 0; i < keys.size(); ++i)
                         csv << (i ? "," : "") << keys[i];
                     csv << "\n";
                     for (const auto& r : rows) {
                         for (size_t i = 0; i < keys.size(); ++i) {
                             const json v = r.value(keys[i], json());
                             csv << (i ? "," : "") << (v.is_string() ? v.get<std::string>() : v.is_number() ? csvNumber(v.get<double>()) : v.dump());
                         }
                         csv << "\n";
                     }
                     const auto path = resolvePath(c, p.value("path", std::string("outputs.csv")));
                     if (Status s = writeText(path, csv.str()); !s)
                         return Result<json>::failure(s.message());
                     return json{{"path", path.string()}, {"rows", rows.size()}};
                 });
    registry.add({"report.html", "io", "Write an HTML report of results", {{"path", ""}, {"title", ""}, {"sections", "[{heading, text?, data?}]"}}, {"path"}},
                 [](ActionContext& c, const json& p) -> Result<json> {
                     std::ostringstream h;
                     const std::string title = p.value("title", std::string("Resonant workflow report"));
                     h << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>" << htmlEscape(title)
                       << "</title><style>body{font-family:sans-serif;margin:2em}table{border-collapse:collapse}th,td{border:1px solid #bbb;"
                          "padding:3px 8px;text-align:left;vertical-align:top}</style></head><body><h1>"
                       << htmlEscape(title) << "</h1><p>Project: " << htmlEscape(c.project.name) << " &middot; generated " << optimization::utcNow()
                       << "</p>";
                     for (const auto& s : p.value("sections", json::array())) {
                         h << "<h2>" << htmlEscape(s.value("heading", std::string())) << "</h2>";
                         if (s.contains("text"))
                             h << "<p>" << htmlEscape(s["text"].get<std::string>()) << "</p>";
                         if (s.contains("data"))
                             htmlValue(h, s["data"]);
                     }
                     h << "<p><small>Calculated by Resonant workflow actions; each value states the model it comes from.</small></p></body></html>\n";
                     const auto path = resolvePath(c, p.value("path", std::string("report.html")));
                     if (Status st = writeText(path, h.str()); !st)
                         return Result<json>::failure(st.message());
                     return json{{"path", path.string()}};
                 });
    registry.add({"workflow.check", "workflow", "Fail unless a value lies in a range (guards a workflow's assumptions)",
                  {{"value", "a number, usually ${steps...}"}, {"min", ""}, {"max", ""}, {"message", ""}}, {"value"}},
                 [](ActionContext&, const json& p) -> Result<json> {
                     if (!p.contains("value") || !p["value"].is_number())
                         return Result<json>::failure("workflow.check needs a numeric value");
                     const double v = p["value"].get<double>();
                     const std::string message = p.value("message", std::string("check failed"));
                     if (p.contains("min") && v < p["min"].get<double>())
                         return Result<json>::failure(message + " (" + csvNumber(v) + " < " + csvNumber(p["min"].get<double>()) + ")");
                     if (p.contains("max") && v > p["max"].get<double>())
                         return Result<json>::failure(message + " (" + csvNumber(v) + " > " + csvNumber(p["max"].get<double>()) + ")");
                     return json{{"value", v}};
                 });
}

}  // namespace resonant::coupling

namespace resonant::automation {

ActionRegistry& ActionRegistry::instance()
{
    static ActionRegistry registry = [] {
        ActionRegistry created;
        coupling::registerWorkflowActions(created);
        return created;
    }();
    return registry;
}

}  // namespace resonant::automation
