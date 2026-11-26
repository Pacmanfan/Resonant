// The acoustic example projects. Each example is built here with the public
// engine, compared with the file in examples/acoustics (so the files cannot
// go stale), and its physics is checked.
//
// To regenerate the files after an intended change:
//   RESONANT_WRITE_EXAMPLES=1 resonant_core_tests --gtest_filter='AcousticExamples.*'
#include "acoustics/FieldGrid.h"
#include "acoustics/Focusing.h"
#include "acoustics/InverseDesign.h"
#include "acoustics/Settings.h"
#include "foundation/Units.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace resonant;
using namespace resonant::acoustics;

namespace {

constexpr double kFrequency = 40000.0;

std::unique_ptr<Project> newExample(const std::string& slug, const std::string& name)
{
    auto project = Project::createNew(name);
    project->id = Id("ex-acoustics-" + slug);
    project->createdUtc = project->modifiedUtc = "2026-11-26T00:00:00Z";
    AcousticSettings settings;
    settings.medium = airAtTemperature(293.15);
    settings.frequencyHz = kFrequency;
    storeSettings(*project, settings);
    return project;
}

SceneObject* add(Project& project, const std::string& id, const char* type, const std::string& name, ComponentPtr component,
                 const Vec3& position = Vec3::Zero())
{
    auto object = std::make_unique<SceneObject>(Id(id), type);
    object->state.name = name;
    object->state.component = std::move(component);
    object->state.local.position = position;
    return project.scene.insert(std::move(object)).value();
}

EmitterSettings emitter()
{
    EmitterSettings s;
    s.frequencyHz = kFrequency;
    s.amplitudePa = 1.0;
    s.radius = 0.002;
    return s;
}

RegionSettings planeXZ(double sizeX, double sizeZ, int nu, int nv)
{
    RegionSettings r;
    r.kind = RegionKind::PlaneXZ;
    r.size = Vec3(sizeX, 0.1, sizeZ);
    r.countU = nu;
    r.countV = nv;
    return r;
}

double wavelength()
{
    return airAtTemperature(293.15).wavelength(kFrequency);
}

FieldEvaluator evaluatorOf(const Project& project)
{
    const AcousticSettings s = settingsOf(project);
    return FieldEvaluator(collectSources(project.scene), s.frequencyHz, s.medium,
                          std::make_shared<FreeFieldModel>(s.propagation));
}

Vec3 positionOf(const Project& project, const char* id)
{
    const SceneObject* object = project.scene.find(Id(id));
    return transformPoint(project.scene.worldMatrix(*object), Vec3::Zero());
}

// --- A: two-source interference -------------------------------------------------
std::unique_ptr<Project> buildTwoSources()
{
    auto project = newExample("a", "two-source-interference");
    const double separation = 3.0 * wavelength();
    add(*project, "ex-a-emitter-1", acoustics::object_types::Emitter, "Emitter 1", std::make_shared<EmitterComponent>(emitter()),
        Vec3(-0.5 * separation, 0, 0));
    add(*project, "ex-a-emitter-2", acoustics::object_types::Emitter, "Emitter 2", std::make_shared<EmitterComponent>(emitter()),
        Vec3(0.5 * separation, 0, 0));
    add(*project, "ex-a-region", acoustics::object_types::Region, "Field plane", std::make_shared<RegionComponent>(planeXZ(0.2, 0.2, 241, 241)),
        Vec3(0, 0, 0.105));
    // On the bisector the two paths are equal: constructive.
    add(*project, "ex-a-probe-max", acoustics::object_types::Probe, "Probe - constructive", std::make_shared<ProbeComponent>(),
        Vec3(0, 0, 0.1));
    // Where the path difference is half a wavelength: destructive. Found by
    // bisection on x at z = 0.1 m.
    const auto difference = [&](double x) {
        return std::hypot(x + 0.5 * separation, 0.1) - std::hypot(x - 0.5 * separation, 0.1);
    };
    double low = 0.0, high = 0.05;
    for (int i = 0; i < 80; ++i) {
        const double mid = 0.5 * (low + high);
        (difference(mid) < 0.5 * wavelength() ? low : high) = mid;
    }
    add(*project, "ex-a-probe-min", acoustics::object_types::Probe, "Probe - destructive", std::make_shared<ProbeComponent>(),
        Vec3(0.5 * (low + high), 0, 0.1));
    return project;
}

// --- B: linear array ----------------------------------------------------------------
std::unique_ptr<Project> buildLinearArray()
{
    auto project = newExample("b", "linear-array");
    ArraySettings array;
    array.layout = ArrayLayout::Linear;
    array.countX = 16;
    array.spacingX = 0.5 * wavelength();
    array.element = emitter();
    add(*project, "ex-b-array", acoustics::object_types::Array, "Linear array", std::make_shared<ArrayComponent>(array));
    add(*project, "ex-b-region", acoustics::object_types::Region, "Field plane", std::make_shared<RegionComponent>(planeXZ(0.3, 0.3, 301, 301)),
        Vec3(0, 0, 0.155));
    add(*project, "ex-b-probe-axis", acoustics::object_types::Probe, "Probe - on axis", std::make_shared<ProbeComponent>(), Vec3(0, 0, 0.25));
    add(*project, "ex-b-probe-side", acoustics::object_types::Probe, "Probe - 30 degrees off axis", std::make_shared<ProbeComponent>(),
        Vec3(0.25 * std::sin(units::pi / 6.0), 0, 0.25 * std::cos(units::pi / 6.0)));
    return project;
}

// --- C: focused planar array ----------------------------------------------------------
ArraySettings planarArray()
{
    ArraySettings array;
    array.layout = ArrayLayout::RectangularPlanar;
    array.countX = array.countY = 8;
    array.spacingX = array.spacingY = 0.01;  // a common 10 mm transducer pitch
    array.element = emitter();
    array.element.radius = 0.004;
    return array;
}

std::unique_ptr<Project> buildFocusedArray()
{
    auto project = newExample("c", "focused-planar-array");
    const Vec3 target(0.01, 0.0, 0.1);
    ArraySettings array = planarArray();
    const FocusSolution focus = computeFocus(generateElements(array), Mat4::Identity(), target,
                                             settingsOf(*project).medium, kFrequency, FocusControl::Phase);
    array = applyFocus(array, focus, Taper::Uniform);
    add(*project, "ex-c-array", acoustics::object_types::Array, "Planar array (focused)", std::make_shared<ArrayComponent>(array));
    TargetSettings wanted;
    add(*project, "ex-c-target", acoustics::object_types::Target, "Focus target", std::make_shared<TargetComponent>(wanted), target);
    add(*project, "ex-c-region", acoustics::object_types::Region, "Field plane", std::make_shared<RegionComponent>(planeXZ(0.12, 0.16, 241, 321)),
        Vec3(0, 0, 0.09));
    return project;
}

// --- D: multiple target points -----------------------------------------------------------
std::unique_ptr<Project> buildMultipleTargets()
{
    auto project = newExample("d", "multiple-target-points");
    ArraySettings array = planarArray();
    const SceneObject* object =
        add(*project, "ex-d-array", acoustics::object_types::Array, "Planar array (inverse design)", std::make_shared<ArrayComponent>(array));
    TargetSettings left;
    left.desiredAmplitudePa = 60.0;
    add(*project, "ex-d-target-1", acoustics::object_types::Target, "Target 1 (60 Pa)", std::make_shared<TargetComponent>(left), Vec3(-0.02, 0, 0.1));
    TargetSettings right;
    right.desiredAmplitudePa = 40.0;
    right.desiredPhaseRad = units::pi / 2.0;
    add(*project, "ex-d-target-2", acoustics::object_types::Target, "Target 2 (40 Pa, +90 deg)", std::make_shared<TargetComponent>(right), Vec3(0.02, 0, 0.1));
    TargetSettings quiet;
    quiet.suppress = true;
    add(*project, "ex-d-target-3", acoustics::object_types::Target, "Target 3 (suppress)", std::make_shared<TargetComponent>(quiet), Vec3(0.0, 0, 0.1));

    const AcousticSettings settings = settingsOf(*project);
    const auto sources = collectSources(project->scene);
    const InverseSolution solution = solveInverse(sources, collectTargets(project->scene).points, settings.medium,
                                                  FreeFieldModel(settings.propagation));
    ObjectState state = object->state;
    state.component = std::make_shared<ArrayComponent>(applyWeights(array, object->id(), sources, solution.weights));
    [[maybe_unused]] const Status status = project->scene.setState(object->id(), state);
    add(*project, "ex-d-region", acoustics::object_types::Region, "Field plane", std::make_shared<RegionComponent>(planeXZ(0.12, 0.16, 241, 321)),
        Vec3(0, 0, 0.09));
    return project;
}

// Writes or compares the example file; returns the project loaded from disk.
std::unique_ptr<Project> checkedExample(const std::string& file, const Project& built)
{
    const std::filesystem::path path = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "acoustics" / file;
    const nlohmann::json expected = serializeProject(built);
    if (std::getenv("RESONANT_WRITE_EXAMPLES")) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << expected.dump(2) << "\n";
    }
    auto loaded = loadProjectFile(path);
    EXPECT_TRUE(loaded.ok()) << loaded.message();
    if (!loaded.ok())
        return nullptr;
    // The stored file is exactly what the engine builds today. Numbers are
    // compared through their text form, which is what the file holds.
    EXPECT_EQ(serializeProject(*loaded.value().project).dump(), nlohmann::json::parse(expected.dump()).dump())
        << file << " is stale; regenerate it with RESONANT_WRITE_EXAMPLES=1";
    return std::move(loaded.value().project);
}

}  // namespace

