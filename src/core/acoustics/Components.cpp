#include "acoustics/Components.h"

#include "foundation/Units.h"
#include "project/ProjectSerializer.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace resonant::acoustics {

using nlohmann::json;

// --- names ----------------------------------------------------------------------

std::string_view toString(DirectivityModel model)
{
    switch (model) {
    case DirectivityModel::Omnidirectional: return "omnidirectional";
    case DirectivityModel::Cardioid: return "cardioid";
    case DirectivityModel::BaffledPiston: return "baffledPiston";
    }
    return "omnidirectional";
}

std::string_view toString(ArrayLayout layout)
{
    switch (layout) {
    case ArrayLayout::Linear: return "linear";
    case ArrayLayout::RectangularPlanar: return "rectangularPlanar";
    case ArrayLayout::Circular: return "circular";
    case ArrayLayout::Cylindrical: return "cylindrical";
    case ArrayLayout::Spherical: return "spherical";
    case ArrayLayout::Custom: return "custom";
    }
    return "linear";
}

std::string_view toString(ElementFacing facing)
{
    switch (facing) {
    case ElementFacing::Axial: return "axial";
    case ElementFacing::Outward: return "outward";
    case ElementFacing::Inward: return "inward";
    }
    return "axial";
}

std::string_view toString(TargetKind kind)
{
    switch (kind) {
    case TargetKind::Point: return "point";
    case TargetKind::Line: return "line";
    case TargetKind::Plane: return "plane";
    case TargetKind::Surface: return "surface";
    case TargetKind::Volume: return "volume";
    }
    return "point";
}

std::string_view toString(RegionKind kind)
{
    switch (kind) {
    case RegionKind::PlaneXY: return "planeXY";
    case RegionKind::PlaneXZ: return "planeXZ";
    case RegionKind::PlaneYZ: return "planeYZ";
    case RegionKind::Volume: return "volume";
    }
    return "planeXZ";
}

namespace {

template <typename Enum, size_t N>
bool parseEnum(const std::string& text, const Enum (&values)[N], Enum& out)
{
    for (const Enum value : values) {
        if (toString(value) == text) {
            out = value;
            return true;
        }
    }
    return false;
}

constexpr DirectivityModel kDirectivityModels[] = {DirectivityModel::Omnidirectional, DirectivityModel::Cardioid,
                                                   DirectivityModel::BaffledPiston};
constexpr ArrayLayout kLayouts[] = {ArrayLayout::Linear,      ArrayLayout::RectangularPlanar, ArrayLayout::Circular,
                                    ArrayLayout::Cylindrical, ArrayLayout::Spherical,         ArrayLayout::Custom};
constexpr ElementFacing kFacings[] = {ElementFacing::Axial, ElementFacing::Outward, ElementFacing::Inward};
constexpr TargetKind kTargetKinds[] = {TargetKind::Point, TargetKind::Line, TargetKind::Plane, TargetKind::Surface,
                                       TargetKind::Volume};
constexpr RegionKind kRegionKinds[] = {RegionKind::PlaneXY, RegionKind::PlaneXZ, RegionKind::PlaneYZ,
                                       RegionKind::Volume};

json emitterToJson(const EmitterSettings& s)
{
    return {{"frequencyHz", s.frequencyHz},
            {"amplitudePa", s.amplitudePa},
            {"phaseRad", s.phaseRad},
            {"delaySec", s.delaySec},
            {"waveform", s.waveform},
            {"directivity",
             {{"model", std::string(toString(s.directivity.model))},
              {"order", s.directivity.order},
              {"pistonRadius", s.directivity.pistonRadius}}},
            {"active", s.active},
            {"physicalTransducer", s.physicalTransducer},
            {"radius", s.radius},
            {"transducerProfile", s.transducerProfile}};
}

// Throws json::exception or std::runtime_error; callers convert to Result.
EmitterSettings emitterFromJson(const json& v)
{
    EmitterSettings s;
    s.frequencyHz = v.at("frequencyHz").get<double>();
    s.amplitudePa = v.at("amplitudePa").get<double>();
    s.phaseRad = v.at("phaseRad").get<double>();
    s.delaySec = v.at("delaySec").get<double>();
    s.waveform = v.value("waveform", std::string("sine"));
    const json& d = v.at("directivity");
    if (!parseEnum(d.at("model").get<std::string>(), kDirectivityModels, s.directivity.model))
        throw std::runtime_error("unknown directivity model '" + d.at("model").get<std::string>() + "'");
    s.directivity.order = d.value("order", 1.0);
    s.directivity.pistonRadius = d.value("pistonRadius", 0.005);
    s.active = v.value("active", true);
    s.physicalTransducer = v.value("physicalTransducer", false);
    s.radius = v.value("radius", 0.005);
    s.transducerProfile = v.value("transducerProfile", std::string());
    if (Status status = s.validate(); !status)
        throw std::runtime_error(status.message());
    return s;
}

template <typename Function>
Result<ComponentPtr> guarded(const char* what, Function&& build)
{
    try {
        return ComponentPtr(build());
    } catch (const json::exception& error) {
        return Result<ComponentPtr>::failure(std::string("invalid ") + what + ": " + error.what());
    } catch (const std::runtime_error& error) {
        return Result<ComponentPtr>::failure(std::string("invalid ") + what + ": " + error.what());
    }
}

template <typename T>
bool sameSettings(const T& self, const Component& other)
{
    const auto* typed = dynamic_cast<const T*>(&other);
    return typed && typed->settings == self.settings;
}

}  // namespace

