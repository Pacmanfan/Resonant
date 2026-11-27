#include "acoustic_optimization/AcousticOptimization.h"

#include "acoustics/Components.h"
#include "acoustics/FieldEvaluator.h"
#include "acoustics/FieldGrid.h"
#include "acoustics/Medium.h"
#include "acoustics/Propagation.h"
#include "acoustics/Settings.h"
#include "fdtd/FdtdSetup.h"
#include "foundation/Transform.h"
#include "foundation/Units.h"
#include "geometry/Geometry.h"
#include "manipulators/Manipulator.h"
#include "manipulators/ReflectorModel.h"
#include "manipulators/ReflectorSurface.h"
#include "optimization/Persistence.h"
#include "optimization/Problem.h"
#include "optimization/SceneBindings.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "scene/Scene.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numbers>

namespace resonant::acoustics {

namespace opt = resonant::optimization;
using opt::Evaluation;
using opt::Fidelity;
using opt::Capabilities;

namespace {

constexpr const char* kFree = "acoustic.freefield.analytical";
constexpr const char* kReflector = "acoustic.reflector.kirchhoff";
constexpr const char* kFdtd = "acoustic.fdtd.staggered";
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kPi = std::numbers::pi;

using Bindings = std::map<std::string, double, std::less<>>;

struct SampledTarget {
    std::string label;
    Vec3 position = Vec3::Zero();
    Complex desired = Complex(0.0, 0.0);
    double weight = 1.0;
    bool suppress = false;
    std::string objectId;
};

struct TargetSet {
    std::vector<SampledTarget> points;
    std::optional<Vec3> focus;
    std::string focusLabel;
    std::vector<std::string> notes;
};

// Point targets as they are; line, plane, surface and volume targets sampled
// at `spacing` in their local frame (at most 400 samples each), each sample
// carrying its share of the target's weight.
TargetSet expandTargets(const Scene& scene, double spacing, const std::string& focusId)
{
    TargetSet set;
    std::optional<Vec3> firstFocus;
    std::string firstLabel;
    scene.forEach([&](const SceneObject& object) {
        const auto* t = componentOf<TargetComponent>(object);
        if (!t || !scene.isEffectivelyVisible(object))
            return;
        const TargetSettings& s = t->settings;
        const Mat4 world = scene.worldMatrix(object);
        const Complex desired = s.suppress ? Complex(0.0, 0.0) : std::polar(s.desiredAmplitudePa, s.desiredPhaseRad);
        std::array<int, 3> n{1, 1, 1};
        const Vec3 extent = s.extent.cwiseAbs();
        double h = spacing;
        const auto counts = [&](double step) {
            std::array<int, 3> c{1, 1, 1};
            if (s.kind == TargetKind::Line)
                c[0] = std::max(2, static_cast<int>(std::ceil(extent.x() / step)) + 1);
            else if (s.kind == TargetKind::Plane || s.kind == TargetKind::Surface) {
                c[0] = std::max(2, static_cast<int>(std::ceil(extent.x() / step)) + 1);
                c[1] = std::max(2, static_cast<int>(std::ceil(extent.y() / step)) + 1);
            } else if (s.kind == TargetKind::Volume) {
                for (int a = 0; a < 3; ++a)
                    c[a] = std::max(2, static_cast<int>(std::ceil(extent[a] / step)) + 1);
            }
            return c;
        };
        n = counts(h);
        while (static_cast<long>(n[0]) * n[1] * n[2] > 400) {
            h *= 1.25;
            n = counts(h);
        }
        if (s.kind != TargetKind::Point && extent.maxCoeff() <= 0.0) {
            set.notes.push_back("target '" + object.state.name + "' is a " + std::string(toString(s.kind)) +
                                " target without an extent; it is treated as a point");
            n = {1, 1, 1};
        }
        const long count = static_cast<long>(n[0]) * n[1] * n[2];
        for (int i = 0; i < n[0]; ++i)
            for (int j = 0; j < n[1]; ++j)
                for (int k = 0; k < n[2]; ++k) {
                    Vec3 local = Vec3::Zero();
                    const int idx[3] = {i, j, k};
                    for (int a = 0; a < 3; ++a)
                        if (n[a] > 1)
                            local[a] = -0.5 * extent[a] + extent[a] * idx[a] / (n[a] - 1);
                    SampledTarget p;
                    p.label = count == 1 ? object.state.name : object.state.name + " #" + std::to_string(set.points.size());
                    p.position = transformPoint(world, local);
                    p.desired = desired;
                    p.weight = s.weight / static_cast<double>(count);
                    p.suppress = s.suppress;
                    p.objectId = object.id().str();
                    set.points.push_back(p);
                }
        if (!s.suppress) {
            const Vec3 centre = transformPoint(world, Vec3::Zero());
            if (object.id().str() == focusId) {
                set.focus = centre;
                set.focusLabel = object.state.name;
            }
            if (!firstFocus) {
                firstFocus = centre;
                firstLabel = object.state.name;
            }
        }
    });
    if (!set.focus && focusId.empty() && firstFocus) {
        set.focus = firstFocus;
        set.focusLabel = firstLabel;
    }
    return set;
}

// Trilinear sample of a field on a grid frame; nullopt outside it.
std::optional<Complex> sampleGrid(const FieldResult& field, const Vec3& point)
{
    const GridFrame& f = field.frame;
    const Vec3 d = point - f.origin;
    double c[3];
    const Vec3 axes[3] = {f.axisU, f.axisV, f.axisW};
    const int n[3] = {f.nu, f.nv, f.nw};
    for (int a = 0; a < 3; ++a) {
        if (n[a] <= 1) {
            c[a] = 0.0;
            continue;
        }
        const double len2 = axes[a].squaredNorm();
        c[a] = len2 > 0.0 ? d.dot(axes[a]) / len2 * (n[a] - 1) : 0.0;
        if (c[a] < -1e-9 || c[a] > n[a] - 1 + 1e-9)
            return std::nullopt;
        c[a] = std::clamp(c[a], 0.0, static_cast<double>(n[a] - 1));
    }
    // A point off the plane of a 2D grid.
    if (f.nw <= 1) {
        const Vec3 normal = f.axisU.cross(f.axisV);
        if (normal.norm() > 0.0 && std::abs(d.dot(normal.normalized())) > 0.5 * f.maxSpacing())
            return std::nullopt;
    }
    int i0[3];
    double t[3];
    for (int a = 0; a < 3; ++a) {
        i0[a] = std::min(static_cast<int>(std::floor(c[a])), std::max(0, n[a] - 2));
        t[a] = n[a] > 1 ? c[a] - i0[a] : 0.0;
        if (n[a] <= 1)
            i0[a] = 0;
    }
    Complex v(0.0, 0.0);
    for (int du = 0; du <= (n[0] > 1 ? 1 : 0); ++du)
        for (int dv = 0; dv <= (n[1] > 1 ? 1 : 0); ++dv)
            for (int dw = 0; dw <= (n[2] > 1 ? 1 : 0); ++dw) {
                const double w = (n[0] > 1 ? (du ? t[0] : 1.0 - t[0]) : 1.0) * (n[1] > 1 ? (dv ? t[1] : 1.0 - t[1]) : 1.0) *
                                 (n[2] > 1 ? (dw ? t[2] : 1.0 - t[2]) : 1.0);
                const std::complex<float> s = field.pressure[f.index(i0[0] + du, i0[1] + dv, i0[2] + dw)];
                v += w * Complex(s.real(), s.imag());
            }
    return v;
}

double db(double ratio) { return ratio > 0.0 ? 20.0 * std::log10(ratio) : -200.0; }

const char* elementField(const std::string& f)
{
    static const char* fields[] = {"amplitude", "phase", "delay", "active"};
    for (const char* x : fields)
        if (f == x)
            return x;
    return nullptr;
}

class AcousticAdapter final : public opt::SnapshotAdapter {
public:
    AcousticAdapter(nlohmann::json snapshot, nlohmann::json configuration, const Project& project)
        : SnapshotAdapter(std::move(snapshot), std::move(configuration))
    {
        focusId_ = configuration_.value("focus", std::string());
        regionId_ = configuration_.value("region", std::string());
        describe(project);
        finishBinding();
    }