TEST(AcousticExamples, A_TwoSourceInterference)
{
    const auto project = checkedExample("a-two-source-interference.resonant", *buildTwoSources());
    ASSERT_NE(project, nullptr);
    const FieldEvaluator field = evaluatorOf(*project);
    ASSERT_EQ(field.sources().size(), 2u);
    const Vec3 constructive = positionOf(*project, "ex-a-probe-max");
    const Vec3 destructive = positionOf(*project, "ex-a-probe-min");
    // Constructive: twice one source's 1 Pa * (1 m / r).
    const double r = (constructive - field.sources()[0].position).norm();
    EXPECT_NEAR(std::abs(field.pressure(constructive)), 2.0 / r, 1e-9);
    // Destructive: the two amplitudes differ slightly (different distances),
    // so the minimum is their difference, a few percent of the maximum.
    const double r1 = (destructive - field.sources()[0].position).norm();
    const double r2 = (destructive - field.sources()[1].position).norm();
    EXPECT_NEAR(std::abs(r1 - r2), 0.5 * wavelength(), 1e-12);
    EXPECT_NEAR(std::abs(field.pressure(destructive)), std::abs(1.0 / r1 - 1.0 / r2), 1e-8);
    EXPECT_LT(std::abs(field.pressure(destructive)), 0.05 * std::abs(field.pressure(constructive)));
}

