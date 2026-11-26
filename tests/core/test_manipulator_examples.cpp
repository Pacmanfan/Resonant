// The wave-manipulator example projects: built here with the public engine,
// compared with the files in examples/manipulators, and checked physically.
//
// To regenerate the files after an intended change:
//   RESONANT_WRITE_EXAMPLES=1 resonant_core_tests --gtest_filter='ManipulatorExamples.*'
#include "acoustics/Settings.h"
#include "foundation/Units.h"
#include "manipulators/Analytics.h"
#include "manipulators/GeometryOptimization.h"
#include "manipulators/ReflectorModel.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace resonant;
using namespace resonant::acoustics;
using namespace resonant::manipulators;

namespace {

constexpr double kFrequency = 40000.0;

std::unique_ptr<Project> newExample(const std::string& slug, const std::string& name, bool reflectorModel, bool water = false)
{
    auto project = Project::createNew(name);
    project->id = Id("ex-manipulators-" + slug);
    project->createdUtc = project->modifiedUtc = "2026-11-26T00:00:00Z";
    AcousticSettings settings;
    settings.medium = water ? waterNominal() : airAtTemperature(293.15);
    settings.frequencyHz = kFrequency;
    settings.modelId = reflectorModel ? kReflectorModelId : kFreeFieldModelId;
    storeSettings(*project, settings);
    return project;
}

SceneObject* add(Project& project, const std::string& id, const char* type, const std::string& name, ComponentPtr component,
                 const Vec3& position = Vec3::Zero(), std::optional<Geometry> geometry = {})
{
    auto object = std::make_unique<SceneObject>(Id(id), type);
    object->state.name = name;
    object->state.component = std::move(component);
    object->state.local.position = position;
    object->state.geometry = std::move(geometry);
    return project.scene.insert(std::move(object)).value();
}

Geometry shape(const char* kind, std::initializer_list<std::pair<const char*, double>> parameters)
{
    Geometry geometry = Geometry::create(kind).take();
    for (const auto& [key, value] : parameters)
        [[maybe_unused]] const Status status = geometry.setParameter(key, value);
    return geometry;
}

ComponentPtr manipulator(Category category, std::optional<Vec3> focus = {}, std::optional<Vec3> source = {}, std::string notes = {})
{
    ManipulatorSettings settings;
    settings.category = category;
    settings.intendedFocus = focus;
    settings.intendedSource = source;
    settings.notes = std::move(notes);
    return std::make_shared<ManipulatorComponent>(settings);
}

ComponentPtr emitter()
{
    EmitterSettings s;
    s.frequencyHz = kFrequency;
    s.radius = 0.002;
    return std::make_shared<EmitterComponent>(s);
}

ComponentPtr plane(double sizeX, double sizeZ, int nu, int nv)
{
    RegionSettings r;
    r.kind = RegionKind::PlaneXZ;
    r.size = Vec3(sizeX, 0.1, sizeZ);
    r.countU = nu;
    r.countV = nv;
    return std::make_shared<RegionComponent>(r);
}

std::shared_ptr<const ReflectorModel> modelOf(const Project& project)
{
    const AcousticSettings s = settingsOf(project);
    ReflectorOptions options;
    options.elementsPerWavelength = s.reflectorElementsPerWavelength;
    return std::make_shared<const ReflectorModel>(
        s.propagation, collectReflectors(project.scene, project.materials, s.medium.wavelength(s.frequencyHz), options).surfaces);
}

ReflectorFieldEvaluator evaluatorOf(const Project& project)
{
    const AcousticSettings s = settingsOf(project);
    return ReflectorFieldEvaluator(collectSources(project.scene), s.frequencyHz, s.medium, modelOf(project));
}

double wavelength()
{
    return airAtTemperature(293.15).wavelength(kFrequency);
}

// Position of the largest |P| on the Z axis between z0 and z1.
double axialPeak(const ReflectorFieldEvaluator& evaluator, double z0, double z1)
{
    double bestZ = z0, best = 0.0;
    for (double z = z0; z <= z1; z += 0.00025) {
        const double value = std::abs(evaluator.pressure(Vec3(0, 0, z)));
        if (value > best) {
            best = value;
            bestZ = z;
        }
    }
    return bestZ;
}

// --- A: parabolic reflector ---------------------------------------------------------
std::unique_ptr<Project> buildParabolic()
{
    auto project = newExample("a", "parabolic-reflector", true);
    add(*project, "ex-ma-reflector", kManipulatorType, "Parabolic reflector",
        manipulator(Category::Reflector, Vec3(0, 0, 0.05), Vec3(0, 0, 1.0), "Focal length 50 mm, aperture 200 mm; rigid."),
        Vec3::Zero(), shape("reflector.parabolic", {{"focalLength", 0.05}, {"aperture", 0.2}}));
    add(*project, "ex-ma-emitter", acoustics::object_types::Emitter, "Emitter (1 m on axis)", emitter(), Vec3(0, 0, 1.0));
    add(*project, "ex-ma-region", acoustics::object_types::Region, "Field plane", plane(0.08, 0.08, 161, 161), Vec3(0, 0, 0.055));
    add(*project, "ex-ma-probe", acoustics::object_types::Probe, "Probe at the geometric focus", std::make_shared<ProbeComponent>(), Vec3(0, 0, 0.05));
    return project;
}

// --- B: elliptical reflector ----------------------------------------------------------
std::unique_ptr<Project> buildElliptical()
{
    auto project = newExample("b", "elliptical-reflector", true);
    const double c = ellipseFocalDistance(0.15, 0.09);
    add(*project, "ex-mb-reflector", kManipulatorType, "Elliptical reflector",
        manipulator(Category::Reflector, Vec3(0, 0, c), Vec3(0, 0, -c), "Semi-axes 150 mm and 90 mm; the half around the first focus."),
        Vec3::Zero(), shape("reflector.elliptical", {{"semiMajor", 0.15}, {"semiMinor", 0.09}, {"cut", 0.0}}));
    add(*project, "ex-mb-emitter", acoustics::object_types::Emitter, "Emitter at focus 1", emitter(), Vec3(0, 0, -c));
    TargetSettings wanted;
    add(*project, "ex-mb-target", acoustics::object_types::Target, "Target at focus 2", std::make_shared<TargetComponent>(wanted), Vec3(0, 0, c));
    add(*project, "ex-mb-region", acoustics::object_types::Region, "Field plane", plane(0.08, 0.1, 161, 201), Vec3(0, 0, c));
    return project;
}

// --- C: acoustic lens (analytical estimate only) ----------------------------------------
std::unique_ptr<Project> buildLens()
{
    auto project = newExample("c", "acoustic-lens", false, true);
    // A biconcave acrylic lens in water. Acrylic is faster than water, so a
    // CONCAVE lens converges; the thick-lens estimate gives its focal length.
    SceneObject* lens = add(*project, "ex-mc-lens", kManipulatorType, "Acrylic lens (biconcave)",
                            manipulator(Category::Lens, {}, Vec3(0, 0, -0.5),
                                        "Analytical estimate only: no field model exists for lenses, so this object "
                                        "does not change the simulated field."),
                            Vec3::Zero(),
                            shape("lens.spherical", {{"aperture", 0.08}, {"centerThickness", 0.004}, {"radius1", -0.08}, {"radius2", 0.08}}));
    ObjectState state = lens->state;
    state.materialId = Id("mat-acrylic");
    [[maybe_unused]] const Status status = project->scene.setState(lens->id(), state);
    add(*project, "ex-mc-emitter", acoustics::object_types::Emitter, "Emitter", emitter(), Vec3(0, 0, -0.5));
    add(*project, "ex-mc-region", acoustics::object_types::Region, "Field plane", plane(0.1, 0.3, 101, 301), Vec3(0, 0, 0.16));
    return project;
}

// --- D: hybrid focusing ------------------------------------------------------------------
ArraySettings hybridArray()
{
    ArraySettings array;
    array.layout = ArrayLayout::RectangularPlanar;
    array.countX = array.countY = 6;
    array.spacingX = array.spacingY = 0.01;
    array.element.frequencyHz = kFrequency;
    array.element.radius = 0.004;
    return array;
}

std::unique_ptr<Project> buildHybrid()
{
    auto project = newExample("d", "hybrid-focusing", true);
    const Vec3 target(0.005, 0.0, 0.07);
    add(*project, "ex-md-reflector", kManipulatorType, "Parabolic reflector", manipulator(Category::Reflector, target),
        Vec3::Zero(), shape("reflector.parabolic", {{"focalLength", 0.06}, {"aperture", 0.18}}));
    // The array faces the reflector: turned half a turn about X, 0.3 m up the axis.
    SceneObject* array = add(*project, "ex-md-array", acoustics::object_types::Array, "Planar array (hybrid focus)",
                             std::make_shared<ArrayComponent>(hybridArray()), Vec3(0, 0, 0.3));
    ObjectState state = array->state;
    state.local.rotation = Vec3(units::pi, 0, 0);
    [[maybe_unused]] Status status = project->scene.setState(array->id(), state);
    TargetSettings wanted;
    add(*project, "ex-md-target", acoustics::object_types::Target, "Target", std::make_shared<TargetComponent>(wanted), target);

    // Active phase compensation from the reflector model's own responses.
    const AcousticSettings settings = settingsOf(*project);
    const FocusSolution focus = computeModelFocus(collectSources(project->scene, {array->id()}), target, settings.medium, *modelOf(*project));
    state.component = std::make_shared<ArrayComponent>(applyFocus(hybridArray(), focus, Taper::Uniform));
    status = project->scene.setState(array->id(), state);
    add(*project, "ex-md-region", acoustics::object_types::Region, "Field plane", plane(0.06, 0.06, 121, 121), Vec3(0, 0, 0.07));
    return project;
}

// --- E: geometry optimization ---------------------------------------------------------------
constexpr double kOptimizationSource = 0.5;
constexpr double kOptimizationTarget = 0.07;

OptimizationOutcome optimizeExampleReflector(const Project& project)
{
    const AcousticSettings settings = settingsOf(project);
    const auto sources = collectSources(project.scene);
    const SceneObject* original = project.scene.find(Id("ex-me-reflector-original"));
    const GeometryObjective objective = [&](const Geometry& candidate) {
        Scene trial;
        auto object = original->cloneSubtree(false);
        object->state.geometry = candidate;
        [[maybe_unused]] const auto inserted = trial.insert(std::move(object));
        ReflectorOptions options;
        options.elementsPerWavelength = settings.reflectorElementsPerWavelength;
        const ReflectorModel model(settings.propagation,
                                   collectReflectors(trial, project.materials, settings.medium.wavelength(kFrequency), options).surfaces);
        return std::abs(model.transfer(sources.front(), Vec3(0, 0, kOptimizationTarget), settings.medium).value);
    };
    OptimizationSettings settings2;
    settings2.maxEvaluations = 40;
    settings2.tolerance = 5e-3;
    return optimizeGeometry(*original->state.geometry, {{"focalLength", 0.03, 0.12}}, objective, settings2);
}

std::unique_ptr<Project> buildOptimization()
{
    auto project = newExample("e", "geometry-optimization", true);
    const Vec3 target(0, 0, kOptimizationTarget);
    add(*project, "ex-me-reflector-original", kManipulatorType, "Reflector - original (f = 40 mm)",
        manipulator(Category::Reflector, target, Vec3(0, 0, kOptimizationSource), "Starting design."), Vec3::Zero(),
        shape("reflector.parabolic", {{"focalLength", 0.04}, {"aperture", 0.16}}));
    add(*project, "ex-me-emitter", acoustics::object_types::Emitter, "Emitter (0.5 m on axis)", emitter(), Vec3(0, 0, kOptimizationSource));
    TargetSettings wanted;
    add(*project, "ex-me-target", acoustics::object_types::Target, "Target (70 mm)", std::make_shared<TargetComponent>(wanted), target);
    add(*project, "ex-me-region", acoustics::object_types::Region, "Field plane", plane(0.06, 0.08, 121, 161), Vec3(0, 0, 0.07));

    // The optimized design, stored hidden beside the original: show one and
    // hide the other to compare them (a hidden reflector is not simulated).
    const OptimizationOutcome outcome = optimizeExampleReflector(*project);
    SceneObject* optimized = add(*project, "ex-me-reflector-optimized", kManipulatorType, "Reflector - optimized (hidden)",
                                 manipulator(Category::Reflector, target, Vec3(0, 0, kOptimizationSource),
                                             "Focal length found by the geometry optimizer (golden-section search, "
                                             "reflector model) to maximize |P| at the target."),
                                 Vec3::Zero(), *outcome.bestGeometry);
    ObjectState state = optimized->state;
    state.visible = false;
    [[maybe_unused]] const Status status = project->scene.setState(optimized->id(), state);
    return project;
}

std::unique_ptr<Project> checkedExample(const std::string& file, const Project& built)
{
    const std::filesystem::path path = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "manipulators" / file;
    const nlohmann::json expected = serializeProject(built);
    if (std::getenv("RESONANT_WRITE_EXAMPLES")) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << expected.dump(2) << "\n";
    }
    auto loaded = loadProjectFile(path);
    EXPECT_TRUE(loaded.ok()) << loaded.message();
    if (!loaded.ok())
        return nullptr;
    const bool same = serializeProject(*loaded.value().project).dump() == nlohmann::json::parse(expected.dump()).dump();
    EXPECT_TRUE(same) << file << " is stale; regenerate it with RESONANT_WRITE_EXAMPLES=1";
    return std::move(loaded.value().project);
}

}  // namespace

