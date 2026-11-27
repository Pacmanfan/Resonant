// The native part of the Resonant Python API (`resonant._resonant`, ADR-000215).
//
// A deliberate facade, not a mirror of the C++ classes:
//   * a Project owns its C++ project through a shared holder; ObjectRef keeps
//     the holder and an object id, and looks the object up on every access,
//     so no Python object ever holds a raw pointer into the scene. Removing an
//     object makes its references raise; Python garbage collection can never
//     free a C++ object that is still in use;
//   * structured data (components, settings, results, problems, workflows)
//     cross the boundary as Python dicts and lists (the project's own JSON
//     forms), arrays as NumPy arrays;
//   * long calculations release the GIL; progress and cancellation callbacks
//     are called with the GIL held; every failure raises resonant.Error with
//     the reason.
// Thread safety: one Project may be used from one Python thread at a time
// (calls on it are serialized by its lock); different projects are
// independent.
#include "automation/WorkflowEngine.h"
#include "coupling/WorkflowActions.h"
#include "acoustics/Components.h"
#include "acoustics/FieldEvaluator.h"
#include "acoustics/FieldGrid.h"
#include "acoustics/Propagation.h"
#include "acoustics/Settings.h"
#include "electromagnetics/ArrayFactor.h"
#include "electromagnetics/EmComponents.h"
#include "electromagnetics/EmStudy.h"
#include "experiments/Experiment.h"
#include "experiments/ExperimentProject.h"
#include "fdtd/FdtdSetup.h"
#include "foundation/Version.h"
#include "geometry/Geometry.h"
#include "manipulators/ReflectorModel.h"
#include "materials/Material.h"
#include "mechanics/Study.h"
#include "optimization/Engine.h"
#include "optimization/Persistence.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "scene/Scene.h"

#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <mutex>

#ifdef RESONANT_HAVE_AUTOMATION_API
#include "Dispatcher.h"
#include "services/Services.h"
#endif

namespace py = pybind11;
using nlohmann::json;
using namespace resonant;

