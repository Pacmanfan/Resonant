// The mechanical domain driven from projects: idealization, attachments,
// analyses, targeting, band structure, measured data, radiation, and the
// mechanics example projects.
//
// To regenerate the example files after an intended change:
//   RESONANT_WRITE_EXAMPLES=1 resonant_core_tests --gtest_filter='MechanicalExamples.*'
#include "acoustics/Settings.h"
#include "coupling/StructuralRadiation.h"
#include "dsp/Spectrum.h"
#include "mechanics/Bloch.h"
#include "mechanics/DisplayMesh.h"
#include "mechanics/Measurement.h"
#include "mechanics/Meshing.h"
#include "mechanics/Study.h"
#include "mechanics/Targeting.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace resonant;
using namespace resonant::mechanics;

namespace {

constexpr double pi = 3.14159265358979323846;

std::unique_ptr<Project> newProject(const std::string& slug, const std::string& name, const MechanicsSettings& settings = {})
{
    auto project = Project::createNew(name);
    project->id = Id("ex-mechanics-" + slug);
    project->createdUtc = project->modifiedUtc = "2026-11-26T00:00:00Z";
    storeMechanicsSettings(*project, settings);
    return project;
}

SceneObject* add(Project& project, const std::string& id, const char* type, const std::string& name, ComponentPtr component,
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

Geometry shape(const char* kind, std::initializer_list<std::pair<const char*, double>> parameters)
{
    Geometry g = Geometry::create(kind).take();
    for (const auto& [key, value] : parameters) {
        const Status status = g.setParameter(key, value);
        EXPECT_TRUE(status.ok()) << key << ": " << status.message();
    }
    return g;
}

ComponentPtr structure(Idealization idealization, std::array<Support, 6> supports, double elementSize = 0.0)
{
    StructureSettings s;
    s.idealization = idealization;
    s.supports = supports;
    s.elementSize = elementSize;
    return std::make_shared<StructureComponent>(s);
}

ComponentPtr actuator(double amplitude = 1.0, dsp::Waveform waveform = {}, double phase = 0.0)
{
    ActuatorSettings s;
    s.amplitude = amplitude;
    s.phase = phase;
    s.waveform = waveform;
    return std::make_shared<ActuatorComponent>(s);
}

ComponentPtr sensor()
{
    return std::make_shared<SensorComponent>();
}

constexpr Support F = Support::Free, P = Support::Pinned, C = Support::Clamped;

std::shared_ptr<const StructureStudy> studyOf(const Project& project, const char* id)
{
    auto study = std::make_shared<const StructureStudy>(prepareStudy(project, Id(id)));
    EXPECT_TRUE(study->allowed) << study->refusal;
    return study;
}

std::shared_ptr<const StructuralResult> outcome(StructuralOutcome o)
{
    EXPECT_TRUE(o.ok()) << o.message();
    return o.ok() ? o.take() : nullptr;
}

ElasticMaterial libraryMaterial(const Project& project, const char* id)
{
    return elasticMaterialOf(*project.materials.find(Id(id))).take();
}

// Fraction of a mode's translational motion along a world axis.
double motionAlong(const StructuralModel& model, const Eigen::VectorXd& shape, int axis)
{
    double along = 0.0, total = 0.0;
    for (size_t n = 0; n < model.nodes.size(); ++n) {
        const Vec3 t = model.translation(shape, n);
        along += t(axis) * t(axis);
        total += t.squaredNorm();
    }
    return total > 0.0 ? along / total : 0.0;
}

std::unique_ptr<Project> checkedExample(const std::string& file, const Project& built)
{
    const std::filesystem::path path = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "mechanics" / file;
    const nlohmann::json expected = serializeProject(built);
    if (std::getenv("RESONANT_WRITE_EXAMPLES")) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << expected.dump(2) << "\n";
    }
    auto loaded = loadProjectFile(path);
    EXPECT_TRUE(loaded.ok()) << loaded.message();
    if (!loaded.ok())
        return nullptr;
    EXPECT_TRUE(serializeProject(*loaded.value().project).dump() == nlohmann::json::parse(expected.dump()).dump())
        << file << " is stale; regenerate it with RESONANT_WRITE_EXAMPLES=1";
    return std::move(loaded.value().project);
}

// --- example builders ----------------------------------------------------------------------

// A: an aluminium cantilever 500 x 30 x 5 mm along X.
std::unique_ptr<Project> buildBeam(bool withActuator = false)
{
    MechanicsSettings s;
    s.modeCount = 10;
    s.frequencyMin = 5.0;
    s.frequencyMax = 500.0;
    s.frequencyPoints = 800;
    s.damping = Damping{DampingKind::LossFactor, 0.0, 0.0, 0.02, true, 0.0};
    auto project = newProject(withActuator ? "c" : "a", withActuator ? "forced-resonance" : "vibrating-beam", s);
    add(*project, "ex-me-beam", resonant::object_types::Geometry, "Aluminium cantilever", structure(Idealization::Beam, {C, F, F, F, F, F}),
        Vec3::Zero(), shape("box", {{"width", 0.5}, {"depth", 0.03}, {"height", 0.005}}), Id("mat-aluminum"));
    if (withActuator) {
        add(*project, "ex-me-actuator", mechanics::object_types::Actuator, "Shaker at the tip (ideal force)", actuator(1.0), Vec3(0.245, 0, 0.0025));
        add(*project, "ex-me-sensor", mechanics::object_types::Sensor, "Accelerometer at the tip", sensor(), Vec3(0.25, 0, 0.0025));
        add(*project, "ex-me-sensor-mid", mechanics::object_types::Sensor, "Accelerometer at mid-length", sensor(), Vec3(0.0, 0, 0.0025));
    }
    return project;
}

// B: an aluminium plate 400 x 300 x 2 mm, simply supported.
std::unique_ptr<Project> buildPlate()
{
    MechanicsSettings s;
    s.modeCount = 8;
    auto project = newProject("b", "vibrating-plate", s);
    add(*project, "ex-mb-plate", resonant::object_types::Geometry, "Aluminium plate (simply supported)", structure(Idealization::Plate, {P, P, P, P, F, F}),
        Vec3::Zero(), shape("box", {{"width", 0.4}, {"depth", 0.3}, {"height", 0.002}}), Id("mat-aluminum"));
    return project;
}

// D: a clamped steel plate struck by a short force pulse.
std::unique_ptr<Project> buildTransient()
{
    MechanicsSettings s;
    s.modeCount = 6;
    s.transientDuration = 0.2;
    s.stepsPerPeriod = 20.0;
    s.transientDamping = Damping{DampingKind::Rayleigh, 2.0, 0.0, 0.0, true, 0.0};
    s.snapshotCount = 80;
    auto project = newProject("d", "transient-vibration", s);
    add(*project, "ex-md-plate", resonant::object_types::Geometry, "Steel plate (clamped)", structure(Idealization::Plate, {C, C, C, C, F, F}),
        Vec3::Zero(), shape("box", {{"width", 0.3}, {"depth", 0.2}, {"height", 0.002}}), Id("mat-steel"));
    dsp::Waveform hit;
    hit.kind = dsp::WaveformKind::Ricker;
    hit.frequencyHz = 400.0;
    add(*project, "ex-md-hammer", mechanics::object_types::Actuator, "Impulse (Ricker force pulse)", actuator(5.0, hit), Vec3(-0.07, 0.04, 0.001));
    add(*project, "ex-md-sensor", mechanics::object_types::Sensor, "Accelerometer", sensor(), Vec3(0.05, -0.03, 0.001));
    return project;
}

// E: a free steel rod 2 m long, diameter 10 mm, hit axially at one end.
std::unique_ptr<Project> buildRod()
{
    MechanicsSettings s;
    s.transientDuration = 0.8e-3;
    s.stepsPerPeriod = 20.0;
    s.snapshotCount = 80;
    auto project = newProject("e", "wave-propagation", s);
    add(*project, "ex-mf-rod", resonant::object_types::Geometry, "Steel rod (free-free)", structure(Idealization::Beam, {F, F, F, F, F, F}, 0.005),
        Vec3::Zero(), shape("cylinder", {{"radius", 0.005}, {"height", 2.0}}), Id("mat-steel"));
    dsp::Waveform pulse;
    pulse.kind = dsp::WaveformKind::GaussianPulse;
    pulse.frequencyHz = 20000.0;
    pulse.fractionalBandwidth = 0.8;
    add(*project, "ex-mf-hammer", mechanics::object_types::Actuator, "Axial force pulse at the end", actuator(100.0, pulse), Vec3(0, 0, -1.0));
    add(*project, "ex-mf-s1", mechanics::object_types::Sensor, "Axial sensor at 0.5 m", sensor(), Vec3(0, 0, -0.5));
    add(*project, "ex-mf-s2", mechanics::object_types::Sensor, "Axial sensor at 1.5 m", sensor(), Vec3(0, 0, 0.5));
    return project;
}

// F: four actuators on a simply supported plate, one target and two quiet points.
std::unique_ptr<Project> concentrationScene(const std::vector<Complex>& forces)
{
    MechanicsSettings s;
    s.targetFrequency = 150.0;
    s.regularization = 1e-6;
    s.frequencyMin = 20.0;
    s.frequencyMax = 400.0;
    auto project = newProject("f", "vibration-concentration", s);
    add(*project, "ex-mg-plate", resonant::object_types::Geometry, "Aluminium plate (simply supported)", structure(Idealization::Plate, {P, P, P, P, F, F}),
        Vec3::Zero(), shape("box", {{"width", 0.4}, {"depth", 0.3}, {"height", 0.002}}), Id("mat-aluminum"));
    const Vec3 positions[4] = {{-0.12, -0.09, 0.001}, {0.12, -0.09, 0.001}, {0.12, 0.09, 0.001}, {-0.12, 0.09, 0.001}};
    for (int i = 0; i < 4; ++i) {
        const Complex f = forces.empty() ? Complex(1.0, 0.0) : forces[static_cast<size_t>(i)];
        double phase = std::arg(f);
        if (phase < 0.0)
            phase += 2.0 * pi;
        add(*project, "ex-mg-a" + std::to_string(i + 1), mechanics::object_types::Actuator, "Actuator " + std::to_string(i + 1),
            actuator(std::round(std::abs(f) * 1e9) / 1e9, {}, std::round(phase * 1e6) / 1e6), positions[i]);
    }
    VibrationTargetSettings wanted;
    wanted.amplitude = 10e-6;
    add(*project, "ex-mg-target", mechanics::object_types::VibrationTarget, "Target: 10 um", std::make_shared<VibrationTargetComponent>(wanted), Vec3(0.05, 0.04, 0.001));
    VibrationTargetSettings quiet;
    quiet.suppress = true;
    add(*project, "ex-mg-quiet1", mechanics::object_types::VibrationTarget, "Quiet point 1", std::make_shared<VibrationTargetComponent>(quiet), Vec3(-0.1, 0.05, 0.001));
    add(*project, "ex-mg-quiet2", mechanics::object_types::VibrationTarget, "Quiet point 2", std::make_shared<VibrationTargetComponent>(quiet), Vec3(0.1, -0.08, 0.001));
    return project;
}

std::vector<TargetRequest> requestsOf(const StructureStudy& study)
{
    std::vector<TargetRequest> r;
    for (const TargetPoint& t : study.targets)
        r.push_back({t.label, t.point, t.desired, t.weight});
    return r;
}

Result<TargetingSolution> solveFor(const Project& project)
{
    const auto study = studyOf(project, "ex-mg-plate");
    const MechanicsSettings s = mechanicsSettingsOf(project);
    return solveTargets(*study->model, assemble(*study->model), study->forces, requestsOf(*study), s.targetFrequency, s.damping, s.regularization);
}

std::unique_ptr<Project> buildConcentration()
{
    auto scene = concentrationScene({});
    const auto solved = solveFor(*scene);
    EXPECT_TRUE(solved.ok()) << solved.message();
    return concentrationScene(solved.ok() ? solved.value().forces : std::vector<Complex>{});
}

// G: a periodic steel rod, eight cells of two diameters.
std::unique_ptr<Project> buildPhononic()
{
    MechanicsSettings s;
    s.frequencyMin = 1000.0;
    s.frequencyMax = 45000.0;
    s.frequencyPoints = 600;
    s.logSpacing = false;
    s.damping = Damping{DampingKind::LossFactor, 0.0, 0.0, 0.002, true, 0.0};
    auto project = newProject("g", "phononic-rod", s);
    add(*project, "ex-mh-rod", resonant::object_types::Geometry, "Periodic steel rod (8 cells)", structure(Idealization::Beam, {F, F, F, F, F, F}, 0.005),
        Vec3::Zero(), shape("rod.periodic", {{"radiusA", 0.01}, {"lengthA", 0.06}, {"radiusB", 0.004}, {"lengthB", 0.04}, {"cells", 8}}),
        Id("mat-steel"));
    add(*project, "ex-mh-drive", mechanics::object_types::Actuator, "Axial drive", actuator(1.0), Vec3(0, 0, -0.4));
    add(*project, "ex-mh-out", mechanics::object_types::Sensor, "Axial response at the far end", sensor(), Vec3(0, 0, 0.4));
    return project;
}

// H: a stepped titanium horn.
std::unique_ptr<Project> buildHorn()
{
    MechanicsSettings s;
    s.modeCount = 16;
    auto project = newProject("h", "stepped-horn", s);
    add(*project, "ex-mi-horn", resonant::object_types::Geometry, "Stepped horn (titanium)", structure(Idealization::Beam, {F, F, F, F, F, F}, 0.0025),
        Vec3::Zero(), shape("rod.horn", {{"inputRadius", 0.02}, {"outputRadius", 0.01}, {"length", 0.25}, {"profile", 2}}), Id("mat-titanium"));
    return project;
}

}  // namespace