    Evaluation evaluate(const Bindings& bindings, const std::string& modelId, JobContext* context) const override;
    Result<opt::LinearSystem> linearSystem(const Bindings& bindings, const std::string& modelId) const override;

protected:
    Result<bool> applyDomainBinding(Project& project, std::string_view key, double value) const override;

private:
    void describe(const Project& project);
    std::string focusId_, regionId_;
};

void AcousticAdapter::describe(const Project& project)
{
    Capabilities& c = capabilities_;
    c.adapterId = kAcousticAdapterId;
    c.domain = "acoustic";
    c.version = "1";
    c.supportsLinearInverse = true;
    const AcousticSettings s = settingsOf(project);
    const double lambda = s.medium.wavelength(s.frequencyHz);
    c.constants = {{"wavelength", lambda}, {"frequency", s.frequencyHz}, {"soundSpeed", s.medium.soundSpeed}};

    const bool hasRegion = !regionId_.empty() && project.scene.find(Id(regionId_)) &&
                           componentOf<RegionComponent>(*project.scene.find(Id(regionId_)));
    opt::ModelInfo free{kFree, "Analytical free field (point-source superposition)", "1", Fidelity::Analytical,
                        FreeFieldModel(s.propagation).assumptions(), true, {}, 0.001};
    opt::ModelInfo reflector{kReflector, "Free field + reflectors (Kirchhoff approximation)", "1", Fidelity::Reduced,
                             {"one Kirchhoff (physical-optics) reflection per reflector; no multiple reflections, no shadowing",
                              "degrades when surfaces or their radii of curvature are not large against the wavelength"},
                             true, {}, 0.05};
    opt::ModelInfo fdtd{kFdtd, "Full-wave FDTD (staggered grid)", fdtd::kFdtdSolverVersion, Fidelity::HighFidelity,
                        {"the linear acoustic equations on a uniform grid; solids are represented as fluids",
                         "a plane region is a two-dimensional problem (line sources), not a slice of the 3D field"},
                        hasRegion, hasRegion ? "" : "configure a field region (adapter option 'region') to evaluate with the full-wave solver",
                        10.0};
    c.models = {free, reflector, fdtd};
    c.limitations = {"lens and waveguide manipulators have no analytical field model: only the full-wave model sees them",
                     "the free-field model ignores every object of the scene",
                     "quantities on the region and the focal line are sampled; their resolution is part of their definition"};

    const auto add = [&](std::string key, std::string label, std::string unit, std::string definition, std::vector<std::string> models = {},
                         bool linear = false) {
        c.quantities.push_back({std::move(key), std::move(label), std::move(unit), std::move(definition), {}, std::move(models), linear});
    };
    add("focus.pressure", "Pressure magnitude at the focus", "Pa", "|p(x_f)|, complex peak pressure at the focus target", {}, true);
    add("focus.pressureDb", "Pressure level at the focus", "dB", "20 log10(|p(x_f)| / 1 Pa), dB re 1 Pa (peak)");
    add("focus.phase", "Pressure phase at the focus", "rad", "arg p(x_f)");
    add("focus.gain", "Focal gain over the free field", "", "|p(x_f)| / |p_free(x_f)|: the field of the model over the free field of the same sources");
    add("targets.error", "Target field error", "",
        "sqrt(sum q_m |p_m - d_m|^2 / sum q_m |d_m|^2) over every target sample (Pa when every target is a suppression: sqrt(sum q_m |p_m|^2 / sum q_m))");
    add("targets.magnitudeError", "Target magnitude error", "", "sqrt(sum q_m (|p_m| - |d_m|)^2 / sum q_m |d_m|^2), phase ignored");
    add("targets.phaseErrorRms", "Target phase error (rms)", "rad", "rms of the wrapped phase difference arg p_m - arg d_m over targets that are not suppressions");
    add("targets.minPressure", "Smallest pressure at a focal target", "Pa", "min |p_m| over target samples that are not suppressions");
    add("suppression.maxPressure", "Largest pressure at a suppression target", "Pa", "max |p_m| over suppression target samples");
    const std::vector<std::string> regionModels{kFree, kReflector, kFdtd};
    add("focus.contrastDb", "Focal-to-background contrast", "dB",
        "10 log10(|p(x_f)|^2 / mean |p|^2 over region samples farther than the focal radius from the focus)");
    add("focus.energyOutside", "Energy outside the focal radius", "",
        "sum |p|^2 over region samples farther than the focal radius from the focus / sum over all region samples");
    add("focus.peakOffset", "Distance of the field peak from the focus", "m", "|x_peak - x_f|, x_peak the region sample with the largest |p|");
    add("region.peakPressure", "Largest pressure on the region", "Pa", "max |p| over region samples outside the near-source limit");
    add("focus.width", "Focal width (-6 dB)", "m", "full width of |p| >= peak / 2 around the peak nearest the focus on the focal line");
    add("focus.sidelobeDb", "Peak sidelobe level", "dB",
        "20 log10(max |p| outside the main lobe / main-lobe peak) on the focal line; the main lobe ends at the first minimum on each side");
    add("sources.effort", "Drive effort", "Pa^2", "sum |A_i|^2 of the source strengths (Pa at 1 m): proportional to drive power for identical sources");
    add("sources.maxAmplitude", "Largest source strength", "Pa", "max |A_i| (Pa at 1 m)");
    add("sources.clearance", "Smallest clearance between sources", "m", "min over source pairs of |r_i - r_j| - (a_i + a_j), a the source radius");
    add("manipulators.minThickness", "Thinnest manipulator wall", "m", "smallest thickness / wallThickness / centerThickness parameter of a manipulator");
    add("manipulators.maxExtent", "Largest manipulator extent", "m", "largest bounding-box side of a manipulator's geometry");

    // Variables.
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
    };
    variable("acoustics/frequency", "Analysis frequency (every source follows it)", "Hz", VariableRole::Frequency, s.frequencyHz, 0.5 * s.frequencyHz,
             2.0 * s.frequencyHz, "Acoustics");
    if (s.medium == airAtTemperature(s.medium.temperatureK) ||
        (s.medium.name == "Air" && s.medium.basis.find("ideal gas") != std::string::npos))
        variable("acoustics/medium/temperature", "Air temperature (ideal gas)", "K", VariableRole::MaterialParameter, s.medium.temperatureK, 250.0, 330.0,
                 "Acoustics");
    project.scene.forEach([&](const SceneObject& o) {
        const std::string id = o.id().str();
        const std::string& name = o.state.name;
        if (const auto* e = componentOf<EmitterComponent>(o)) {
            const EmitterSettings& es = e->settings;
            variable(opt::objectBinding(id, "emitter/amplitude"), name + ": amplitude", "Pa", VariableRole::Amplitude, es.amplitudePa, 0.0,
                     std::max(10.0 * es.amplitudePa, 1.0), name);
            variable(opt::objectBinding(id, "emitter/phase"), name + ": phase", "rad", VariableRole::Phase, es.phaseRad, -kPi, kPi, name);
            variable(opt::objectBinding(id, "emitter/delay"), name + ": delay", "s", VariableRole::Delay, es.delaySec, 0.0, 10.0 / es.frequencyHz, name);
            opt::appendSceneVariables(project, o, c.variables, false, true, std::nullopt);
        } else if (const auto* a = componentOf<ArrayComponent>(o)) {
            const ArraySettings& as = a->settings;
            variable(opt::objectBinding(id, "array/amplitude"), name + ": element amplitude", "Pa", VariableRole::Amplitude, as.element.amplitudePa, 0.0,
                     std::max(10.0 * as.element.amplitudePa, 1.0), name);
            if (as.layout == ArrayLayout::Linear || as.layout == ArrayLayout::RectangularPlanar)
                variable(opt::objectBinding(id, "array/spacingX"), name + ": spacing X", "m", VariableRole::Spacing, as.spacingX, 0.25 * as.spacingX,
                         4.0 * as.spacingX, name);
            if (as.layout == ArrayLayout::RectangularPlanar || as.layout == ArrayLayout::Cylindrical)
                variable(opt::objectBinding(id, "array/spacingY"), name + ": spacing Y", "m", VariableRole::Spacing, as.spacingY, 0.25 * as.spacingY,
                         4.0 * as.spacingY, name);
            if (as.layout == ArrayLayout::Circular || as.layout == ArrayLayout::Cylindrical || as.layout == ArrayLayout::Spherical)
                variable(opt::objectBinding(id, "array/radius"), name + ": radius", "m", VariableRole::Radius, as.radius, 0.25 * as.radius, 4.0 * as.radius,
                         name);
            if (as.layout != ArrayLayout::Custom)
                variable(opt::objectBinding(id, "array/countX"), name + ": elements (X)", "", VariableRole::Count, as.countX, 1.0,
                         std::max(4.0 * as.countX, 2.0), name, VariableKind::Integer);
            for (const ArrayElement& el : generateElements(as)) {
                const std::string k = "array/element/" + std::to_string(el.index) + "/";
                const std::string l = name + " #" + std::to_string(el.index) + ": ";
                variable(opt::objectBinding(id, k + "amplitude"), l + "amplitude", "Pa", VariableRole::Amplitude, el.settings.amplitudePa, 0.0,
                         std::max(10.0 * as.element.amplitudePa, 1.0), name + " elements");
                variable(opt::objectBinding(id, k + "phase"), l + "phase", "rad", VariableRole::Phase, el.settings.phaseRad, -kPi, kPi, name + " elements");
                variable(opt::objectBinding(id, k + "delay"), l + "delay", "s", VariableRole::Delay, el.settings.delaySec, 0.0,
                         10.0 / el.settings.frequencyHz, name + " elements");
                variable(opt::objectBinding(id, k + "active"), l + "active (0/1)", "", VariableRole::Activation, el.settings.active ? 1.0 : 0.0, 0.0, 1.0,
                         name + " elements", VariableKind::Integer);
            }
            opt::appendSceneVariables(project, o, c.variables, false, true, std::nullopt);
        } else if (dynamic_cast<const manipulators::ManipulatorComponent*>(o.state.component.get())) {
            opt::appendSceneVariables(project, o, c.variables, true, true, PhysicsDomain::Acoustic);
        }
    });
}

Result<bool> AcousticAdapter::applyDomainBinding(Project& project, std::string_view key, double value) const
{
    if (key == "acoustics/frequency") {
        if (!(value > 0.0))
            return Result<bool>::failure("the frequency must be positive");
        AcousticSettings s = settingsOf(project);
        s.frequencyHz = value;
        storeSettings(project, s);
        std::vector<std::pair<Id, ObjectState>> changes;
        project.scene.forEach([&](const SceneObject& o) {
            if (const auto* e = componentOf<EmitterComponent>(o)) {
                auto copy = std::make_shared<EmitterComponent>(*e);
                copy->settings.frequencyHz = value;
                ObjectState st = o.state;
                st.component = copy;
                changes.emplace_back(o.id(), std::move(st));
            } else if (const auto* a = componentOf<ArrayComponent>(o)) {
                auto copy = std::make_shared<ArrayComponent>(*a);
                copy->settings.element.frequencyHz = value;
                for (auto& [index, override] : copy->settings.overrides)
                    override.frequencyHz.reset();
                ObjectState st = o.state;
                st.component = copy;
                changes.emplace_back(o.id(), std::move(st));
            }
        });
        for (auto& [id, st] : changes)
            if (Status s2 = project.scene.setState(id, std::move(st)); !s2)
                return Result<bool>::failure(s2.message());
        return true;
    }
    if (key == "acoustics/medium/temperature") {
        AcousticSettings s = settingsOf(project);
        const double attenuation = s.medium.attenuation;
        s.medium = airAtTemperature(value);
        s.medium.attenuation = attenuation;
        if (Status v = s.medium.validate(); !v)
            return Result<bool>::failure(v.message());
        storeSettings(project, s);
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
    if (what.rfind("emitter/", 0) == 0) {
        const auto* e = componentOf<EmitterComponent>(*object);
        if (!e)
            return false;
        auto copy = std::make_shared<EmitterComponent>(*e);
        const std::string f = what.substr(8);
        if (f == "amplitude")
            copy->settings.amplitudePa = value;
        else if (f == "phase")
            copy->settings.phaseRad = value;
        else if (f == "delay")
            copy->settings.delaySec = value;
        else
            return false;
        if (Status v = copy->settings.validate(); !v)
            return Result<bool>::failure("'" + object->state.name + "': " + v.message());
        state.component = copy;
    } else if (what.rfind("array/", 0) == 0) {
        const auto* a = componentOf<ArrayComponent>(*object);
        if (!a)
            return false;
        auto copy = std::make_shared<ArrayComponent>(*a);
        ArraySettings& s = copy->settings;
        const std::string f = what.substr(6);
        if (f == "amplitude")
            s.element.amplitudePa = value;
        else if (f == "spacingX")
            s.spacingX = value;
        else if (f == "spacingY")
            s.spacingY = value;
        else if (f == "radius")
            s.radius = value;
        else if (f == "countX")
            s.countX = static_cast<int>(std::lround(value));
        else if (f == "countY")
            s.countY = static_cast<int>(std::lround(value));
        else if (f.rfind("element/", 0) == 0) {
            const std::string tail = f.substr(8);
            const size_t sl = tail.find('/');
            if (sl == std::string::npos)
                return false;
            const int index = std::stoi(tail.substr(0, sl));
            const char* field = elementField(tail.substr(sl + 1));
            if (!field)
                return false;
            if (index < 0 || index >= s.elementCount())
                return Result<bool>::failure("'" + object->state.name + "' has no element " + std::to_string(index));
            ElementOverride& o = s.overrides[index];
            const std::string fs = field;
            if (fs == "amplitude")
                o.amplitudePa = value;
            else if (fs == "phase")
                o.phaseRad = value;
            else if (fs == "delay")
                o.delaySec = value;
            else
                o.active = std::lround(value) != 0;
        } else
            return false;
        if (Status v = s.validate(); !v)
            return Result<bool>::failure("'" + object->state.name + "': " + v.message());
        state.component = copy;
    } else {
        return false;
    }
    if (Status s = project.scene.setState(id, std::move(state)); !s)
        return Result<bool>::failure(s.message());
    return true;
}

Evaluation AcousticAdapter::evaluate(const Bindings& bindings, const std::string& modelId, JobContext* context) const
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
    if (!info->available) {
        e.status = Evaluation::Status::Refused;
        e.message = info->unavailableReason;
        return e;
    }
    auto built = build(bindings);
    if (!built) {
        e.status = Evaluation::Status::InvalidDesign;
        e.message = built.message();
        return e;
    }
    Project& project = *built.value();
    const AcousticSettings s = settingsOf(project);
    const double f = s.frequencyHz;
    const Medium& medium = s.medium;
    const double lambda = medium.wavelength(f);
    e.model.settings = {{"frequencyHz", f}, {"medium", toJson(medium)}, {"propagation", {{"attenuation", s.propagation.includeAttenuation},
                                                                                       {"directivity", s.propagation.includeDirectivity},
                                                                                       {"minimumDistance", s.propagation.minimumDistance}}}};
    std::vector<PointSource> sources;
    size_t excluded = 0;
    for (const PointSource& p : collectSources(project.scene)) {
        if (std::abs(p.frequencyHz - f) <= 1e-9 * f)
            sources.push_back(p);
        else
            ++excluded;
    }
    if (excluded)
        e.notes.push_back(std::to_string(excluded) + " source(s) at another frequency are not part of this single-frequency evaluation");
    if (sources.empty()) {
        e.status = Evaluation::Status::InvalidDesign;
        e.message = "no active source at the analysis frequency";
        return e;
    }
    const double spacing = configuration_.value("targetSpacing", 0.5 * lambda);
    TargetSet targets = expandTargets(project.scene, spacing, focusId_);
    for (const std::string& n : targets.notes)
        e.notes.push_back(n);
    if (!focusId_.empty() && !targets.focus)
        e.warnings.push_back("the configured focus target is missing or is a suppression target");

    // The field of the chosen model.
    auto freeModel = std::make_shared<const FreeFieldModel>(s.propagation);
    const FieldEvaluator freeField(sources, f, medium, freeModel);
    std::shared_ptr<const FieldEvaluator> evaluator;
    std::shared_ptr<const fdtd::FdtdResult> fdtdResult;
    if (modelId == kFree) {
        evaluator = std::make_shared<const FieldEvaluator>(sources, f, medium, freeModel);
    } else if (modelId == kReflector) {
        manipulators::ReflectorOptions ro;
        ro.elementsPerWavelength = s.reflectorElementsPerWavelength;
        auto reflectors = manipulators::collectReflectors(project.scene, project.materials, lambda, ro);
        for (const std::string& n : reflectors.notes)
            e.notes.push_back(n);
        if (reflectors.surfaces.empty())
            e.notes.push_back("no reflector in the scene: this model equals the free field");
        e.model.settings["reflectorElementsPerWavelength"] = ro.elementsPerWavelength;
        e.model.settings["surfaceElements"] = reflectors.elementCount;
        auto model = std::make_shared<const manipulators::ReflectorModel>(s.propagation, std::move(reflectors.surfaces));
        evaluator = std::make_shared<const manipulators::ReflectorFieldEvaluator>(sources, f, medium, model);
    } else {
        fdtd::FdtdSettings fs = fdtd::fdtdSettingsOf(project);
        if (const auto it = bindings.find("memoryBudgetBytes"); it != bindings.end() && it->second > 0.0)
            fs.memoryLimitBytes = static_cast<size_t>(it->second);
        fdtd::Prepared prepared = fdtd::prepare(project, Id(regionId_), medium, fs);
        e.model.settings["fdtd"] = fdtd::toJson(fs);
        e.model.settings["plan"] = {{"dimensions", prepared.plan.dimensions}, {"dx", prepared.plan.dx}, {"dt", prepared.plan.dt},
                                    {"cells", prepared.plan.cellCount},      {"steps", prepared.plan.steps}};
        for (const std::string& a : prepared.plan.assumptions)
            e.model.assumptions.push_back(a);
        for (const std::string& w : prepared.plan.warnings)
            e.warnings.push_back(w);
        if (!prepared.plan.allowed) {
            e.status = Evaluation::Status::Refused;
            e.message = "the full-wave solver refuses this design: " + prepared.plan.refusal;
            return e;
        }
        auto run = fdtd::simulate("optimization candidate", std::move(prepared), fs, medium, {}, context);
        if (!run) {
            const bool cancelled = context && context->cancelRequested();
            e.status = cancelled ? Evaluation::Status::Cancelled : Evaluation::Status::Failed;
            e.message = run.message();
            return e;
        }
        fdtdResult = run.take();
        if (!fdtdResult->harmonicField) {
            e.status = Evaluation::Status::Refused;
            e.message = "the full-wave run produced no steady-state field: use a continuous signal in the FDTD settings";
            return e;
        }
    }
    const auto field = [&](const Vec3& x) -> std::optional<Complex> {
        if (fdtdResult)
            return sampleGrid(*fdtdResult->harmonicField, x);
        return evaluator->pressure(x);
    };

    // Targets.
    double num = 0.0, den = 0.0, magNum = 0.0, suppressionMax = 0.0, minFocal = std::numeric_limits<double>::infinity(), phaseSq = 0.0, qsum = 0.0;
    int phaseCount = 0;
    bool targetsComplete = true;
    nlohmann::json achieved = nlohmann::json::array();
    for (const SampledTarget& t : targets.points) {
        const auto p = field(t.position);
        if (!p) {
            targetsComplete = false;
            continue;
        }
        num += t.weight * std::norm(*p - t.desired);
        den += t.weight * std::norm(t.desired);
        magNum += t.weight * std::pow(std::abs(*p) - std::abs(t.desired), 2);
        qsum += t.weight;
        if (t.suppress)
            suppressionMax = std::max(suppressionMax, std::abs(*p));
        else {
            minFocal = std::min(minFocal, std::abs(*p));
            if (std::abs(t.desired) > 0.0) {
                double d = std::arg(*p) - std::arg(t.desired);
                d = std::remainder(d, 2.0 * kPi);
                phaseSq += d * d;
                ++phaseCount;
            }
        }
        if (achieved.size() < 64)
            achieved.push_back({{"label", t.label}, {"magnitude", std::abs(*p)}, {"phase", std::arg(*p)}, {"wanted", std::abs(t.desired)}});
    }
    if (!targetsComplete)
        e.notes.push_back("some targets are outside the full-wave region and were left out of the target quantities");
    if (!targets.points.empty()) {
        e.quantities["targets.error"] = den > 0.0 ? std::sqrt(num / den) : std::sqrt(num / std::max(qsum, 1e-300));
        if (den > 0.0)
            e.quantities["targets.magnitudeError"] = std::sqrt(magNum / den);
        if (phaseCount)
            e.quantities["targets.phaseErrorRms"] = std::sqrt(phaseSq / phaseCount);
        if (std::isfinite(minFocal))
            e.quantities["targets.minPressure"] = minFocal;
        bool anySuppress = std::any_of(targets.points.begin(), targets.points.end(), [](const SampledTarget& t) { return t.suppress; });
        if (anySuppress)
            e.quantities["suppression.maxPressure"] = suppressionMax;
    }
    e.details["targets"] = achieved;

    // Focus.
    const double focalRadius = configuration_.value("focalRadius", 0.0) > 0.0 ? configuration_.value("focalRadius", 0.0) : lambda;
    if (targets.focus) {
        const Vec3 xf = *targets.focus;
        if (const auto p = field(xf)) {
            e.quantities["focus.pressure"] = std::abs(*p);
            e.quantities["focus.pressureDb"] = db(std::abs(*p));
            e.quantities["focus.phase"] = std::arg(*p);
            const double direct = std::abs(freeField.pressure(xf));
            if (direct > 0.0)
                e.quantities["focus.gain"] = std::abs(*p) / direct;
            e.details["focus"] = {{"label", targets.focusLabel}, {"position", {xf.x(), xf.y(), xf.z()}}};
        } else {
            e.notes.push_back("the focus is outside the full-wave region");
        }
        // The line through the focus.
        Vec3 axis = Vec3::UnitX();
        if (configuration_.contains("lineAxis") && configuration_["lineAxis"].is_array() && configuration_["lineAxis"].size() == 3)
            axis = Vec3(configuration_["lineAxis"][0].get<double>(), configuration_["lineAxis"][1].get<double>(), configuration_["lineAxis"][2].get<double>());
        else if (const SceneObject* r = regionId_.empty() ? nullptr : project.scene.find(Id(regionId_)))
            if (const auto* rc = componentOf<RegionComponent>(*r)) {
                const GridFrame g = gridOf(rc->settings, project.scene.worldMatrix(*r));
                if (g.axisU.norm() > 0.0)
                    axis = g.axisU;
            }
        axis = axis.norm() > 0.0 ? axis.normalized() : Vec3::UnitX();
        const double half = configuration_.value("lineHalfLength", 0.0) > 0.0 ? configuration_.value("lineHalfLength", 0.0) : 4.0 * lambda;
        const int n = std::max(21, configuration_.value("lineSamples", 241));
        std::vector<double> t, m;
        for (int i = 0; i < n; ++i) {
            const double ti = -half + 2.0 * half * i / (n - 1);
            const auto p = field(xf + ti * axis);
            if (!p)
                continue;
            t.push_back(ti);
            m.push_back(std::abs(*p));
        }
        if (t.size() >= 5) {
            size_t peak = 0;
            double best = -1.0;
            for (size_t i = 0; i < t.size(); ++i)
                if (std::abs(t[i]) <= focalRadius && m[i] > best) {
                    best = m[i];
                    peak = i;
                }
            if (best > 0.0) {
                const double halfLevel = 0.5 * best;
                size_t l = peak, r = peak;
                while (l > 0 && m[l] >= halfLevel)
                    --l;
                while (r + 1 < t.size() && m[r] >= halfLevel)
                    ++r;
                if (m[l] < halfLevel && m[r] < halfLevel) {
                    const double tl = t[l] + (halfLevel - m[l]) / (m[l + 1] - m[l]) * (t[l + 1] - t[l]);
                    const double tr = t[r - 1] + (m[r - 1] - halfLevel) / (m[r - 1] - m[r]) * (t[r] - t[r - 1]);
                    e.quantities["focus.width"] = tr - tl;
                } else {
                    e.notes.push_back("the -6 dB width is not reached on the focal line: lengthen it (adapter option lineHalfLength)");
                }
                // Main lobe: to the first minimum on each side.
                size_t ml = peak, mr = peak;
                while (ml > 0 && m[ml - 1] <= m[ml])
                    --ml;
                while (mr + 1 < t.size() && m[mr + 1] <= m[mr])
                    ++mr;
                double side = 0.0;
                for (size_t i = 0; i < t.size(); ++i)
                    if (i < ml || i > mr)
                        side = std::max(side, m[i]);
                e.quantities["focus.sidelobeDb"] = side > 0.0 ? db(side / best) : -200.0;
            }
            nlohmann::json cut = nlohmann::json::array();
            const size_t stride = std::max<size_t>(1, t.size() / 121);
            for (size_t i = 0; i < t.size(); i += stride)
                cut.push_back({t[i], m[i]});
            e.details["focalLine"] = {{"axis", {axis.x(), axis.y(), axis.z()}}, {"samples", cut}};
        }
        // Region quantities.
        if (const SceneObject* r = regionId_.empty() ? nullptr : project.scene.find(Id(regionId_)))
            if (const auto* rc = componentOf<RegionComponent>(*r)) {
                std::vector<std::pair<Vec3, double>> samples;
                const int limit = std::max(5, configuration_.value("regionSamples", 41));
                if (fdtdResult) {
                    const FieldResult& h = *fdtdResult->harmonicField;
                    const GridFrame& g = h.frame;
                    const int su = std::max(1, g.nu / limit), sv = std::max(1, g.nv / limit), sw = std::max(1, g.nw / limit);
                    for (int iw = 0; iw < g.nw; iw += sw)
                        for (int iv = 0; iv < g.nv; iv += sv)
                            for (int iu = 0; iu < g.nu; iu += su) {
                                const auto v = h.pressure[g.index(iu, iv, iw)];
                                samples.emplace_back(g.point(iu, iv, iw), std::abs(Complex(v.real(), v.imag())));
                            }
                } else {
                    RegionSettings rs = rc->settings;
                    rs.countU = std::min(rs.countU, limit);
                    rs.countV = std::min(rs.countV, limit);
                    rs.countW = std::min(rs.countW, limit);
                    const GridFrame g = gridOf(rs, project.scene.worldMatrix(*r));
                    for (int iw = 0; iw < g.nw; ++iw)
                        for (int iv = 0; iv < g.nv; ++iv)
                            for (int iu = 0; iu < g.nu; ++iu) {
                                const Vec3 x = g.point(iu, iv, iw);
                                const FieldSample fs = evaluator->sample(x);
                                if (!fs.nearSource)
                                    samples.emplace_back(x, std::abs(fs.pressure));
                            }
                }
                double all = 0.0, outside = 0.0, peak = 0.0;
                size_t outsideCount = 0;
                Vec3 peakAt = xf;
                for (const auto& [x, v] : samples) {
                    all += v * v;
                    if ((x - xf).norm() > focalRadius) {
                        outside += v * v;
                        ++outsideCount;
                    }
                    if (v > peak) {
                        peak = v;
                        peakAt = x;
                    }
                }
                if (!samples.empty()) {
                    e.quantities["region.peakPressure"] = peak;
                    e.quantities["focus.peakOffset"] = (peakAt - xf).norm();
                    if (all > 0.0)
                        e.quantities["focus.energyOutside"] = outside / all;
                    if (outsideCount && e.quantities.contains("focus.pressure") && outside > 0.0)
                        e.quantities["focus.contrastDb"] =
                            10.0 * std::log10(std::pow(e.quantities["focus.pressure"], 2) / (outside / static_cast<double>(outsideCount)));
                    e.details["regionSamples"] = samples.size();
                }
            }
    } else {
        e.notes.push_back("no focus target: the focus, line and region quantities are not available");
    }

    // Sources and geometry.
    double effort = 0.0, maxAmp = 0.0, clearance = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < sources.size(); ++i) {
        effort += std::norm(sources[i].amplitude);
        maxAmp = std::max(maxAmp, std::abs(sources[i].amplitude));
        for (size_t j = i + 1; j < sources.size(); ++j)
            clearance = std::min(clearance, (sources[i].position - sources[j].position).norm() - (sources[i].radius + sources[j].radius));
    }
    e.quantities["sources.effort"] = effort;
    e.quantities["sources.maxAmplitude"] = maxAmp;
    if (std::isfinite(clearance))
        e.quantities["sources.clearance"] = clearance;
    double minThickness = std::numeric_limits<double>::infinity(), maxExtent = 0.0;
    project.scene.forEach([&](const SceneObject& o) {
        if (!dynamic_cast<const manipulators::ManipulatorComponent*>(o.state.component.get()) || !o.state.geometry)
            return;
        for (const auto& [k, v] : o.state.geometry->parameters())
            if (k == "thickness" || k == "wallThickness" || k == "centerThickness")
                minThickness = std::min(minThickness, v);
        TessellationOptions to;
        to.segments = 24;
        const Aabb b = o.state.geometry->tessellate(to).bounds();
        if (b.isValid())
            maxExtent = std::max(maxExtent, b.size().cast<double>().maxCoeff());
    });
    if (std::isfinite(minThickness))
        e.quantities["manipulators.minThickness"] = minThickness;
    if (maxExtent > 0.0)
        e.quantities["manipulators.maxExtent"] = maxExtent;
    return e;
}