namespace {

constexpr const char* kApiVersion = "1.0";

struct Holder {
    std::unique_ptr<Project> project;
    std::recursive_mutex mutex;
};
using Box = std::shared_ptr<Holder>;

py::object toPy(const json& j)
{
    switch (j.type()) {
    case json::value_t::null: return py::none();
    case json::value_t::boolean: return py::bool_(j.get<bool>());
    case json::value_t::number_integer: return py::int_(j.get<std::int64_t>());
    case json::value_t::number_unsigned: return py::int_(j.get<std::uint64_t>());
    case json::value_t::number_float: return py::float_(j.get<double>());
    case json::value_t::string: return py::str(j.get<std::string>());
    case json::value_t::array: {
        py::list l;
        for (const auto& v : j)
            l.append(toPy(v));
        return l;
    }
    case json::value_t::object: {
        py::dict d;
        for (const auto& [k, v] : j.items())
            d[py::str(k)] = toPy(v);
        return d;
    }
    default: return py::none();
    }
}

json fromPy(py::handle h)
{
    if (h.is_none())
        return nullptr;
    if (py::isinstance<py::bool_>(h))
        return h.cast<bool>();
    if (py::isinstance<py::int_>(h))
        return h.cast<std::int64_t>();
    if (py::isinstance<py::float_>(h))
        return h.cast<double>();
    if (py::isinstance<py::str>(h))
        return h.cast<std::string>();
    if (py::isinstance<py::dict>(h)) {
        json out = json::object();
        for (auto item : h.cast<py::dict>())
            out[py::str(item.first).cast<std::string>()] = fromPy(item.second);
        return out;
    }
    if (py::isinstance<py::list>(h) || py::isinstance<py::tuple>(h)) {
        json out = json::array();
        for (auto item : h)
            out.push_back(fromPy(item));
        return out;
    }
    if (py::hasattr(h, "tolist"))  // NumPy arrays and scalars
        return fromPy(h.attr("tolist")());
    if (py::hasattr(h, "__float__"))
        return h.cast<double>();
    throw py::type_error("cannot convert a " + std::string(py::str(py::type::of(h).attr("__name__"))) + " to a Resonant value");
}

[[noreturn]] void fail(const std::string& message);

template <typename T>
T unwrap(Result<T> r)
{
    if (!r)
        fail(r.message());
    return r.take();
}

void check(const Status& s)
{
    if (!s)
        fail(s.message());
}

py::object errorType;

void fail(const std::string& message)
{
    PyErr_SetString(errorType.ptr(), message.c_str());
    throw py::error_already_set();
}

// Progress and cancellation from Python callables.
class CallbackContext final : public JobContext {
public:
    CallbackContext(py::object progress, py::object cancel) : progress_(std::move(progress)), cancel_(std::move(cancel)) {}
    bool cancelRequested() const override
    {
        if (cancelled_)
            return true;
        if (cancel_.is_none())
            return false;
        py::gil_scoped_acquire gil;
        try {
            cancelled_ = cancel_().cast<bool>();
        } catch (py::error_already_set&) {
            cancelled_ = true;  // a failing cancel callback stops the work
        }
        return cancelled_;
    }
    void reportProgress(double fraction, std::string_view message) override
    {
        if (progress_.is_none())
            return;
        py::gil_scoped_acquire gil;
        try {
            progress_(fraction, std::string(message));
        } catch (py::error_already_set&) {
            PyErr_Clear();
        }
    }

private:
    py::object progress_, cancel_;
    mutable bool cancelled_ = false;
};

class PyProject;

class ObjectRef {
public:
    ObjectRef(Box box, Id id) : box_(std::move(box)), id_(std::move(id)) {}
    const SceneObject& object() const
    {
        const SceneObject* o = box_->project->scene.find(id_);
        if (!o)
            fail("the object " + id_.str() + " is no longer in the project");
        return *o;
    }
    std::string id() const { return id_.str(); }
    bool valid() const { return box_->project->scene.find(id_) != nullptr; }
    std::string type() const { return object().type(); }
    std::string name() const { return object().state.name; }
    void setName(const std::string& name) { change({{"name", name}}); }
    std::array<double, 3> position() const
    {
        const Vec3 p = object().state.local.position;
        return {p.x(), p.y(), p.z()};
    }
    void setPosition(const std::array<double, 3>& p) { change({{"position", {p[0], p[1], p[2]}}}); }
    std::array<double, 3> rotation() const
    {
        const Vec3 r = object().state.local.rotation;
        return {r.x(), r.y(), r.z()};
    }
    void setRotation(const std::array<double, 3>& r) { change({{"rotation", {r[0], r[1], r[2]}}}); }
    bool visible() const { return object().state.visible; }
    void setVisible(bool v) { change({{"visible", v}}); }
    py::object geometryKind() const
    {
        const auto& g = object().state.geometry;
        return g ? py::object(py::str(g->kindId())) : py::none();
    }
    py::dict parameters() const
    {
        py::dict d;
        if (const auto& g = object().state.geometry)
            for (const auto& [k, v] : g->parameters())
                d[py::str(k)] = v;
        return d;
    }
    void setParameters(py::dict values) { change({{"parameters", fromPy(values)}}); }
    py::object material() const
    {
        const Id& m = object().state.materialId;
        return m.isNull() ? py::none() : py::object(py::str(m.str()));
    }
    void setMaterial(py::object m) { change({{"material", m.is_none() ? json(nullptr) : json(m.cast<std::string>())}}); }
    py::object componentType() const
    {
        const auto& c = object().state.component;
        return c ? py::object(py::str(c->typeId())) : py::none();
    }
    py::object component() const
    {
        const auto& c = object().state.component;
        return c ? toPy(c->toJson()) : py::none();
    }
    void setComponent(py::dict patch) { change({{"component", fromPy(patch)}}); }
    py::object parent() const;
    std::vector<ObjectRef> children() const
    {
        std::vector<ObjectRef> out;
        for (const auto& c : object().children())
            out.emplace_back(box_, c->id());
        return out;
    }
    std::array<double, 3> worldPosition() const
    {
        const Vec3 p = transformPoint(box_->project->scene.worldMatrix(object()), Vec3::Zero());
        return {p.x(), p.y(), p.z()};
    }
    void change(const json& changes)
    {
        std::lock_guard lock(box_->mutex);
        check(coupling::changeSceneObject(*box_->project, id_.str(), changes));
    }
    std::string repr() const { return valid() ? "<resonant.ObjectRef '" + name() + "' (" + type() + ") " + id_.str() + ">" : "<resonant.ObjectRef (removed) " + id_.str() + ">"; }

private:
    Box box_;
    Id id_;
};

py::object ObjectRef::parent() const
{
    const SceneObject* p = object().parent();
    if (!p || box_->project->scene.isRoot(*p))
        return py::none();
    return py::cast(ObjectRef(box_, p->id()));
}

class PyProject {
public:
    explicit PyProject(const std::string& name) : box_(std::make_shared<Holder>()) { box_->project = Project::createNew(name); }
    explicit PyProject(std::unique_ptr<Project> p) : box_(std::make_shared<Holder>()) { box_->project = std::move(p); }
    static PyProject load(const std::string& path)
    {
        auto loaded = loadProjectFile(path);
        if (!loaded)
            fail(loaded.message());
        return PyProject(std::move(loaded.value().project));
    }
    static PyProject fromJson(const std::string& text)
    {
        json j;
        try {
            j = json::parse(text);
        } catch (const std::exception& e) {
            fail(std::string("not JSON: ") + e.what());
        }
        auto loaded = deserializeProject(j);
        if (!loaded)
            fail(loaded.message());
        return PyProject(std::move(loaded.value().project));
    }
    void save(const std::string& path)
    {
        std::lock_guard lock(box_->mutex);
        check(saveProjectFile(*box_->project, path));
    }
    std::string toJson() const { return serializeProject(*box_->project).dump(); }
    PyProject copy() const { return fromJson(toJson()); }
    std::string name() const { return box_->project->name; }
    void setName(const std::string& n) { box_->project->name = n; }
    std::string id() const { return box_->project->id.str(); }
    std::vector<ObjectRef> objects() const
    {
        std::vector<ObjectRef> out;
        box_->project->scene.forEach([&](const SceneObject& o) { out.emplace_back(box_, o.id()); });
        return out;
    }
    ObjectRef object(const std::string& reference) const
    {
        const SceneObject* o = coupling::findSceneObject(*box_->project, reference);
        if (!o)
            fail("no object '" + reference + "'");
        return ObjectRef(box_, o->id());
    }
    ObjectRef add(py::dict description)
    {
        std::lock_guard lock(box_->mutex);
        const std::string id = unwrap(coupling::addSceneObject(*box_->project, fromPy(description)));
        return ObjectRef(box_, Id(id));
    }
    void remove(const std::string& reference)
    {
        std::lock_guard lock(box_->mutex);
        const SceneObject* o = coupling::findSceneObject(*box_->project, reference);
        if (!o)
            fail("no object '" + reference + "'");
        box_->project->scene.detach(o->id());
    }
    py::list materials() const
    {
        py::list out;
        for (const Material& m : box_->project->materials.materials())
            out.append(toPy(resonant::toJson(m)));
        return out;
    }
    py::dict material(const std::string& reference) const
    {
        const Material* m = box_->project->materials.find(Id(reference));
        if (!m)
            m = box_->project->materials.findByName(reference);
        if (!m)
            fail("no material '" + reference + "'");
        return toPy(resonant::toJson(*m));
    }
    void addMaterial(py::dict value)
    {
        std::lock_guard lock(box_->mutex);
        auto m = materialFromJson(fromPy(value));
        if (!m)
            fail(m.message());
        check(box_->project->materials.add(m.take()));
    }
    py::object settings(const std::string& key) const
    {
        const Project& p = *box_->project;
        if (key == "acoustics")
            return toPy(acoustics::toJson(acoustics::settingsOf(p)));
        if (key == "mechanics")
            return toPy(mechanics::toJson(mechanics::mechanicsSettingsOf(p)));
        if (key == "electromagnetics")
            return toPy(em::toJson(em::emSettingsOf(p)));
        if (key == "fdtd")
            return toPy(fdtd::toJson(fdtd::fdtdSettingsOf(p)));
        return p.extensions.contains(key) ? toPy(p.extensions[key]) : py::none();
    }
    void setSettings(const std::string& key, py::dict patch)
    {
        std::lock_guard lock(box_->mutex);
        const nlohmann::json empty = nlohmann::json::object();
        automation::ActionContext context{*box_->project, nullptr, empty, {}};
        unwrap(automation::ActionRegistry::instance().run("settings.set", context, {{"key", key}, {"values", fromPy(patch)}}));
    }
    Project& project() const { return *box_->project; }
    Box box() const { return box_; }
    std::recursive_mutex& mutex() const { return box_->mutex; }
    std::string repr() const { return "<resonant.Project '" + name() + "', " + std::to_string(box_->project->scene.objectCount()) + " objects>"; }

private:
    Box box_;
};

std::vector<acoustics::PointSource> sourcesAt(const Project& p, double f)
{
    std::vector<acoustics::PointSource> out;
    for (const auto& s : acoustics::collectSources(p.scene))
        if (std::abs(s.frequencyHz - f) <= 1e-9 * f)
            out.push_back(s);
    return out;
}

std::shared_ptr<const acoustics::FieldEvaluator> evaluatorOf(const Project& p, const acoustics::AcousticSettings& s, const std::string& model)
{
    const auto sources = sourcesAt(p, s.frequencyHz);
    if (sources.empty())
        fail("no active source at the analysis frequency");
    const bool reflector = model == "reflector" || (model == "auto" && s.modelId == acoustics::kReflectorModelId);
    if (reflector) {
        manipulators::ReflectorOptions o;
        o.elementsPerWavelength = s.reflectorElementsPerWavelength;
        auto m = std::make_shared<const manipulators::ReflectorModel>(
            s.propagation, manipulators::collectReflectors(p.scene, p.materials, s.medium.wavelength(s.frequencyHz), o).surfaces);
        return std::make_shared<const manipulators::ReflectorFieldEvaluator>(sources, s.frequencyHz, s.medium, m);
    }
    return std::make_shared<const acoustics::FieldEvaluator>(sources, s.frequencyHz, s.medium, std::make_shared<const acoustics::FreeFieldModel>(s.propagation));
}

py::array_t<std::complex<double>> complexArray(const std::vector<std::complex<float>>& values, std::vector<py::ssize_t> shape)
{
    py::array_t<std::complex<double>> a(shape);
    auto* data = a.mutable_data();
    for (size_t i = 0; i < values.size(); ++i)
        data[i] = std::complex<double>(values[i].real(), values[i].imag());
    return a;
}

py::dict fieldDict(const acoustics::FieldResult& r)
{
    const auto& f = r.frame;
    py::dict d;
    d["frequency"] = r.frequencyHz;
    d["origin"] = std::vector<double>{f.origin.x(), f.origin.y(), f.origin.z()};
    d["axis_u"] = std::vector<double>{f.axisU.x(), f.axisU.y(), f.axisU.z()};
    d["axis_v"] = std::vector<double>{f.axisV.x(), f.axisV.y(), f.axisV.z()};
    d["axis_w"] = std::vector<double>{f.axisW.x(), f.axisW.y(), f.axisW.z()};
    d["shape"] = py::make_tuple(f.nw, f.nv, f.nu);
    d["pressure"] = complexArray(r.pressure, {f.nw, f.nv, f.nu});  // [w][v][u], Pa (complex peak)
    d["max_pressure"] = r.statistics.maxMagnitude;
    d["max_point"] = std::vector<double>{r.statistics.maxPoint.x(), r.statistics.maxPoint.y(), r.statistics.maxPoint.z()};
    d["assumptions"] = r.assumptions;
    d["solver"] = r.metadata.solverId;
    return d;
}

#ifdef RESONANT_HAVE_AUTOMATION_API
// The automation API in this process (REQ-CHAT-20261126-012): the same
// resonant.v1 contract and services as the socket and gRPC, around a Python
// project. Calls hold the project's lock and release the GIL; jobs run on the
// session's workers.
class PyAutomationSession {
    class Host final : public services::ProjectHost {
    public:
        explicit Host(Box box) : box_(std::move(box)), jobs_(std::make_unique<JobManager>(2)) {}
        ~Host() override { jobs_.reset(); }
        Project& project() override { return *box_->project; }
        CommandStack& commands() override { return commands_; }
        JobManager& jobs() override { return *jobs_; }
        std::string filePath() const override { return path_; }
        void setFilePath(const std::string& path) override { path_ = path; }
        void installProject(std::unique_ptr<Project> project, const std::string& path) override
        {
            box_->project = std::move(project);
            path_ = path;
            commands_.clear();
            if (session_)
                session_->projectReplaced(path.empty() ? "project.created" : "project.opened");
        }
        void setSession(services::Session* s) { session_ = s; }
        std::recursive_mutex& mutex() { return box_->mutex; }