// --- idealization ---------------------------------------------------------------------------

TEST(MechanicalStudy, IdealizationsFollowTheGeometryAndExplainThemselves)
{
    const auto project = buildBeam(true);
    const auto study = studyOf(*project, "ex-me-beam");
    EXPECT_EQ(study->model->kind, ElementKind::Frame);
    EXPECT_EQ(study->grid[0], 40);
    EXPECT_NEAR(study->elementSize, 0.5 / 40, 1e-12);
    EXPECT_EQ(study->forces.size(), 1u);
    EXPECT_EQ(study->sensors.size(), 2u);
    // The beam axis runs along the longest side; the section is 30 x 5 mm.
    EXPECT_TRUE(study->model->nodes.front().isApprox(Vec3(-0.25, 0, 0)));
    EXPECT_NEAR(study->model->sections.front().area, 1.5e-4, 1e-12);
    bool explained = false;
    for (const std::string& note : study->notes)
        explained = explained || note.find("Beam along local X, length 500 mm, rectangle 30 x 5 mm") != std::string::npos;
    EXPECT_TRUE(explained);
    bool eulerBernoulli = false;
    for (const std::string& a : study->assumptions)
        eulerBernoulli = eulerBernoulli || a.find("Euler-Bernoulli") != std::string::npos;
    EXPECT_TRUE(eulerBernoulli);
    EXPECT_NE(study->summary().find("frame (Euler-Bernoulli beam)"), std::string::npos);

    // A stubby beam and a thick plate are warned about.
    auto stubby = newProject("x", "x");
    add(*stubby, "b", resonant::object_types::Geometry, "Block", structure(Idealization::Beam, {C, F, F, F, F, F}), Vec3::Zero(),
        shape("box", {{"width", 0.1}, {"depth", 0.03}, {"height", 0.03}}), Id("mat-steel"));
    add(*stubby, "p", resonant::object_types::Geometry, "Slab", structure(Idealization::Plate, {P, P, P, P, F, F}), Vec3(1, 0, 0),
        shape("box", {{"width", 0.1}, {"depth", 0.1}, {"height", 0.01}}), Id("mat-steel"));
    const StructureStudy beam = prepareStudy(*stubby, Id("b"));
    ASSERT_FALSE(beam.warnings.empty());
    EXPECT_NE(beam.warnings[0].find("stubby"), std::string::npos);
    const StructureStudy slab = prepareStudy(*stubby, Id("p"));
    ASSERT_FALSE(slab.warnings.empty());
    EXPECT_NE(slab.warnings[0].find("thick"), std::string::npos);

    // What cannot be idealized is refused with the reason.
    auto odd = newProject("y", "y");
    add(*odd, "s", resonant::object_types::Geometry, "Ball", structure(Idealization::Plate, {P, P, P, P, F, F}), Vec3::Zero(), shape("sphere", {{"radius", 0.1}}),
        Id("mat-steel"));
    add(*odd, "n", resonant::object_types::Geometry, "No material", structure(Idealization::Solid, {C, F, F, F, F, F}), Vec3(1, 0, 0),
        shape("box", {{"width", 0.1}, {"depth", 0.1}, {"height", 0.1}}));
    add(*odd, "w", resonant::object_types::Geometry, "Water box", structure(Idealization::Solid, {C, F, F, F, F, F}), Vec3(2, 0, 0),
        shape("box", {{"width", 0.1}, {"depth", 0.1}, {"height", 0.1}}), Id("mat-water"));
    add(*odd, "q", resonant::object_types::Geometry, "Sheet", structure(Idealization::Membrane, {F, F, F, F, F, F}), Vec3(3, 0, 0),
        shape("plane", {{"width", 0.1}, {"depth", 0.1}}), Id("mat-steel"));
    EXPECT_NE(prepareStudy(*odd, Id("s")).refusal.find("cannot be idealized as a plate"), std::string::npos);
    EXPECT_NE(prepareStudy(*odd, Id("n")).refusal.find("assign a material"), std::string::npos);
    EXPECT_NE(prepareStudy(*odd, Id("w")).refusal.find("has no mechanical"), std::string::npos);
    EXPECT_NE(prepareStudy(*odd, Id("q")).refusal.find("set the structure's thickness"), std::string::npos);
    EXPECT_NE(prepareStudy(*odd, Id("nothing")).refusal.find("select a structure"), std::string::npos);
    // A mesh that is too fine for the memory limit is refused, not coarsened.
    auto fine = newProject("z", "z");
    add(*fine, "f", resonant::object_types::Geometry, "Block", structure(Idealization::Solid, {C, F, F, F, F, F}, 0.001), Vec3::Zero(),
        shape("box", {{"width", 0.1}, {"depth", 0.1}, {"height", 0.1}}), Id("mat-steel"));
    EXPECT_NE(prepareStudy(*fine, Id("f")).refusal.find("300 000"), std::string::npos);
}