// --- emitter -----------------------------------------------------------------------

double Directivity::evaluate(double cosTheta, double wavenumber) const
{
    cosTheta = std::clamp(cosTheta, -1.0, 1.0);
    switch (model) {
    case DirectivityModel::Omnidirectional: return 1.0;
    case DirectivityModel::Cardioid: return std::pow(0.5 * (1.0 + cosTheta), order);
    case DirectivityModel::BaffledPiston: {
        if (cosTheta <= 0.0)
            return 0.0;  // an infinite baffle radiates into the front half-space only
        const double x = wavenumber * pistonRadius * std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));
        if (x < 1e-6)
            return 1.0 - x * x / 8.0;  // series of 2 J1(x) / x
        return 2.0 * std::cyl_bessel_j(1.0, x) / x;
    }
    }
    return 1.0;
}

Status EmitterSettings::validate() const
{
    if (!(std::isfinite(frequencyHz) && frequencyHz > 0.0))
        return Status::failure("the frequency must be greater than zero");
    if (!(std::isfinite(amplitudePa) && amplitudePa >= 0.0))
        return Status::failure("the amplitude must not be negative");
    if (!std::isfinite(phaseRad))
        return Status::failure("the phase must be a finite number");
    if (!(std::isfinite(delaySec) && delaySec >= 0.0))
        return Status::failure("the time delay must not be negative");
    if (!(std::isfinite(radius) && radius > 0.0))
        return Status::failure("the emitter radius must be greater than zero");
    if (!(std::isfinite(directivity.order) && directivity.order >= 0.0))
        return Status::failure("the directivity order must not be negative");
    if (!(std::isfinite(directivity.pistonRadius) && directivity.pistonRadius > 0.0))
        return Status::failure("the piston radius must be greater than zero");
    return Status::success();
}

json EmitterComponent::toJson() const { return emitterToJson(settings); }
bool EmitterComponent::equals(const Component& other) const { return sameSettings(*this, other); }

// --- array --------------------------------------------------------------------------

int ArraySettings::elementCount() const
{
    switch (layout) {
    case ArrayLayout::Linear:
    case ArrayLayout::Circular:
    case ArrayLayout::Spherical: return countX;
    case ArrayLayout::RectangularPlanar:
    case ArrayLayout::Cylindrical: return countX * countY;
    case ArrayLayout::Custom: return static_cast<int>(customPositions.size());
    }
    return 0;
}

Status ArraySettings::validate() const
{
    if (countX < 1 || countY < 1)
        return Status::failure("an array needs at least one element in each direction");
    if (static_cast<long long>(countX) * countY > 100000)
        return Status::failure("an array is limited to 100 000 elements");
    if (!(std::isfinite(spacingX) && spacingX > 0.0 && std::isfinite(spacingY) && spacingY > 0.0))
        return Status::failure("the element spacing must be greater than zero");
    if (!(std::isfinite(radius) && radius > 0.0))
        return Status::failure("the array radius must be greater than zero");
    for (const Vec3& position : customPositions)
        if (!position.allFinite())
            return Status::failure("custom element positions must be finite");
    return element.validate();
}

