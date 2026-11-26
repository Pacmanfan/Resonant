// The acoustic scene objects: emitter, array, target, probe and simulation
// region. Each is a scene object whose domain data is one of the components
// below (scene/Component.h). The object's transform places it in the world;
// everything here is in the object's local frame and in SI units.
#pragma once

#include "foundation/Types.h"
#include "scene/Scene.h"

#include <map>
#include <optional>
#include <vector>

namespace resonant::acoustics {

namespace object_types {
inline constexpr const char* Emitter = "acoustic.emitter";
inline constexpr const char* Array = "acoustic.array";
inline constexpr const char* Target = "acoustic.target";
inline constexpr const char* Probe = "acoustic.probe";
inline constexpr const char* Region = "acoustic.region";
}  // namespace object_types

// --- emitter -----------------------------------------------------------------

enum class DirectivityModel {
    Omnidirectional,  // ideal point source, D = 1
    Cardioid,         // D = ((1 + cos theta) / 2)^order
    BaffledPiston,    // D = 2 J1(k a sin theta) / (k a sin theta) in front, 0 behind
};

struct Directivity {
    DirectivityModel model = DirectivityModel::Omnidirectional;
    double order = 1.0;          // Cardioid exponent, >= 0
    double pistonRadius = 0.005; // m, BaffledPiston

    // Relative amplitude in a direction at angle theta from the emitter axis.
    double evaluate(double cosTheta, double wavenumber) const;

    friend bool operator==(const Directivity&, const Directivity&) = default;
};

struct EmitterSettings {
    double frequencyHz = 40000.0;
    double amplitudePa = 1.0;   // peak pressure amplitude at 1 m on axis, free field
    double phaseRad = 0.0;      // initial phase; positive leads
    double delaySec = 0.0;      // true time delay; multiplies by exp(-j omega delay)
    std::string waveform = "sine";  // designation only: the analytical model is harmonic
    Directivity directivity;
    bool active = true;
    // False: an idealized mathematical source. True: a model standing in for
    // a physical transducer, whose real response (frequency, phase,
    // directivity, impedance) must come from measurement or a data sheet.
    bool physicalTransducer = false;
    double radius = 0.005;           // m, physical size; also the near-source limit
    std::string transducerProfile;   // name of a measured / data-sheet profile; none is modelled yet

    Status validate() const;
    friend bool operator==(const EmitterSettings&, const EmitterSettings&) = default;
};

class EmitterComponent final : public Component {
public:
    explicit EmitterComponent(EmitterSettings initial = {}) : settings(std::move(initial)) {}
    EmitterSettings settings;

