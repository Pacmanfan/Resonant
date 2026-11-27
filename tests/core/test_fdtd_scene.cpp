// The FDTD solver driven from projects: planning, simulation geometry,
// sources, and the advanced-acoustics example projects.
//
// To regenerate the example files after an intended change:
//   RESONANT_WRITE_EXAMPLES=1 resonant_core_tests --gtest_filter='FdtdExamples.*'
#include "acoustics/Focusing.h"
#include "acoustics/Settings.h"
#include "fdtd/FdtdSetup.h"
#include "fdtd/SolidModel.h"
#include "foundation/Units.h"
#include "manipulators/Analytics.h"
#include "manipulators/Manipulator.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace resonant;
using namespace resonant::acoustics;
using namespace resonant::fdtd;
using namespace resonant::manipulators;

namespace {

constexpr double pi = 3.14159265358979323846;

SceneObject* add(Project& project, const std::string& id, const char* type, const std::string& name, ComponentPtr component,
                 const Vec3& position = Vec3::Zero(), std::optional<Geometry> geometry = {}, const Id& material = {})
{
    auto object = std::make_unique<SceneObject>(Id(id), type);
    object->state.name = name;
    object->state.component = std::move(component);
    object->state.local.position = position;
    object->state.geometry = std::move(geometry);
    object->state.materialId = material;
    return project.scene.insert(std::move(object)).value();
}

Geometry shape(const char* kind, std::initializer_list<std::pair<const char*, double>> parameters)
{
    Geometry geometry = Geometry::create(kind).take();
    for (const auto& [key, value] : parameters) {
        const Status status = geometry.setParameter(key, value);
        EXPECT_TRUE(status.ok()) << key << ": " << status.message();
    }
    return geometry;
}

ComponentPtr emitter(double frequency = 40000.0, double amplitude = 1.0, double delay = 0.0)
{
    EmitterSettings s;
    s.frequencyHz = frequency;
    s.amplitudePa = amplitude;
    s.delaySec = delay;
    s.radius = 0.002;
    return std::make_shared<EmitterComponent>(s);
}

ComponentPtr plane(double sizeX, double sizeZ)
{
    RegionSettings r;
    r.kind = RegionKind::PlaneXZ;
    r.size = Vec3(sizeX, 0.1, sizeZ);
    return std::make_shared<RegionComponent>(r);
}

ComponentPtr volume(const Vec3& size)
{
    RegionSettings r;
    r.kind = RegionKind::Volume;
    r.size = size;
    return std::make_shared<RegionComponent>(r);
}

ComponentPtr manipulator(Category category, BoundaryCondition boundary = BoundaryCondition::Rigid, std::optional<Vec3> focus = {})
{
    ManipulatorSettings settings;
    settings.category = category;
    settings.boundary = boundary;
    settings.intendedFocus = focus;
    return std::make_shared<ManipulatorComponent>(settings);
}

ComponentPtr probe()
{
    return std::make_shared<ProbeComponent>();
}

std::unique_ptr<Project> newProject(const std::string& slug, const std::string& name, const FdtdSettings& settings, bool water = false)
{
    auto project = Project::createNew(name);
    project->id = Id("ex-advanced-" + slug);
    project->createdUtc = project->modifiedUtc = "2026-11-26T00:00:00Z";
    AcousticSettings acoustic;
    acoustic.medium = water ? waterNominal() : airAtTemperature(293.15);
    storeSettings(*project, acoustic);
    storeFdtdSettings(*project, settings);
    return project;
}

Id regionOf(const Project& project)
{
    Id region;
    project.scene.forEach([&](const SceneObject& object) {
        if (region.isNull() && dynamic_cast<const RegionComponent*>(object.state.component.get()))
            region = object.id();
    });
    return region;
}

Prepared prepared(const Project& project)
{
    return prepare(project, regionOf(project), settingsOf(project).medium, fdtdSettingsOf(project));
}

std::shared_ptr<const FdtdResult> simulated(const Project& project)
{
    Prepared p = prepared(project);
    EXPECT_TRUE(p.plan.allowed) << p.plan.refusal;
    auto result = simulate(project.name, std::move(p), fdtdSettingsOf(project), settingsOf(project).medium);
    EXPECT_TRUE(result.ok()) << result.message();
    return result.ok() ? result.take() : nullptr;
}

const ProbeTrace& trace(const FdtdResult& result, const char* id)
{
    for (const ProbeTrace& t : result.output.probes)
        if (t.objectId == Id(id))
            return t;
    ADD_FAILURE() << "no probe " << id;
    return result.output.probes.front();
}

double peakAbs(const std::vector<double>& x)
{
    double best = 0.0;
    for (const double v : x)
        best = std::max(best, std::abs(v));
    return best;
}

// Time at which the envelope of a pulse peaks: centroid of the squared signal.
double pulseTime(const ProbeTrace& t, double dt)
{
    double weight = 0.0, moment = 0.0;
    for (size_t n = 0; n < t.pressure.size(); ++n) {
        const double w = t.pressure[n] * t.pressure[n];
        weight += w;
        moment += w * static_cast<double>(n + 1) * dt;
    }
    return moment / weight;
}

// |P| of the harmonic field at the cell containing a world point.
double harmonicAt(const FdtdResult& result, const Vec3& world)
{
    const GridFrame& f = result.plan.frame;
    const Vec3 d = world - f.origin;
    const auto cell = [&](const Vec3& axis, int n) {
        return n > 1 ? std::clamp(static_cast<int>(std::lround(d.dot(axis) / axis.squaredNorm() * (n - 1))), 0, n - 1) : 0;
    };
    return std::abs(result.harmonicField->pressure[f.index(cell(f.axisU, f.nu), cell(f.axisV, f.nv), f.nw > 1 ? cell(f.axisW, f.nw) : 0)]);
}

std::unique_ptr<Project> checkedExample(const std::string& file, const Project& built)
{
    const std::filesystem::path path = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "advanced_acoustics" / file;
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

// --- simulation geometry ----------------------------------------------------------------

TEST(FdtdSolidModel, EveryGeometryKindWithABodyHasAnInsideTest)
{
    for (const GeometryKind* kind : GeometryRegistry::instance().kinds()) {
        const Geometry g = Geometry::create(kind->id).take();
        // Shells are given a thickness the sampling below can find.
        const auto thin = solidModelOf(g, 1e-6);
        ASSERT_TRUE(thin.has_value()) << kind->id;
        const auto model = solidModelOf(g, 0.15 * thin->boundingRadius);
        ASSERT_TRUE(model.has_value()) << kind->id;
        EXPECT_GT(model->boundingRadius, 0.0) << kind->id;
        // Nothing of the body lies outside its bounding radius, and the
        // body is not empty.
        size_t inside = 0;
        const double r = model->boundingRadius;
        for (int i = 0; i < 40; ++i)
            for (int j = 0; j < 40; ++j)
                for (int k = 0; k < 40; ++k) {
                    const Vec3 q(-1.2 * r + 2.4 * r * (i + 0.5) / 40, -1.2 * r + 2.4 * r * (j + 0.5) / 40, -1.2 * r + 2.4 * r * (k + 0.5) / 40);
                    if (model->contains(q)) {
                        ++inside;
                        EXPECT_LE(q.norm(), r * (1.0 + 1e-9)) << kind->id;
                    }
                }
        // Slender bodies (rods) can fall between the samples; they contain their own axis.
        EXPECT_TRUE(inside > 0u || model->contains(Vec3::Zero())) << kind->id;
    }
}

TEST(FdtdSolidModel, ShapesFollowTheirDefinitionsAndThinShellsAreThickenedWithANote)
{
    const Geometry parabola = shape("reflector.parabolic", {{"focalLength", 0.05}, {"aperture", 0.2}, {"thickness", 0.004}});
    auto model = solidModelOf(parabola, 0.001).value();
    EXPECT_TRUE(model.note.empty());
    // On the axis the shell is between z = -4 mm and 0; at r = 80 mm the
    // surface is at 80^2 / 200 = 32 mm.
    EXPECT_TRUE(model.contains(Vec3(0, 0, -0.002)));
    EXPECT_FALSE(model.contains(Vec3(0, 0, 0.001)));
    EXPECT_FALSE(model.contains(Vec3(0, 0, -0.005)));
    EXPECT_TRUE(model.contains(Vec3(0.08, 0, 0.031)));
    EXPECT_FALSE(model.contains(Vec3(0.08, 0, 0.033)));
    EXPECT_FALSE(model.contains(Vec3(0.11, 0, 0.058)));  // beyond the aperture
    // A grid that cannot hold 4 mm.
    model = solidModelOf(parabola, 0.01).value();
    EXPECT_NE(model.note.find("represented as 10 mm"), std::string::npos);
    EXPECT_TRUE(model.contains(Vec3(0, 0, -0.008)));

    // A lens: thick in the middle for a biconvex shape.
    const Geometry lens = shape("lens.spherical", {{"aperture", 0.08}, {"centerThickness", 0.02}, {"radius1", 0.1}, {"radius2", -0.1}});
    model = solidModelOf(lens, 0.001).value();
    EXPECT_TRUE(model.contains(Vec3(0, 0, 0.009)));
    EXPECT_FALSE(model.contains(Vec3(0, 0, 0.011)));
    EXPECT_FALSE(model.contains(Vec3(0.039, 0, 0.009)));  // thinner at the edge
    EXPECT_TRUE(model.contains(Vec3(0.039, 0, 0.001)));

    // A slit plate has its gap.
    const Geometry slit = shape("diffraction.slit", {{"width", 0.2}, {"height", 0.2}, {"slitWidth", 0.02}, {"thickness", 0.004}});
    model = solidModelOf(slit, 0.001).value();
    EXPECT_FALSE(model.contains(Vec3(0.005, 0, -0.002)));
    EXPECT_TRUE(model.contains(Vec3(0.015, 0, -0.002)));

    // A cavity is hollow.
    const Geometry cavity = shape("cavity.rectangular", {{"sizeX", 0.04}, {"sizeY", 0.04}, {"sizeZ", 0.03}, {"wallThickness", 0.004}});
    model = solidModelOf(cavity, 0.001).value();
    EXPECT_FALSE(model.contains(Vec3(0.019, 0, 0.014)));
    EXPECT_TRUE(model.contains(Vec3(0.021, 0, 0.0)));
    EXPECT_FALSE(model.contains(Vec3(0.025, 0, 0.0)));
}

// --- planning -------------------------------------------------------------------------------

TEST(FdtdPlan, ResolutionTimeStepAndCostFollowTheSettings)
{
    FdtdSettings settings;
    settings.pointsPerWavelength = 10.0;
    settings.courantFraction = 0.8;
    auto project = newProject("plan", "plan", settings);
    add(*project, "e", acoustics::object_types::Emitter, "Emitter", emitter(34323.197767690584));
    add(*project, "r", acoustics::object_types::Region, "Plane", plane(0.1, 0.05));
    const Medium air = settingsOf(*project).medium;
    Prepared p = prepared(*project);
    ASSERT_TRUE(p.plan.allowed) << p.plan.refusal;
    const FdtdPlan& plan = p.plan;
    EXPECT_EQ(plan.dimensions, 2);
    // Wavelength exactly 10 mm, 10 cells per wavelength: 1 mm cells.
    EXPECT_NEAR(plan.dx, 1e-3, 1e-12);
    EXPECT_EQ(plan.nx, 100);
    EXPECT_EQ(plan.ny, 50);
    EXPECT_EQ(plan.nz, 1);
    EXPECT_NEAR(plan.stabilityLimit, 1e-3 / (air.soundSpeed * std::sqrt(2.0)), 1e-15);
    EXPECT_NEAR(plan.dt, 0.8 * plan.stabilityLimit, 1e-18);
    EXPECT_NEAR(plan.courantNumber, 0.8 / std::sqrt(2.0), 1e-12);
    EXPECT_EQ(plan.totalCells[0], 124);  // 12 absorbing cells on each side
    EXPECT_EQ(plan.cellCount, 124u * 74u);
    EXPECT_EQ(plan.steps, static_cast<long>(std::ceil(plan.duration / plan.dt)));
    EXPECT_NEAR(plan.cellUpdates, static_cast<double>(plan.cellCount) * plan.steps, 1.0);
    EXPECT_NEAR(plan.phaseVelocityError, 1.0 - axialPhaseVelocityRatio(10.0, 0.8 / std::sqrt(2.0)), 1e-12);
    EXPECT_GT(plan.stateBytes, 124u * 74u * 40u);
    EXPECT_NE(plan.durationBasis.find("automatic"), std::string::npos);
    EXPECT_NE(plan.summary().find("10 cells per wavelength"), std::string::npos);
    EXPECT_NE(plan.summary().find("Courant number"), std::string::npos);
    // The cell centres span the region.
    EXPECT_TRUE(plan.frame.point(0, 0, 0).isApprox(Vec3(-0.0495, 0.0, -0.0245), 1e-12));
    EXPECT_TRUE(plan.frame.point(99, 49, 0).isApprox(Vec3(0.0495, 0.0, 0.0245), 1e-12));
    // Two-dimensional runs say what that means.
    bool lineSources = false;
    for (const std::string& a : plan.assumptions)
        lineSources = lineSources || a.find("LINE source") != std::string::npos;
    EXPECT_TRUE(lineSources);

    // Coarse grids warn, with the size of the error.
    settings.pointsPerWavelength = 6.0;
    p = prepare(*project, Id("r"), air, settings);
    ASSERT_FALSE(p.plan.warnings.empty());
    EXPECT_NE(p.plan.warnings[0].find("coarse"), std::string::npos);
    // A short run warns.
    settings.pointsPerWavelength = 10.0;
    settings.duration = 1e-4;
    p = prepare(*project, Id("r"), air, settings);
    bool shortRun = false;
    for (const std::string& w : p.plan.warnings)
        shortRun = shortRun || w.find("shorter than one crossing") != std::string::npos;
    EXPECT_TRUE(shortRun);
    EXPECT_NE(p.plan.durationBasis.find("as set"), std::string::npos);
}

TEST(FdtdPlan, RefusesWhatItCannotDoAndNeverLowersTheResolution)
{
    FdtdSettings settings;
    auto project = newProject("refuse", "refuse", settings);
    add(*project, "r", acoustics::object_types::Region, "Volume", volume(Vec3(0.5, 0.5, 0.5)));
    const Medium air = settingsOf(*project).medium;
    // No emitter.
    EXPECT_NE(prepare(*project, Id("r"), air, settings).plan.refusal.find("no active emitter"), std::string::npos);
    add(*project, "e", acoustics::object_types::Emitter, "Emitter", emitter());
    // 0.5 m cube at 40 kHz, 12 cells per wavelength: 700^3 cells.
    Prepared p = prepare(*project, Id("r"), air, settings);
    EXPECT_FALSE(p.plan.allowed);
    EXPECT_NE(p.plan.refusal.find("memory limit"), std::string::npos);
    EXPECT_NE(p.plan.refusal.find("not lowered automatically"), std::string::npos);
    EXPECT_NEAR(p.plan.dx, air.soundSpeed / 40000.0 / 12.0, 1e-12);  // the plan still states what was asked for
    EXPECT_TRUE(p.problem.grid.cellMaterial.empty());
    EXPECT_FALSE(simulate("x", std::move(p), settings, air).ok());
    // Not a region.
    EXPECT_NE(prepare(*project, Id("e"), air, settings).plan.refusal.find("select a field region"), std::string::npos);
    // Bad settings.
    settings.courantFraction = 1.2;
    EXPECT_NE(prepare(*project, Id("r"), air, settings).plan.refusal.find("stability limit"), std::string::npos);
    settings = FdtdSettings{};
    settings.pointsPerWavelength = 2.0;
    EXPECT_FALSE(prepare(*project, Id("r"), air, settings).plan.allowed);
    // An emitter outside a small region.
    auto small = newProject("outside", "outside", FdtdSettings{});
    add(*small, "e", acoustics::object_types::Emitter, "Emitter", emitter(), Vec3(0, 0, 1.0));
    add(*small, "r", acoustics::object_types::Region, "Plane", plane(0.05, 0.05));
    p = prepared(*small);
    EXPECT_FALSE(p.plan.allowed);
    EXPECT_NE(p.plan.refusal.find("no active emitter lies in the plane"), std::string::npos);
}

TEST(FdtdPlan, ObjectsBecomeRigidPressureReleaseOrMaterialCellsAndTheFastestMaterialSetsTheTimeStep)
{
    FdtdSettings settings;
    auto project = newProject("objects", "objects", settings, true);
    const Medium water = settingsOf(*project).medium;
    add(*project, "e", acoustics::object_types::Emitter, "Emitter", emitter(100000.0), Vec3(0, 0, -0.04));
    add(*project, "r", acoustics::object_types::Region, "Plane", plane(0.12, 0.12));
    add(*project, "steel", "geometry", "Unassigned box", nullptr, Vec3(-0.03, 0, 0.02), shape("box", {{"width", 0.02}, {"depth", 0.1}, {"height", 0.02}}));
    add(*project, "acrylic", "geometry", "Acrylic box", nullptr, Vec3(0.03, 0, 0.02),
        shape("box", {{"width", 0.02}, {"depth", 0.1}, {"height", 0.02}}), Id("mat-acrylic"));
    add(*project, "soft", kManipulatorType, "Soft plate", manipulator(Category::Reflector, BoundaryCondition::PressureRelease),
        Vec3(0, 0, 0.05), shape("reflector.flat", {{"width", 0.04}, {"height", 0.1}, {"thickness", 0.004}}));
    add(*project, "absorber", kManipulatorType, "Ideal absorber", manipulator(Category::Reflector, BoundaryCondition::Absorbing),
        Vec3(0, 0, -0.055), shape("reflector.flat", {{"width", 0.02}, {"height", 0.1}, {"thickness", 0.004}}));
    SceneObject* hidden = add(*project, "hidden", "geometry", "Hidden sphere", nullptr, Vec3::Zero(), shape("sphere", {{"radius", 0.01}}));
    ObjectState state = hidden->state;
    state.visible = false;
    ASSERT_TRUE(project->scene.setState(hidden->id(), state).ok());

    const Prepared p = prepared(*project);
    ASSERT_TRUE(p.plan.allowed) << p.plan.refusal;
    const FdtdPlan& plan = p.plan;
    const Material* acrylic = project->materials.find(Id("mat-acrylic"));
    const double cAcrylic = acrylic->property(PhysicsDomain::Acoustic, "soundSpeed")->value;
    ASSERT_GT(cAcrylic, water.soundSpeed);
    EXPECT_DOUBLE_EQ(plan.fastestSpeed, cAcrylic);
    EXPECT_DOUBLE_EQ(plan.slowestSpeed, water.soundSpeed);
    EXPECT_NEAR(plan.dt, 0.95 * plan.dx / (cAcrylic * std::sqrt(2.0)), 1e-18);
    // 20 x 20 mm boxes on this grid.
    const double perBox = 0.02 * 0.02 / (plan.dx * plan.dx);
    EXPECT_NEAR(static_cast<double>(plan.materialCells), perBox, 0.12 * perBox);
    EXPECT_GT(plan.rigidCells, static_cast<size_t>(perBox));  // the unassigned box and the "absorber"
    EXPECT_GT(plan.pressureReleaseCells, 0u);
    const std::string objects = [&] {
        std::string all;
        for (const std::string& line : plan.objects)
            all += line + "\n";
        return all;
    }();
    EXPECT_NE(objects.find("'Unassigned box': rigid body (no material assigned)"), std::string::npos) << objects;
    EXPECT_NE(objects.find("'Acrylic box': material 'Acrylic' as a fluid"), std::string::npos) << objects;
    EXPECT_NE(objects.find("no shear waves"), std::string::npos);
    EXPECT_NE(objects.find("'Soft plate': pressure-release body"), std::string::npos);
    EXPECT_NE(objects.find("'Hidden sphere' is hidden and was left out"), std::string::npos);
    bool substitution = false;
    for (const std::string& w : plan.warnings)
        substitution = substitution || w.find("'Ideal absorber' is set to an ideal absorbing boundary, which this solver cannot represent; it is simulated as RIGID") != std::string::npos;
    EXPECT_TRUE(substitution);
    ASSERT_EQ(p.materialNames.size(), 4u);  // medium, rigid, acrylic, pressure release
    EXPECT_EQ(p.materialNames[0], "Water");

    // The solver assessment: only the full-wave solver represents all of this.
    const auto assessments = solvers::acousticSolvers().assess(traitsOf(*project, Id("r"), water, settings, plan));
    ASSERT_EQ(assessments.size(), 3u);
    EXPECT_FALSE(assessments[0].complete);
    EXPECT_FALSE(assessments[1].complete);
    EXPECT_TRUE(assessments[2].complete);
    EXPECT_EQ(assessments[2].solver->id, kFdtdSolverId);
    EXPECT_TRUE(assessments[2].solver->fullWave);
    const std::string text = solvers::describeAssessments(assessments);
    EXPECT_NE(text.find("Analytical free field [approximate model]: would LEAVE OUT"), std::string::npos) << text;
    EXPECT_NE(text.find("TWO-DIMENSIONAL problem"), std::string::npos);
    // In an empty scene the analytical solver is complete and the cheapest.
    auto empty = newProject("empty", "empty", settings);
    add(*empty, "e", acoustics::object_types::Emitter, "Emitter", emitter());
    add(*empty, "r", acoustics::object_types::Region, "Plane", plane(0.05, 0.05));
    const Prepared q = prepared(*empty);
    const auto simple = solvers::acousticSolvers().assess(traitsOf(*empty, Id("r"), settingsOf(*empty).medium, settings, q.plan));
    EXPECT_TRUE(simple[0].complete);
    EXPECT_NE(solvers::describeAssessments(simple).find("by far the cheapest"), std::string::npos);
}

TEST(FdtdSettingsTest, PersistAndValidate)
{
    FdtdSettings settings;
    settings.pointsPerWavelength = 16.0;
    settings.waveform.kind = WaveformKind::ToneBurst;
    settings.waveform.cycles = 6.0;
    settings.boundary[XMin] = BoundaryKind::Rigid;
    settings.boundary[YMax] = BoundaryKind::Impedance;
    settings.duration = 2e-3;
    auto project = Project::createNew("p");
    EXPECT_EQ(fdtdSettingsOf(*project), FdtdSettings{});
    storeFdtdSettings(*project, settings);
    auto reloaded = deserializeProject(serializeProject(*project));
    ASSERT_TRUE(reloaded.ok()) << reloaded.message();
    EXPECT_EQ(fdtdSettingsOf(*reloaded.value().project), settings);
    nlohmann::json bad = toJson(settings);
    bad["boundary"][0] = "sticky";
    EXPECT_FALSE(fdtdSettingsFromJson(bad).ok());
    bad = toJson(settings);
    bad["absorbingCells"] = 1;
    EXPECT_FALSE(fdtdSettingsFromJson(bad).ok());
}

// --- sources in a scene ----------------------------------------------------------------------

TEST(FdtdScene, EmitterAmplitudeAt1mIsHonouredIn2DAndTimeDelaysAreRealDelays)
{
    FdtdSettings settings;
    settings.pointsPerWavelength = 14.0;
    settings.snapshotCount = 0;
    auto project = newProject("amp", "amp", settings);
    // A low frequency so that 1 m... is far: check the calibrated law instead
    // at 60 mm, |P(r)| = A |H0(kr)| / |H0(k 1 m)|.
    add(*project, "e", acoustics::object_types::Emitter, "Emitter", emitter(40000.0, 2.5));
    add(*project, "r", acoustics::object_types::Region, "Plane", plane(0.16, 0.16));
    add(*project, "p", acoustics::object_types::Probe, "Probe", probe(), Vec3(0.06, 0, 0));
    const auto result = simulated(*project);
    ASSERT_NE(result, nullptr);
    ASSERT_NE(result->harmonicField, nullptr);
    const Medium air = settingsOf(*project).medium;
    const double k = 2.0 * pi * 40000.0 / air.soundSpeed;
    const auto h = [](double x) { return std::hypot(std::cyl_bessel_j(0.0, x), std::cyl_neumann(0.0, x)); };
    // The source sits at a cell centre half a cell from the origin.
    const double r = (trace(*result, "p").position - result->plan.frame.point(result->plan.nx / 2, result->plan.ny / 2, 0)).norm();
    const double expected = 2.5 * h(k * r) / h(k * 1.0);
    EXPECT_NEAR(harmonicAt(*result, trace(*result, "p").position), expected, 0.04 * expected);
    EXPECT_EQ(result->metadata.solverId, kFdtdSolverId);
    EXPECT_EQ(result->harmonicField->metadata.solverId, kFdtdSolverId);
    EXPECT_NE(result->harmonicField->assumptions[0].find("Harmonic amplitude at 40000 Hz"), std::string::npos);
    EXPECT_EQ(result->harmonicField->statistics.nearSourceSamples, 1u);

    // A pulse from a delayed emitter arrives later by exactly the delay.
    settings.waveform.kind = WaveformKind::GaussianPulse;
    auto pulsed = newProject("delay", "delay", settings);
    add(*pulsed, "e", acoustics::object_types::Emitter, "Emitter", emitter(40000.0, 1.0, 0.0));
    add(*pulsed, "r", acoustics::object_types::Region, "Plane", plane(0.12, 0.12));
    add(*pulsed, "p", acoustics::object_types::Probe, "Probe", probe(), Vec3(0.04, 0, 0));
    const auto prompt = simulated(*pulsed);
    auto delayed = newProject("delay2", "delay2", settings);
    add(*delayed, "e", acoustics::object_types::Emitter, "Emitter", emitter(40000.0, 1.0, 100e-6));
    add(*delayed, "r", acoustics::object_types::Region, "Plane", plane(0.12, 0.12));
    add(*delayed, "p", acoustics::object_types::Probe, "Probe", probe(), Vec3(0.04, 0, 0));
    const auto late = simulated(*delayed);
    ASSERT_NE(prompt, nullptr);
    ASSERT_NE(late, nullptr);
    EXPECT_EQ(prompt->harmonicField, nullptr);  // a pulse has no steady state
    EXPECT_NEAR(pulseTime(trace(*late, "p"), late->output.dt) - pulseTime(trace(*prompt, "p"), prompt->output.dt), 100e-6, 1e-6);
    EXPECT_NEAR(peakAbs(trace(*late, "p").pressure), peakAbs(trace(*prompt, "p").pressure), 1e-3 * peakAbs(trace(*prompt, "p").pressure));
    // Snapshots can be shown as instantaneous fields.
    EXPECT_EQ(prompt->snapshotField(0), nullptr);
}

// --- examples ---------------------------------------------------------------------------------

namespace {

FdtdSettings pulseSettings()
{
    FdtdSettings s;
    s.waveform.kind = WaveformKind::GaussianPulse;
    s.waveform.fractionalBandwidth = 0.6;
    return s;
}

// A: free-field propagation, 2D.
std::unique_ptr<Project> buildFreeField2D()
{
    auto project = newProject("a2", "free-field-pulse-2d", pulseSettings());
    add(*project, "ex-aa-emitter", acoustics::object_types::Emitter, "Emitter", emitter());
    add(*project, "ex-aa-region", acoustics::object_types::Region, "Simulation plane", plane(0.12, 0.12));
    add(*project, "ex-aa-near", acoustics::object_types::Probe, "Probe at 20 mm", probe(), Vec3(0.02, 0, 0));
    add(*project, "ex-aa-far", acoustics::object_types::Probe, "Probe at 50 mm", probe(), Vec3(0.05, 0, 0));
    return project;
}

// A: free-field propagation, 3D.
std::unique_ptr<Project> buildFreeField3D()
{
    FdtdSettings s = pulseSettings();
    s.pointsPerWavelength = 10.0;
    s.absorbingCells = 8;
    s.snapshotCount = 12;
    auto project = newProject("a3", "free-field-pulse-3d", s);
    add(*project, "ex-ab-emitter", acoustics::object_types::Emitter, "Emitter", emitter());
    add(*project, "ex-ab-region", acoustics::object_types::Region, "Simulation volume", volume(Vec3(0.034, 0.034, 0.034)));
    add(*project, "ex-ab-near", acoustics::object_types::Probe, "Probe at 6 mm", probe(), Vec3(0.006, 0, 0));
    add(*project, "ex-ab-far", acoustics::object_types::Probe, "Probe at 12 mm", probe(), Vec3(0.012, 0, 0));
    return project;
}

// B: reflection from a rigid plate, standing wave.
std::unique_ptr<Project> buildReflection()
{
    auto project = newProject("b", "reflection", FdtdSettings{});
    add(*project, "ex-ba-emitter", acoustics::object_types::Emitter, "Emitter", emitter(), Vec3(0, 0, 0.05));
    add(*project, "ex-ba-plate", kManipulatorType, "Rigid plate", manipulator(Category::Reflector), Vec3::Zero(),
        shape("reflector.flat", {{"width", 0.14}, {"height", 0.2}, {"thickness", 0.004}}));
    add(*project, "ex-ba-region", acoustics::object_types::Region, "Simulation plane", plane(0.12, 0.09), Vec3(0, 0, 0.035));
    return project;
}

// C: diffraction through a slit.
std::unique_ptr<Project> buildDiffraction()
{
    auto project = newProject("c", "diffraction", FdtdSettings{});
    add(*project, "ex-ca-emitter", acoustics::object_types::Emitter, "Emitter", emitter(), Vec3(0, 0, -0.04));
    add(*project, "ex-ca-plate", kManipulatorType, "Plate with a slit", manipulator(Category::DiffractionStructure), Vec3::Zero(),
        shape("diffraction.slit", {{"width", 0.3}, {"height", 0.2}, {"slitWidth", 0.005}, {"thickness", 0.004}}));
    add(*project, "ex-ca-region", acoustics::object_types::Region, "Simulation plane", plane(0.12, 0.12));
    add(*project, "ex-ca-axis", acoustics::object_types::Probe, "Probe behind the slit", probe(), Vec3(0, 0, 0.04));
    add(*project, "ex-ca-shadow", acoustics::object_types::Probe, "Probe in the geometric shadow", probe(), Vec3(0.04, 0, 0.02));
    return project;
}

// D: material interface.
std::unique_ptr<Project> buildInterface()
{
    FdtdSettings s = pulseSettings();
    s.waveform.frequencyHz = 100000.0;  // not used: the carrier is the emitter's
    auto project = newProject("d", "material-interface", s, true);
    add(*project, "ex-da-emitter", acoustics::object_types::Emitter, "Emitter", emitter(100000.0), Vec3(0, 0, -0.06));
    add(*project, "ex-da-block", "geometry", "Acrylic block", nullptr, Vec3(0, 0, 0.05),
        shape("box", {{"width", 0.4}, {"depth", 0.2}, {"height", 0.1}}), Id("mat-acrylic"));
    add(*project, "ex-da-region", acoustics::object_types::Region, "Simulation plane", plane(0.2, 0.2));
    add(*project, "ex-da-water", acoustics::object_types::Probe, "Probe in the water", probe(), Vec3(0, 0, -0.02));
    add(*project, "ex-da-in1", acoustics::object_types::Probe, "Probe 20 mm into the acrylic", probe(), Vec3(0, 0, 0.02));
    add(*project, "ex-da-in2", acoustics::object_types::Probe, "Probe 70 mm into the acrylic", probe(), Vec3(0, 0, 0.07));
    return project;
}

// E: resonant cavity.
constexpr double kCavityX = 0.040, kCavityZ = 0.030;
std::unique_ptr<Project> buildCavity()
{
    FdtdSettings s;
    s.waveform.kind = WaveformKind::Ricker;
    s.waveform.frequencyHz = 8000.0;
    s.pointsPerWavelength = 30.0;
    s.duration = 0.010;
    s.snapshotCount = 0;
    s.boundary.fill(BoundaryKind::Rigid);  // nothing outside the cavity matters
    auto project = newProject("e", "resonant-cavity", s);
    add(*project, "ex-ea-cavity", kManipulatorType, "Rigid cavity", manipulator(Category::ResonantStructure), Vec3::Zero(),
        shape("cavity.rectangular", {{"sizeX", kCavityX}, {"sizeY", 0.2}, {"sizeZ", kCavityZ}, {"wallThickness", 0.004}}));
    add(*project, "ex-ea-emitter", acoustics::object_types::Emitter, "Emitter (corner)", emitter(8000.0), Vec3(-0.015, 0, -0.011));
    add(*project, "ex-ea-probe", acoustics::object_types::Probe, "Probe (opposite corner)", probe(), Vec3(0.016, 0, 0.012));
    add(*project, "ex-ea-region", acoustics::object_types::Region, "Simulation plane", plane(0.052, 0.042));
    return project;
}

// F: passive focusing by a parabolic reflector.
constexpr double kFocalLength = 0.03, kSourceDistance = 0.15;
std::unique_ptr<Project> buildPassiveFocus(bool withReflector = true)
{
    auto project = newProject("f", "passive-focusing", FdtdSettings{});
    add(*project, "ex-fa-emitter", acoustics::object_types::Emitter, "Emitter", emitter(), Vec3(0, 0, kSourceDistance));
    SceneObject* reflector = add(*project, "ex-fa-reflector", kManipulatorType, "Parabolic reflector",
                                 manipulator(Category::Reflector, BoundaryCondition::Rigid, Vec3(0, 0, kFocalLength)), Vec3::Zero(),
                                 shape("reflector.parabolic", {{"focalLength", kFocalLength}, {"aperture", 0.1}, {"thickness", 0.004}}));
    if (!withReflector) {
        ObjectState state = reflector->state;
        state.visible = false;
        [[maybe_unused]] const Status status = project->scene.setState(reflector->id(), state);
    }
    add(*project, "ex-fa-region", acoustics::object_types::Region, "Simulation plane", plane(0.12, 0.18), Vec3(0, 0, 0.08));
    return project;
}

// G: hybrid focusing, phases from the full-wave solver itself.
const Vec3 kHybridTarget(0.006, 0.0, 0.036);
ArraySettings hybridArray()
{
    ArraySettings array;
    array.layout = ArrayLayout::Linear;
    array.countX = 8;
    array.spacingX = 0.008;
    array.element.frequencyHz = 40000.0;
    array.element.radius = 0.002;
    return array;
}

std::unique_ptr<Project> hybridScene(const ArraySettings& array)
{
    auto project = newProject("g", "hybrid-focusing", FdtdSettings{});
    add(*project, "ex-ga-reflector", kManipulatorType, "Parabolic reflector",
        manipulator(Category::Reflector, BoundaryCondition::Rigid, kHybridTarget), Vec3::Zero(),
        shape("reflector.parabolic", {{"focalLength", 0.03}, {"aperture", 0.1}, {"thickness", 0.004}}));
    SceneObject* object = add(*project, "ex-ga-array", acoustics::object_types::Array, "Linear array (hybrid focus)",
                              std::make_shared<ArrayComponent>(array), Vec3(0, 0, 0.11));
    ObjectState state = object->state;
    state.local.rotation = Vec3(0, units::pi, 0);  // facing the reflector
    [[maybe_unused]] const Status status = project->scene.setState(object->id(), state);
    add(*project, "ex-ga-target", acoustics::object_types::Target, "Target", std::make_shared<TargetComponent>(), kHybridTarget);
    add(*project, "ex-ga-region", acoustics::object_types::Region, "Simulation plane", plane(0.12, 0.14), Vec3(0, 0, 0.06));
    return project;
}

// The response of every array element at the target, by reciprocity: one
// full-wave run with a source at the target, read at the element positions.
std::vector<Complex> reciprocalResponses(const ArraySettings& array)
{
    auto reversed = hybridScene(array);
    const std::vector<PointSource> elements = collectSources(reversed->scene);
    ObjectState off = reversed->scene.find(Id("ex-ga-array"))->state;
    ArraySettings silent = array;
    silent.element.active = false;
    off.component = std::make_shared<ArrayComponent>(silent);
    [[maybe_unused]] const Status status = reversed->scene.setState(Id("ex-ga-array"), off);
    add(*reversed, "reciprocal-source", acoustics::object_types::Emitter, "Source at the target", emitter(), kHybridTarget);
    const auto result = simulated(*reversed);
    std::vector<Complex> responses;
    const GridFrame& f = result->plan.frame;
    for (const PointSource& element : elements) {
        const Vec3 d = element.position - f.origin;
        const int i = std::clamp(static_cast<int>(std::lround(d.dot(f.axisU) / f.axisU.squaredNorm() * (f.nu - 1))), 0, f.nu - 1);
        const int j = std::clamp(static_cast<int>(std::lround(d.dot(f.axisV) / f.axisV.squaredNorm() * (f.nv - 1))), 0, f.nv - 1);
        const std::complex<float> p = result->harmonicField->pressure[f.index(i, j, 0)];
        responses.emplace_back(p.real(), p.imag());
    }
    return responses;
}

std::unique_ptr<Project> buildHybrid()
{
    ArraySettings array = hybridArray();
    const std::vector<Complex> responses = reciprocalResponses(array);
    for (size_t i = 0; i < responses.size(); ++i) {
        // Conjugate phases: every element's field arrives at the target in phase.
        double phase = -std::arg(responses[i]);
        if (phase < 0.0)
            phase += 2.0 * pi;
        // Stored to 1e-6 rad so that the file does not depend on the last bits of a long calculation.
        array.overrides[static_cast<int>(i)].phaseRad = std::round(phase * 1e6) / 1e6;
    }
    return hybridScene(array);
}

}  // namespace

TEST(FdtdExamples, A_PulseSpreadsFromAnEmitterIn2DAnd3D)
{
    const auto project = checkedExample("a-free-field-pulse-2d.resonant", *buildFreeField2D());
    ASSERT_NE(project, nullptr);
    const auto result = simulated(*project);
    ASSERT_NE(result, nullptr);
    const Medium air = settingsOf(*project).medium;
    const ProbeTrace& near = trace(*result, "ex-aa-near");
    const ProbeTrace& far = trace(*result, "ex-aa-far");
    const double separation = (far.position - near.position).norm();
    EXPECT_NEAR(separation / (pulseTime(far, result->output.dt) - pulseTime(near, result->output.dt)), air.soundSpeed, 0.01 * air.soundSpeed);
    // Cylindrical spreading: amplitude falls roughly as 1 / sqrt(r).
    EXPECT_NEAR(peakAbs(far.pressure) / peakAbs(near.pressure), std::sqrt(0.02 / 0.05), 0.06);
    // The pulse leaves through the absorbing layers.
    EXPECT_LT(result->finalEnergyFraction(), 1e-4);
    EXPECT_EQ(result->output.snapshots.size(), 40u);
    const auto snapshot = result->snapshotField(10);
    ASSERT_NE(snapshot, nullptr);
    EXPECT_NE(snapshot->assumptions[0].find("INSTANTANEOUS pressure"), std::string::npos);
    EXPECT_GT(snapshot->statistics.maxMagnitude, 0.0);

    const auto project3 = checkedExample("a-free-field-pulse-3d.resonant", *buildFreeField3D());
    ASSERT_NE(project3, nullptr);
    const auto result3 = simulated(*project3);
    ASSERT_NE(result3, nullptr);
    EXPECT_EQ(result3->plan.dimensions, 3);
    const ProbeTrace& near3 = trace(*result3, "ex-ab-near");
    const ProbeTrace& far3 = trace(*result3, "ex-ab-far");
    // Spherical spreading: a point source's pulse falls as 1 / r. The
    // source cell is half a cell from the emitter's position.
    const Vec3 sourceCell = result3->plan.frame.point(result3->plan.nx / 2, result3->plan.ny / 2, result3->plan.nz / 2);
    const double r1 = (near3.position - sourceCell).norm(), r2 = (far3.position - sourceCell).norm();
    EXPECT_NEAR(peakAbs(far3.pressure) / peakAbs(near3.pressure), r1 / r2, 0.05);
    // And its amplitude is the emitter's 1 Pa at 1 m.
    EXPECT_NEAR(peakAbs(near3.pressure), 1.0 / r1, 0.08 / r1);
    EXPECT_NEAR((r2 - r1) / (pulseTime(far3, result3->output.dt) - pulseTime(near3, result3->output.dt)), air.soundSpeed, 0.03 * air.soundSpeed);
}

TEST(FdtdExamples, B_RigidPlateBuildsAStandingWave)
{
    const auto project = checkedExample("b-reflection.resonant", *buildReflection());
    ASSERT_NE(project, nullptr);
    const auto result = simulated(*project);
    ASSERT_NE(result, nullptr);
    ASSERT_NE(result->harmonicField, nullptr);
    EXPECT_GT(result->plan.rigidCells, 0u);
    const double wavelength = settingsOf(*project).medium.wavelength(40000.0);
    // Along the axis between plate and emitter: minima half a wavelength apart.
    std::vector<double> minima;
    const double step = result->plan.dx;
    double previous = harmonicAt(*result, Vec3(0, 0, 0.5 * step)), current = harmonicAt(*result, Vec3(0, 0, 1.5 * step));
    for (double z = 2.5 * step; z < 0.04; z += step) {
        const double next = harmonicAt(*result, Vec3(0, 0, z));
        if (current < previous && current <= next)
            minima.push_back(z - step);
        previous = current;
        current = next;
    }
    ASSERT_GE(minima.size(), 4u);
    const double spacing = (minima.back() - minima.front()) / static_cast<double>(minima.size() - 1);
    EXPECT_NEAR(spacing, 0.5 * wavelength, 0.04 * wavelength);
    // A pressure maximum at the rigid surface, and deep minima.
    const double atWall = harmonicAt(*result, Vec3(0, 0, 0.5 * step));
    const double firstMinimum = harmonicAt(*result, Vec3(0, 0, minima.front()));
    EXPECT_NEAR(minima.front(), 0.25 * wavelength, step + 0.03 * wavelength);  // the wall is where the grid puts it: within a cell
    EXPECT_GT(atWall, 3.0 * firstMinimum);
    // Behind the plate: shadow.
    EXPECT_LT(harmonicAt(*result, Vec3(0, 0, -0.008)), 0.15 * atWall);
}

TEST(FdtdExamples, C_SoundDiffractsThroughASlitIntoTheGeometricShadow)
{
    const auto project = checkedExample("c-diffraction.resonant", *buildDiffraction());
    ASSERT_NE(project, nullptr);
    const auto result = simulated(*project);
    ASSERT_NE(result, nullptr);
    const double onAxis = harmonicAt(*result, Vec3(0, 0, 0.04));
    const double shadow = harmonicAt(*result, Vec3(0.04, 0, 0.02));  // 63 degrees off the axis: no straight path from the emitter
    EXPECT_GT(onAxis, 0.0);
    // A slit narrower than the wavelength radiates almost evenly: the shadow is not dark.
    EXPECT_GT(shadow, 0.3 * onAxis);
    EXPECT_LT(shadow, 1.2 * onAxis);
    // Without the slit nothing gets through.
    auto closed = buildDiffraction();
    ObjectState state = closed->scene.find(Id("ex-ca-plate"))->state;
    state.geometry = shape("reflector.flat", {{"width", 0.3}, {"height", 0.2}, {"thickness", 0.004}});
    ASSERT_TRUE(closed->scene.setState(Id("ex-ca-plate"), state).ok());
    const auto blocked = simulated(*closed);
    EXPECT_LT(harmonicAt(*blocked, Vec3(0, 0, 0.04)), 1e-6 * onAxis);
    // The analytical solvers would ignore the plate, and the assessment says so.
    const Prepared p = prepared(*project);
    const auto assessments = solvers::acousticSolvers().assess(
        traitsOf(*project, regionOf(*project), settingsOf(*project).medium, fdtdSettingsOf(*project), p.plan));
    EXPECT_FALSE(assessments[0].complete);
    EXPECT_FALSE(assessments[1].complete);
    EXPECT_TRUE(assessments[2].complete);
}

TEST(FdtdExamples, D_PulseEntersAcrylicAndTravelsAtItsSpeed)
{
    const auto project = checkedExample("d-material-interface.resonant", *buildInterface());
    ASSERT_NE(project, nullptr);
    const auto result = simulated(*project);
    ASSERT_NE(result, nullptr);
    const Material* acrylic = project->materials.find(Id("mat-acrylic"));
    const double c2 = acrylic->property(PhysicsDomain::Acoustic, "soundSpeed")->value;
    const double rho2 = acrylic->property(PhysicsDomain::Acoustic, "density")->value;
    const Medium water = settingsOf(*project).medium;
    EXPECT_GT(result->plan.materialCells, 1000u);
    const ProbeTrace& in1 = trace(*result, "ex-da-in1");
    const ProbeTrace& in2 = trace(*result, "ex-da-in2");
    const double speed = (in2.position - in1.position).norm() / (pulseTime(in2, result->output.dt) - pulseTime(in1, result->output.dt));
    EXPECT_NEAR(speed, c2, 0.03 * c2);
    EXPECT_GT(speed, 1.5 * water.soundSpeed);
    // The water probe sees the incident pulse and then its reflection from
    // the block: with the normal-incidence coefficient, reduced by the
    // cylindrical spreading over the longer path.
    const ProbeTrace& w = trace(*result, "ex-da-water");
    const double dt = result->output.dt;
    const double zSource = -0.06, zProbe = w.position.z(), zFace = 0.0;
    const double tIncident = (zProbe - zSource) / water.soundSpeed, tReflected = ((zFace - zSource) + (zFace - zProbe)) / water.soundSpeed;
    const double lead = 0.5 * result->settings.waveform.length() * 100000.0 / 100000.0;  // the pulse peaks half its length after its start
    const auto windowPeak = [&](double centre) {
        double best = 0.0;
        for (size_t n = 0; n < w.pressure.size(); ++n) {
            const double t = static_cast<double>(n + 1) * dt - lead;
            if (std::abs(t - centre) < 6e-6)
                best = std::max(best, std::abs(w.pressure[n]));
        }
        return best;
    };
    Waveform carrier = result->settings.waveform;
    carrier.frequencyHz = 100000.0;
    (void)carrier;
    const double z1 = water.impedance(), z2 = rho2 * c2;
    const double reflection = (z2 - z1) / (z2 + z1);
    const double spreading = std::sqrt((zProbe - zSource) / ((zFace - zSource) + (zFace - zProbe)));
    EXPECT_NEAR(windowPeak(tReflected) / windowPeak(tIncident), reflection * spreading, 0.06);
    EXPECT_GT(reflection, 0.3);
}

TEST(FdtdExamples, E_CavityRingsAtItsAnalyticalResonances)
{
    const auto project = checkedExample("e-resonant-cavity.resonant", *buildCavity());
    ASSERT_NE(project, nullptr);
    const auto result = simulated(*project);
    ASSERT_NE(result, nullptr);
    const double c = settingsOf(*project).medium.soundSpeed;
    const Spectrum spectrum = result->probeSpectrum(0);
    EXPECT_NEAR(spectrum.resolutionHz, 200.0, 2.0);  // 10 ms, Hann window
    const std::vector<SpectralPeak> peaks = findPeaks(spectrum, 0.03, 2000.0, 12000.0);
    // The inner cross-section as the grid holds it: count the air cells
    // through the middle of the cavity.
    const double dx = result->plan.dx;
    const auto airCells = [&](bool alongU) {
        const auto air = [&](int q) {
            const int i = alongU ? q : result->plan.nx / 2, j = alongU ? result->plan.ny / 2 : q;
            return result->materialMap[static_cast<size_t>(i) + static_cast<size_t>(result->plan.nx) * static_cast<size_t>(j)] == 0;
        };
        const int n = alongU ? result->plan.nx : result->plan.ny;
        int count = 1;  // the centre cell, then outwards to the walls
        for (int q = n / 2 + 1; q < n && air(q); ++q)
            ++count;
        for (int q = n / 2 - 1; q >= 0 && air(q); --q)
            ++count;
        return count;
    };
    const double lx = airCells(true) * dx, lz = airCells(false) * dx;
    EXPECT_NEAR(lx, kCavityX, dx);
    EXPECT_NEAR(lz, kCavityZ, dx);
    for (const auto& [m, n] : {std::pair{1, 0}, {0, 1}, {1, 1}, {2, 0}}) {
        const double designed = 0.5 * c * std::hypot(m / kCavityX, n / kCavityZ);
        const double asGridded = 0.5 * c * std::hypot(m / lx, n / lz);
        double nearest = 1e300;
        for (const SpectralPeak& p : peaks)
            if (std::abs(p.frequencyHz - asGridded) < std::abs(nearest - asGridded))
                nearest = p.frequencyHz;
        EXPECT_NEAR(nearest, asGridded, 0.5 * spectrum.resolutionHz) << m << "," << n;
        // The designed cavity differs from the gridded one by less than a cell per wall.
        EXPECT_NEAR(nearest, designed, 0.04 * designed) << m << "," << n;
    }
    // A closed rigid cavity keeps its energy.
    EXPECT_GT(result->finalEnergyFraction(), 0.95);
    EXPECT_EQ(result->harmonicField, nullptr);
}

TEST(FdtdExamples, F_ParabolicReflectorFocusesInAFullWaveCalculation)
{
    const auto project = checkedExample("f-passive-focusing.resonant", *buildPassiveFocus());
    ASSERT_NE(project, nullptr);
    const auto result = simulated(*project);
    ASSERT_NE(result, nullptr);
    const double wavelength = settingsOf(*project).medium.wavelength(40000.0);
    // The same run without the reflector, on the same grid: the difference
    // of the two complex fields is what the reflector adds.
    const auto bare = simulated(*buildPassiveFocus(false));
    ASSERT_NE(bare, nullptr);
    EXPECT_EQ(bare->plan.rigidCells, 0u);
    ASSERT_EQ(bare->harmonicField->pressure.size(), result->harmonicField->pressure.size());
    const GridFrame& f = result->plan.frame;
    const auto cellOf = [&](const Vec3& world) {
        const Vec3 d = world - f.origin;
        const int i = std::clamp(static_cast<int>(std::lround(d.dot(f.axisU) / f.axisU.squaredNorm() * (f.nu - 1))), 0, f.nu - 1);
        const int j = std::clamp(static_cast<int>(std::lround(d.dot(f.axisV) / f.axisV.squaredNorm() * (f.nv - 1))), 0, f.nv - 1);
        return f.index(i, j, 0);
    };
    const auto reflected = [&](const Vec3& world) {
        return static_cast<double>(std::abs(result->harmonicField->pressure[cellOf(world)] - bare->harmonicField->pressure[cellOf(world)]));
    };
    double bestZ = 0.0, best = 0.0;
    for (double z = 0.012; z < 0.08; z += 0.5 * result->plan.dx) {
        const double value = reflected(Vec3(0, 0, z));
        if (value > best) {
            best = value;
            bestZ = z;
        }
    }
    // The mirror equation puts the image of a source at 150 mm at 37.5 mm.
    EXPECT_NEAR(bestZ, mirrorImageDistance(kFocalLength, kSourceDistance), 0.5 * wavelength);
    // The reflected field at the focus is several times the direct field there.
    const double gain = best / harmonicAt(*bare, Vec3(0, 0, bestZ));
    EXPECT_GT(gain, 3.0);
    // The focal spot is about a wavelength wide, not a point.
    double halfWidth = 0.0;
    for (double x = 0.0; x < 0.03; x += 0.5 * result->plan.dx)
        if (reflected(Vec3(x, 0, bestZ)) < 0.5 * best) {
            halfWidth = x;
            break;
        }
    EXPECT_GT(2.0 * halfWidth, 0.3 * wavelength);
    EXPECT_LT(2.0 * halfWidth, 2.0 * wavelength);
}

TEST(FdtdExamples, G_HybridFocusAlignsEveryElementThroughTheReflector)
{
    const auto project = checkedExample("g-hybrid-focusing.resonant", *buildHybrid());
    ASSERT_NE(project, nullptr);
    const auto focused = simulated(*project);
    ASSERT_NE(focused, nullptr);
    EXPECT_EQ(focused->plan.sourceCount, 8u);
    const double atTarget = harmonicAt(*focused, kHybridTarget);
    // The same array without phase control.
    const auto plain = simulated(*hybridScene(hybridArray()));
    ASSERT_NE(plain, nullptr);
    EXPECT_GT(atTarget, 1.5 * harmonicAt(*plain, kHybridTarget));
    // With conjugate phases the field at the target is the sum of the
    // magnitudes of the eight single-element responses: the most these
    // elements can deliver there. (Reciprocity gives the responses up to
    // one common factor, fixed here by the forward run of one element.)
    const std::vector<Complex> responses = reciprocalResponses(hybridArray());
    ArraySettings single = hybridArray();
    for (int i = 1; i < 8; ++i)
        single.overrides[i].active = false;
    const double oneElement = harmonicAt(*simulated(*hybridScene(single)), kHybridTarget);
    double sum = 0.0;
    for (const Complex& h : responses)
        sum += std::abs(h);
    const double bound = sum * oneElement / std::abs(responses[0]);
    EXPECT_NEAR(atTarget, bound, 0.03 * bound);
}