TEST(MechanicalStudy, AttachmentsDirectionsAndLostForceComponentsAreReported)
{
    auto project = buildPlate();
    add(*project, "a", mechanics::object_types::Actuator, "Tilted actuator", actuator(2.0), Vec3(0.05, 0.05, 0.001), {}, {}, Vec3(0.0, pi / 3, 0.0));
    add(*project, "far", mechanics::object_types::Actuator, "Far actuator", actuator(), Vec3(5, 5, 5));
    ActuatorSettings named;
    named.structure = Id("ex-mb-plate");
    add(*project, "named", mechanics::object_types::Actuator, "Named but far", std::make_shared<ActuatorComponent>(named), Vec3(0, 0, 0.5));
    add(*project, "s", mechanics::object_types::Sensor, "Sensor", sensor(), Vec3(-0.1, 0.0, -0.001));
    const StructureStudy study = prepareStudy(*project, Id("ex-mb-plate"));
    ASSERT_TRUE(study.allowed);
    ASSERT_EQ(study.forces.size(), 1u);
    // Tilted by 60 degrees: only cos(60) of the force is out of plane.
    EXPECT_NEAR(study.forces[0].pattern.sum(), 0.5, 1e-9);
    std::string warnings;
    for (const std::string& w : study.warnings)
        warnings += w + "\n";
    EXPECT_NE(warnings.find("50 % of the force of 'Tilted actuator'"), std::string::npos) << warnings;
    EXPECT_NE(warnings.find("'Named but far' is 500 mm from the structure"), std::string::npos) << warnings;
    EXPECT_EQ(warnings.find("Far actuator"), std::string::npos);  // not meant for this structure: silently not attached
    ASSERT_EQ(study.sensors.size(), 1u);
}