    std::string typeId() const override { return object_types::Emitter; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

// --- array --------------------------------------------------------------------

enum class ArrayLayout { Linear, RectangularPlanar, Circular, Cylindrical, Spherical, Custom };
enum class ElementFacing { Axial, Outward, Inward };  // +Z, away from / towards the array axis or centre

// Per-element settings that differ from the array's element defaults.
struct ElementOverride {
    std::optional<double> amplitudePa;
    std::optional<double> phaseRad;
    std::optional<double> delaySec;
    std::optional<double> frequencyHz;
    std::optional<bool> active;

    bool empty() const { return !amplitudePa && !phaseRad && !delaySec && !frequencyHz && !active; }
    friend bool operator==(const ElementOverride&, const ElementOverride&) = default;
};

struct ArraySettings {
    ArrayLayout layout = ArrayLayout::Linear;
    int countX = 8;             // elements along X; per ring for Circular / Cylindrical; total for Spherical
    int countY = 1;             // rows along Y (RectangularPlanar); rings along Z (Cylindrical)
    double spacingX = 0.0043;   // m, centre to centre
    double spacingY = 0.0043;   // m; ring spacing along Z for Cylindrical
    double radius = 0.05;       // m, Circular / Cylindrical / Spherical
    ElementFacing facing = ElementFacing::Axial;
    std::vector<Vec3> customPositions;  // Custom layout, local coordinates
    EmitterSettings element;            // defaults of every element

    // Overrides by element index. Indices are stable for a given layout and
    // counts; an override whose index is beyond the current element count is
    // kept (it applies again if the array grows back) and is ignored
    // meanwhile.
    std::map<int, ElementOverride> overrides;

    int elementCount() const;
    Status validate() const;
    friend bool operator==(const ArraySettings&, const ArraySettings&) = default;
};

struct ArrayElement {
    int index = 0;
    Vec3 localPosition = Vec3::Zero();
    Vec3 localForward = Vec3::UnitZ();
    EmitterSettings settings;  // defaults with this element's override applied
};

// The elements of an array in its local frame, in index order.
//   Linear            along X, centred
//   RectangularPlanar in the XY plane, centred; index = ix + countX * iy
//   Circular          a ring of radius `radius` in the XY plane
//   Cylindrical       countY rings stacked along Z, centred
//   Spherical         countX points on a sphere (Fibonacci lattice)
//   Custom            customPositions
std::vector<ArrayElement> generateElements(const ArraySettings& settings);

class ArrayComponent final : public Component {
public:
    explicit ArrayComponent(ArraySettings initial = {}) : settings(std::move(initial)) {}
    ArraySettings settings;

    std::string typeId() const override { return object_types::Array; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

// --- target, probe, region ---------------------------------------------------

// What a target asks for. Only Point targets are solved by the current
// focusing and inverse-design algorithms; the other kinds are part of the
// data model so later solvers can use them, and are reported as unsupported.
enum class TargetKind { Point, Line, Plane, Surface, Volume };

struct TargetSettings {
    TargetKind kind = TargetKind::Point;
    double desiredAmplitudePa = 1.0;  // peak pressure amplitude wanted at the target
    double desiredPhaseRad = 0.0;
    double weight = 1.0;              // relative importance among several targets, > 0
    bool suppress = false;            // true: the field should be zero here
    Vec3 extent = Vec3::Zero();       // size of non-point targets in the local frame (reserved)

    Status validate() const;
    friend bool operator==(const TargetSettings&, const TargetSettings&) = default;
};

class TargetComponent final : public Component {
public:
    explicit TargetComponent(TargetSettings initial = {}) : settings(std::move(initial)) {}
    TargetSettings settings;

    std::string typeId() const override { return object_types::Target; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

// A probe marks a point where field quantities are reported.
class ProbeComponent final : public Component {
public:
    std::string typeId() const override { return object_types::Probe; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

// A region over which a field is sampled, in the object's local frame and
// centred on its origin. Rotating the object gives an arbitrary plane.
enum class RegionKind { PlaneXY, PlaneXZ, PlaneYZ, Volume };

struct RegionSettings {
    RegionKind kind = RegionKind::PlaneXZ;
    Vec3 size = Vec3(0.2, 0.2, 0.2);  // m along local X, Y, Z (the unused axis of a plane is ignored)
    int countU = 121;                 // samples along the first in-plane axis (X; Y for PlaneYZ)
    int countV = 121;                 // samples along the second in-plane axis
    int countW = 41;                  // samples along Z (Volume only)

    bool isVolume() const { return kind == RegionKind::Volume; }
    size_t pointCount() const;
    // Local position of sample (iu, iv, iw).
    Vec3 localPoint(int iu, int iv, int iw) const;
    // Local corner / axis description of the sampled box or rectangle.
    Vec3 localOrigin() const;  // position of sample (0, 0, 0)
    Vec3 localAxisU() const;   // vector from sample (0,0,0) to (countU-1, 0, 0)
    Vec3 localAxisV() const;
    Vec3 localAxisW() const;   // zero for a plane

    Status validate() const;
    friend bool operator==(const RegionSettings&, const RegionSettings&) = default;
};

class RegionComponent final : public Component {
public:
    explicit RegionComponent(RegionSettings initial = {}) : settings(std::move(initial)) {}
    RegionSettings settings;

    std::string typeId() const override { return object_types::Region; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

// --- helpers -------------------------------------------------------------------

// Detached scene objects of each kind, ready to insert.
std::unique_ptr<SceneObject> makeEmitter(std::string name, EmitterSettings settings = {});
std::unique_ptr<SceneObject> makeArray(std::string name, ArraySettings settings = {});
std::unique_ptr<SceneObject> makeTarget(std::string name, TargetSettings settings = {});
std::unique_ptr<SceneObject> makeProbe(std::string name);
std::unique_ptr<SceneObject> makeRegion(std::string name, RegionSettings settings = {});

// The component of an object as a given type, or nullptr.
template <typename T>
const T* componentOf(const SceneObject& object)
{
    return dynamic_cast<const T*>(object.state.component.get());
}

void registerAcousticComponents(ComponentRegistry& registry);

std::string_view toString(DirectivityModel model);
std::string_view toString(ArrayLayout layout);
std::string_view toString(ElementFacing facing);
std::string_view toString(TargetKind kind);
std::string_view toString(RegionKind kind);

}  // namespace resonant::acoustics