std::vector<ArrayElement> generateElements(const ArraySettings& s)
{
    std::vector<ArrayElement> elements;
    const auto add = [&](const Vec3& position, const Vec3& radial) {
        ArrayElement element;
        element.index = static_cast<int>(elements.size());
        element.localPosition = position;
        switch (s.facing) {
        case ElementFacing::Axial: element.localForward = Vec3::UnitZ(); break;
        case ElementFacing::Outward: element.localForward = radial.norm() > 1e-12 ? radial.normalized() : Vec3::UnitZ(); break;
        case ElementFacing::Inward: element.localForward = radial.norm() > 1e-12 ? Vec3(-radial.normalized()) : Vec3::UnitZ(); break;
        }
        element.settings = s.element;
        if (const auto it = s.overrides.find(element.index); it != s.overrides.end()) {
            const ElementOverride& o = it->second;
            if (o.amplitudePa) element.settings.amplitudePa = *o.amplitudePa;
            if (o.phaseRad) element.settings.phaseRad = *o.phaseRad;
            if (o.delaySec) element.settings.delaySec = *o.delaySec;
            if (o.frequencyHz) element.settings.frequencyHz = *o.frequencyHz;
            if (o.active) element.settings.active = *o.active;
        }
        elements.push_back(std::move(element));
    };

    switch (s.layout) {
    case ArrayLayout::Linear:
        for (int i = 0; i < s.countX; ++i)
            add(Vec3((i - 0.5 * (s.countX - 1)) * s.spacingX, 0.0, 0.0), Vec3::Zero());
        break;
    case ArrayLayout::RectangularPlanar:
        for (int j = 0; j < s.countY; ++j)
            for (int i = 0; i < s.countX; ++i)
                add(Vec3((i - 0.5 * (s.countX - 1)) * s.spacingX, (j - 0.5 * (s.countY - 1)) * s.spacingY, 0.0),
                    Vec3::Zero());
        break;
    case ArrayLayout::Circular:
    case ArrayLayout::Cylindrical: {
        const int rings = s.layout == ArrayLayout::Cylindrical ? s.countY : 1;
        for (int ring = 0; ring < rings; ++ring) {
            const double z = (ring - 0.5 * (rings - 1)) * s.spacingY;
            for (int i = 0; i < s.countX; ++i) {
                const double angle = 2.0 * units::pi * i / s.countX;
                const Vec3 radial(std::cos(angle), std::sin(angle), 0.0);
                add(s.radius * radial + Vec3(0.0, 0.0, z), radial);
            }
        }
        break;
    }
    case ArrayLayout::Spherical: {
        // Fibonacci lattice: nearly uniform, not exactly equal-area, and
        // without any symmetry; adequate for exploring spherical arrays.
        const double golden = units::pi * (3.0 - std::sqrt(5.0));
        for (int i = 0; i < s.countX; ++i) {
            const double z = s.countX == 1 ? 1.0 : 1.0 - 2.0 * (i + 0.5) / s.countX;
            const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
            const Vec3 radial(r * std::cos(golden * i), r * std::sin(golden * i), z);
            add(s.radius * radial, radial);
        }
        break;
    }
    case ArrayLayout::Custom:
        for (const Vec3& position : s.customPositions)
            add(position, position);
        break;
    }
    return elements;
}

json ArrayComponent::toJson() const
{
    json custom = json::array();
    for (const Vec3& position : settings.customPositions)
        custom.push_back(resonant::toJson(position));
    json overrides = json::object();
    for (const auto& [index, o] : settings.overrides) {
        if (o.empty())
            continue;
        json entry = json::object();
        if (o.amplitudePa) entry["amplitudePa"] = *o.amplitudePa;
        if (o.phaseRad) entry["phaseRad"] = *o.phaseRad;
        if (o.delaySec) entry["delaySec"] = *o.delaySec;
        if (o.frequencyHz) entry["frequencyHz"] = *o.frequencyHz;
        if (o.active) entry["active"] = *o.active;
        overrides[std::to_string(index)] = std::move(entry);
    }
    return {{"layout", std::string(toString(settings.layout))},
            {"countX", settings.countX},
            {"countY", settings.countY},
            {"spacingX", settings.spacingX},
            {"spacingY", settings.spacingY},
            {"radius", settings.radius},
            {"facing", std::string(toString(settings.facing))},
            {"customPositions", std::move(custom)},
            {"element", emitterToJson(settings.element)},
            {"overrides", std::move(overrides)}};
}