TEST(MechanicalStudy, SettingsPersistValidateAndChooseTheTimeStep)
{
    MechanicsSettings s;
    s.modeCount = 20;
    s.harmonicMethod = HarmonicMethod::Modal;
    s.damping = Damping{DampingKind::Rayleigh, 3.0, 1e-6, 0.0, true, 0.0};
    s.timeStep = 0.0;
    auto project = Project::createNew("p");
    storeMechanicsSettings(*project, s);
    auto reloaded = deserializeProject(serializeProject(*project));
    ASSERT_TRUE(reloaded.ok());
    EXPECT_TRUE(mechanicsSettingsOf(*reloaded.value().project) == s);
    MechanicsSettings bad = s;
    bad.transientDamping.kind = DampingKind::LossFactor;
    EXPECT_FALSE(bad.validate().ok());
    bad = s;
    bad.frequencyMax = bad.frequencyMin;
    EXPECT_FALSE(bad.validate().ok());
    const std::vector<double> f = MechanicsSettings{}.frequencies();
    EXPECT_DOUBLE_EQ(f.front(), 10.0);
    EXPECT_NEAR(f.back(), 2000.0, 1e-9);
    EXPECT_NEAR(f[1] / f[0], f[2] / f[1], 1e-12);  // log spacing

    const auto rod = buildRod();
    const auto study = studyOf(*rod, "ex-mf-rod");
    std::string basis;
    const double dt = transientTimeStep(*study, mechanicsSettingsOf(*rod), &basis);
    EXPECT_NEAR(dt, 1.0 / (20.0 * 20000.0 * 1.8), 1e-15);
    EXPECT_NE(basis.find("20 steps per period"), std::string::npos);
}

// --- examples -------------------------------------------------------------------------------

