#include "mechanics/MechanicalOptimization.h"

#include "geometry/Geometry.h"
#include "materials/Material.h"
#include "mechanics/Analyses.h"
#include "mechanics/ElasticMaterial.h"
#include "mechanics/MechanicalComponents.h"
#include "mechanics/StructuralModel.h"
#include "mechanics/Study.h"
#include "mechanics/Targeting.h"
#include "optimization/Persistence.h"
#include "optimization/Problem.h"
#include "optimization/SceneBindings.h"
#include "project/Project.h"
#include "scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace resonant::mechanics {

namespace opt = resonant::optimization;
using opt::Evaluation;
using opt::Fidelity;
using opt::Capabilities;

namespace {

constexpr const char* kAnalytical = "mechanical.analytical";
constexpr const char* kCoarse = "mechanical.fe.coarse";
constexpr const char* kFe = "mechanical.fe";
constexpr double kPi = std::numbers::pi;
using Bindings = std::map<std::string, double, std::less<>>;

template <typename T>
const T* as(const SceneObject& o)
{
    return dynamic_cast<const T*>(o.state.component.get());
}

const SceneObject* findStructure(const Scene& scene, const std::string& id)
{
    if (!id.empty()) {
        const SceneObject* o = scene.find(Id(id));
        return o && as<StructureComponent>(*o) ? o : nullptr;
    }
    const SceneObject* found = nullptr;
    scene.forEach([&](const SceneObject& o) {
        if (!found && as<StructureComponent>(o) && o.state.geometry)
            found = &o;
    });
    return found;
}

// Beta_n L of an Euler-Bernoulli beam for its end conditions.
std::vector<double> betaL(Support a, Support b, int count)
{
    const auto pair = [&](Support x, Support y) { return (a == x && b == y) || (a == y && b == x); };
    std::vector<double> out;
    for (int n = 1; n <= count; ++n) {
        double v = 0.0;
        if (pair(Support::Clamped, Support::Free)) {
            static const double k[] = {1.87510406871196, 4.69409113297418, 7.85475743823761, 10.9955407348755};
            v = n <= 4 ? k[n - 1] : (2 * n - 1) * kPi / 2.0;
        } else if (pair(Support::Pinned, Support::Pinned)) {
            v = n * kPi;
        } else if (pair(Support::Clamped, Support::Clamped) || pair(Support::Free, Support::Free)) {
            static const double k[] = {4.73004074486270, 7.85320462409584, 10.9956078380017, 14.1371654912575};
            v = n <= 4 ? k[n - 1] : (2 * n + 1) * kPi / 2.0;
        } else if (pair(Support::Clamped, Support::Pinned)) {
            static const double k[] = {3.92660231204792, 7.06858274562873, 10.2101761241668, 13.3517687777841};
            v = n <= 4 ? k[n - 1] : (4 * n + 1) * kPi / 4.0;
        } else {
            return {};
        }
        out.push_back(v);
    }
    return out;
}

}  // namespace