bool ArrayComponent::equals(const Component& other) const { return sameSettings(*this, other); }

// --- target, probe, region ------------------------------------------------------------

Status TargetSettings::validate() const
{
    if (!(std::isfinite(desiredAmplitudePa) && desiredAmplitudePa >= 0.0))
        return Status::failure("the desired amplitude must not be negative");
    if (!std::isfinite(desiredPhaseRad))
        return Status::failure("the desired phase must be a finite number");
    if (!(std::isfinite(weight) && weight > 0.0))
        return Status::failure("a target weight must be greater than zero");
    return Status::success();
}

json TargetComponent::toJson() const
{
    return {{"kind", std::string(toString(settings.kind))},
            {"desiredAmplitudePa", settings.desiredAmplitudePa},
            {"desiredPhaseRad", settings.desiredPhaseRad},
            {"weight", settings.weight},
            {"suppress", settings.suppress},
            {"extent", resonant::toJson(settings.extent)}};
}

bool TargetComponent::equals(const Component& other) const { return sameSettings(*this, other); }

json ProbeComponent::toJson() const { return json::object(); }
bool ProbeComponent::equals(const Component& other) const { return dynamic_cast<const ProbeComponent*>(&other) != nullptr; }

size_t RegionSettings::pointCount() const
{
    return static_cast<size_t>(countU) * static_cast<size_t>(countV) * static_cast<size_t>(isVolume() ? countW : 1);
}

Vec3 RegionSettings::localAxisU() const
{
    return kind == RegionKind::PlaneYZ ? Vec3(0.0, size.y(), 0.0) : Vec3(size.x(), 0.0, 0.0);
}

Vec3 RegionSettings::localAxisV() const
{
    switch (kind) {
    case RegionKind::PlaneXY:
    case RegionKind::Volume: return Vec3(0.0, size.y(), 0.0);
    case RegionKind::PlaneXZ:
    case RegionKind::PlaneYZ: return Vec3(0.0, 0.0, size.z());
    }
    return Vec3::Zero();
}

Vec3 RegionSettings::localAxisW() const
{
    return isVolume() ? Vec3(0.0, 0.0, size.z()) : Vec3::Zero();
}

Vec3 RegionSettings::localOrigin() const
{
    return -0.5 * (localAxisU() + localAxisV() + localAxisW());
}

Vec3 RegionSettings::localPoint(int iu, int iv, int iw) const
{
    const auto fraction = [](int index, int count) { return count > 1 ? static_cast<double>(index) / (count - 1) : 0.5; };
    return localOrigin() + fraction(iu, countU) * localAxisU() + fraction(iv, countV) * localAxisV()
           + (isVolume() ? fraction(iw, countW) : 0.0) * localAxisW();
}

Status RegionSettings::validate() const
{
    if (!(size.allFinite() && (size.array() > 0.0).all()))
        return Status::failure("the region size must be greater than zero in every direction");
    if (countU < 2 || countV < 2 || (isVolume() && countW < 2))
        return Status::failure("a region needs at least two samples in every sampled direction");
    return Status::success();
}

json RegionComponent::toJson() const
{
    return {{"kind", std::string(toString(settings.kind))},
            {"size", resonant::toJson(settings.size)},
            {"countU", settings.countU},
            {"countV", settings.countV},
            {"countW", settings.countW}};
}

bool RegionComponent::equals(const Component& other) const { return sameSettings(*this, other); }

// --- construction and registration ---------------------------------------------------

namespace {
std::unique_ptr<SceneObject> makeObject(const char* type, std::string name, ComponentPtr component)
{
    auto object = std::make_unique<SceneObject>(Id::generate(), type);
    object->state.name = std::move(name);
    object->state.component = std::move(component);
    return object;
}

Vec3 vec3Of(const json& value, const char* what)
{
    auto result = vec3FromJson(value, what);
    if (!result)
        throw std::runtime_error(result.message());
    return result.take();
}
}  // namespace

std::unique_ptr<SceneObject> makeEmitter(std::string name, EmitterSettings settings)
{
    return makeObject(object_types::Emitter, std::move(name), std::make_shared<EmitterComponent>(std::move(settings)));
}

std::unique_ptr<SceneObject> makeArray(std::string name, ArraySettings settings)
{
    return makeObject(object_types::Array, std::move(name), std::make_shared<ArrayComponent>(std::move(settings)));
}