Result<opt::LinearSystem> AcousticAdapter::linearSystem(const Bindings& bindings, const std::string& modelId) const
{
    using R = Result<opt::LinearSystem>;
    if (modelId == kFdtd)
        return R::failure("the full-wave model has no source-to-target matrix here; use the free-field or reflector model for inverse design "
                          "and re-evaluate the result with the full-wave model");
    auto built = build(bindings);
    if (!built)
        return R::failure(built.message());
    const Project& project = *built.value();
    const AcousticSettings s = settingsOf(project);
    const double f = s.frequencyHz, lambda = s.medium.wavelength(f);
    std::vector<PointSource> sources;
    for (const PointSource& p : collectSources(project.scene))
        if (std::abs(p.frequencyHz - f) <= 1e-9 * f)
            sources.push_back(p);
    const TargetSet targets = expandTargets(project.scene, configuration_.value("targetSpacing", 0.5 * lambda), focusId_);
    if (sources.empty())
        return R::failure("no active source at the analysis frequency");
    if (targets.points.empty())
        return R::failure("the scene has no target");
    std::shared_ptr<const PropagationModel> model;
    if (modelId == kReflector) {
        manipulators::ReflectorOptions ro;
        ro.elementsPerWavelength = s.reflectorElementsPerWavelength;
        model = std::make_shared<const manipulators::ReflectorModel>(s.propagation,
                                                                     manipulators::collectReflectors(project.scene, project.materials, lambda, ro).surfaces);
    } else {
        model = std::make_shared<const FreeFieldModel>(s.propagation);
    }
    opt::LinearSystem sys;
    const Eigen::Index m = static_cast<Eigen::Index>(targets.points.size()), n = static_cast<Eigen::Index>(sources.size());
    sys.response.resize(m, n);
    sys.desired.resize(m);
    sys.rowWeights.resize(m);
    sys.current.resize(n);
    for (Eigen::Index i = 0; i < m; ++i) {
        const SampledTarget& t = targets.points[static_cast<size_t>(i)];
        sys.desired(i) = t.desired;
        sys.rowWeights(i) = t.weight;
        sys.targetLabels.push_back(t.label);
        for (Eigen::Index j = 0; j < n; ++j)
            sys.response(i, j) = model->transfer(sources[static_cast<size_t>(j)], t.position, s.medium).value;
    }
    for (Eigen::Index j = 0; j < n; ++j) {
        const PointSource& p = sources[static_cast<size_t>(j)];
        sys.current(j) = p.amplitude;
        sys.sourceLabels.push_back(p.label);
        const std::string id = p.objectId.str();
        if (p.elementIndex >= 0) {
            const std::string k = "array/element/" + std::to_string(p.elementIndex) + "/";
            sys.amplitudeBindings.push_back(opt::objectBinding(id, k + "amplitude"));
            sys.phaseBindings.push_back(opt::objectBinding(id, k + "phase"));
            sys.zeroBindings.push_back(opt::objectBinding(id, k + "delay"));
        } else {
            sys.amplitudeBindings.push_back(opt::objectBinding(id, "emitter/amplitude"));
            sys.phaseBindings.push_back(opt::objectBinding(id, "emitter/phase"));
            sys.zeroBindings.push_back(opt::objectBinding(id, "emitter/delay"));
        }
    }
    sys.weightUnit = "Pa at 1 m";
    sys.targetUnit = "Pa";
    sys.model = model->name();
    sys.notes = targets.notes;
    return sys;
}