    private:
        Box box_;
        CommandStack commands_;
        std::unique_ptr<JobManager> jobs_;
        std::string path_;
        services::Session* session_ = nullptr;
    };

public:
    explicit PyAutomationSession(const PyProject& project)
        : host_(std::make_unique<Host>(project.box())), session_(std::make_unique<services::Session>(*host_, "python")),
          dispatcher_(std::make_unique<automation_api::Dispatcher>(*session_, [this](const std::function<void()>& f) {
              std::lock_guard lock(host_->mutex());
              f();
          }))
    {
        host_->setSession(session_.get());
    }
    ~PyAutomationSession()
    {
        dispatcher_.reset();
        session_.reset();
        host_.reset();
    }
    py::tuple call(const std::string& method, const py::bytes& payload)
    {
        const std::string data = payload;
        automation_api::Reply r;
        {
            py::gil_scoped_release release;
            r = dispatcher_->call(method, data, services::Interface::Python);
        }
        return py::make_tuple(r.status, py::bytes(r.payload), py::bytes(r.error));
    }
    py::tuple stream(const std::string& method, const py::bytes& payload)
    {
        // In this process only bounded streams: a subscription waits for events that this thread would have to produce.
        if (method == "/resonant.v1.EventService/SubscribeEvents")
            fail("SubscribeEvents needs the application's socket or gRPC: in process, use EventService.ListEvents");
        const std::string data = payload;
        std::vector<std::string> items;
        automation_api::Reply r;
        {
            py::gil_scoped_release release;
            r = dispatcher_->stream(method, data, services::Interface::Python, [&items](const std::string& item) {
                if (!item.empty())
                    items.push_back(item);
                return true;
            });
        }
        py::list out;
        for (const std::string& item : items)
            out.append(py::bytes(item));
        return py::make_tuple(r.status, out, py::bytes(r.error));
    }
    std::vector<std::string> methods() const
    {
        std::vector<std::string> out;
        for (const auto& m : dispatcher_->methods())
            out.push_back(m.method);
        return out;
    }

private:
    std::unique_ptr<Host> host_;
    std::unique_ptr<services::Session> session_;
    std::unique_ptr<automation_api::Dispatcher> dispatcher_;
};
#endif

}  // namespace