TEST(MechanicalExamples, A_CantileverModesMatchEulerBernoulli)
{
    const auto project = checkedExample("a-vibrating-beam.resonant", *buildBeam());
    ASSERT_NE(project, nullptr);
    const auto result = outcome(runModal(studyOf(*project, "ex-me-beam"), mechanicsSettingsOf(*project)));
    ASSERT_NE(result, nullptr);
    const ElasticMaterial m = libraryMaterial(*project, "mat-aluminum");
    const double I = 0.03 * std::pow(0.005, 3) / 12.0, A = 1.5e-4, L = 0.5;
    const double betaL[] = {1.875104, 4.694091, 7.854757, 10.995541};
    std::vector<double> bendingZ;
    for (const Mode& mode : result->modal->modes)
        if (motionAlong(*result->study->model, mode.shape, 2) > 0.99)
            bendingZ.push_back(mode.frequencyHz);
    ASSERT_GE(bendingZ.size(), 4u);
    for (size_t n = 0; n < 4; ++n)
        EXPECT_NEAR(bendingZ[n], betaL[n] * betaL[n] / (2 * pi * L * L) * std::sqrt(m.youngsModulus * I / (m.density * A)), 0.001 * bendingZ[n]);
    EXPECT_EQ(result->metadata.solverId, "mechanical.fem.modal");
    EXPECT_EQ(result->metadata.domain, PhysicsDomain::Mechanical);
    EXPECT_EQ(result->metadata.configuration["structure"]["material"]["name"], "Aluminum");
    bool arbitrary = false;
    for (const std::string& a : result->metadata.assumptions)
        arbitrary = arbitrary || a.find("amplitude is arbitrary") != std::string::npos;
    EXPECT_TRUE(arbitrary);
    // The display surface of a mode: a tube that follows the deformed axis, with the undeformed axis as outline.
    const Mode& first = result->modal->modes.front();
    std::vector<double> values(result->study->model->nodes.size());
    for (size_t n = 0; n < values.size(); ++n)
        values[n] = result->study->model->translation(first.shape, n).norm();
    const double scale = automaticScale(*result->study->model, first.shape);
    const SurfaceMesh surface = deformedSurface(*result->study->model, result->study->grid, first.shape, scale, values);
    EXPECT_EQ(surface.positions.size(), 41u * 4u);
    EXPECT_EQ(surface.outline.size(), 1u);
    float top = 0.0f;
    for (const Eigen::Vector3f& p : surface.positions)
        top = std::max(top, std::abs(p.z()));
    EXPECT_NEAR(top, 0.1 * 0.5 + 0.0025, 1e-4);  // 10 % of the length, plus half the height
}

TEST(MechanicalExamples, B_PlateModesMatchNavierAndShowTheirNodalLines)
{
    const auto project = checkedExample("b-vibrating-plate.resonant", *buildPlate());
    ASSERT_NE(project, nullptr);
    const auto result = outcome(runModal(studyOf(*project, "ex-mb-plate"), mechanicsSettingsOf(*project)));
    ASSERT_NE(result, nullptr);
    const ElasticMaterial m = libraryMaterial(*project, "mat-aluminum");
    const double a = 0.4, b = 0.3, h = 0.002, D = m.plateBendingStiffness(h);
    struct Expected {
        double f;
        int mx, ny;
    };
    std::vector<Expected> exact;
    for (int mx = 1; mx <= 4; ++mx)
        for (int ny = 1; ny <= 4; ++ny)
            exact.push_back({pi / 2 * ((mx / a) * (mx / a) + (ny / b) * (ny / b)) * std::sqrt(D / (m.density * h)), mx, ny});
    std::sort(exact.begin(), exact.end(), [](const Expected& x, const Expected& y) { return x.f < y.f; });
    for (size_t r = 0; r < 6; ++r)
        EXPECT_NEAR(result->modal->modes[r].frequencyHz / exact[r].f, 1.0, 0.01) << r;
    // The second mode is (2,1): a nodal line across the middle of the long side.
    ASSERT_EQ(exact[1].mx, 2);
    const StructuralModel& model = *result->study->model;
    const Eigen::VectorXd& shape = result->modal->modes[1].shape;
    double onLine = 0.0, largest = 0.0;
    for (size_t n = 0; n < model.nodes.size(); ++n) {
        const double w = std::abs(shape(static_cast<Eigen::Index>(model.dof(n, 0))));
        largest = std::max(largest, w);
        if (std::abs(model.nodes[n].x()) < 1e-9)
            onLine = std::max(onLine, w);
    }
    EXPECT_LT(onLine, 1e-6 * largest);
}

TEST(MechanicalExamples, C_FrequencySweepPeaksAtTheNaturalFrequenciesWithTheLossFactorBandwidth)
{
    const auto project = checkedExample("c-forced-resonance.resonant", *buildBeam(true));
    ASSERT_NE(project, nullptr);
    const auto study = studyOf(*project, "ex-me-beam");
    const MechanicsSettings settings = mechanicsSettingsOf(*project);
    const auto harmonic = outcome(runHarmonic(study, settings));
    const auto modal = outcome(runModal(study, settings));
    ASSERT_NE(harmonic, nullptr);
    const auto& tip = harmonic->harmonic->responses[0];
    const auto& f = harmonic->harmonic->frequenciesHz;
    // Peaks of the tip response.
    std::vector<double> peaks;
    for (size_t i = 1; i + 1 < tip.size(); ++i)
        if (std::abs(tip[i]) > std::abs(tip[i - 1]) && std::abs(tip[i]) >= std::abs(tip[i + 1]))
            peaks.push_back(f[i]);
    std::vector<double> bendingZ;
    for (const Mode& mode : modal->modal->modes)
        if (motionAlong(*study->model, mode.shape, 2) > 0.99 && mode.frequencyHz < 500.0)
            bendingZ.push_back(mode.frequencyHz);
    ASSERT_GE(peaks.size(), 3u);
    for (size_t n = 0; n < 3; ++n)
        EXPECT_NEAR(peaks[n], bendingZ[n], 0.006 * bendingZ[n]) << n;  // within the sweep's spacing
    // Fine sweep around the first resonance: half-power bandwidth = eta f.
    MechanicsSettings fine = settings;
    fine.frequencyMin = 0.95 * bendingZ[0];
    fine.frequencyMax = 1.05 * bendingZ[0];
    fine.frequencyPoints = 2001;
    fine.logSpacing = false;
    const auto zoom = outcome(runHarmonic(study, fine));
    const auto& r = zoom->harmonic->responses[0];
    size_t peak = 0;
    for (size_t i = 0; i < r.size(); ++i)
        if (std::abs(r[i]) > std::abs(r[peak]))
            peak = i;
    size_t lo = peak, hi = peak;
    while (lo > 0 && std::abs(r[lo]) > std::abs(r[peak]) / std::sqrt(2.0))
        --lo;
    while (hi + 1 < r.size() && std::abs(r[hi]) > std::abs(r[peak]) / std::sqrt(2.0))
        ++hi;
    const double bandwidth = zoom->harmonic->frequenciesHz[hi] - zoom->harmonic->frequenciesHz[lo];
    EXPECT_NEAR(bandwidth / bendingZ[0], 0.02, 0.001);
    // Reproducibility metadata.
    EXPECT_EQ(harmonic->metadata.solverId, "mechanical.fem.harmonic");
    EXPECT_EQ(harmonic->metadata.configuration["forces"].size(), 1u);
    EXPECT_NE(harmonic->metadata.configuration["damping"].get<std::string>().find("loss factor"), std::string::npos);
}

