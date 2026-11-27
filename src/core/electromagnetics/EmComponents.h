#pragma once

// Scene objects of the electromagnetic domain.
//
//   Antenna   an object with an antenna geometry kind (antenna.dipole,
//             antenna.monopole, antenna.loop, antenna.patch, antenna.horn,
//             antenna.helix) and this component: frequency, feed, reference
//             impedance. Whether an analytical model exists depends on the
//             kind (supportOf).
//   Source    an idealized radiator: Hertzian (electric) dipole, small loop
//             (magnetic dipole) or plane wave.
//   Array     an array of identical elements with an element pattern, a
//             layout and per-element excitation.
#include "dsp/Waveform.h"
#include "electromagnetics/ArrayFactor.h"
#include "geometry/Geometry.h"
#include "scene/Component.h"

#include <map>
#include <optional>

namespace resonant::em {

namespace object_types {
inline constexpr const char* Antenna = "em.antenna";
inline constexpr const char* Source = "em.source";
inline constexpr const char* Array = "em.array";
inline constexpr const char* Guide = "em.guide";
}  // namespace object_types

enum class AntennaModel {
    Analytical,  // closed-form fields and impedance under stated assumptions
    Estimate,    // design formulas only (resonant length, approximate gain); no field
};
// What exists for an antenna geometry kind.
AntennaModel supportOf(const Geometry& geometry);
std::string supportDescription(const Geometry& geometry);
bool isAntennaGeometry(const Geometry& geometry);

struct AntennaSettings {
    double frequencyHz = 1e9;
    double feedCurrent = 1.0;     // A, peak current at the feed
    double feedPhase = 0.0;       // rad
    double delay = 0.0;           // s
    double referenceImpedance = 50.0;  // ohm
    dsp::Waveform waveform;       // for time-domain solvers
    bool active = true;
    std::string measuredData;     // name of imported measured data, if any

    Status validate() const;
    friend bool operator==(const AntennaSettings&, const AntennaSettings&) = default;
};

class AntennaComponent final : public Component {
public:
    explicit AntennaComponent(AntennaSettings s = {}) : settings(std::move(s)) {}
    AntennaSettings settings;
    std::string typeId() const override { return object_types::Antenna; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

enum class SourceKind { ElectricDipole, MagneticDipole, PlaneWave };
std::string_view toString(SourceKind kind);

struct SourceSettings {
    SourceKind kind = SourceKind::ElectricDipole;
    double frequencyHz = 1e9;
    double amplitude = 1e-3;  // electric dipole: current moment I l (A m); magnetic: I A (A m^2); plane wave: |E| (V/m)
    double phase = 0.0;
    double delay = 0.0;
    dsp::Waveform waveform;
    bool active = true;
    // Along the local axes: dipoles point along +Z; a plane wave travels
    // along +Z with E along +X.
    Status validate() const;
    friend bool operator==(const SourceSettings&, const SourceSettings&) = default;
};

class SourceComponent final : public Component {
public:
    explicit SourceComponent(SourceSettings s = {}) : settings(std::move(s)) {}
    SourceSettings settings;
    std::string typeId() const override { return object_types::Source; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

enum class EmArrayLayout { Linear, Planar, Circular, Custom };
std::string_view toString(EmArrayLayout layout);

struct ElementExcitation {
    std::optional<double> amplitude;
    std::optional<double> phase;
    std::optional<double> delay;
    std::optional<bool> active;
    bool empty() const { return !amplitude && !phase && !delay && !active; }
    friend bool operator==(const ElementExcitation&, const ElementExcitation&) = default;
};

struct EmArraySettings {
    EmArrayLayout layout = EmArrayLayout::Linear;
    int countX = 8, countY = 1;          // Linear: countX along local X; Planar: X by Y; Circular: countX on the ring
    double spacingX = 0.15, spacingY = 0.15;  // m
    double radius = 0.3;                 // m, Circular
    std::vector<Vec3> customPositions;   // local, m
    ElementPattern pattern = ElementPattern::HalfWaveDipole;
    double frequencyHz = 1e9;
    double amplitude = 1.0;              // element weight magnitude (relative, or A for dipole elements)
    std::map<int, ElementExcitation> overrides;  // by element index, kept when the layout changes
    dsp::Waveform waveform;
    bool active = true;

    int elementCount() const;
    Status validate() const;
    friend bool operator==(const EmArraySettings&, const EmArraySettings&) = default;
};

class EmArrayComponent final : public Component {
public:
    explicit EmArrayComponent(EmArraySettings s = {}) : settings(std::move(s)) {}
    EmArraySettings settings;
    std::string typeId() const override { return object_types::Array; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

// The array model of the settings in the array's local frame (element axes local +Z).
ArrayModel arrayModelOf(const EmArraySettings& settings);
// Writes weights and delays of a model back as overrides.
EmArraySettings withExcitation(const EmArraySettings& settings, const ArrayModel& model);

// A waveguide or cavity: an object with a guide geometry kind
// (guide.rectangular, guide.circular, guide.coaxial, cavity.rectangular) and
// this component. Walls are perfect conductors in the analytical models;
// the filling is the object's material (vacuum when it has none).
struct GuideSettings {
    double frequencyHz = 10e9;  // the frequency of interest
    Status validate() const;
    friend bool operator==(const GuideSettings&, const GuideSettings&) = default;
};

class GuideComponent final : public Component {
public:
    explicit GuideComponent(GuideSettings s = {}) : settings(std::move(s)) {}
    GuideSettings settings;
    std::string typeId() const override { return object_types::Guide; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};
bool isGuideGeometry(const Geometry& geometry);

void registerElectromagneticComponents(ComponentRegistry& registry);

}  // namespace resonant::em

namespace resonant {
void registerAntennaGeometry(GeometryRegistry& registry);
}