std::unique_ptr<SceneObject> makeTarget(std::string name, TargetSettings settings)
{
    return makeObject(object_types::Target, std::move(name), std::make_shared<TargetComponent>(std::move(settings)));
}

std::unique_ptr<SceneObject> makeProbe(std::string name)
{
    return makeObject(object_types::Probe, std::move(name), std::make_shared<ProbeComponent>());
}

std::unique_ptr<SceneObject> makeRegion(std::string name, RegionSettings settings)
{
    return makeObject(object_types::Region, std::move(name), std::make_shared<RegionComponent>(std::move(settings)));
}

void registerAcousticComponents(ComponentRegistry& registry)
{
    registry.add(object_types::Emitter, [](const json& v) {
        return guarded("acoustic emitter", [&] { return std::make_shared<EmitterComponent>(emitterFromJson(v)); });
    });
    registry.add(object_types::Array, [](const json& v) {
        return guarded("acoustic array", [&] {
            ArraySettings s;
            if (!parseEnum(v.at("layout").get<std::string>(), kLayouts, s.layout))
                throw std::runtime_error("unknown layout '" + v.at("layout").get<std::string>() + "'");
            s.countX = v.at("countX").get<int>();
            s.countY = v.at("countY").get<int>();
            s.spacingX = v.at("spacingX").get<double>();
            s.spacingY = v.at("spacingY").get<double>();
            s.radius = v.at("radius").get<double>();
            if (!parseEnum(v.value("facing", std::string("axial")), kFacings, s.facing))
                throw std::runtime_error("unknown element facing");
            const json customPositions = v.value("customPositions", json::array());
            for (const json& position : customPositions)
                s.customPositions.push_back(vec3Of(position, "custom element position"));
            s.element = emitterFromJson(v.at("element"));
            const json overrides = v.value("overrides", json::object());
            for (const auto& [key, entry] : overrides.items()) {
                ElementOverride o;
                if (entry.contains("amplitudePa")) o.amplitudePa = entry["amplitudePa"].get<double>();
                if (entry.contains("phaseRad")) o.phaseRad = entry["phaseRad"].get<double>();
                if (entry.contains("delaySec")) o.delaySec = entry["delaySec"].get<double>();
                if (entry.contains("frequencyHz")) o.frequencyHz = entry["frequencyHz"].get<double>();
                if (entry.contains("active")) o.active = entry["active"].get<bool>();
                s.overrides[std::stoi(key)] = o;
            }
            if (Status status = s.validate(); !status)
                throw std::runtime_error(status.message());
            return std::make_shared<ArrayComponent>(std::move(s));
        });
    });
    registry.add(object_types::Target, [](const json& v) {
        return guarded("acoustic target", [&] {
            TargetSettings s;
            if (!parseEnum(v.at("kind").get<std::string>(), kTargetKinds, s.kind))
                throw std::runtime_error("unknown target kind '" + v.at("kind").get<std::string>() + "'");
            s.desiredAmplitudePa = v.at("desiredAmplitudePa").get<double>();
            s.desiredPhaseRad = v.at("desiredPhaseRad").get<double>();
            s.weight = v.value("weight", 1.0);
            s.suppress = v.value("suppress", false);
            if (v.contains("extent"))
                s.extent = vec3Of(v["extent"], "target extent");
            if (Status status = s.validate(); !status)
                throw std::runtime_error(status.message());
            return std::make_shared<TargetComponent>(s);
        });
    });
    registry.add(object_types::Probe,
                 [](const json&) { return Result<ComponentPtr>(ComponentPtr(std::make_shared<ProbeComponent>())); });
    registry.add(object_types::Region, [](const json& v) {
        return guarded("simulation region", [&] {
            RegionSettings s;
            if (!parseEnum(v.at("kind").get<std::string>(), kRegionKinds, s.kind))
                throw std::runtime_error("unknown region kind '" + v.at("kind").get<std::string>() + "'");
            s.size = vec3Of(v.at("size"), "region size");
            s.countU = v.at("countU").get<int>();
            s.countV = v.at("countV").get<int>();
            s.countW = v.value("countW", 2);
            if (Status status = s.validate(); !status)
                throw std::runtime_error(status.message());
            return std::make_shared<RegionComponent>(s);
        });
    });
}

}  // namespace resonant::acoustics