TEST(MechanicalExamples, D_ImpulseResponseRingsAtThePlateModesAndDecays)
{
    const auto project = checkedExample("d-transient-vibration.resonant", *buildTransient());
    ASSERT_NE(project, nullptr);
    const auto study = studyOf(*project, "ex-md-plate");
    const MechanicsSettings settings = mechanicsSettingsOf(*project);
    const auto transient = outcome(runTransient(study, settings));
    const auto modal = outcome(runModal(study, settings));
    ASSERT_NE(transient, nullptr);
    const TransientSolution& t = *transient->transient;
    EXPECT_EQ(t.snapshots.size(), 80u);
    // Spectrum of the sensor's displacement: the strongest peaks are plate modes.
    const dsp::Spectrum spectrum = dsp::computeSpectrum(t.displacement[0], t.dt);
    const auto peaks = dsp::findPeaks(spectrum, 0.1, 20.0, 1000.0);
    ASSERT_GE(peaks.size(), 2u);
    for (size_t p = 0; p < 2; ++p) {
        double nearest = 1e300;
        for (const Mode& mode : modal->modal->modes)
            nearest = std::min(nearest, std::abs(mode.frequencyHz - peaks[p].frequencyHz));
        EXPECT_LT(nearest, spectrum.resolutionHz) << peaks[p].frequencyHz;
    }
    // After the pulse the mechanical energy only decreases (Rayleigh damping), and the balance closes.
    for (size_t i = 1; i < t.energyTimes.size(); ++i) {
        if (t.energyTimes[i - 1] < 0.01)
            continue;
        EXPECT_LE(t.kinetic[i] + t.strain[i], (t.kinetic[i - 1] + t.strain[i - 1]) * (1.0 + 1e-9));
        EXPECT_GE(t.dissipated[i], t.dissipated[i - 1] - 1e-12 * t.externalWork.back());
    }
    EXPECT_NE(transient->metadata.configuration["method"].get<std::string>().find("Newmark"), std::string::npos);
}

TEST(MechanicalExamples, E_AxialPulseTravelsAtTheBarSpeedAndReturnsFromTheFreeEnd)
{
    const auto project = checkedExample("e-wave-propagation.resonant", *buildRod());
    ASSERT_NE(project, nullptr);
    const auto result = outcome(runTransient(studyOf(*project, "ex-mf-rod"), mechanicsSettingsOf(*project)));
    ASSERT_NE(result, nullptr);
    const TransientSolution& t = *result->transient;
    const double c = libraryMaterial(*project, "mat-steel").barSpeed();
    const auto arrival = [&](const std::vector<double>& v, double before) {
        size_t best = 0;
        for (size_t i = 0; i < v.size() && t.time[i] < before; ++i)
            if (std::abs(v[i]) > std::abs(v[best]))
                best = i;
        return t.time[best];
    };
    const double speed = 1.0 / (arrival(t.velocity[1], 2.4 / c) - arrival(t.velocity[0], 2.4 / c));
    EXPECT_NEAR(speed, c, 0.015 * c);
    // The free far end returns the pulse with the same sign of velocity.
    double incident = 0.0, reflected = 0.0;
    for (size_t i = 0; i < t.time.size(); ++i) {
        const double v = t.velocity[1][i];
        if (t.time[i] < 2.2 / c && std::abs(v) > std::abs(incident))
            incident = v;
        if (t.time[i] > 2.2 / c && t.time[i] < 3.2 / c && std::abs(v) > std::abs(reflected))
            reflected = v;
    }
    EXPECT_NEAR(reflected / incident, 1.0, 0.06);
}

TEST(MechanicalExamples, F_ActuatorInputsConcentrateVibrationAtTheTargetAndQuietTheOthers)
{
    const auto project = checkedExample("f-vibration-concentration.resonant", *buildConcentration());
    ASSERT_NE(project, nullptr);
    // Re-solving the stored scene reproduces the stored inputs.
    const auto solved = solveFor(*concentrationScene({}));
    ASSERT_TRUE(solved.ok()) << solved.message();
    EXPECT_LT(solved.value().relativeResidual, 1e-4);
    ASSERT_EQ(solved.value().singularValues.size(), 3u);
    // A harmonic calculation of the stored project at the target frequency.
    const auto study = studyOf(*project, "ex-mg-plate");
    const MechanicsSettings s = mechanicsSettingsOf(*project);
    HarmonicSettings h;
    h.frequenciesHz = {s.targetFrequency};
    h.damping = s.damping;
    std::vector<ResponsePoint> points;
    for (const TargetPoint& t : study->targets)
        points.push_back(t.point);
    const auto response = solveHarmonic(*study->model, assemble(*study->model), study->forces, points, h);
    ASSERT_TRUE(response.ok());
    EXPECT_NEAR(std::abs(response.value().responses[0][0]), 10e-6, 0.01 * 10e-6);
    EXPECT_LT(std::abs(response.value().responses[1][0]), 0.01 * 10e-6);
    EXPECT_LT(std::abs(response.value().responses[2][0]), 0.01 * 10e-6);
    // Without the solved inputs (all actuators 1 N in phase) the quiet points are not quiet.
    const auto plain = studyOf(*concentrationScene({}), "ex-mg-plate");
    const auto uncontrolled = solveHarmonic(*plain->model, assemble(*plain->model), plain->forces, points, h);
    EXPECT_GT(std::abs(uncontrolled.value().responses[1][0]), 0.1 * std::abs(uncontrolled.value().responses[0][0]));
    // More targets than actuators: reported, residual non-zero.
    std::vector<TargetRequest> many = requestsOf(*plain);
    for (int i = 0; i < 4; ++i)
        many.push_back({"extra", responseAt(*plain->model, static_cast<size_t>(40 + 37 * i), Vec3::UnitZ()), Complex(5e-6, 0.0), 1.0});
    const auto over = solveTargets(*plain->model, assemble(*plain->model), plain->forces, many, 150.0, s.damping, 1e-6);
    ASSERT_TRUE(over.ok());
    EXPECT_GT(over.value().relativeResidual, 0.01);
    EXPECT_NE(over.value().notes.find("More targets than actuators"), std::string::npos);
}

