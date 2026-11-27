#pragma once

// Scenes shared by the optimization domain tests and the example projects
// (REQ-CHAT-20261126-009). Every value is SI; materials come from the example
// library (illustrative values).
#include "acoustics/Components.h"
#include "acoustics/Medium.h"
#include "acoustics/Settings.h"
#include "electromagnetics/EmComponents.h"
#include "fdtd/FdtdSetup.h"
#include "geometry/Geometry.h"
#include "manipulators/Manipulator.h"
#include "mechanics/MechanicalComponents.h"
#include "mechanics/Study.h"
#include "project/Project.h"
#include "scene/Scene.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>

namespace optimization_fixtures {

using namespace resonant;

inline constexpr double kAcousticFrequency = 40000.0;

inline std::unique_ptr<Project> newProject(const std::string& id, const std::string& name)
{
    auto project = Project::createNew(name);
    project->id = Id(id);
    project->createdUtc = project->modifiedUtc = "2026-11-27T00:00:00Z";
    return project;
}

inline SceneObject* add(Project& project, const std::string& id, const char* type, const std::string& name, ComponentPtr component,
                        const Vec3& position = Vec3::Zero(), std::optional<Geometry> geometry = {}, const Id& material = {},
                        const Vec3& rotation = Vec3::Zero())
{
    auto object = std::make_unique<SceneObject>(Id(id), type);
    object->state.name = name;
    object->state.component = std::move(component);
    object->state.local.position = position;
    object->state.local.rotation = rotation;
    object->state.geometry = std::move(geometry);
    object->state.materialId = material;
    return project.scene.insert(std::move(object)).value();
}

inline Geometry shape(const char* kind, std::initializer_list<std::pair<const char*, double>> parameters)
{
    Geometry g = Geometry::create(kind).take();
    for (const auto& [key, value] : parameters) {
        const Status status = g.setParameter(key, value);
        EXPECT_TRUE(status.ok()) << key << ": " << status.message();
    }
    return g;
}

inline void acousticSettings(Project& project)
{
    acoustics::AcousticSettings s;
    s.frequencyHz = kAcousticFrequency;
    s.medium = acoustics::airAtTemperature(293.15);
    acoustics::storeSettings(project, s);
}

// A linear array of omnidirectional elements along X (spacing half a
// wavelength, 1 Pa at 1 m each), facing +Z, and a focus target off axis.
inline std::unique_ptr<Project> acousticArray(const std::string& id, int elements = 8, bool region = false, bool suppression = false)
{
    auto project = newProject(id, "Acoustic focal array");
    acousticSettings(*project);
    const double lambda = acoustics::airAtTemperature(293.15).wavelength(kAcousticFrequency);
    acoustics::ArraySettings a;
    a.layout = acoustics::ArrayLayout::Linear;
    a.countX = elements;
    a.spacingX = 0.5 * lambda;
    a.element.frequencyHz = kAcousticFrequency;
    a.element.amplitudePa = 1.0;
    a.element.radius = 0.001;
    add(*project, id + "-array", acoustics::object_types::Array, "Linear array", std::make_shared<acoustics::ArrayComponent>(a));
    acoustics::TargetSettings t;
    t.desiredAmplitudePa = 5.0;
    add(*project, id + "-focus", acoustics::object_types::Target, "Focus", std::make_shared<acoustics::TargetComponent>(t), Vec3(0.02, 0.0, 0.08));
    if (suppression) {
        acoustics::TargetSettings q;
        q.suppress = true;
        add(*project, id + "-quiet", acoustics::object_types::Target, "Quiet point", std::make_shared<acoustics::TargetComponent>(q),
            Vec3(-0.03, 0.0, 0.08));
    }
    if (region) {
        acoustics::RegionSettings r;
        r.kind = acoustics::RegionKind::PlaneXZ;
        r.size = Vec3(0.07, 0.01, 0.1);
        r.countU = 41;
        r.countV = 41;
        add(*project, id + "-region", acoustics::object_types::Region, "Field plane", std::make_shared<acoustics::RegionComponent>(r),
            Vec3(0.0, 0.0, 0.045));
        fdtd::FdtdSettings f;
        f.pointsPerWavelength = 10.0;
        f.snapshotCount = 0;
        f.harmonicPeriods = 6.0;
        fdtd::storeFdtdSettings(*project, f);
    }
    return project;
}

// A parabolic reflector opening towards +Z, a distant emitter on the axis
// facing it and a target on the axis at `targetDistance` from the vertex.
inline std::unique_ptr<Project> acousticReflector(const std::string& id, double focalLength, double targetDistance, double sourceDistance,
                                                  const char* kind = "reflector.parabolic")
{
    auto project = newProject(id, "Acoustic reflector");
    acoustics::AcousticSettings s;
    s.frequencyHz = kAcousticFrequency;
    s.medium = acoustics::airAtTemperature(293.15);
    s.modelId = acoustics::kReflectorModelId;
    s.reflectorElementsPerWavelength = 5.0;
    acoustics::storeSettings(*project, s);
    auto reflector = manipulators::makeManipulator("Reflector", kind).take();
    Geometry g = *reflector->state.geometry;
    if (std::string(kind) == "reflector.parabolic") {
        EXPECT_TRUE(g.setParameter("focalLength", focalLength).ok());
        EXPECT_TRUE(g.setParameter("aperture", 0.2).ok());
    }
    reflector->state.geometry = g;
    auto inserted = project->scene.insert(std::move(reflector));
    const Id reflectorId = inserted.value()->id();
    // Stable ids for reproducible files: re-create with a fixed id.
    auto detached = project->scene.detach(reflectorId);
    auto fixed = std::make_unique<SceneObject>(Id(id + "-reflector"), detached->type());
    fixed->state = detached->state;
    fixed->state.materialId = Id("mat-aluminum");
    project->scene.insert(std::move(fixed));
    acoustics::EmitterSettings e;
    e.frequencyHz = kAcousticFrequency;
    e.amplitudePa = 1.0;
    e.radius = 0.002;
    add(*project, id + "-source", acoustics::object_types::Emitter, "Distant source", std::make_shared<acoustics::EmitterComponent>(e),
        Vec3(0.0, 0.0, sourceDistance), {}, {}, Vec3(3.14159265358979323846, 0.0, 0.0));
    acoustics::TargetSettings t;
    add(*project, id + "-focus", acoustics::object_types::Target, "Wanted focus", std::make_shared<acoustics::TargetComponent>(t),
        Vec3(0.0, 0.0, targetDistance));
    return project;
}

// An aluminium cantilever 500 x 30 x 5 mm along X.
inline std::unique_ptr<Project> cantilever(const std::string& id)
{
    auto project = newProject(id, "Cantilever resonator");
    mechanics::MechanicsSettings ms;
    ms.modeCount = 6;
    mechanics::storeMechanicsSettings(*project, ms);
    mechanics::StructureSettings s;
    s.idealization = mechanics::Idealization::Beam;
    s.supports = {mechanics::Support::Clamped, mechanics::Support::Free, mechanics::Support::Free,
                  mechanics::Support::Free,    mechanics::Support::Free, mechanics::Support::Free};
    add(*project, id + "-beam", resonant::object_types::Geometry, "Aluminium cantilever", std::make_shared<mechanics::StructureComponent>(s),
        Vec3::Zero(), shape("box", {{"width", 0.5}, {"depth", 0.03}, {"height", 0.005}}), Id("mat-aluminum"));
    return project;
}

// A simply supported aluminium plate 400 x 300 x 2 mm with two actuators,
// a point where vibration is wanted and a point where it must vanish.
inline std::unique_ptr<Project> plateWithActuators(const std::string& id, double frequency = 120.0)
{
    auto project = newProject(id, "Plate vibration control");
    mechanics::MechanicsSettings ms;
    ms.targetFrequency = frequency;
    ms.damping = mechanics::Damping{mechanics::DampingKind::LossFactor, 0.0, 0.0, 0.02, true, 0.0};
    mechanics::storeMechanicsSettings(*project, ms);
    mechanics::StructureSettings s;
    s.idealization = mechanics::Idealization::Plate;
    s.supports = {mechanics::Support::Pinned, mechanics::Support::Pinned, mechanics::Support::Pinned,
                  mechanics::Support::Pinned, mechanics::Support::Free,   mechanics::Support::Free};
    s.elementSize = 0.025;
    add(*project, id + "-plate", resonant::object_types::Geometry, "Aluminium plate", std::make_shared<mechanics::StructureComponent>(s), Vec3::Zero(),
        shape("box", {{"width", 0.4}, {"depth", 0.3}, {"height", 0.002}}), Id("mat-aluminum"));
    const auto actuator = [](double amplitude) {
        mechanics::ActuatorSettings a;
        a.amplitude = amplitude;
        return std::make_shared<mechanics::ActuatorComponent>(a);
    };
    add(*project, id + "-a1", mechanics::object_types::Actuator, "Actuator 1", actuator(1.0), Vec3(-0.1, 0.05, 0.001));
    add(*project, id + "-a2", mechanics::object_types::Actuator, "Actuator 2", actuator(1.0), Vec3(0.12, -0.06, 0.001));
    mechanics::VibrationTargetSettings quiet;
    quiet.suppress = true;
    add(*project, id + "-quiet", mechanics::object_types::VibrationTarget, "Quiet point", std::make_shared<mechanics::VibrationTargetComponent>(quiet),
        Vec3(0.05, 0.0, 0.001));
    mechanics::VibrationTargetSettings wanted;
    wanted.amplitude = 2e-6;
    add(*project, id + "-wanted", mechanics::object_types::VibrationTarget, "Wanted motion",
        std::make_shared<mechanics::VibrationTargetComponent>(wanted), Vec3(-0.12, -0.08, 0.001));
    return project;
}

// A linear array of `count` elements along X at 1 GHz, half a wavelength apart.
inline std::unique_ptr<Project> emArray(const std::string& id, int count = 8, em::ElementPattern pattern = em::ElementPattern::Isotropic)
{
    auto project = newProject(id, "Electromagnetic array");
    em::EmArraySettings s;
    s.layout = em::EmArrayLayout::Linear;
    s.countX = count;
    s.frequencyHz = 1e9;
    s.spacingX = 0.5 * 299792458.0 / s.frequencyHz;
    s.pattern = pattern;
    add(*project, id + "-array", em::object_types::Array, "Linear array", std::make_shared<em::EmArrayComponent>(s));
    return project;
}

inline std::unique_ptr<Project> dipole(const std::string& id, double length = 0.15)
{
    auto project = newProject(id, "Dipole antenna");
    em::AntennaSettings a;
    a.frequencyHz = 1e9;
    add(*project, id + "-dipole", em::object_types::Antenna, "Dipole", std::make_shared<em::AntennaComponent>(a), Vec3::Zero(),
        shape("antenna.dipole", {{"length", length}, {"wireRadius", 0.001}}), Id("mat-copper"));
    return project;
}

}  // namespace optimization_fixtures
