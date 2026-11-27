#pragma once

// Scene objects of the mechanical domain.
//
//   Structure          a component on an object with geometry: how the
//                      object is idealized for structural analysis (beam,
//                      plate, membrane, solid), its mesh size and supports.
//   Actuator           an object that applies a force to a structure.
//   Sensor             a measurement point on a structure.
//   Vibration target   a point where a displacement is wanted (or none).
//
// The actuator's commanded drive and the force it produces are distinct:
// the force is what this milestone models; a physical device's response
// (shaker, piezo, voice coil) is named but not modelled.
#include "dsp/Waveform.h"
#include "foundation/Id.h"
#include "scene/Component.h"

#include <array>
#include <optional>

namespace resonant::mechanics {

namespace object_types {
inline constexpr const char* Actuator = "mechanical.actuator";
inline constexpr const char* Sensor = "mechanical.sensor";
inline constexpr const char* VibrationTarget = "mechanical.target";
}  // namespace object_types
inline constexpr const char* kStructureComponentType = "mechanical.structure";

enum class Idealization { Beam, Plate, Membrane, Solid };
// Support of a beam end, plate edge or solid face. Pinned: translations
// held (a plate edge: simply supported); Clamped: everything held.
enum class Support { Free, Pinned, Clamped };

std::string_view toString(Idealization idealization);
std::string_view toString(Support support);

struct StructureSettings {
    Idealization idealization = Idealization::Plate;
    // Beam: the local axis along the beam; Plate / Membrane: the local axis
    // normal to the plane; Solid: unused. -1 = automatic (the longest axis
    // for a beam, the shortest for a plate).
    int axis = -1;
    double elementSize = 0.0;  // m; 0 = automatic
    // Beam: [0] start, [1] end of the axis. Plate / Membrane: edges at the
    // start and end of the first in-plane axis, then of the second. Solid:
    // faces -X, +X, -Y, +Y, -Z, +Z (local). A membrane is held at every edge.
    std::array<Support, 6> supports{Support::Clamped, Support::Free, Support::Free, Support::Free, Support::Free, Support::Free};
    double thickness = 0.0;   // m; Plate / Membrane on a shape without thickness (plane)
    double tension = 100.0;   // N/m, Membrane pre-tension

    Status validate() const;
    friend bool operator==(const StructureSettings&, const StructureSettings&) = default;
};

class StructureComponent final : public Component {
public:
    explicit StructureComponent(StructureSettings initial = {}) : settings(std::move(initial)) {}
    StructureSettings settings;
    std::string typeId() const override { return kStructureComponentType; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

enum class Excitation {
    PointForce,       // at the nearest node
    DistributedForce  // spread evenly over the nodes within a radius
};
std::string_view toString(Excitation excitation);

struct ActuatorSettings {
    Excitation excitation = Excitation::PointForce;
    double amplitude = 1.0;   // N, peak force; acts along the actuator's local +Z axis
    double phase = 0.0;       // rad
    double delay = 0.0;       // s
    dsp::Waveform waveform;   // the force over time (transient analysis)
    double radius = 0.0;      // m, DistributedForce
    bool active = true;
    Id structure;             // the structure object it acts on; null = the nearest structure
    // A physical device named here is NOT modelled: the force above is
    // assumed to reach the structure as set.
    bool physicalDevice = false;
    std::string deviceProfile;

    Status validate() const;
    friend bool operator==(const ActuatorSettings&, const ActuatorSettings&) = default;
};

class ActuatorComponent final : public Component {
public:
    explicit ActuatorComponent(ActuatorSettings initial = {}) : settings(std::move(initial)) {}
    ActuatorSettings settings;
    std::string typeId() const override { return object_types::Actuator; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

enum class Quantity { Displacement, Velocity, Acceleration };
std::string_view toString(Quantity quantity);

struct SensorSettings {
    Quantity quantity = Quantity::Acceleration;  // what a real sensor at this point measures
    Id structure;
    // Calibration of a physical sensor, for measured data (not used by the solvers).
    double sensitivity = 0.0;  // output per unit of the quantity, 0 = unknown
    std::string sensitivityUnit;
    std::string sensorModel;

    friend bool operator==(const SensorSettings&, const SensorSettings&) = default;
};

class SensorComponent final : public Component {
public:
    explicit SensorComponent(SensorSettings initial = {}) : settings(std::move(initial)) {}
    SensorSettings settings;
    std::string typeId() const override { return object_types::Sensor; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

struct VibrationTargetSettings {
    double amplitude = 1e-6;  // m, peak displacement wanted along the target's local +Z axis
    double phase = 0.0;
    double weight = 1.0;
    bool suppress = false;    // the displacement should be zero
    Id structure;

    Status validate() const;
    friend bool operator==(const VibrationTargetSettings&, const VibrationTargetSettings&) = default;
};

class VibrationTargetComponent final : public Component {
public:
    explicit VibrationTargetComponent(VibrationTargetSettings initial = {}) : settings(std::move(initial)) {}
    VibrationTargetSettings settings;
    std::string typeId() const override { return object_types::VibrationTarget; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

void registerMechanicalComponents(ComponentRegistry& registry);

}  // namespace resonant::mechanics