TEST(ManipulatorExamples, A_ParabolicReflectorFocusesNearItsGeometricFocus)
{
    const auto project = checkedExample("a-parabolic-reflector.resonant", *buildParabolic());
    ASSERT_NE(project, nullptr);
    const ReflectorFieldEvaluator evaluator = evaluatorOf(*project);
    // The source is 1 m away, not at infinity: the focus is where the mirror
    // equation puts the image, 2.6 mm beyond the geometric focus.
    const double peak = axialPeak(evaluator, 0.03, 0.08);
    EXPECT_NEAR(peak, mirrorImageDistance(0.05, 1.0), 0.5 * wavelength());
    EXPECT_GT(peak, 0.05);
    EXPECT_GT(focalGain(evaluator, Vec3(0, 0, peak)), 20.0);
    EXPECT_EQ(evaluator.solverId(), "acoustic.reflector.kirchhoff");
}

TEST(ManipulatorExamples, B_EllipticalReflectorFocusesAtItsSecondFocus)
{
    const auto project = checkedExample("b-elliptical-reflector.resonant", *buildElliptical());
    ASSERT_NE(project, nullptr);
    const ReflectorFieldEvaluator evaluator = evaluatorOf(*project);
    const double c = ellipseFocalDistance(0.15, 0.09);
    EXPECT_NEAR(axialPeak(evaluator, 0.06, 0.18), c, wavelength());
    EXPECT_GT(focalGain(evaluator, Vec3(0, 0, c)), 8.0);
}