Result<std::vector<double>> analyticalFrequencies(const Project& project, const Id& structureId, int count)
{
    using R = Result<std::vector<double>>;
    const SceneObject* o = project.scene.find(structureId);
    const auto* sc = o ? as<StructureComponent>(*o) : nullptr;
    if (!sc || !o->state.geometry)
        return R::failure("not a structure with geometry");
    const Material* material = project.materials.find(o->state.materialId);
    if (!material)
        return R::failure("the structure has no material");
    auto em = elasticMaterialOf(*material);
    if (!em)
        return R::failure(em.message());
    const ElasticMaterial& m = em.value();
    const Geometry& g = *o->state.geometry;
    const StructureSettings& s = sc->settings;
    if (o->state.local.scale != Vec3::Ones())
        return R::failure("the analytical model needs an unscaled object");
    if (s.idealization == Idealization::Beam) {
        double length = 0.0, area = 0.0, inertiaWeak = 0.0, inertiaStrong = 0.0;
        if (g.kindId() == "box") {
            const double d[3] = {g.parameter("width"), g.parameter("depth"), g.parameter("height")};
            int axis = s.axis;
            if (axis < 0)
                axis = static_cast<int>(std::max_element(d, d + 3) - d);
            length = d[axis];
            const double b = d[(axis + 1) % 3], h = d[(axis + 2) % 3];
            area = b * h;
            inertiaWeak = std::min(b * h * h * h, h * b * b * b) / 12.0;
            inertiaStrong = std::max(b * h * h * h, h * b * b * b) / 12.0;
        } else if (g.kindId() == "cylinder") {
            if (s.axis >= 0 && s.axis != 2)
                return R::failure("a cylinder beam must lie along its own axis");
            const double r = g.parameter("radius");
            length = g.parameter("height");
            area = kPi * r * r;
            inertiaWeak = inertiaStrong = kPi * r * r * r * r / 4.0;
        } else {
            return R::failure("the analytical beam model covers box and cylinder beams only");
        }
        const auto beta = betaL(s.supports[0], s.supports[1], count);
        if (beta.empty())
            return R::failure("no closed form for these end conditions");
        // Bending in both principal planes, merged and sorted (a round section gives pairs).
        std::vector<double> f;
        for (double inertia : {inertiaWeak, inertiaStrong})
            for (double bl : beta)
                f.push_back(bl * bl / (2.0 * kPi * length * length) * std::sqrt(m.youngsModulus * inertia / (m.density * area)));
        std::sort(f.begin(), f.end());
        f.resize(static_cast<size_t>(count));
        return f;
    }
    if (s.idealization == Idealization::Plate) {
        if (g.kindId() != "box")
            return R::failure("the analytical plate model covers rectangular (box) plates only");
        for (int k = 0; k < 4; ++k)
            if (s.supports[static_cast<size_t>(k)] != Support::Pinned)
                return R::failure("the analytical plate model needs every edge simply supported (pinned)");
        const double d[3] = {g.parameter("width"), g.parameter("depth"), g.parameter("height")};
        int normal = s.axis;
        if (normal < 0)
            normal = static_cast<int>(std::min_element(d, d + 3) - d);
        const double h = s.thickness > 0.0 && g.kindId() == "plane" ? s.thickness : d[normal];
        const double a = d[(normal + 1) % 3], b = d[(normal + 2) % 3];
        const double dflex = m.plateBendingStiffness(h);
        std::vector<double> f;
        for (int i = 1; i <= count + 2; ++i)
            for (int j = 1; j <= count + 2; ++j)
                f.push_back(kPi / 2.0 * (i * i / (a * a) + j * j / (b * b)) * std::sqrt(dflex / (m.density * h)));
        std::sort(f.begin(), f.end());
        f.resize(static_cast<size_t>(count));
        return f;
    }
    return R::failure("no analytical model for a " + std::string(toString(s.idealization)) + " structure");
}