// --- templates -----------------------------------------------------------------------------------------

const SceneObject* firstWith(const Scene& scene, const std::function<bool(const SceneObject&)>& test)
{
    const SceneObject* found = nullptr;
    scene.forEach([&](const SceneObject& o) {
        if (!found && test(o))
            found = &o;
    });
    return found;
}

std::string firstRegion(const Scene& scene)
{
    const SceneObject* r = firstWith(scene, [](const SceneObject& o) { return componentOf<RegionComponent>(o) != nullptr; });
    return r ? r->id().str() : std::string();
}

opt::DesignVariable offered(const opt::Capabilities& caps, const std::string& binding)
{
    const opt::DesignVariable* v = caps.variable(binding);
    return v ? *v : opt::DesignVariable{};
}

struct TemplateStart {
    opt::OptimizationProblem problem;
    std::shared_ptr<const opt::DesignEvaluator> evaluator;
    const SceneObject* array = nullptr;
};

Result<TemplateStart> start(const Project& project, const std::string& templateId, const std::string& name, bool needArray, bool needTarget)
{
    using R = Result<TemplateStart>;
    TemplateStart t;
    t.array = firstWith(project.scene, [](const SceneObject& o) { return componentOf<ArrayComponent>(o) != nullptr; });
    const SceneObject* target = firstWith(project.scene, [](const SceneObject& o) {
        const auto* c = componentOf<TargetComponent>(o);
        return c && !c->settings.suppress;
    });
    if (needArray && !t.array)
        return R::failure("the project has no acoustic array");
    if (needTarget && !target)
        return R::failure("the project has no acoustic target (a focus) that is not a suppression target");
    nlohmann::json configuration = nlohmann::json::object();
    if (target)
        configuration["focus"] = target->id().str();
    if (const std::string region = firstRegion(project.scene); !region.empty())
        configuration["region"] = region;
    auto bound = bindAcousticAdapter(opt::designSnapshot(project), configuration);
    if (!bound)
        return R::failure(bound.message());
    t.evaluator = bound.take();
    opt::OptimizationProblem& p = t.problem;
    p.id = templateId + "-" + Id::generate().str().substr(0, 8);
    p.name = name;
    p.templateId = templateId;
    p.adapterId = kAcousticAdapterId;
    p.adapterConfig = configuration;
    p.modelId = kFree;
    if (configuration.contains("region"))
        p.validationModelId = kFdtd;
    return t;
}

