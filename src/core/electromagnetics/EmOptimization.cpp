#include "electromagnetics/EmOptimization.h"

#include "electromagnetics/Antennas.h"
#include "electromagnetics/ArrayFactor.h"
#include "electromagnetics/Conventions.h"
#include "electromagnetics/EmComponents.h"
#include "electromagnetics/EmStudy.h"
#include "geometry/Geometry.h"
#include "optimization/Persistence.h"
#include "optimization/Problem.h"
#include "optimization/SceneBindings.h"
#include "project/Project.h"
#include "scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <numbers>

namespace resonant::em {

namespace opt = resonant::optimization;
using opt::Capabilities;
using opt::Evaluation;
using opt::Fidelity;

namespace {

constexpr const char* kArrayFactor = "em.arrayfactor";
constexpr const char* kExact = "em.dipoles.exact";
constexpr const char* kAntenna = "em.antenna.analytical";
constexpr double kPi = std::numbers::pi;
constexpr double kC = 299792458.0;
using Bindings = std::map<std::string, double, std::less<>>;

template <typename T>
const T* as(const SceneObject& o)
{
    return dynamic_cast<const T*>(o.state.component.get());
}

struct Direction {
    std::string label;
    Vec3 vector;
    double thetaDeg = 0.0, phiDeg = 0.0;
    std::optional<double> levelDb;
    bool null = false;
};

std::vector<Direction> directionsOf(const nlohmann::json& configuration)
{
    std::vector<Direction> out;
    for (const auto& d : configuration.value("directions", nlohmann::json::array())) {
        Direction x;
        x.thetaDeg = d.value("thetaDeg", 0.0);
        x.phiDeg = d.value("phiDeg", 0.0);
        x.label = d.value("label", "d" + std::to_string(out.size() + 1));
        x.vector = directionOf(x.thetaDeg * kPi / 180.0, x.phiDeg * kPi / 180.0);
        if (d.contains("levelDb") && d["levelDb"].is_number())
            x.levelDb = d["levelDb"].get<double>();
        x.null = d.value("null", false);
        out.push_back(std::move(x));
    }
    return out;
}

double angleDeg(const Vec3& a, const Vec3& b) { return std::acos(std::clamp(a.normalized().dot(b.normalized()), -1.0, 1.0)) * 180.0 / kPi; }

struct Metrics {
    double directivity = 0.0;
    Vec3 main = Vec3::UnitZ();
    double maxU = 0.0;
    double beamwidthTheta = std::numeric_limits<double>::quiet_NaN(), beamwidthPhi = std::numeric_limits<double>::quiet_NaN();
    double sidelobeDb = -200.0;
    double mainLobeFraction = 0.0;  // share of the sphere's samples in the main lobe
};

// Pattern metrics of a radiation intensity U(direction) sampled on the sphere.
//
// The main lobe is the set of samples reachable from the largest one without
// climbing (each step to a neighbouring sample of at most the same U), so a
// ring of equal maxima (the fan beam of a linear array) is one lobe while a
// separate grating lobe is not part of it. The peak sidelobe level is the
// largest U outside the main lobe. Among equal maxima, the one nearest the
// `towards` direction is the main-lobe direction.
Metrics analyze(const std::function<double(const Vec3&)>& u, double resolutionDeg, const std::optional<Vec3>& towards)
{
    Metrics m;
    const double step = resolutionDeg * kPi / 180.0;
    const int nt = static_cast<int>(std::round(kPi / step)) + 1;
    const int np = std::max(4, static_cast<int>(std::round(2.0 * kPi / (2.0 * step))));
    const auto at = [&](int i, int j) { return directionOf(kPi * i / (nt - 1), 2.0 * kPi * j / np); };
    std::vector<double> values(static_cast<size_t>(nt) * np);
    double integral = 0.0;
    for (int i = 0; i < nt; ++i) {
        const double theta = kPi * i / (nt - 1);
        const double weight = std::sin(theta) * (kPi / (nt - 1)) * (2.0 * kPi / np) * ((i == 0 || i == nt - 1) ? 0.5 : 1.0);
        for (int j = 0; j < np; ++j) {
            const double v = u(at(i, j));
            values[static_cast<size_t>(i) * np + j] = v;
            integral += v * weight;
            m.maxU = std::max(m.maxU, v);
        }
    }
    if (!(m.maxU > 0.0) || !(integral > 0.0))
        return m;
    m.directivity = 4.0 * kPi * m.maxU / integral;
    size_t best = 0;
    double closest = -2.0;
    for (size_t k = 0; k < values.size(); ++k)
        if (values[k] >= m.maxU * (1.0 - 1e-9)) {
            const double c = towards ? at(static_cast<int>(k) / np, static_cast<int>(k) % np).dot(*towards) : 0.0;
            if (c > closest + 1e-12) {
                closest = c;
                best = k;
            }
        }
    m.main = at(static_cast<int>(best) / np, static_cast<int>(best) % np);
    // The main lobe by descent from the global maximum.
    std::vector<char> inLobe(values.size(), 0);
    std::vector<size_t> stack;
    for (size_t k = 0; k < values.size(); ++k)
        if (values[k] >= m.maxU * (1.0 - 1e-9) && !inLobe[k]) {
            // Every sample of the global maximum starts the lobe only if connected; start from the chosen one.
            if (k != best)
                continue;
            inLobe[k] = 1;
            stack.push_back(k);
        }
    const auto visit = [&](size_t from, int i, int j) {
        if (i < 0 || i >= nt)
            return;
        j = ((j % np) + np) % np;
        const size_t q = static_cast<size_t>(i) * np + j;
        if (!inLobe[q] && values[q] <= values[from] * (1.0 + 1e-9)) {
            inLobe[q] = 1;
            stack.push_back(q);
        }
    };
    while (!stack.empty()) {
        const size_t k = stack.back();
        stack.pop_back();
        const int i = static_cast<int>(k) / np, j = static_cast<int>(k) % np;
        visit(k, i - 1, j);
        visit(k, i + 1, j);
        visit(k, i, j - 1);
        visit(k, i, j + 1);
        if (i == 0 || i == nt - 1)  // a pole: every sample of the row is the same direction
            for (int jj = 0; jj < np; ++jj)
                visit(k, i, jj);
    }
    double side = 0.0;
    size_t lobe = 0;
    for (size_t k = 0; k < values.size(); ++k) {
        if (inLobe[k])
            ++lobe;
        else
            side = std::max(side, values[k]);
    }
    m.mainLobeFraction = static_cast<double>(lobe) / values.size();
    m.sidelobeDb = side > 0.0 ? 10.0 * std::log10(side / m.maxU) : -200.0;
    // Half-power widths along the meridian through the main lobe and the great circle across it.
    Vec3 e1 = Vec3::UnitZ() - m.main.z() * m.main;
    if (e1.norm() < 1e-9)
        e1 = Vec3::UnitX();  // the main lobe is at a pole: use the phi = 0 meridian
    e1.normalize();
    const Vec3 e2 = m.main.cross(e1).normalized();
    const auto halfPower = [&](const Vec3& e) {
        double width = 0.0;
        for (int sign : {+1, -1}) {
            double found = std::numeric_limits<double>::quiet_NaN();
            for (double a = step / 8.0; a <= kPi; a += step / 8.0)
                if (u(std::cos(a) * m.main + sign * std::sin(a) * e) < 0.5 * m.maxU) {
                    found = a * 180.0 / kPi;
                    break;
                }
            if (std::isnan(found))
                return std::numeric_limits<double>::quiet_NaN();
            width += found;
        }
        return width;
    };
    m.beamwidthTheta = halfPower(e1);
    m.beamwidthPhi = halfPower(e2);
    return m;
}

const char* fieldName(const std::string& f)
{
    static const char* fields[] = {"amplitude", "phase", "delay", "active"};
    for (const char* x : fields)
        if (f == x)
            return x;
    return nullptr;
}

class EmAdapter final : public opt::SnapshotAdapter {
public:
    EmAdapter(nlohmann::json snapshot, nlohmann::json configuration, const Project& project, std::string objectId, bool isArray)
        : SnapshotAdapter(std::move(snapshot), std::move(configuration)), objectId_(std::move(objectId)), isArray_(isArray)
    {
        directions_ = directionsOf(configuration_);
        describe(project);
        finishBinding();
    }
    Evaluation evaluate(const Bindings& bindings, const std::string& modelId, JobContext* context) const override;
    Result<opt::LinearSystem> linearSystem(const Bindings& bindings, const std::string& modelId) const override;

protected:
    Result<bool> applyDomainBinding(Project& project, std::string_view key, double value) const override;

private:
    void describe(const Project& project);
    std::string objectId_;
    bool isArray_ = true;
    std::vector<Direction> directions_;
};

void EmAdapter::describe(const Project& project)
{
    Capabilities& c = capabilities_;
    c.adapterId = kEmAdapterId;
    c.domain = "electromagnetic";
    c.version = "1";
    c.supportsLinearInverse = isArray_;
    const SceneObject* o = project.scene.find(Id(objectId_));
    double f = 1e9;
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
    const auto add = [&](std::string key, std::string label, std::string unit, std::string definition, std::vector<std::string> models) {
        c.quantities.push_back({std::move(key), std::move(label), std::move(unit), std::move(definition), {}, std::move(models), false});
    };
    if (isArray_) {
        const EmArraySettings& s = as<EmArrayComponent>(*o)->settings;
        f = s.frequencyHz;
        const bool dipoles = s.pattern == ElementPattern::HalfWaveDipole || s.pattern == ElementPattern::ShortDipole;
        c.models = {{kArrayFactor, "Array factor x element pattern (far field)", "1", Fidelity::Analytical,
                     {"identical, isolated elements: no mutual coupling, no feed network, no scattering", "far field"}, true, {}, 0.02},
                    {kExact, "Superposed exact dipole fields at a finite distance", "1", Fidelity::Reduced,
                     {"each element radiates the exact field of its assumed sinusoidal current (thin dipole)",
                      "no mutual coupling: the currents are not changed by the neighbouring elements"},
                     dipoles, dipoles ? "" : "the exact-field model needs dipole elements", 0.5}};
        const std::string id = objectId_;
        const std::string& name = o->state.name;
        variable(opt::objectBinding(id, "array/frequency"), name + ": frequency", "Hz", VariableRole::Frequency, f, 0.5 * f, 2.0 * f, name);
        if (s.layout == EmArrayLayout::Linear || s.layout == EmArrayLayout::Planar)
            variable(opt::objectBinding(id, "array/spacingX"), name + ": spacing X", "m", VariableRole::Spacing, s.spacingX, 0.25 * s.spacingX,
                     4.0 * s.spacingX, name);
        if (s.layout == EmArrayLayout::Planar)
            variable(opt::objectBinding(id, "array/spacingY"), name + ": spacing Y", "m", VariableRole::Spacing, s.spacingY, 0.25 * s.spacingY,
                     4.0 * s.spacingY, name);
        if (s.layout == EmArrayLayout::Circular)
            variable(opt::objectBinding(id, "array/radius"), name + ": radius", "m", VariableRole::Radius, s.radius, 0.25 * s.radius, 4.0 * s.radius, name);
        if (s.layout != EmArrayLayout::Custom)
            variable(opt::objectBinding(id, "array/countX"), name + ": elements (X)", "", VariableRole::Count, s.countX, 1.0, std::max(2.0, 4.0 * s.countX),
                     name, VariableKind::Integer);
        const ArrayModel model = arrayModelOf(s);
        for (size_t n = 0; n < model.elements.size(); ++n) {
            const std::string k = "array/element/" + std::to_string(n) + "/";
            const std::string l = name + " #" + std::to_string(n) + ": ";
            const ArrayElement& e = model.elements[n];
            variable(opt::objectBinding(id, k + "amplitude"), l + "amplitude", "", VariableRole::Amplitude, std::abs(e.weight), 0.0,
                     std::max(1.0, 4.0 * std::abs(e.weight)), name + " elements");
            variable(opt::objectBinding(id, k + "phase"), l + "phase", "rad", VariableRole::Phase, std::arg(e.weight), -kPi, kPi, name + " elements");
            variable(opt::objectBinding(id, k + "delay"), l + "delay", "s", VariableRole::Delay, e.delay, 0.0, 10.0 / f, name + " elements");
            variable(opt::objectBinding(id, k + "active"), l + "active (0/1)", "", VariableRole::Activation, e.active ? 1.0 : 0.0, 0.0, 1.0,
                     name + " elements", VariableKind::Integer);
        }
        opt::appendSceneVariables(project, *o, c.variables, false, true, std::nullopt);
        add("pattern.directivityDbi", "Directivity", "dBi", "10 log10(4 pi U_max / integral of U over the sphere) of the model's pattern", {});
        add("pattern.sidelobeDb", "Peak sidelobe level", "dB",
            "10 log10(max U outside the main lobe / U_max); the main lobe is every sample reachable from the maximum without climbing", {});
        add("pattern.beamwidthThetaDeg", "Half-power beamwidth (meridian)", "deg",
            "width of U >= U_max / 2 along the meridian through the main lobe (not available for a fan beam along it)", {});
        add("pattern.beamwidthPhiDeg", "Half-power beamwidth (across)", "deg",
            "width of U >= U_max / 2 along the great circle across the meridian through the main lobe", {});
        add("pattern.mainLobeErrorDeg", "Main-lobe pointing error", "deg",
            "angle between the main lobe (of equal maxima, the one nearest the target) and the first configured direction", {});
        for (const Direction& d : directions_) {
            add("pattern.levelDb/" + d.label, "Level toward " + d.label, "dB", "10 log10(U(direction) / U_max)", {});
            add("pattern.gainDbi/" + d.label, "Directivity toward " + d.label, "dBi", "directivity + level toward the direction (idealized model: no losses)", {});
        }
        add("array.effort", "Excitation effort", "", "sum |w_n|^2 of the active elements' weights", {});
        add("array.maxWeight", "Largest element weight", "", "max |w_n|", {});
        add("array.minSpacingWavelengths", "Smallest element spacing", "wavelengths", "min distance between elements / wavelength", {});
    } else {
        const AntennaSettings& s = as<AntennaComponent>(*o)->settings;
        f = s.frequencyHz;
        const bool analytical = supportOf(*o->state.geometry) == AntennaModel::Analytical;
        c.models = {{kAntenna, "Analytical antenna (induced EMF)", "1", Fidelity::Analytical,
                     {"the assumed sinusoidal current of a thin wire; induced-EMF impedance", "infinite perfect ground for a monopole"},
                     analytical, analytical ? "" : "only dipole, monopole and small-loop antennas have an analytical model", 0.001}};
        opt::appendSceneVariables(project, *o, c.variables, true, false, PhysicsDomain::Electromagnetic);
        variable(opt::objectBinding(objectId_, "antenna/frequency"), o->state.name + ": frequency", "Hz", VariableRole::Frequency, f, 0.5 * f, 2.0 * f,
                 o->state.name);
        variable(opt::objectBinding(objectId_, "antenna/referenceImpedance"), o->state.name + ": reference impedance", "ohm", VariableRole::Other,
                 s.referenceImpedance, 10.0, 300.0, o->state.name);
        add("antenna.inputResistance", "Input resistance", "ohm", "Re Z_in (induced EMF)", {});
        add("antenna.inputReactance", "Input reactance", "ohm", "Im Z_in (induced EMF)", {});
        add("antenna.reflectionMagnitude", "Reflection coefficient magnitude", "", "|Gamma| = |(Z_in - Z_0) / (Z_in + Z_0)|", {});
        add("antenna.returnLossDb", "Return loss", "dB", "-20 log10 |Gamma|", {});
        add("antenna.vswr", "VSWR", "", "(1 + |Gamma|) / (1 - |Gamma|)", {});
        add("antenna.directivityDbi", "Directivity", "dBi", "of the assumed current's pattern", {});
        add("antenna.gainDbi", "Gain", "dBi", "directivity x radiation efficiency", {});
        add("antenna.realizedGainDbi", "Realized gain", "dBi", "gain x (1 - |Gamma|^2)", {});
        add("antenna.efficiency", "Radiation efficiency", "", "R_rad / (R_rad + R_loss) with the conductor loss of the material", {});
    }
    c.constants = {{"wavelength", kC / f}, {"frequency", f}};
    c.limitations = {"no mutual coupling between elements in either array model", "patterns in the object's local frame"};
}

Result<bool> EmAdapter::applyDomainBinding(Project& project, std::string_view key, double value) const
{
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
    if (what.rfind("array/", 0) == 0) {
        const auto* a = as<EmArrayComponent>(*object);
        if (!a)
            return false;
        auto copy = std::make_shared<EmArrayComponent>(*a);
        EmArraySettings& s = copy->settings;
        const std::string f = what.substr(6);
        if (f == "frequency")
            s.frequencyHz = value;
        else if (f == "spacingX")
            s.spacingX = value;
        else if (f == "spacingY")
            s.spacingY = value;
        else if (f == "radius")
            s.radius = value;
        else if (f == "countX")
            s.countX = static_cast<int>(std::lround(value));
        else if (f == "amplitude")
            s.amplitude = value;
        else if (f.rfind("element/", 0) == 0) {
            const std::string tail = f.substr(8);
            const size_t sl = tail.find('/');
            if (sl == std::string::npos)
                return false;
            const int index = std::stoi(tail.substr(0, sl));
            const char* field = fieldName(tail.substr(sl + 1));
            if (!field)
                return false;
            if (index < 0 || index >= s.elementCount())
                return Result<bool>::failure("'" + object->state.name + "' has no element " + std::to_string(index));
            ElementExcitation& x = s.overrides[index];
            const std::string fs = field;
            if (fs == "amplitude")
                x.amplitude = value;
            else if (fs == "phase")
                x.phase = value;
            else if (fs == "delay")
                x.delay = value;
            else
                x.active = std::lround(value) != 0;
        } else
            return false;
        if (Status v = s.validate(); !v)
            return Result<bool>::failure("'" + object->state.name + "': " + v.message());
        state.component = copy;
    } else if (what.rfind("antenna/", 0) == 0) {
        const auto* a = as<AntennaComponent>(*object);
        if (!a)
            return false;
        auto copy = std::make_shared<AntennaComponent>(*a);
        if (what == "antenna/frequency")
            copy->settings.frequencyHz = value;
        else if (what == "antenna/referenceImpedance")
            copy->settings.referenceImpedance = value;
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

Evaluation EmAdapter::evaluate(const Bindings& bindings, const std::string& modelId, JobContext*) const
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
    const Project& project = *built.value();
    const SceneObject* o = project.scene.find(Id(objectId_));
    if (!isArray_) {
        const AntennaReport r = analyzeAntenna(project, o->id());
        if (!r.analytical || !r.impedance) {
            e.status = Evaluation::Status::Refused;
            e.message = r.notes.empty() ? "no analytical description" : r.notes.front();
            return e;
        }
        if (!r.impedance->valid) {
            e.status = Evaluation::Status::Refused;
            e.message = "near antiresonance the induced-EMF model has no valid input impedance";
            return e;
        }
        e.quantities["antenna.inputResistance"] = r.impedance->input.real();
        e.quantities["antenna.inputReactance"] = r.impedance->input.imag();
        if (r.matching) {
            const double g = std::abs(r.matching->reflection);
            e.quantities["antenna.reflectionMagnitude"] = g;
            e.quantities["antenna.returnLossDb"] = r.matching->returnLossDb;
            if (std::isfinite(r.matching->vswr))
                e.quantities["antenna.vswr"] = r.matching->vswr;
        }
        const auto dbi = [](double v) { return v > 0.0 ? 10.0 * std::log10(v) : -200.0; };
        e.quantities["antenna.directivityDbi"] = dbi(r.directivity);
        e.quantities["antenna.gainDbi"] = dbi(r.gain);
        e.quantities["antenna.realizedGainDbi"] = dbi(r.realizedGain);
        e.quantities["antenna.efficiency"] = r.radiationEfficiency;
        e.notes = r.notes;
        e.model.settings = {{"frequencyHz", r.frequencyHz}, {"model", r.model}};
        return e;
    }
    const EmArraySettings& s = as<EmArrayComponent>(*o)->settings;
    const ArrayModel model = arrayModelOf(s);
    const double f = s.frequencyHz, lambda = kC / f, omega = 2.0 * kPi * f;
    std::function<double(const Vec3&)> intensity;
    double resolution = configuration_.value("resolutionDeg", modelId == kExact ? 2.0 : 1.0);
    if (modelId == kArrayFactor) {
        intensity = [&](const Vec3& d) { return std::norm(farFieldResponse(model, d, f)); };
    } else {
        const double distance = configuration_.value("distance", 100.0 * lambda);
        const double length = s.pattern == ElementPattern::HalfWaveDipole ? 0.5 * lambda : 0.02 * lambda;
        e.model.settings["distance"] = distance;
        e.model.settings["elementLength"] = length;
        intensity = [&, distance, length](const Vec3& d) {
            const Vec3 point = distance * d;
            Eigen::Vector3cd field = Eigen::Vector3cd::Zero();
            for (const ArrayElement& el : model.elements)
                if (el.active) {
                    const Complex w = el.weight * std::polar(1.0, -omega * el.delay);
                    field += thinDipoleField(point - el.position, length, w, f).e;
                }
            return field.squaredNorm() * distance * distance;
        };
    }
    const Metrics m = analyze(intensity, resolution, directions_.empty() ? std::optional<Vec3>() : std::optional<Vec3>(directions_.front().vector));
    if (!(m.maxU > 0.0)) {
        e.status = Evaluation::Status::InvalidDesign;
        e.message = "the array radiates nothing (every element inactive or of zero weight)";
        return e;
    }
    const double dbi = 10.0 * std::log10(m.directivity);
    e.quantities["pattern.directivityDbi"] = dbi;
    e.quantities["pattern.sidelobeDb"] = m.sidelobeDb;
    if (std::isfinite(m.beamwidthTheta))
        e.quantities["pattern.beamwidthThetaDeg"] = m.beamwidthTheta;
    if (std::isfinite(m.beamwidthPhi))
        e.quantities["pattern.beamwidthPhiDeg"] = m.beamwidthPhi;
    if (!directions_.empty())
        e.quantities["pattern.mainLobeErrorDeg"] = angleDeg(m.main, directions_.front().vector);
    for (const Direction& d : directions_) {
        const double level = 10.0 * std::log10(std::max(intensity(d.vector), 1e-300) / m.maxU);
        e.quantities["pattern.levelDb/" + d.label] = std::max(level, -300.0);
        e.quantities["pattern.gainDbi/" + d.label] = dbi + std::max(level, -300.0);
    }
    double effort = 0.0, maxWeight = 0.0, minSpacing = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < model.elements.size(); ++i) {
        if (!model.elements[i].active)
            continue;
        effort += std::norm(model.elements[i].weight);
        maxWeight = std::max(maxWeight, std::abs(model.elements[i].weight));
        for (size_t j = i + 1; j < model.elements.size(); ++j)
            minSpacing = std::min(minSpacing, (model.elements[i].position - model.elements[j].position).norm());
    }
    e.quantities["array.effort"] = effort;
    e.quantities["array.maxWeight"] = maxWeight;
    if (std::isfinite(minSpacing))
        e.quantities["array.minSpacingWavelengths"] = minSpacing / lambda;
    e.model.settings["frequencyHz"] = f;
    e.model.settings["resolutionDeg"] = resolution;
    e.model.settings["elements"] = model.elements.size();
    e.details["mainLobe"] = {{"x", m.main.x()}, {"y", m.main.y()}, {"z", m.main.z()}, {"lobeFraction", m.mainLobeFraction}};
    return e;
}

Result<opt::LinearSystem> EmAdapter::linearSystem(const Bindings& bindings, const std::string& modelId) const
{
    using R = Result<opt::LinearSystem>;
    if (!isArray_)
        return R::failure("inverse design needs an array");
    if (modelId != kArrayFactor)
        return R::failure("inverse design uses the array-factor model; re-evaluate the result with the exact-field model");
    auto built = build(bindings);
    if (!built)
        return R::failure(built.message());
    const SceneObject* o = built.value()->scene.find(Id(objectId_));
    const EmArraySettings& s = as<EmArrayComponent>(*o)->settings;
    const ArrayModel model = arrayModelOf(s);
    const double f = s.frequencyHz, k = 2.0 * kPi * f / kC;
    std::vector<const Direction*> wanted;
    for (const Direction& d : directions_)
        if (d.levelDb || d.null)
            wanted.push_back(&d);
    if (wanted.empty())
        return R::failure("give at least one direction a wanted level (levelDb) or mark it as a null");
    opt::LinearSystem sys;
    const Eigen::Index m = static_cast<Eigen::Index>(wanted.size()), n = static_cast<Eigen::Index>(model.elements.size());
    sys.response.resize(m, n);
    sys.desired.resize(m);
    sys.rowWeights = Eigen::VectorXd::Ones(m);
    for (Eigen::Index i = 0; i < m; ++i) {
        const Direction& d = *wanted[static_cast<size_t>(i)];
        sys.desired(i) = d.null ? Complex(0.0, 0.0) : Complex(std::pow(10.0, *d.levelDb / 20.0), 0.0);
        sys.targetLabels.push_back(d.label);
        for (Eigen::Index j = 0; j < n; ++j) {
            const ArrayElement& el = model.elements[static_cast<size_t>(j)];
            sys.response(i, j) = el.active ? elementPatternAmplitude(model.pattern, el.axis, d.vector) * std::polar(1.0, k * d.vector.dot(el.position))
                                           : Complex(0.0, 0.0);
        }
    }
    sys.current.resize(n);
    for (Eigen::Index j = 0; j < n; ++j) {
        sys.current(j) = model.elements[static_cast<size_t>(j)].weight;
        const std::string key = "array/element/" + std::to_string(j) + "/";
        sys.sourceLabels.push_back(o->state.name + " #" + std::to_string(j));
        sys.amplitudeBindings.push_back(opt::objectBinding(objectId_, key + "amplitude"));
        sys.phaseBindings.push_back(opt::objectBinding(objectId_, key + "phase"));
        sys.zeroBindings.push_back(opt::objectBinding(objectId_, key + "delay"));
    }
    sys.weightUnit = "relative";
    sys.targetUnit = "relative far field";
    sys.model = "array factor x element pattern";
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

const SceneObject* findObject(const Scene& scene, const std::string& id, bool& isArray)
{
    if (!id.empty()) {
        const SceneObject* o = scene.find(Id(id));
        if (o && as<EmArrayComponent>(*o)) {
            isArray = true;
            return o;
        }
        if (o && as<AntennaComponent>(*o) && o->state.geometry) {
            isArray = false;
            return o;
        }
        return nullptr;
    }
    const SceneObject* array = nullptr;
    const SceneObject* antenna = nullptr;
    scene.forEach([&](const SceneObject& o) {
        if (!array && as<EmArrayComponent>(o))
            array = &o;
        if (!antenna && as<AntennaComponent>(o) && o.state.geometry)
            antenna = &o;
    });
    isArray = array != nullptr;
    return array ? array : antenna;
}

}  // namespace

Result<std::shared_ptr<const opt::DesignEvaluator>> bindEmAdapter(const nlohmann::json& snapshot, const nlohmann::json& configuration)
{
    using R = Result<std::shared_ptr<const opt::DesignEvaluator>>;
    auto project = opt::projectFromSnapshot(snapshot);
    if (!project)
        return R::failure("the design snapshot could not be read: " + project.message());
    if (!configuration.is_object())
        return R::failure("the electromagnetic adapter's configuration must be an object");
    bool isArray = true;
    const SceneObject* o = findObject(project.value()->scene, configuration.value("object", std::string()), isArray);
    if (!o)
        return R::failure("the project has no electromagnetic array or antenna" +
                          (configuration.contains("object") ? " with the id '" + configuration.value("object", std::string()) + "'" : std::string()));
    return std::shared_ptr<const opt::DesignEvaluator>(std::make_shared<EmAdapter>(snapshot, configuration, *project.value(), o->id().str(), isArray));
}

void registerEmOptimization(opt::AdapterRegistry& registry)
{
    registry.add(kEmAdapterId, "Electromagnetic arrays and antennas (array factor, exact dipole fields, induced EMF)", bindEmAdapter);
    const auto arrayStart = [](const Project& project, const std::string& templateId, const std::string& name, nlohmann::json directions)
        -> Result<std::pair<opt::OptimizationProblem, std::shared_ptr<const opt::DesignEvaluator>>> {
        using R = Result<std::pair<opt::OptimizationProblem, std::shared_ptr<const opt::DesignEvaluator>>>;
        bool isArray = false;
        const SceneObject* o = findObject(project.scene, {}, isArray);
        if (!o || !isArray)
            return R::failure("the project has no electromagnetic array");
        nlohmann::json configuration{{"object", o->id().str()}, {"directions", directions}};
        auto bound = bindEmAdapter(opt::designSnapshot(project), configuration);
        if (!bound)
            return R::failure(bound.message());
        opt::OptimizationProblem p;
        p.id = templateId + "-" + Id::generate().str().substr(0, 8);
        p.name = name;
        p.templateId = templateId;
        p.adapterId = kEmAdapterId;
        p.adapterConfig = configuration;
        p.modelId = kArrayFactor;
        if (bound.value()->capabilities().model(kExact)->available)
            p.validationModelId = kExact;
        return std::make_pair(p, bound.take());
    };
    const auto elements = [](opt::OptimizationProblem& p, const opt::Capabilities& caps, const char* field, bool locked) {
        for (const opt::DesignVariable& v : caps.variables)
            if (v.key().find("/array/element/") != std::string::npos && v.key().size() > std::strlen(field) &&
                v.key().compare(v.key().size() - std::strlen(field), std::strlen(field), field) == 0) {
                opt::DesignVariable copy = v;
                copy.locked = locked;
                p.space.variables.push_back(copy);
            }
    };
    const auto steering = [](const Project& project) {
        const EmSettings s = emSettingsOf(project);
        return nlohmann::json::array({{{"label", "target"}, {"thetaDeg", s.steerThetaDeg != 0.0 ? s.steerThetaDeg : 30.0}, {"phiDeg", s.steerPhiDeg}}});
    };
    registry.addTemplate({"em.beam-steering", "Electromagnetic: steer an array to a direction (element phases)",
                          "Maximize the directivity toward the steering direction (the EM settings' steering angles, 30 degrees when none) "
                          "by the element phases, with the array-factor model; the analytical answer is the phase steering -k r^.r_n.",
                          kEmAdapterId, [arrayStart, elements, steering](const Project& project) -> Result<nlohmann::json> {
                              auto t = arrayStart(project, "em.beam-steering", "Steer the array", steering(project));
                              if (!t)
                                  return Result<nlohmann::json>::failure(t.message());
                              auto& [p, ev] = t.value();
                              elements(p, ev->capabilities(), "/phase", false);
                              p.objectives.push_back(objective("gain", "Directivity toward the target", "pattern.gainDbi/target", opt::Sense::Maximize, 1.0));
                              p.algorithm.algorithm = "lbfgs";
                              p.algorithm.maxEvaluations = 600;
                              return opt::toJson(p);
                          }});
    registry.addTemplate({"em.sidelobe-tradeoff", "Electromagnetic: directivity against sidelobes (Pareto)",
                          "Element amplitudes for two competing objectives - directivity and peak sidelobe level - with NSGA-II; compare "
                          "the front with the Dolph-Chebyshev taper.",
                          kEmAdapterId, [arrayStart, elements](const Project& project) -> Result<nlohmann::json> {
                              auto t = arrayStart(project, "em.sidelobe-tradeoff", "Directivity against sidelobes",
                                                  nlohmann::json::array({{{"label", "broadside"}, {"thetaDeg", 90.0}, {"phiDeg", 90.0}}}));
                              if (!t)
                                  return Result<nlohmann::json>::failure(t.message());
                              auto& [p, ev] = t.value();
                              elements(p, ev->capabilities(), "/amplitude", false);
                              for (opt::DesignVariable& v : p.space.variables) {
                                  v.lower = 0.05;
                                  v.upper = 1.0;
                                  v.initial = std::clamp(v.initial, v.lower, v.upper);
                              }
                              p.objectives.push_back(objective("directivity", "Directivity", "pattern.directivityDbi", opt::Sense::Maximize, 1.0));
                              p.objectives.push_back(objective("sidelobes", "Peak sidelobe level", "pattern.sidelobeDb", opt::Sense::Minimize, 1.0));
                              p.algorithm.algorithm = "nsga2";
                              p.algorithm.maxEvaluations = 1200;
                              return opt::toJson(p);
                          }});
    registry.addTemplate({"em.null-placement", "Electromagnetic: keep the beam, place a null",
                          "Maximize the directivity toward the target while a hard constraint keeps the level toward the null direction "
                          "40 dB below the main lobe; element amplitudes and phases.",
                          kEmAdapterId, [arrayStart, elements, steering](const Project& project) -> Result<nlohmann::json> {
                              nlohmann::json directions = steering(project);
                              directions.push_back({{"label", "null"}, {"thetaDeg", 60.0}, {"phiDeg", 0.0}, {"null", true}});
                              auto t = arrayStart(project, "em.null-placement", "Keep the beam, place a null", directions);
                              if (!t)
                                  return Result<nlohmann::json>::failure(t.message());
                              auto& [p, ev] = t.value();
                              elements(p, ev->capabilities(), "/amplitude", false);
                              elements(p, ev->capabilities(), "/phase", false);
                              p.objectives.push_back(objective("gain", "Directivity toward the target", "pattern.gainDbi/target", opt::Sense::Maximize, 1.0));
                              opt::DesignConstraint c;
                              c.id = "null";
                              c.label = "Level toward the null";
                              c.quantity = "pattern.levelDb/null";
                              c.relation = opt::Relation::AtMost;
                              c.limit = -40.0;
                              c.scale = 10.0;
                              c.category = opt::ConstraintCategory::Physical;
                              p.constraints.push_back(c);
                              p.algorithm.algorithm = "nelder-mead";
                              p.algorithm.maxEvaluations = 3000;
                              return opt::toJson(p);
                          }});
    registry.addTemplate({"em.dipole-reactance", "Electromagnetic: dipole length for zero input reactance",
                          "Sweep the dipole length for an input reactance of zero (induced-EMF model); compare with the analytical "
                          "resonant length.",
                          kEmAdapterId, [](const Project& project) -> Result<nlohmann::json> {
                              bool isArray = true;
                              const SceneObject* o = nullptr;
                              project.scene.forEach([&](const SceneObject& x) {
                                  if (!o && as<AntennaComponent>(x) && x.state.geometry && x.state.geometry->kindId() == "antenna.dipole")
                                      o = &x;
                              });
                              (void)isArray;
                              if (!o)
                                  return Result<nlohmann::json>::failure("the project has no dipole antenna");
                              nlohmann::json configuration{{"object", o->id().str()}};
                              auto bound = bindEmAdapter(opt::designSnapshot(project), configuration);
                              if (!bound)
                                  return Result<nlohmann::json>::failure(bound.message());
                              opt::OptimizationProblem p;
                              p.id = "em.dipole-reactance-" + Id::generate().str().substr(0, 8);
                              p.name = "Dipole length for zero reactance";
                              p.templateId = "em.dipole-reactance";
                              p.adapterId = kEmAdapterId;
                              p.adapterConfig = configuration;
                              p.modelId = kAntenna;
                              const opt::DesignVariable* v = bound.value()->capabilities().variable(opt::objectBinding(o->id().str(), "geometry/length"));
                              if (!v)
                                  return Result<nlohmann::json>::failure("the dipole length cannot be changed");
                              opt::DesignVariable length = *v;
                              length.lower = 0.8 * v->initial;
                              length.upper = 1.1 * v->initial;
                              p.space.variables.push_back(length);
                              p.objectives.push_back(objective("reactance", "Input reactance", "antenna.inputReactance", opt::Sense::Target, 1.0, 0.0));
                              p.algorithm.algorithm = "nelder-mead";
                              p.algorithm.initialStep = 0.05;
                              p.algorithm.tolerance = 1e-10;
                              p.algorithm.maxEvaluations = 80;
                              return opt::toJson(p);
                          }});
}

}  // namespace resonant::em