namespace {

class MechanicalAdapter final : public opt::SnapshotAdapter {
public:
    MechanicalAdapter(nlohmann::json snapshot, nlohmann::json configuration, const Project& project, std::string structureId)
        : SnapshotAdapter(std::move(snapshot), std::move(configuration)), structureId_(std::move(structureId))
    {
        modes_ = std::clamp(configuration_.value("modes", 6), 1, 60);
        describe(project);
        finishBinding();
    }
    Evaluation evaluate(const Bindings& bindings, const std::string& modelId, JobContext* context) const override;
    Result<opt::LinearSystem> linearSystem(const Bindings& bindings, const std::string& modelId) const override;

protected:
    Result<bool> applyDomainBinding(Project& project, std::string_view key, double value) const override;

private:
    void describe(const Project& project);
    double harmonicFrequency(const Project& project) const
    {
        const auto it = project.extensions.find("optimizationMechanicsFrequency");
        if (it != project.extensions.end() && it->is_number())
            return it->get<double>();
        return configuration_.value("frequency", mechanicsSettingsOf(project).targetFrequency);
    }
    std::string structureId_;
    int modes_ = 6;
};

void MechanicalAdapter::describe(const Project& project)
{
    Capabilities& c = capabilities_;
    c.adapterId = kMechanicalAdapterId;
    c.domain = "mechanical";
    c.version = "1";
    c.supportsLinearInverse = true;
    const SceneObject* s = project.scene.find(Id(structureId_));
    const MechanicsSettings ms = mechanicsSettingsOf(project);
    const auto analytical = analyticalFrequencies(project, Id(structureId_), 1);
    c.models = {{kAnalytical, "Analytical beam / plate formulas", "1", Fidelity::Analytical,
                 {"Euler-Bernoulli beam (bending only, no shear or rotary inertia) or simply supported Kirchhoff plate",
                  "bending modes of both principal planes; axial and torsional modes are not included"},
                 analytical.ok(), analytical.ok() ? "" : analytical.message(), 0.0001},
                {kCoarse, "Finite elements, coarse mesh (element size x 2)", kMechanicsSolverVersion, Fidelity::Reduced,
                 {"the study's idealization and elements with twice the element size"}, true, {}, 0.05},
                {kFe, "Finite elements (the study's mesh)", kMechanicsSolverVersion, Fidelity::HighFidelity,
                 {"the study's idealization, elements and mesh"}, true, {}, 0.2}};
    c.limitations = {"linear elasticity, small displacements; damping only as configured in the mechanics settings",
                     "hypothetical material scalings are not real materials and are labelled in every result",
                     "a free-free structure has rigid-body modes near 0 Hz among its finite-element frequencies"};
    c.constants = {{"frequency", configuration_.value("frequency", ms.targetFrequency)}};
    const std::vector<std::string> fe{kCoarse, kFe};
    const auto add = [&](std::string key, std::string label, std::string unit, std::string definition, std::vector<std::string> models) {
        c.quantities.push_back({std::move(key), std::move(label), std::move(unit), std::move(definition), {}, std::move(models), false});
    };
    for (int n = 1; n <= modes_; ++n)
        add("modal.f" + std::to_string(n), "Natural frequency " + std::to_string(n), "Hz",
            "the " + std::to_string(n) + ". lowest natural frequency of the model (undamped)", {});
    add("modal.clearance", "Distance of the nearest resonance from the frequency to avoid", "Hz", "min_i |f_i - f_avoid| over the reported modes", {});
    for (int n = 1; n <= modes_; ++n)
        add("modal.massFractionZ" + std::to_string(n), "Effective mass fraction (Z) of mode " + std::to_string(n), "",
            "effective modal mass along world Z over the total mass of the free dofs", fe);
    add("mass", "Mass", "kg", "total mass of the model (analytical: density x volume)", {});
    add("vibration.targetError", "Vibration target error", "",
        "sqrt(sum w |u_t - d_t|^2 / sum w |d_t|^2) over the vibration targets at the harmonic frequency (m when every target is a suppression)", fe);
    add("vibration.suppressionMax", "Largest response at a suppression target", "m", "max |u| over suppression targets at the harmonic frequency", fe);
    add("vibration.targetMin", "Smallest response at a target", "m", "min |u| over targets that are not suppressions", fe);
    add("vibration.sensorMax", "Largest response at a sensor", "m", "max |u| over sensors along their axis at the harmonic frequency", fe);
    add("static.maxDisplacement", "Largest static displacement", "m", "max translation under the actuator forces as static loads", fe);
    add("static.maxVonMises", "Largest von Mises stress", "Pa", "max over elements of the element von Mises stress (static)", fe);
    add("static.stressConcentration", "Stress concentration", "", "max / mean element von Mises stress (static)", fe);
    add("static.compliance", "Compliance", "J", "f^T u under the static loads", fe);
    add("actuators.effort", "Actuator effort", "N^2", "sum of the squared force amplitudes of the active actuators", {});
    project.scene.forEach([&](const SceneObject& o) {
        if (as<SensorComponent>(o) || as<VibrationTargetComponent>(o))
            add("response/" + o.id().str(), "Response at " + o.state.name, "m", "|u| along the object's axis at the harmonic frequency", fe);
    });

    using opt::DesignVariable;
    using opt::VariableKind;
    using opt::VariableRole;
    const auto variable = [&](std::string binding, std::string label, std::string unit, VariableRole role, double initial, double lo, double hi,
                              std::string group, VariableKind kind = VariableKind::Continuous) {
        DesignVariable v;
        v.id = v.binding = std::move(binding);
        v.label = std::move(label);
        v.unit = std::move(unit);
        v.role = role;
        v.kind = kind;
        v.lower = lo;
        v.upper = hi;
        v.initial = std::clamp(initial, lo, hi);
        v.group = std::move(group);
        c.variables.push_back(std::move(v));
        return &c.variables.back();
    };
    if (s) {
        const auto* sc = as<StructureComponent>(*s);
        opt::appendSceneVariables(project, *s, c.variables, true, false, PhysicsDomain::Mechanical);
        const std::string id = s->id().str();
        if (sc->settings.thickness > 0.0)
            variable(opt::objectBinding(id, "structure/thickness"), s->state.name + ": thickness", "m", VariableRole::Thickness, sc->settings.thickness,
                     0.25 * sc->settings.thickness, 4.0 * sc->settings.thickness, s->state.name);
        const int sides = sc->settings.idealization == Idealization::Beam ? 2 : sc->settings.idealization == Idealization::Solid ? 6 : 4;
        for (int k = 0; k < sides; ++k) {
            DesignVariable* v = variable(opt::objectBinding(id, "structure/support/" + std::to_string(k)), s->state.name + ": support " + std::to_string(k),
                                         "", VariableRole::Configuration, static_cast<double>(sc->settings.supports[static_cast<size_t>(k)]), 0.0, 2.0,
                                         s->state.name, VariableKind::Categorical);
            v->choices = {"free", "pinned", "clamped"};
            v->choiceLabels = {"Free", "Pinned", "Clamped"};
        }
        variable(opt::objectBinding(id, "material/youngsModulusScale"), s->state.name + ": Young's modulus scale (HYPOTHETICAL material)", "",
                 VariableRole::MaterialParameter, 1.0, 0.5, 2.0, s->state.name);
        variable(opt::objectBinding(id, "material/densityScale"), s->state.name + ": density scale (HYPOTHETICAL material)", "",
                 VariableRole::MaterialParameter, 1.0, 0.5, 2.0, s->state.name);
    }
    variable("mechanics/damping/eta", "Loss factor (elements without their own)", "", VariableRole::MaterialParameter,
             ms.damping.eta > 0.0 ? ms.damping.eta : 0.01, 1e-4, 0.3, "Damping");
    const double f = configuration_.value("frequency", ms.targetFrequency);
    variable("mechanics/frequency", "Harmonic frequency", "Hz", VariableRole::Frequency, f, 0.5 * f, 2.0 * f, "Analysis");
    project.scene.forEach([&](const SceneObject& o) {
        const auto* a = as<ActuatorComponent>(o);
        if (!a)
            return;
        const std::string id = o.id().str();
        const std::string& name = o.state.name;
        variable(opt::objectBinding(id, "actuator/amplitude"), name + ": force amplitude", "N", VariableRole::Amplitude, a->settings.amplitude, 0.0,
                 std::max(10.0 * a->settings.amplitude, 1.0), name);
        variable(opt::objectBinding(id, "actuator/phase"), name + ": phase", "rad", VariableRole::Phase, a->settings.phase, -kPi, kPi, name);
        variable(opt::objectBinding(id, "actuator/delay"), name + ": delay", "s", VariableRole::Delay, a->settings.delay, 0.0, 10.0 / std::max(f, 1.0), name);
        variable(opt::objectBinding(id, "actuator/active"), name + ": active (0/1)", "", VariableRole::Activation, a->settings.active ? 1.0 : 0.0, 0.0, 1.0,
                 name, VariableKind::Integer);
        opt::appendSceneVariables(project, o, c.variables, false, true, std::nullopt);
    });
}

Result<bool> MechanicalAdapter::applyDomainBinding(Project& project, std::string_view key, double value) const
{
    if (key == "mechanics/damping/eta") {
        MechanicsSettings s = mechanicsSettingsOf(project);
        s.damping.kind = DampingKind::LossFactor;
        s.damping.eta = value;
        if (Status v = s.validate(); !v)
            return Result<bool>::failure(v.message());
        storeMechanicsSettings(project, s);
        return true;
    }
    if (key == "mechanics/frequency") {
        if (!(value > 0.0))
            return Result<bool>::failure("the harmonic frequency must be positive");
        project.extensions["optimizationMechanicsFrequency"] = value;
        return true;
    }
    if (key.rfind("object/", 0) != 0)
        return false;
    const std::string_view rest = key.substr(7);
    const size_t slash = rest.find('/');
    if (slash == std::string_view::npos)
        return false;
    const Id id{std::string(rest.substr(0, slash))};
    const std::string what(rest.substr(slash + 1));
    SceneObject* object = project.scene.find(id);
    if (!object)
        return Result<bool>::failure("the design refers to an object that is not in the project");
    ObjectState state = object->state;
    if (what.rfind("structure/", 0) == 0) {
        const auto* sc = as<StructureComponent>(*object);
        if (!sc)
            return false;
        auto copy = std::make_shared<StructureComponent>(*sc);
        if (what == "structure/thickness")
            copy->settings.thickness = value;
        else if (what.rfind("structure/support/", 0) == 0) {
            const int k = std::stoi(what.substr(18));
            const long choice = std::lround(value);
            if (k < 0 || k > 5 || choice < 0 || choice > 2)
                return Result<bool>::failure("bad support choice");
            copy->settings.supports[static_cast<size_t>(k)] = static_cast<Support>(choice);
        } else
            return false;
        if (Status v = copy->settings.validate(); !v)
            return Result<bool>::failure(v.message());
        state.component = copy;
    } else if (what == "material/youngsModulusScale" || what == "material/densityScale") {
        const Material* m = project.materials.find(object->state.materialId);
        if (!m)
            return Result<bool>::failure("'" + object->state.name + "' has no material to scale");
        if (std::abs(value - 1.0) < 1e-15)
            return true;
        const bool youngs = what == "material/youngsModulusScale";
        const char* property = youngs ? "youngsModulus" : "density";
        const PropertyValue* p = m->property(PhysicsDomain::Mechanical, property);
        if (!p)
            return Result<bool>::failure("the material has no " + std::string(property));
        Material hypothetical = *m;
        // A new id per scaled material, so the original is left as it is.
        const Id newId(m->id().str() + (youngs ? "~E" : "~rho"));
        hypothetical = Material(newId, m->name() + " (HYPOTHETICAL: " + property + " x " + std::to_string(value) + ")");
        for (PhysicsDomain d : m->domains())
            if (const PropertySet* set = m->properties(d))
                for (const auto& [k, v] : *set)
                    [[maybe_unused]] Status st = hypothetical.setProperty(d, k, v);
        PropertyValue scaled = *p;
        scaled.value *= value;
        scaled.provenance = Provenance::Theoretical;
        scaled.source = "HYPOTHETICAL: an optimization variable scaled the " + std::string(property) +
                        " of '" + m->name() + "'; this is not a catalogue material";
        if (Status st = hypothetical.setProperty(PhysicsDomain::Mechanical, property, scaled); !st)
            return Result<bool>::failure(st.message());
        hypothetical.setDescription("hypothetical material made by an optimization variable");
        if (project.materials.find(newId))
            [[maybe_unused]] Status st = project.materials.update(hypothetical);
        else if (Status st = project.materials.add(hypothetical); !st)
            return Result<bool>::failure(st.message());
        state.materialId = newId;
    } else if (what.rfind("actuator/", 0) == 0) {
        const auto* a = as<ActuatorComponent>(*object);
        if (!a)
            return false;
        auto copy = std::make_shared<ActuatorComponent>(*a);
        const std::string f = what.substr(9);
        if (f == "amplitude")
            copy->settings.amplitude = value;
        else if (f == "phase")
            copy->settings.phase = value;
        else if (f == "delay")
            copy->settings.delay = value;
        else if (f == "active")
            copy->settings.active = std::lround(value) != 0;
        else
            return false;
        if (Status v = copy->settings.validate(); !v)
            return Result<bool>::failure(v.message());
        state.component = copy;
    } else {
        return false;
    }
    if (Status s = project.scene.setState(id, std::move(state)); !s)
        return Result<bool>::failure(s.message());
    return true;
}

Evaluation MechanicalAdapter::evaluate(const Bindings& bindings, const std::string& modelId, JobContext* context) const
{
    Evaluation e;
    const opt::ModelInfo* info = capabilities_.model(modelId);
    if (!info) {
        e.status = Evaluation::Status::Refused;
        e.message = "unknown model '" + modelId + "'";
        return e;
    }
    e.model.modelId = info->id;
    e.model.modelName = info->name;
    e.model.version = info->version;
    e.model.fidelity = info->fidelity;
    e.model.assumptions = info->assumptions;
    auto built = build(bindings);
    if (!built) {
        e.status = Evaluation::Status::InvalidDesign;
        e.message = built.message();
        return e;
    }
    Project& project = *built.value();
    MechanicsSettings settings = mechanicsSettingsOf(project);
    const double fh = harmonicFrequency(project);
    const double avoid = configuration_.value("avoidFrequency", fh);
    if (const auto it = bindings.find("memoryBudgetBytes"); it != bindings.end() && it->second > 0.0)
        settings.memoryLimitBytes = static_cast<size_t>(it->second);
    for (const Material& m : project.materials.materials())
        if (m.description() == "hypothetical material made by an optimization variable")
            e.notes.push_back("HYPOTHETICAL material '" + m.name() + "': not a real, available material");
    const SceneObject* structure = project.scene.find(Id(structureId_));
    if (!structure) {
        e.status = Evaluation::Status::InvalidDesign;
        e.message = "the structure is missing";
        return e;
    }
    const auto modalQuantities = [&](const std::vector<double>& f) {
        for (size_t n = 0; n < f.size() && n < static_cast<size_t>(modes_); ++n)
            e.quantities["modal.f" + std::to_string(n + 1)] = f[n];
        double clearance = std::numeric_limits<double>::infinity();
        for (double v : f)
            clearance = std::min(clearance, std::abs(v - avoid));
        if (std::isfinite(clearance))
            e.quantities["modal.clearance"] = clearance;
    };
    double effort = 0.0;
    project.scene.forEach([&](const SceneObject& o) {
        if (const auto* a = as<ActuatorComponent>(o); a && a->settings.active)
            effort += a->settings.amplitude * a->settings.amplitude;
    });
    e.quantities["actuators.effort"] = effort;

    if (modelId == kAnalytical) {
        auto f = analyticalFrequencies(project, Id(structureId_), modes_);
        if (!f) {
            e.status = Evaluation::Status::Refused;
            e.message = f.message();
            return e;
        }
        modalQuantities(f.value());
        // Mass: density x volume of the solid.
        const Material* m = project.materials.find(structure->state.materialId);
        if (auto em = m ? elasticMaterialOf(*m) : Result<ElasticMaterial>::failure("no material")) {
            const Geometry& g = *structure->state.geometry;
            double volume = 0.0;
            if (g.kindId() == "box")
                volume = g.parameter("width") * g.parameter("depth") * g.parameter("height");
            else if (g.kindId() == "cylinder")
                volume = kPi * std::pow(g.parameter("radius"), 2) * g.parameter("height");
            if (volume > 0.0)
                e.quantities["mass"] = em.value().density * volume;
        }
        e.model.settings = {{"modes", modes_}};
        return e;
    }

    // Finite elements.
    Project* target = &project;
    if (modelId == kCoarse) {
        const auto* sc = as<StructureComponent>(*structure);
        auto probe = prepareStudy(project, structure->id(), settings.memoryLimitBytes);
        if (!probe.allowed) {
            e.status = Evaluation::Status::Refused;
            e.message = probe.refusal;
            return e;
        }
        auto copy = std::make_shared<StructureComponent>(*sc);
        copy->settings.elementSize = 2.0 * probe.elementSize;
        ObjectState st = structure->state;
        st.component = copy;
        [[maybe_unused]] Status s = project.scene.setState(structure->id(), std::move(st));
    }
    auto study = std::make_shared<StructureStudy>(prepareStudy(*target, structure->id(), settings.memoryLimitBytes));
    if (!study->allowed) {
        e.status = Evaluation::Status::Refused;
        e.message = study->refusal;
        return e;
    }
    for (const std::string& w : study->warnings)
        e.warnings.push_back(w);
    e.model.settings = {{"elementSize", study->elementSize}, {"grid", study->grid}, {"freeDofs", study->freeDofs}, {"damping", settings.damping.describe()},
                        {"harmonicFrequencyHz", fh}, {"modes", modes_}};
    e.model.assumptions.insert(e.model.assumptions.end(), study->assumptions.begin(), study->assumptions.end());
    const StructuralModel& model = *study->model;
    const SystemMatrices system = assemble(model);
    e.quantities["mass"] = model.totalMass();

    std::vector<std::string> analyses = configuration_.value("analyses", std::vector<std::string>());
    if (analyses.empty() || std::find(analyses.begin(), analyses.end(), "modal") != analyses.end()) {
        auto modal = solveModal(model, system, std::min<int>(modes_, static_cast<int>(system.map.freeCount()) - 1), context);
        if (!modal) {
            const bool cancelled = context && context->cancelRequested();
            e.status = cancelled ? Evaluation::Status::Cancelled : Evaluation::Status::Failed;
            e.message = modal.message();
            return e;
        }
        std::vector<double> f;
        for (size_t n = 0; n < modal.value().modes.size(); ++n) {
            const Mode& mode = modal.value().modes[n];
            f.push_back(mode.frequencyHz);
            e.quantities["modal.massFractionZ" + std::to_string(n + 1)] = mode.effectiveMassFraction.z();
        }
        modalQuantities(f);
    }
    const bool hasForces = !study->forces.empty();
    const auto wants = [&](const char* a) {
        return analyses.empty() ? hasForces && std::string(a) != "modal" : std::find(analyses.begin(), analyses.end(), a) != analyses.end();
    };
    if (wants("harmonic") && hasForces) {
        std::vector<ResponsePoint> points;
        std::vector<int> kind;  // 0 sensor, 1 target
        std::vector<const TargetPoint*> targetOf;
        for (const ResponsePoint& p : study->sensors) {
            points.push_back(p);
            kind.push_back(0);
            targetOf.push_back(nullptr);
        }
        for (const TargetPoint& t : study->targets) {
            points.push_back(t.point);
            kind.push_back(1);
            targetOf.push_back(&t);
        }
        if (!points.empty()) {
            HarmonicSettings hs;
            hs.frequenciesHz = {fh};
            hs.damping = settings.damping;
            hs.method = HarmonicMethod::Direct;
            hs.keepFields = false;
            auto h = solveHarmonic(model, system, study->forces, points, hs, context);
            if (!h) {
                e.status = context && context->cancelRequested() ? Evaluation::Status::Cancelled : Evaluation::Status::Failed;
                e.message = h.message();
                return e;
            }
            double num = 0.0, den = 0.0, wsum = 0.0, suppression = 0.0, sensorMax = 0.0, targetMin = std::numeric_limits<double>::infinity();
            bool anySuppress = false, anyTarget = false;
            for (size_t i = 0; i < points.size(); ++i) {
                const Complex u = h.value().responses[i][0];
                e.quantities["response/" + points[i].objectId.str()] = std::abs(u);
                if (kind[i] == 0) {
                    sensorMax = std::max(sensorMax, std::abs(u));
                    continue;
                }
                const TargetPoint& t = *targetOf[i];
                const Complex d = t.suppress ? Complex(0.0, 0.0) : t.desired;
                num += t.weight * std::norm(u - d);
                den += t.weight * std::norm(d);
                wsum += t.weight;
                if (t.suppress) {
                    anySuppress = true;
                    suppression = std::max(suppression, std::abs(u));
                } else {
                    anyTarget = true;
                    targetMin = std::min(targetMin, std::abs(u));
                }
            }
            if (!study->targets.empty())
                e.quantities["vibration.targetError"] = den > 0.0 ? std::sqrt(num / den) : std::sqrt(num / std::max(wsum, 1e-300));
            if (anySuppress)
                e.quantities["vibration.suppressionMax"] = suppression;
            if (anyTarget)
                e.quantities["vibration.targetMin"] = targetMin;
            if (!study->sensors.empty())
                e.quantities["vibration.sensorMax"] = sensorMax;
        }
    }
    if (wants("static") && hasForces && hasStress(model.kind)) {
        auto st = solveStatic(model, system, study->forces);
        if (st) {
            e.quantities["static.maxDisplacement"] = st.value().maxTranslation;
            e.quantities["static.compliance"] = st.value().compliance;
            double maxVm = 0.0, sum = 0.0;
            size_t count = 0;
            for (const Element& el : model.elements) {
                const ElementStress s = elementStress(model, el, st.value().displacement);
                if (!s.available)
                    continue;
                maxVm = std::max(maxVm, s.vonMises);
                sum += s.vonMises;
                ++count;
            }
            if (count) {
                e.quantities["static.maxVonMises"] = maxVm;
                if (sum > 0.0)
                    e.quantities["static.stressConcentration"] = maxVm / (sum / count);
            }
        } else {
            e.warnings.push_back("static analysis: " + st.message());
        }
    }
    return e;
}

Result<opt::LinearSystem> MechanicalAdapter::linearSystem(const Bindings& bindings, const std::string& modelId) const
{
    using R = Result<opt::LinearSystem>;
    if (modelId == kAnalytical)
        return R::failure("the analytical model has no forced response");
    auto built = build(bindings);
    if (!built)
        return R::failure(built.message());
    Project& project = *built.value();
    const MechanicsSettings settings = mechanicsSettingsOf(project);
    auto study = prepareStudy(project, Id(structureId_), settings.memoryLimitBytes);
    if (!study.allowed)
        return R::failure(study.refusal);
    if (study.forces.empty() || study.targets.empty())
        return R::failure("the structure needs actuators and vibration targets for inverse design");
    std::vector<TargetRequest> requests;
    for (const TargetPoint& t : study.targets)
        requests.push_back({t.label, t.point, t.suppress ? Complex(0.0, 0.0) : t.desired, t.weight});
    const double fh = harmonicFrequency(project);
    auto solved = solveTargets(*study.model, assemble(*study.model), study.forces, requests, fh, settings.damping, 0.0);
    if (!solved)
        return R::failure(solved.message());
    opt::LinearSystem sys;
    sys.response = solved.value().transfer;
    const Eigen::Index m = sys.response.rows(), n = sys.response.cols();
    sys.desired.resize(m);
    sys.rowWeights.resize(m);
    for (Eigen::Index i = 0; i < m; ++i) {
        sys.desired(i) = requests[static_cast<size_t>(i)].desired;
        sys.rowWeights(i) = requests[static_cast<size_t>(i)].weight;
        sys.targetLabels.push_back(requests[static_cast<size_t>(i)].label);
    }
    sys.current.resize(n);
    const double omega = 2.0 * kPi * fh;
    for (Eigen::Index j = 0; j < n; ++j) {
        const ForceInput& fi = study.forces[static_cast<size_t>(j)];
        sys.current(j) = fi.phasor(omega);
        sys.sourceLabels.push_back(fi.label);
        const std::string id = fi.objectId.str();
        sys.amplitudeBindings.push_back(opt::objectBinding(id, "actuator/amplitude"));
        sys.phaseBindings.push_back(opt::objectBinding(id, "actuator/phase"));
        sys.zeroBindings.push_back(opt::objectBinding(id, "actuator/delay"));
    }
    sys.weightUnit = "N";
    sys.targetUnit = "m";
    sys.model = std::string(modelId == kCoarse ? "finite elements, coarse" : "finite elements") + " at " + std::to_string(fh) + " Hz";
    return sys;
}

opt::Objective objective(std::string id, std::string label, std::string quantity, opt::Sense sense, double scale, double target = 0.0)
{
    opt::Objective o;
    o.id = std::move(id);
    o.label = std::move(label);
    o.quantity = std::move(quantity);
    o.sense = sense;
    o.scale = scale;
    o.target = target;
    return o;
}

}  // namespace