void addElements(TemplateStart& t, const std::string& field, bool active)
{
    const auto* a = componentOf<ArrayComponent>(*t.array);
    for (const ArrayElement& el : generateElements(a->settings)) {
        opt::DesignVariable v = offered(t.evaluator->capabilities(),
                                        opt::objectBinding(t.array->id().str(), "array/element/" + std::to_string(el.index) + "/" + field));
        v.active = active;
        if (!v.id.empty())
            t.problem.space.variables.push_back(v);
    }
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

void registerTemplates(opt::AdapterRegistry& registry)
{
    registry.addTemplate({"acoustic.array-phase-focus", "Acoustic: focus an array on a point (element phases)",
                          "Maximize the pressure at the focus target by the phase of every element of the first array (free-field model, "
                          "L-BFGS-B with finite differences). The analytical answer is the phase focus k r_i.",
                          kAcousticAdapterId, [](const Project& project) -> Result<nlohmann::json> {
                              auto t = start(project, "acoustic.array-phase-focus", "Focus the array on the target", true, true);
                              if (!t)
                                  return Result<nlohmann::json>::failure(t.message());
                              addElements(t.value(), "phase", true);
                              opt::OptimizationProblem& p = t.value().problem;
                              p.objectives.push_back(objective("focus", "Pressure at the focus", "focus.pressure", opt::Sense::Maximize, 1.0));
                              p.algorithm.algorithm = "lbfgs";
                              p.algorithm.maxEvaluations = 600;
                              return opt::toJson(p);
                          }});
    registry.addTemplate({"acoustic.array-targets", "Acoustic: match several targets (element amplitudes and phases)",
                          "Minimize the error of the field at every target (focal points and suppression points) by the amplitude and phase "
                          "of every element; the linear inverse solution is available for the same problem.",
                          kAcousticAdapterId, [](const Project& project) -> Result<nlohmann::json> {
                              auto t = start(project, "acoustic.array-targets", "Match the target field", true, true);
                              if (!t)
                                  return Result<nlohmann::json>::failure(t.message());
                              addElements(t.value(), "amplitude", true);
                              addElements(t.value(), "phase", true);
                              opt::OptimizationProblem& p = t.value().problem;
                              p.objectives.push_back(objective("error", "Target field error", "targets.error", opt::Sense::Minimize, 1.0));
                              p.algorithm.algorithm = "lbfgs";
                              p.algorithm.maxEvaluations = 1500;
                              return opt::toJson(p);
                          }});
    registry.addTemplate({"acoustic.reflector-focus", "Acoustic: reflector geometry for a focal target",
                          "Sweep the focal length of the first reflector for the largest pressure at the focus target with the Kirchhoff "
                          "reflector model; other parameters are offered but locked.",
                          kAcousticAdapterId, [](const Project& project) -> Result<nlohmann::json> {
                              auto t = start(project, "acoustic.reflector-focus", "Reflector geometry for the focus", false, true);
                              if (!t)
                                  return Result<nlohmann::json>::failure(t.message());
                              const SceneObject* reflector = firstWith(project.scene, [](const SceneObject& o) {
                                  return dynamic_cast<const manipulators::ManipulatorComponent*>(o.state.component.get()) && o.state.geometry &&
                                         manipulators::isReflectorGeometry(*o.state.geometry);
                              });
                              if (!reflector)
                                  return Result<nlohmann::json>::failure("the project has no reflector");
                              opt::OptimizationProblem& p = t.value().problem;
                              bool first = true;
                              for (const auto& [key, value] : reflector->state.geometry->parameters()) {
                                  opt::DesignVariable v = offered(t.value().evaluator->capabilities(),
                                                                  opt::objectBinding(reflector->id().str(), "geometry/" + key));
                                  if (v.id.empty())
                                      continue;
                                  const bool free = first && (key == "focalLength" || key == "curvatureRadius" || key == "semiMajor");
                                  v.locked = !free;
                                  if (free)
                                      first = false;
                                  p.space.variables.push_back(v);
                              }
                              p.modelId = kReflector;
                              p.objectives.push_back(objective("focus", "Pressure at the focus", "focus.pressure", opt::Sense::Maximize, 1.0));
                              p.algorithm.algorithm = "sweep";
                              p.algorithm.pointsPerVariable = 25;
                              p.algorithm.maxEvaluations = 100;
                              return opt::toJson(p);
                          }});
    registry.addTemplate({"acoustic.sidelobe-tradeoff", "Acoustic: focal pressure against sidelobes (Pareto)",
                          "Element amplitudes of the first array for two competing objectives - the pressure at the focus and the peak "
                          "sidelobe level on the focal line - with NSGA-II; the result is a trade-off front, not one design.",
                          kAcousticAdapterId, [](const Project& project) -> Result<nlohmann::json> {
                              auto t = start(project, "acoustic.sidelobe-tradeoff", "Focal pressure against sidelobes", true, true);
                              if (!t)
                                  return Result<nlohmann::json>::failure(t.message());
                              addElements(t.value(), "amplitude", true);
                              opt::OptimizationProblem& p = t.value().problem;
                              p.objectives.push_back(objective("focus", "Pressure at the focus", "focus.pressure", opt::Sense::Maximize, 1.0));
                              p.objectives.push_back(objective("sidelobes", "Peak sidelobe level", "focus.sidelobeDb", opt::Sense::Minimize, 10.0));
                              p.algorithm.algorithm = "nsga2";
                              p.algorithm.maxEvaluations = 800;
                              return opt::toJson(p);
                          }});
}

}  // namespace

Result<std::shared_ptr<const opt::DesignEvaluator>> bindAcousticAdapter(const nlohmann::json& snapshot, const nlohmann::json& configuration)
{
    using R = Result<std::shared_ptr<const opt::DesignEvaluator>>;
    auto project = opt::projectFromSnapshot(snapshot);
    if (!project)
        return R::failure("the design snapshot could not be read: " + project.message());
    if (!configuration.is_object())
        return R::failure("the acoustic adapter's configuration must be an object");
    return std::shared_ptr<const opt::DesignEvaluator>(std::make_shared<AcousticAdapter>(snapshot, configuration, *project.value()));
}

void registerAcousticOptimization(opt::AdapterRegistry& registry)
{
    registry.add(kAcousticAdapterId, "Acoustic waves (free field, reflectors, full-wave FDTD)", bindAcousticAdapter);
    registerTemplates(registry);
}

}  // namespace resonant::acoustics
