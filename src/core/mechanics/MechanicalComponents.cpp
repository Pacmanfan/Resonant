#include "mechanics/MechanicalComponents.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <stdexcept>

namespace resonant::mechanics {
namespace {

using json = nlohmann::json;

template <typename Enum, size_t N>
Enum parse(const std::string& text, const Enum (&values)[N], const char* what)
{
    for (const Enum value : values)
        if (toString(value) == text)
            return value;
    throw std::runtime_error(std::string("unknown ") + what + " '" + text + "'");
}

constexpr Idealization kIdealizations[] = {Idealization::Beam, Idealization::Plate, Idealization::Membrane, Idealization::Solid};
constexpr Support kSupports[] = {Support::Free, Support::Pinned, Support::Clamped};
constexpr Excitation kExcitations[] = {Excitation::PointForce, Excitation::DistributedForce};
constexpr Quantity kQuantities[] = {Quantity::Displacement, Quantity::Velocity, Quantity::Acceleration};

template <typename Build>
Result<ComponentPtr> guarded(const char* what, Build&& build)
{
    try {
        return ComponentPtr(build());
    } catch (const std::exception& error) {
        return Result<ComponentPtr>::failure(std::string("invalid ") + what + ": " + error.what());
    }
}

Id idOf(const json& v, const char* key)
{
    const std::string text = v.value(key, std::string());
    return text.empty() ? Id() : Id(text);
}

}  // namespace

std::string_view toString(Idealization idealization)
{
    switch (idealization) {
    case Idealization::Beam: return "beam";
    case Idealization::Plate: return "plate";
    case Idealization::Membrane: return "membrane";
    case Idealization::Solid: return "solid";
    }
    return "plate";
}

std::string_view toString(Support support)
{
    switch (support) {
    case Support::Free: return "free";
    case Support::Pinned: return "pinned";
    case Support::Clamped: return "clamped";
    }
    return "free";
}

std::string_view toString(Excitation excitation)
{
    return excitation == Excitation::PointForce ? "pointForce" : "distributedForce";
}

std::string_view toString(Quantity quantity)
{
    switch (quantity) {
    case Quantity::Displacement: return "displacement";
    case Quantity::Velocity: return "velocity";
    case Quantity::Acceleration: return "acceleration";
    }
    return "displacement";
}

Status StructureSettings::validate() const
{
    if (axis < -1 || axis > 2)
        return Status::failure("the structural axis must be X, Y, Z or automatic");
    if (!(std::isfinite(elementSize) && elementSize >= 0.0))
        return Status::failure("the element size must be zero (automatic) or positive");
    if (!(std::isfinite(thickness) && thickness >= 0.0))
        return Status::failure("the thickness must be zero (from the shape) or positive");
    if (!(std::isfinite(tension) && tension > 0.0))
        return Status::failure("the membrane tension must be positive");
    return Status::success();
}

Status ActuatorSettings::validate() const
{
    if (!(std::isfinite(amplitude) && amplitude >= 0.0))
        return Status::failure("the force amplitude must be zero or positive");
    if (!std::isfinite(phase) || !(std::isfinite(delay) && delay >= 0.0))
        return Status::failure("the phase must be finite and the delay zero or positive");
    if (!(std::isfinite(radius) && radius >= 0.0))
        return Status::failure("the radius must be zero or positive");
    return waveform.validate();
}

Status VibrationTargetSettings::validate() const
{
    if (!(std::isfinite(amplitude) && amplitude >= 0.0) || !std::isfinite(phase))
        return Status::failure("the wanted displacement must be zero or positive, with a finite phase");
    if (!(std::isfinite(weight) && weight > 0.0))
        return Status::failure("the weight must be positive");
    return Status::success();
}

json StructureComponent::toJson() const
{
    json supports = json::array();
    for (const Support s : settings.supports)
        supports.push_back(std::string(mechanics::toString(s)));
    return {{"idealization", std::string(mechanics::toString(settings.idealization))},
            {"axis", settings.axis},
            {"elementSize", settings.elementSize},
            {"supports", supports},
            {"thickness", settings.thickness},
            {"tension", settings.tension}};
}

bool StructureComponent::equals(const Component& other) const
{
    const auto* o = dynamic_cast<const StructureComponent*>(&other);
    return o && o->settings == settings;
}

json ActuatorComponent::toJson() const
{
    return {{"excitation", std::string(mechanics::toString(settings.excitation))},
            {"amplitude", settings.amplitude},
            {"phase", settings.phase},
            {"delay", settings.delay},
            {"waveform", dsp::toJson(settings.waveform)},
            {"radius", settings.radius},
            {"active", settings.active},
            {"structure", settings.structure.str()},
            {"physicalDevice", settings.physicalDevice},
            {"deviceProfile", settings.deviceProfile}};
}

bool ActuatorComponent::equals(const Component& other) const
{
    const auto* o = dynamic_cast<const ActuatorComponent*>(&other);
    return o && o->settings == settings;
}

json SensorComponent::toJson() const
{
    return {{"quantity", std::string(mechanics::toString(settings.quantity))},
            {"structure", settings.structure.str()},
            {"sensitivity", settings.sensitivity},
            {"sensitivityUnit", settings.sensitivityUnit},
            {"sensorModel", settings.sensorModel}};
}

bool SensorComponent::equals(const Component& other) const
{
    const auto* o = dynamic_cast<const SensorComponent*>(&other);
    return o && o->settings == settings;
}

json VibrationTargetComponent::toJson() const
{
    return {{"amplitude", settings.amplitude}, {"phase", settings.phase},       {"weight", settings.weight},
            {"suppress", settings.suppress},   {"structure", settings.structure.str()}};
}

bool VibrationTargetComponent::equals(const Component& other) const
{
    const auto* o = dynamic_cast<const VibrationTargetComponent*>(&other);
    return o && o->settings == settings;
}

void registerMechanicalComponents(ComponentRegistry& registry)
{
    registry.add(kStructureComponentType, [](const json& v) {
        return guarded("structure", [&] {
            StructureSettings s;
            s.idealization = parse(v.at("idealization").get<std::string>(), kIdealizations, "idealization");
            s.axis = v.value("axis", -1);
            s.elementSize = v.value("elementSize", 0.0);
            const json supports = v.value("supports", json::array());
            for (size_t i = 0; i < supports.size() && i < 6; ++i)
                s.supports[i] = parse(supports[i].get<std::string>(), kSupports, "support");
            s.thickness = v.value("thickness", 0.0);
            s.tension = v.value("tension", 100.0);
            if (Status status = s.validate(); !status)
                throw std::runtime_error(status.message());
            return std::make_shared<StructureComponent>(s);
        });
    });
    registry.add(object_types::Actuator, [](const json& v) {
        return guarded("actuator", [&] {
            ActuatorSettings s;
            s.excitation = parse(v.value("excitation", std::string("pointForce")), kExcitations, "excitation");
            s.amplitude = v.value("amplitude", 1.0);
            s.phase = v.value("phase", 0.0);
            s.delay = v.value("delay", 0.0);
            if (v.contains("waveform")) {
                auto waveform = dsp::waveformFromJson(v["waveform"]);
                if (!waveform)
                    throw std::runtime_error(waveform.message());
                s.waveform = waveform.take();
            }
            s.radius = v.value("radius", 0.0);
            s.active = v.value("active", true);
            s.structure = idOf(v, "structure");
            s.physicalDevice = v.value("physicalDevice", false);
            s.deviceProfile = v.value("deviceProfile", std::string());
            if (Status status = s.validate(); !status)
                throw std::runtime_error(status.message());
            return std::make_shared<ActuatorComponent>(s);
        });
    });
    registry.add(object_types::Sensor, [](const json& v) {
        return guarded("sensor", [&] {
            SensorSettings s;
            s.quantity = parse(v.value("quantity", std::string("acceleration")), kQuantities, "quantity");
            s.structure = idOf(v, "structure");
            s.sensitivity = v.value("sensitivity", 0.0);
            s.sensitivityUnit = v.value("sensitivityUnit", std::string());
            s.sensorModel = v.value("sensorModel", std::string());
            return std::make_shared<SensorComponent>(s);
        });
    });
    registry.add(object_types::VibrationTarget, [](const json& v) {
        return guarded("vibration target", [&] {
            VibrationTargetSettings s;
            s.amplitude = v.value("amplitude", 1e-6);
            s.phase = v.value("phase", 0.0);
            s.weight = v.value("weight", 1.0);
            s.suppress = v.value("suppress", false);
            s.structure = idOf(v, "structure");
            if (Status status = s.validate(); !status)
                throw std::runtime_error(status.message());
            return std::make_shared<VibrationTargetComponent>(s);
        });
    });
}

}  // namespace resonant::mechanics