TEST(ManipulatorExamples, C_LensIsAnAnalyticalEstimateAndDoesNotChangeTheField)
{
    const auto project = checkedExample("c-acoustic-lens.resonant", *buildLens());
    ASSERT_NE(project, nullptr);
    const SceneObject* lens = project->scene.find(Id("ex-mc-lens"));
    EXPECT_EQ(supportOf(*lens->state.geometry), SupportLevel::AnalyticalEstimate);
    const AcousticSettings s = settingsOf(*project);
    const Material* acrylic = project->materials.find(lens->state.materialId);
    ASSERT_NE(acrylic, nullptr);
    const LensEstimate estimate = estimateLens(-0.08, 0.08, 0.004, s.medium.soundSpeed, s.medium.density,
                                               acrylic->property(PhysicsDomain::Acoustic, "soundSpeed")->value,
                                               acrylic->property(PhysicsDomain::Acoustic, "density")->value);
    EXPECT_LT(estimate.refractiveIndex, 1.0);  // acrylic is faster than water
    EXPECT_GT(estimate.focalLength, 0.0);      // so the concave lens converges
    EXPECT_NEAR(estimate.focalLength, 0.087, 0.004);
    // The reflector model reports the lens as not simulated; the field with
    // and without it is the same.
    const ReflectorCollection collection = collectReflectors(project->scene, project->materials, s.medium.wavelength(s.frequencyHz));
    EXPECT_TRUE(collection.surfaces.empty());
    ASSERT_EQ(collection.notes.size(), 1u);
    EXPECT_NE(collection.notes[0].find("does NOT affect"), std::string::npos);
}