TEST(MechanicalExamples, G_PeriodicRodHasTheStopBandsOfItsBlochAnalysisAndTransmitsNothingInThem)
{
    const auto project = checkedExample("g-phononic-rod.resonant", *buildPhononic());
    ASSERT_NE(project, nullptr);
    const SceneObject* rodObject = project->scene.find(Id("ex-mh-rod"));
    const auto rod = periodicRodOf(*rodObject->state.geometry, libraryMaterial(*project, "mat-steel"));
    ASSERT_TRUE(rod.ok());
    const BandStructure bands = rodBandStructure(rod.value(), 3, 41);
    const auto reference = analyticalStopBands(rod.value(), 60000.0);
    ASSERT_GE(bands.stopBands.size(), 2u);
    ASSERT_GE(reference.size(), 2u);
    for (size_t b = 0; b < 2; ++b) {
        EXPECT_NEAR(bands.stopBands[b].first, reference[b].first, 0.005 * reference[b].first) << b;
        EXPECT_NEAR(bands.stopBands[b].second, reference[b].second, 0.005 * reference[b].second) << b;
    }
    // The first gap contains the Bragg frequency c / (2a), where a wave is half a period long.
    const double c = rod.value().material.barSpeed();
    EXPECT_LT(reference[0].first, c / (2 * 0.1));
    EXPECT_GT(reference[0].second, c / (2 * 0.1));
    // A finite rod of 8 cells: transmission in the middle of the gap is tiny compared with the pass band below it.
    const auto study = studyOf(*project, "ex-mh-rod");
    const auto harmonic = outcome(runHarmonic(study, mechanicsSettingsOf(*project)));
    const auto& out = harmonic->harmonic->responses[0];
    const auto& f = harmonic->harmonic->frequenciesHz;
    double passBand = 0.0, gap = 1e300;
    const double centre = 0.5 * (reference[0].first + reference[0].second);
    for (size_t i = 0; i < f.size(); ++i) {
        if (f[i] < 0.8 * reference[0].first)
            passBand = std::max(passBand, std::abs(out[i]));
        if (std::abs(f[i] - centre) < 0.05 * centre)
            gap = std::min(gap, std::abs(out[i]));
    }
    EXPECT_LT(gap, 1e-3 * passBand);
}

TEST(MechanicalExamples, H_SteppedHornResonatesAtHalfAWavelengthAndAmplifiesByTheAreaRatio)
{
    const auto project = checkedExample("h-stepped-horn.resonant", *buildHorn());
    ASSERT_NE(project, nullptr);
    const auto result = outcome(runModal(studyOf(*project, "ex-mi-horn"), mechanicsSettingsOf(*project)));
    ASSERT_NE(result, nullptr);
    const StructuralModel& model = *result->study->model;
    const double c = libraryMaterial(*project, "mat-titanium").barSpeed();
    // The first elastic axial mode.
    const Mode* axial = nullptr;
    for (const Mode& mode : result->modal->modes)
        if (mode.frequencyHz > 100.0 && motionAlong(model, mode.shape, 2) > 0.99) {
            axial = &mode;
            break;
        }
    ASSERT_NE(axial, nullptr);
    EXPECT_NEAR(axial->frequencyHz, c / (2 * 0.25), 0.005 * c / 0.5);
    const double input = model.translation(axial->shape, 0).z(), output = model.translation(axial->shape, model.nodes.size() - 1).z();
    EXPECT_NEAR(std::abs(output / input), 4.0, 0.08);  // (20 mm / 10 mm)^2
    EXPECT_LT(output / input, 0.0);                    // the ends move in opposite directions
}

// --- radiation, measurement ---------------------------------------------------------------------