PYBIND11_MODULE(_resonant, m)
{
    m.doc() = "Native core of the Resonant Python API (use the `resonant` package).";
    errorType = py::reinterpret_borrow<py::object>(PyErr_NewException("resonant.Error", PyExc_RuntimeError, nullptr));
    m.attr("Error") = errorType;
    m.def("api_version", [] { return std::string(kApiVersion); }, "Version of the Python API (major.minor).");
    m.def("core_version", [] { return std::string(kApplicationVersion); }, "Version of the Resonant core it was built from.");

    py::class_<ObjectRef>(m, "ObjectRef", "A scene object, referred to by id (it never dangles: a removed object raises).")
        .def_property_readonly("id", &ObjectRef::id, "The stable object id (str).")
        .def_property_readonly("valid", &ObjectRef::valid, "False once the object was removed from its project.")
        .def_property_readonly("type", &ObjectRef::type, "What the object is: geometry, group, or its component type.")
        .def_property("name", &ObjectRef::name, &ObjectRef::setName, "The object's name (shown in the scene tree).")
        .def_property("position", &ObjectRef::position, &ObjectRef::setPosition, "Local position, m")
        .def_property("rotation", &ObjectRef::rotation, &ObjectRef::setRotation, "Local Z-Y-X Euler angles [x, y, z], rad")
        .def_property("visible", &ObjectRef::visible, &ObjectRef::setVisible, "Shown in the viewport (hiding does not switch a source off).")
        .def_property_readonly("world_position", &ObjectRef::worldPosition, "Position in world coordinates, m (through the parents).")
        .def_property_readonly("geometry_kind", &ObjectRef::geometryKind, "The geometry kind id (box, sphere, reflector.parabolic, ...), or empty.")
        .def_property_readonly("parameters", &ObjectRef::parameters, "Geometry parameters (SI)")
        .def("set_parameters", &ObjectRef::setParameters, "Set geometry parameters together (validated by the geometry kind).")
        .def_property("material", &ObjectRef::material, &ObjectRef::setMaterial, "The assigned material id (set: an id or name of the library).")
        .def_property_readonly("component_type", &ObjectRef::componentType, "The domain component type (acoustic.array, em.antenna, ...), or empty.")
        .def_property_readonly("component", &ObjectRef::component, "The domain data as a dict (a copy).")
        .def("set_component", &ObjectRef::setComponent, "Merge a dict into the domain data (validated).")
        .def_property_readonly("parent", &ObjectRef::parent, "The parent object (None at the top level).")
        .def_property_readonly("children", &ObjectRef::children, "The child objects, in order.")
        .def("__repr__", &ObjectRef::repr)
        .def("__eq__", [](const ObjectRef& a, const ObjectRef& b) { return a.id() == b.id(); });

    py::class_<PyProject>(m, "Project", "A Resonant project: scene, materials, settings and results records.")
        .def(py::init<const std::string&>(), py::arg("name") = "Untitled", "A new, empty project with the standard material library.")
        .def_static("load", &PyProject::load, py::arg("path"), "Open a .resonant project file (migrated to the current format).")
        .def_static("from_json", &PyProject::fromJson, py::arg("text"), "A project from its JSON text (as to_json() writes it).")
        .def("save", &PyProject::save, py::arg("path"), "Save to a .resonant project file.")
        .def("to_json", &PyProject::toJson, "The project as JSON text (the project file format).")
        .def("copy", &PyProject::copy, "An independent deep copy.")
        .def_property("name", &PyProject::name, &PyProject::setName, "The project's name.")
        .def_property_readonly("id", &PyProject::id, "The project's stable id.")
        .def("objects", &PyProject::objects, "Every scene object, in pre-order.")
        .def("object", &PyProject::object, py::arg("reference"), "An object by id or name.")
        .def("add", &PyProject::add, py::arg("description"), "Add an object from a description dict (type, name, kind, parameters, component, position, rotation, material, id, parent).")
        .def("remove", &PyProject::remove, py::arg("reference"), "Remove an object (by id or name) with its children.")
        .def("materials", &PyProject::materials, "Every material of the project's library, as dicts.")
        .def("material", &PyProject::material, py::arg("reference"), "A material by id or name, as a dict.")
        .def("add_material", &PyProject::addMaterial, py::arg("material"), "Add a material (a dict in the project file's material format).")
        .def("settings", &PyProject::settings, "Project settings of a domain (acoustics, mechanics, electromagnetics, fdtd) as a dict.")
        .def("set_settings", &PyProject::setSettings, "Merge a dict into a domain's settings (validated).")
        .def("__repr__", &PyProject::repr);

    m.def("object_types", &coupling::sceneObjectTypes);
    m.def("geometry_kinds", [] {
        py::list out;
        for (const GeometryKind* k : GeometryRegistry::instance().kinds()) {
            py::list params;
            for (const ParameterDef& p : k->parameters)
                params.append(py::dict(py::arg("key") = p.key, py::arg("label") = p.label, py::arg("unit") = p.unit, py::arg("default") = p.defaultValue,
                                       py::arg("min") = p.minValue, py::arg("max") = p.maxValue, py::arg("integer") = p.integer));
            out.append(py::dict(py::arg("id") = k->id, py::arg("name") = k->displayName, py::arg("category") = k->category, py::arg("parameters") = params));
        }
        return out;
    });

    // --- simulation --------------------------------------------------------------------------------
    m.def(
        "acoustic_field",
        [](PyProject& p, const std::string& region, const std::string& model) {
            std::lock_guard lock(p.mutex());
            const Project& project = p.project();
            const SceneObject* r = coupling::findSceneObject(project, region);
            const auto* rc = r ? dynamic_cast<const acoustics::RegionComponent*>(r->state.component.get()) : nullptr;
            if (!rc)
                fail("no field region '" + region + "'");
            const acoustics::AcousticSettings s = acoustics::settingsOf(project);
            const auto evaluator = evaluatorOf(project, s, model);
            const acoustics::GridFrame frame = acoustics::gridOf(rc->settings, project.scene.worldMatrix(*r));
            const auto estimate = acoustics::estimateField(frame, evaluator->sources().size(), s.medium, s.frequencyHz, s.memoryLimitBytes);
            if (!estimate.allowed)
                fail(estimate.refusal);
            std::shared_ptr<const acoustics::FieldResult> result;
            {
                py::gil_scoped_release release;
                result = acoustics::computeField("python", *evaluator, frame);
            }
            if (!result)
                fail("the field could not be computed");
            return fieldDict(*result);
        },
        py::arg("project"), py::arg("region"), py::arg("model") = "auto",
        "Complex pressure on a field region (model 'auto' follows the project, 'free' or 'reflector').");
    m.def(
        "acoustic_pressure",
        [](PyProject& p, const std::vector<std::array<double, 3>>& points, const std::string& model) {
            std::lock_guard lock(p.mutex());
            const acoustics::AcousticSettings s = acoustics::settingsOf(p.project());
            const auto evaluator = evaluatorOf(p.project(), s, model);
            py::array_t<std::complex<double>> out(std::vector<py::ssize_t>{static_cast<py::ssize_t>(points.size())});  // explicit shape: the count constructor gave stride 0
            auto* data = out.mutable_data();
            for (size_t i = 0; i < points.size(); ++i)
                data[i] = evaluator->pressure(Vec3(points[i][0], points[i][1], points[i][2]));
            return out;
        },
        py::arg("project"), py::arg("points"), py::arg("model") = "auto", "Complex pressure (Pa) at points.");
    m.def(
        "fdtd_run",
        [](PyProject& p, const std::string& region, py::object progress, py::object cancel) {
            std::lock_guard lock(p.mutex());
            const Project& project = p.project();
            const SceneObject* r = coupling::findSceneObject(project, region);
            if (!r)
                fail("no region '" + region + "'");
            const acoustics::AcousticSettings s = acoustics::settingsOf(project);
            const fdtd::FdtdSettings fs = fdtd::fdtdSettingsOf(project);
            fdtd::Prepared prepared = fdtd::prepare(project, r->id(), s.medium, fs);
            if (!prepared.plan.allowed)
                fail("the full-wave solver refuses: " + prepared.plan.refusal);
            const fdtd::FdtdPlan plan = prepared.plan;
            CallbackContext context(progress, cancel);
            Result<std::shared_ptr<const fdtd::FdtdResult>> run = Result<std::shared_ptr<const fdtd::FdtdResult>>::failure("not run");
            {
                py::gil_scoped_release release;
                run = fdtd::simulate("python", std::move(prepared), fs, s.medium, {}, &context);
            }
            if (!run)
                fail(context.cancelRequested() ? std::string("cancelled") : run.message());
            py::dict d;
            d["dimensions"] = plan.dimensions;
            d["cells"] = plan.cellCount;
            d["steps"] = plan.steps;
            d["dx"] = plan.dx;
            d["dt"] = plan.dt;
            d["assumptions"] = plan.assumptions;
            d["harmonic"] = run.value()->harmonicField ? py::object(fieldDict(*run.value()->harmonicField)) : py::none();
            return d;
        },
        py::arg("project"), py::arg("region"), py::arg("progress") = py::none(), py::arg("cancel") = py::none(),
        "Run the full-wave FDTD solver on a region (long-running; cancel() returning True stops it).");
    m.def(
        "mechanics_modal",
        [](PyProject& p, const std::string& structure, int modes) {
            std::lock_guard lock(p.mutex());
            const nlohmann::json empty = nlohmann::json::object();
            automation::ActionContext context{p.project(), nullptr, empty, {}};
            json params{{"modes", modes}};
            if (!structure.empty())
                params["structure"] = structure;
            Result<json> r = Result<json>::failure("not run");
            {
                py::gil_scoped_release release;
                r = automation::ActionRegistry::instance().run("mechanics.modal", context, params);
            }
            return toPy(unwrap(std::move(r)));
        },
        py::arg("project"), py::arg("structure") = "", py::arg("modes") = 6, "Natural frequencies (Hz) of a structure.");
    m.def(
        "em_antenna",
        [](PyProject& p, const std::string& antenna) {
            std::lock_guard lock(p.mutex());
            const nlohmann::json empty = nlohmann::json::object();
            automation::ActionContext context{p.project(), nullptr, empty, {}};
            return toPy(unwrap(automation::ActionRegistry::instance().run("em.antenna", context, {{"object", antenna}})));
        },
        py::arg("project"), py::arg("antenna"));
    m.def(
        "em_pattern",
        [](PyProject& p, const std::string& object, double resolution) {
            std::lock_guard lock(p.mutex());
            const SceneObject* o = coupling::findSceneObject(p.project(), object);
            if (!o)
                fail("no object '" + object + "'");
            const em::PatternGrid g = unwrap(em::patternOf(p.project(), o->id(), resolution));
            py::array_t<double> mag({g.thetaCount, g.phiCount});
            std::copy(g.magnitude.begin(), g.magnitude.end(), mag.mutable_data());
            py::dict d;
            d["magnitude"] = mag;  // [theta][phi], normalized to 1
            d["step_deg"] = g.stepRad * 180.0 / 3.14159265358979323846;
            d["directivity"] = g.directivity;
            d["model"] = g.model;
            return d;
        },
        py::arg("project"), py::arg("object"), py::arg("resolution_deg") = 2.0, "Normalized far-field pattern of an antenna or array.");

    // --- optimization ---------------------------------------------------------------------------------
    m.def("optimization_templates", [] {
        py::list out;
        for (const optimization::ProblemTemplate& t : optimization::AdapterRegistry::instance().templates())
            out.append(py::dict(py::arg("id") = t.id, py::arg("title") = t.title, py::arg("description") = t.description, py::arg("adapter") = t.adapterId));
        return out;
    });
    m.def(
        "optimization_problem",
        [](PyProject& p, const std::string& templateId) {
            const optimization::ProblemTemplate* t = optimization::AdapterRegistry::instance().findTemplate(templateId);
            if (!t)
                fail("no optimization template '" + templateId + "'");
            return toPy(unwrap(t->create(p.project())));
        },
        py::arg("project"), py::arg("template"), "A problem (dict) from a template, for this project.");
    m.def(
        "run_optimization",
        [](PyProject& p, py::dict problem, py::object progress, py::object cancel, bool record) {
            std::lock_guard lock(p.mutex());
            optimization::OptimizationProblem pr = unwrap(optimization::problemFromJson(fromPy(problem)));
            auto ev = unwrap(optimization::AdapterRegistry::instance().bind(pr.adapterId, optimization::designSnapshot(p.project()), pr.adapterConfig));
            CallbackContext context(progress, cancel);
            optimization::RunOptions options;
            if (!progress.is_none())
                options.onCandidate = [&context, budget = std::max(1, pr.algorithm.maxEvaluations), count = 0](const optimization::Candidate& c) mutable {
                    context.reportProgress(std::min(1.0, static_cast<double>(++count) / budget), c.id);
                };
            Result<optimization::RunRecord> run = Result<optimization::RunRecord>::failure("not run");
            {
                py::gil_scoped_release release;
                run = optimization::runOptimization(pr, ev, options, &context);
            }
            if (!run)
                fail(run.message());
            if (record) {
                optimization::OptimizationWorkspace w = optimization::workspaceOf(p.project());
                if (!w.findProblem(pr.id))
                    w.problems.push_back(pr);
                w.runs.push_back(run.value());
                optimization::storeWorkspace(p.project(), w);
            }
            return toPy(optimization::toJson(run.value()));
        },
        py::arg("project"), py::arg("problem"), py::arg("progress") = py::none(), py::arg("cancel") = py::none(), py::arg("record") = true,
        "Run an optimization problem; the design is not changed (promote a candidate for that).");
    m.def(
        "promote",
        [](PyProject& p, py::dict run, const std::string& candidateId) {
            std::lock_guard lock(p.mutex());
            optimization::RunRecord record = unwrap(optimization::runRecordFromJson(fromPy(run)));
            optimization::OptimizationProblem pr = unwrap(optimization::problemFromJson(record.problem));
            const optimization::Candidate* c = nullptr;
            for (const auto& x : record.candidates)
                if (x.id == candidateId)
                    c = &x;
            if (!c)
                fail("no candidate '" + candidateId + "' in the run");
            if (!c->evaluated())
                fail("the candidate was not evaluated: " + c->evaluation.message);
            auto ev = unwrap(optimization::AdapterRegistry::instance().bind(pr.adapterId, optimization::designSnapshot(p.project()), pr.adapterConfig));
            const auto bindings = pr.space.bindings(c->point);
            check(ev->apply(bindings, p.project()));
            optimization::OptimizationWorkspace w = optimization::workspaceOf(p.project());
            optimization::DesignRevision rev;
            rev.id = "REV-" + std::to_string(w.revisions.size() + 1);
            rev.candidateId = c->id;
            rev.runId = record.runId;
            rev.problemId = pr.id;
            rev.promotedUtc = optimization::utcNow();
            rev.model = c->evaluation.model.modelId;
            rev.fidelity = std::string(optimization::toString(c->evaluation.model.fidelity));
            rev.values = bindings;
            for (const auto& [b, v] : bindings)
                if (const auto* var = ev->capabilities().variable(b))
                    rev.previous[b] = var->initial;
            rev.parents = c->parents;
            rev.note = "promoted from Python";
            w.revisions.push_back(rev);
            optimization::storeWorkspace(p.project(), w);
        },
        py::arg("project"), py::arg("run"), py::arg("candidate"), "Write a candidate into the design, recorded as a revision.");

    // --- experiments ------------------------------------------------------------------------------------
    m.def("experiments", [](PyProject& p) { return toPy(experiments::toJson(unwrap(experiments::loadExperiments(p.project())))); });
    m.def("set_experiments", [](PyProject& p, py::dict workspace) {
        std::lock_guard lock(p.mutex());
        experiments::storeExperiments(p.project(), unwrap(experiments::experimentWorkspaceFromJson(fromPy(workspace))));
    });
    m.def("validate_experiment", [](PyProject& p, const std::string& id) {
        const auto w = unwrap(experiments::loadExperiments(p.project()));
        const experiments::Experiment* e = w.experiment(id);
        if (!e)
            fail("no experiment '" + id + "'");
        const auto report = experiments::validateExperiment(*e);
        return py::dict(py::arg("ok") = report.ok(), py::arg("errors") = report.errors, py::arg("warnings") = report.warnings);
    });

    // --- workflows -----------------------------------------------------------------------------------------
    m.def("actions", [] {
        py::list out;
        for (const auto& a : automation::ActionRegistry::instance().actions())
            out.append(py::dict(py::arg("id") = a.id, py::arg("category") = a.category, py::arg("description") = a.description,
                                py::arg("parameters") = toPy(a.parameters), py::arg("outputs") = a.outputs));
        return out;
    });
    m.def(
        "run_action",
        [](PyProject& p, const std::string& action, py::dict params, const std::string& outputDirectory) {
            std::lock_guard lock(p.mutex());
            const json outputs = json::object();
            automation::ActionContext context{p.project(), nullptr, outputs, outputDirectory};
            const json values = fromPy(params);
            Result<json> r = Result<json>::failure("not run");
            {
                py::gil_scoped_release release;
                r = automation::ActionRegistry::instance().run(action, context, values);
            }
            return toPy(unwrap(std::move(r)));
        },
        py::arg("project"), py::arg("action"), py::arg("params") = py::dict(), py::arg("output_dir") = "",
        "Run one registered action (the same operations workflows use) on the project.");
    m.def("workflow_templates", [] {
        py::list out;
        for (const auto& w : automation::WorkflowLibrary::instance().workflows())
            out.append(toPy(automation::toJson(w)));
        return out;
    });
    m.def(
        "run_workflow",
        [](PyProject& p, py::object workflow, py::dict parameters, const std::string& outputDirectory, py::object progress, py::object cancel) {
            std::lock_guard lock(p.mutex());
            automation::Workflow w;
            if (py::isinstance<py::str>(workflow)) {
                auto found = automation::WorkflowLibrary::instance().find(workflow.cast<std::string>());
                if (!found)
                    fail("no workflow '" + workflow.cast<std::string>() + "'");
                w = *found;
            } else {
                w = unwrap(automation::workflowFromJson(fromPy(workflow)));
            }
            const json values = fromPy(parameters);
            CallbackContext context(progress, cancel);
            Result<automation::WorkflowRun> run = Result<automation::WorkflowRun>::failure("not run");
            {
                py::gil_scoped_release release;
                run = automation::runWorkflow(p.project(), w, values, &context, outputDirectory);
            }
            if (!run)
                fail(run.message());
            p.project().extensions[automation::kAutomationExtensionKey] = automation::appendHistory(p.project(), run.value());
            return toPy(automation::toJson(run.value()));
        },
        py::arg("project"), py::arg("workflow"), py::arg("parameters") = py::dict(), py::arg("output_dir") = "", py::arg("progress") = py::none(),
        py::arg("cancel") = py::none(), "Run a workflow (an id or a workflow dict) on the project; returns the run record.");

#ifdef RESONANT_HAVE_AUTOMATION_API
    py::class_<PyAutomationSession>(m, "AutomationSession",
                                    "The automation API (resonant.v1) in this process, around a project: call(method, request bytes) -> "
                                    "(status, response bytes, error bytes). resonant_api.connect(project=...) uses it.")
        .def(py::init<const PyProject&>(), py::arg("project"), py::keep_alive<1, 2>())
        .def("call", &PyAutomationSession::call, py::arg("method"), py::arg("payload"), "A unary method by its full name.")
        .def("stream", &PyAutomationSession::stream, py::arg("method"), py::arg("payload"),
             "A bounded server-streaming method: (status, [item bytes], error bytes).")
        .def("methods", &PyAutomationSession::methods, "The full names of the methods.");
#endif
}
