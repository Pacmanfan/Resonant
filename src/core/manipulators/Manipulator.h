// Wave manipulators: physical structures that shape a wave field through
// their geometry, material and boundary behaviour (reflectors, lenses,
// waveguides, resonant structures, diffraction structures).
//
// A manipulator is a scene object with three separate things:
//   * the DESIGN: this component (category, boundary condition, design
//     intent such as the intended focal point, provenance);
//   * the GEOMETRY: the object's parametric engineering geometry
//     (geometry/Geometry.h; kinds are registered in ManipulatorGeometry.cpp);
//   * a SIMULATION REPRESENTATION derived from the geometry by whichever
//     solver supports it (for reflectors: a set of surface elements).
//
// Whether any physical model calculates a manipulator's effect is stated by
// supportOf(); a manipulator that is only geometry is never presented as
// simulated.
#pragma once

#include "foundation/Types.h"
#include "materials/Material.h"
#include "scene/Scene.h"

#include <optional>

namespace resonant::manipulators {

inline constexpr const char* kManipulatorType = "wave.manipulator";

enum class Category { Reflector, Lens, Waveguide, ResonantStructure, DiffractionStructure };

// Idealized acoustic boundary behaviour of a manipulator's surface.
enum class BoundaryCondition {
    Rigid,            // normal velocity zero; reflection coefficient +1
    PressureRelease,  // pressure zero; reflection coefficient -1
    Impedance,        // locally reacting, impedance rho c of the assigned material
    Absorbing,        // no reflection (reflection coefficient 0)
};

// How far a physical model goes for a manipulator.
enum class SupportLevel {
    GeometryOnly,        // shape and parameters only; no field effect is calculated
    AnalyticalEstimate,  // closed-form estimates (focal length, cut-off, resonance); no field effect
    ApproximateField,    // its effect on the field is calculated by an approximate model
};

std::string_view toString(Category category);
std::string_view toString(BoundaryCondition boundary);
std::string_view toString(SupportLevel level);

struct ManipulatorSettings {
    Category category = Category::Reflector;
    PhysicsDomain domain = PhysicsDomain::Acoustic;
    BoundaryCondition boundary = BoundaryCondition::Rigid;
    // Design intent, in world coordinates. These record what the designer
    // wants; nothing assumes the structure achieves it.
    std::optional<Vec3> intendedFocus;
    std::optional<Vec3> intendedSource;
    std::string notes;       // design and provenance notes

    friend bool operator==(const ManipulatorSettings&, const ManipulatorSettings&) = default;
};

class ManipulatorComponent final : public Component {
public:
    explicit ManipulatorComponent(ManipulatorSettings initial = {}) : settings(std::move(initial)) {}
    ManipulatorSettings settings;

    std::string typeId() const override { return kManipulatorType; }
    nlohmann::json toJson() const override;
    bool equals(const Component& other) const override;
};

// The category a geometry kind belongs to ("reflector.parabolic" -> Reflector).
std::optional<Category> categoryOfGeometry(const Geometry& geometry);

// What is actually calculated for a manipulator with this geometry, and a
// sentence saying so for the user.
SupportLevel supportOf(const Geometry& geometry);
std::string supportDescription(const Geometry& geometry);

// A detached manipulator object of a geometry kind, with default parameters.
Result<std::unique_ptr<SceneObject>> makeManipulator(std::string name, std::string_view geometryKind);

void registerManipulatorComponents(ComponentRegistry& registry);

// --- manufacturing constraints -----------------------------------------------
// Kept apart from the physical parameters. Checking reports violations and
// what would correct them; it never changes the geometry.
struct ManufacturingConstraints {
    std::optional<double> minimumWallThickness;   // m
    std::optional<double> maximumDimension;       // m, largest extent in any direction
    std::optional<double> minimumCurvatureRadius; // m
    std::optional<double> minimumFeatureSize;     // m
};

struct ConstraintViolation {
    std::string constraint;  // "minimum wall thickness"
    double actual = 0.0;
    double limit = 0.0;
    std::string correction;  // what the designer could change
};

std::vector<ConstraintViolation> checkConstraints(const Geometry& geometry, const ManufacturingConstraints& constraints);

}  // namespace resonant::manipulators