TEST(MechanicalCoupling, PistonRadiationMatchesTheFarFieldAndAnAntisymmetricModeIsSilentOnAxis)
{
    ElasticMaterial m;
    m.name = "test";
    m.density = 2700.0;
    m.youngsModulus = 7e10;
    m.poissonsRatio = 0.33;
    const StructuralModel plate = rectangularGrid(ElementKind::Plate, Vec3::Zero(), Vec3::UnitX(), Vec3::UnitY(), 0.2, 0.1, 20, 10, 0.002, 0.0, m);
    const acoustics::Medium air = acoustics::airAtTemperature(293.15);
    const double f = 500.0, w = 1e-6;
    // A rigid piston: every node moves by w.
    Eigen::VectorXcd u = Eigen::VectorXcd::Zero(static_cast<Eigen::Index>(plate.dofCount()));
    for (size_t n = 0; n < plate.nodes.size(); ++n)
        u(static_cast<Eigen::Index>(plate.dof(n, 0))) = w;
    acoustics::GridFrame frame;
    frame.origin = Vec3(0, 0, 10.0);
    frame.axisU = Vec3(0.01, 0, 0);
    frame.axisV = Vec3(0, 0.01, 0);
    const auto radiated = coupling::radiateFromPlate(plate, u, f, air, frame, "piston");
    ASSERT_TRUE(radiated.ok()) << radiated.message();
    const double omega = 2 * pi * f;
    const double farField = air.density * omega * omega * w * 0.02 / (2 * pi * 10.0);
    EXPECT_NEAR(std::abs(std::complex<double>(radiated.value()->pressure[0])), farField, 0.01 * farField);
    EXPECT_EQ(radiated.value()->metadata.solverId, "coupling.structure-to-acoustic.rayleigh");
    // Behind the baffle nothing is calculated.
    frame.origin = Vec3(0, 0, -1.0);
    EXPECT_EQ(coupling::radiateFromPlate(plate, u, f, air, frame, "behind").value()->statistics.nearSourceSamples, 4u);
    // An antisymmetric pattern (+w on one half, -w on the other) cancels on the axis.
    for (size_t n = 0; n < plate.nodes.size(); ++n)
        u(static_cast<Eigen::Index>(plate.dof(n, 0))) = plate.nodes[n].x() > 1e-9 ? w : plate.nodes[n].x() < -1e-9 ? -w : 0.0;
    frame.origin = Vec3(0, 0, 2.0);
    frame.axisU = frame.axisV = Vec3::Zero();
    frame.nu = frame.nv = 1;
    EXPECT_LT(std::abs(std::complex<double>(coupling::radiateFromPlate(plate, u, f, air, frame, "dipole").value()->pressure[0])), 1e-6 * farField);
    // Beams cannot radiate this way.
    const StructuralModel beam = frameLine(Vec3::Zero(), Vec3::UnitX(), 4, circularSection(0.01), m, Vec3::UnitY());
    EXPECT_FALSE(coupling::radiateFromPlate(beam, Eigen::VectorXcd::Zero(static_cast<Eigen::Index>(beam.dofCount())), f, air, frame, "beam").ok());
}

TEST(MechanicalMeasurement, CsvImportMetadataAndComparison)
{
    const std::string csv =
        "# sensor: accelerometer A1\n# quantity: displacement\n# unit: m/N\n# position: 0.25 0 0.0025\n# source: test bench, 2026-11-20\n"
        "frequency_Hz, magnitude, phase_deg\n10, 0.001, 0\n20, 0.004, -90\n30, 0.0005, -180\n";
    const auto measured = parseMeasuredResponse(csv, "bench");
    ASSERT_TRUE(measured.ok()) << measured.message();
    EXPECT_EQ(measured.value().sensor, "accelerometer A1");
    EXPECT_EQ(measured.value().quantity, Quantity::Displacement);
    ASSERT_TRUE(measured.value().position.has_value());
    EXPECT_TRUE(measured.value().position->isApprox(Vec3(0.25, 0, 0.0025)));
    EXPECT_NEAR(measured.value().values[1].imag(), -0.004, 1e-12);
    // Comparison: a simulation twice as large everywhere differs by 6.02 dB.
    const std::vector<double> simF = {5, 15, 25, 35};
    const std::vector<std::complex<double>> sim = {0.002, 0.005, 0.009, 0.001};
    const ResponseComparison c = compareResponses({10, 20, 30}, {0.002, 0.008, 0.001}, measured.value());
    EXPECT_EQ(c.points, 3u);
    EXPECT_NEAR(c.rmsLevelDifferenceDb, 20 * std::log10(2.0), 1e-9);
    EXPECT_DOUBLE_EQ(c.largestPeakMeasuredHz, 20.0);
    EXPECT_EQ(compareResponses(simF, sim, measured.value()).points, 3u);
    // Bad files.
    EXPECT_FALSE(parseMeasuredResponse("frequency_Hz,real\n1,2\n", "x").ok());
    EXPECT_FALSE(parseMeasuredResponse("frequency_Hz,real,imag\n2,1,0\n1,1,0\n", "x").ok());
    EXPECT_FALSE(parseMeasuredResponse("frequency_Hz,real,imag\n1,nan,0\n2,1,0\n", "x").ok());
}

TEST(MechanicalPersistence, ComponentsRoundTripThroughTheProjectFile)
{
    auto project = buildTransient();
    VibrationTargetSettings t;
    t.amplitude = 3e-6;
    t.suppress = true;
    t.structure = Id("ex-md-plate");
    add(*project, "t", mechanics::object_types::VibrationTarget, "Target", std::make_shared<VibrationTargetComponent>(t));
    SensorSettings s;
    s.quantity = Quantity::Velocity;
    s.sensitivity = 0.1;
    s.sensitivityUnit = "V/(m/s)";
    add(*project, "v", mechanics::object_types::Sensor, "Vibrometer", std::make_shared<SensorComponent>(s));
    const nlohmann::json saved = serializeProject(*project);
    auto loaded = deserializeProject(saved);
    ASSERT_TRUE(loaded.ok()) << loaded.message();
    EXPECT_EQ(serializeProject(*loaded.value().project), saved);
    const auto* target = dynamic_cast<const VibrationTargetComponent*>(loaded.value().project->scene.find(Id("t"))->state.component.get());
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->settings, t);
    // A broken component is refused with its reason.
    nlohmann::json broken = saved;
    for (auto& object : broken["scene"]["objects"])
        if (object["id"] == "ex-md-plate")
            object["component"]["data"]["idealization"] = "shell";
    const auto refused = deserializeProject(broken);
    if (refused.ok())
        ADD_FAILURE() << "a structure with an unknown idealization was accepted";
}