TEST(AcousticExamples, B_LinearArrayRadiatesBroadside)
{
    const auto project = checkedExample("b-linear-array.resonant", *buildLinearArray());
    ASSERT_NE(project, nullptr);
    const FieldEvaluator field = evaluatorOf(*project);
    ASSERT_EQ(field.sources().size(), 16u);
    const double onAxis = std::abs(field.pressure(positionOf(*project, "ex-b-probe-axis")));
    const double offAxis = std::abs(field.pressure(positionOf(*project, "ex-b-probe-side")));
    // In phase and unsteered, the array's main lobe is broadside; 30 degrees
    // off axis at the same range is far down a sidelobe.
    EXPECT_GT(onAxis, 8.0 * offAxis);
    // At 0.25 m, beyond the array's near field (L^2 / lambda = 0.55 m is not
    // yet reached), the on-axis sum is below 16 perfectly aligned elements.
    EXPECT_LT(onAxis, 16.0 / 0.25);
    EXPECT_GT(onAxis, 0.5 * 16.0 / 0.25);
}

TEST(AcousticExamples, C_FocusedPlanarArrayPeaksNearItsTarget)
{
    const auto project = checkedExample("c-focused-planar-array.resonant", *buildFocusedArray());
    ASSERT_NE(project, nullptr);
    const FieldEvaluator field = evaluatorOf(*project);
    ASSERT_EQ(field.sources().size(), 64u);
    const Vec3 target = positionOf(*project, "ex-c-target");

    // At the target every element arrives in phase: the sum of 1 / r_i.
    double expected = 0.0;
    for (const PointSource& source : field.sources())
        expected += 1.0 / (target - source.position).norm();
    EXPECT_NEAR(std::abs(field.pressure(target)), expected, 1e-7 * expected);

    // The calculated field over the example's region has its maximum close
    // to the target, slightly towards the array (the usual focal shift of a
    // focused aperture), and it is a real focus: several times the mean.
    const SceneObject* region = project->scene.find(Id("ex-c-region"));
    const GridFrame frame = gridOf(componentOf<RegionComponent>(*region)->settings, project->scene.worldMatrix(*region));
    const auto result = computeField("example C", field, frame);
    const Vec3 peak = result->statistics.maxPoint;
    EXPECT_LT(std::abs(peak.x() - target.x()), 0.002);
    EXPECT_LT(std::abs(peak.z() - target.z()), 0.02);
    EXPECT_LE(peak.z(), target.z() + 1e-9);
    EXPECT_GT(result->statistics.maxMagnitude, 5.0 * result->statistics.meanMagnitude);
    EXPECT_GE(result->statistics.maxMagnitude, std::abs(field.pressure(target)) * 0.999);
}

TEST(AcousticExamples, D_InverseDesignMeetsThreeTargets)
{
    const auto project = checkedExample("d-multiple-target-points.resonant", *buildMultipleTargets());
    ASSERT_NE(project, nullptr);
    const FieldEvaluator field = evaluatorOf(*project);
    const TargetCollection targets = collectTargets(project->scene);
    ASSERT_EQ(targets.points.size(), 3u);
    // Compare the field of the stored array settings with what was asked.
    for (const TargetPoint& target : targets.points) {
        const Complex achieved = field.pressure(target.position);
        EXPECT_NEAR(std::abs(achieved - target.desired), 0.0, 0.05) << target.label;  // of 40 to 60 Pa
    }
    EXPECT_NEAR(std::abs(field.pressure(targets.points[0].position)), 60.0, 0.05);
    EXPECT_NEAR(std::arg(field.pressure(targets.points[1].position)), units::pi / 2.0, 0.002);
    EXPECT_LT(std::abs(field.pressure(targets.points[2].position)), 0.05);
}