Result<std::shared_ptr<const opt::DesignEvaluator>> bindMechanicalAdapter(const nlohmann::json& snapshot, const nlohmann::json& configuration)
{
    using R = Result<std::shared_ptr<const opt::DesignEvaluator>>;
    auto project = opt::projectFromSnapshot(snapshot);
    if (!project)
        return R::failure("the design snapshot could not be read: " + project.message());
    if (!configuration.is_object())
        return R::failure("the mechanical adapter's configuration must be an object");
    const SceneObject* s = findStructure(project.value()->scene, configuration.value("structure", std::string()));
    if (!s)
        return R::failure("the project has no structure object" +
                          (configuration.contains("structure") ? " with the id '" + configuration.value("structure", std::string()) + "'" : std::string()));
    return std::shared_ptr<const opt::DesignEvaluator>(std::make_shared<MechanicalAdapter>(snapshot, configuration, *project.value(), s->id().str()));
}

void registerMechanicalOptimization(opt::AdapterRegistry& registry)
{
    registry.add(kMechanicalAdapterId, "Mechanical structures (analytical, finite elements)", bindMechanicalAdapter);
    const auto start = [](const Project& project, const std::string& templateId, const std::string& name)
        -> Result<std::pair<opt::OptimizationProblem, std::shared_ptr<const opt::DesignEvaluator>>> {
        using R = Result<std::pair<opt::OptimizationProblem, std::shared_ptr<const opt::DesignEvaluator>>>;
        const SceneObject* s = findStructure(project.scene, {});
        if (!s)
            return R::failure("the project has no structure object");
        nlohmann::json configuration{{"structure", s->id().str()}};
        auto bound = bindMechanicalAdapter(opt::designSnapshot(project), configuration);
        if (!bound)
            return R::failure(bound.message());
        opt::OptimizationProblem p;
        p.id = templateId + "-" + Id::generate().str().substr(0, 8);
        p.name = name;
        p.templateId = templateId;
        p.adapterId = kMechanicalAdapterId;
        p.adapterConfig = configuration;
        p.modelId = kFe;
        return std::make_pair(p, bound.take());
    };
    registry.addTemplate({"mechanical.tune-frequency", "Mechanical: tune the first natural frequency",
                          "Change the structure's first geometry parameter so that its first natural frequency meets a target (the present "
                          "frequency x 1.2 to start with), screening with the analytical model where one exists.",
                          kMechanicalAdapterId, [start](const Project& project) -> Result<nlohmann::json> {
                              auto t = start(project, "mechanical.tune-frequency", "Tune the first natural frequency");
                              if (!t)
                                  return Result<nlohmann::json>::failure(t.message());
                              auto& [p, ev] = t.value();
                              const opt::Capabilities& caps = ev->capabilities();
                              bool first = true;
                              for (const opt::DesignVariable& v : caps.variables)
                                  if (v.key().find("/geometry/") != std::string::npos) {
                                      opt::DesignVariable copy = v;
                                      copy.locked = !first;
                                      first = false;
                                      p.space.variables.push_back(copy);
                                  }
                              if (p.space.variables.empty())
                                  return Result<nlohmann::json>::failure("the structure has no geometry parameter");
                              const Evaluation e = ev->evaluate({}, kFe);
                              const double f1 = e.ok() && e.quantities.contains("modal.f1") ? e.quantities.at("modal.f1") : 100.0;
                              p.objectives.push_back(objective("f1", "First natural frequency", "modal.f1", opt::Sense::Target, std::max(1.0, 0.01 * f1), 1.2 * f1));
                              if (caps.model(kAnalytical) && caps.model(kAnalytical)->available)
                                  p.modelId = kAnalytical;
                              p.validationModelId = kFe;
                              p.algorithm.algorithm = "nelder-mead";
                              p.algorithm.maxEvaluations = 80;
                              return opt::toJson(p);
                          }});
    registry.addTemplate({"mechanical.suppress-vibration", "Mechanical: suppress vibration at the targets (actuator forces)",
                          "Choose the amplitude and phase of every actuator to bring the response at the vibration targets to what they ask "
                          "for (zero at suppression targets) at the harmonic frequency; the linear inverse solution exists for the same problem.",
                          kMechanicalAdapterId, [start](const Project& project) -> Result<nlohmann::json> {
                              auto t = start(project, "mechanical.suppress-vibration", "Suppress vibration at the targets");
                              if (!t)
                                  return Result<nlohmann::json>::failure(t.message());
                              auto& [p, ev] = t.value();
                              for (const opt::DesignVariable& v : ev->capabilities().variables) {
                                  const std::string& k = v.key();
                                  if (k.find("/actuator/amplitude") != std::string::npos || k.find("/actuator/phase") != std::string::npos)
                                      p.space.variables.push_back(v);
                              }
                              if (p.space.variables.empty())
                                  return Result<nlohmann::json>::failure("the project has no actuator");
                              p.objectives.push_back(objective("error", "Vibration target error", "vibration.targetError", opt::Sense::Minimize, 1.0));
                              p.adapterConfig["analyses"] = {"harmonic"};
                              p.algorithm.algorithm = "nelder-mead";
                              p.algorithm.maxEvaluations = 600;
                              return opt::toJson(p);
                          }});
}

}  // namespace resonant::mechanics