TEST(ManipulatorExamples, D_HybridFocusAlignsDirectAndReflectedSound)
{
    const auto project = checkedExample("d-hybrid-focusing.resonant", *buildHybrid());
    ASSERT_NE(project, nullptr);
    const AcousticSettings s = settingsOf(*project);
    const auto model = modelOf(*project);
    const Vec3 target = Vec3(0.005, 0.0, 0.07);
    const auto sources = collectSources(project->scene);
    ASSERT_EQ(sources.size(), 36u);
    // With the stored phases the field at the target is the sum of the
    // magnitudes of every element's response (direct + reflected).
    Complex total(0.0, 0.0);
    double bound = 0.0, directBound = 0.0;
    const FreeFieldModel freeField(s.propagation);
    for (const PointSource& source : sources) {
        const Complex h = model->transfer(source, target, s.medium).value;
        total += source.amplitude * h;
        bound += std::abs(h);
        directBound += std::abs(freeField.transfer(source, target, s.medium).value);
    }
    EXPECT_NEAR(std::abs(total), bound, 1e-6 * bound);
    // The reflector contributes: more than the array could deliver alone.
    EXPECT_GT(bound, 1.5 * directBound);
}

TEST(ManipulatorExamples, E_OptimizedReflectorBeatsTheOriginalAtTheTarget)
{
    const auto project = checkedExample("e-geometry-optimization.resonant", *buildOptimization());
    ASSERT_NE(project, nullptr);
    const SceneObject* original = project->scene.find(Id("ex-me-reflector-original"));
    const SceneObject* optimized = project->scene.find(Id("ex-me-reflector-optimized"));
    ASSERT_NE(optimized, nullptr);
    EXPECT_FALSE(optimized->state.visible);
    EXPECT_DOUBLE_EQ(original->state.geometry->parameter("focalLength"), 0.04);

    // The stored optimum is what the optimizer finds, close to the mirror
    // equation's answer for a source at 0.5 m and an image at 70 mm.
    const OptimizationOutcome outcome = optimizeExampleReflector(*project);
    ASSERT_TRUE(outcome.improved);
    EXPECT_DOUBLE_EQ(optimized->state.geometry->parameter("focalLength"), outcome.bestGeometry->parameter("focalLength"));
    EXPECT_NEAR(optimized->state.geometry->parameter("focalLength"),
                1.0 / (1.0 / kOptimizationSource + 1.0 / kOptimizationTarget), 0.004);
    EXPECT_GT(outcome.best.objective, 3.0 * outcome.initial.objective);

    // As stored, only the visible original is simulated.
    const AcousticSettings s = settingsOf(*project);
    const ReflectorCollection collection = collectReflectors(project->scene, project->materials, s.medium.wavelength(s.frequencyHz));
    ASSERT_EQ(collection.surfaces.size(), 1u);
    EXPECT_EQ(collection.surfaces[0].objectId, Id("ex-me-reflector-original"));
}
