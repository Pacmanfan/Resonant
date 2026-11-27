// The electromagnetic domain driven from projects: antennas, patterns,
// arrays, fields, guides, Touchstone data, the full-wave solver from a
// scene, and the electromagnetics example projects.
//
// To regenerate the example files after an intended change:
//   RESONANT_WRITE_EXAMPLES=1 resonant_core_tests --gtest_filter='EmExamples.*'
#include "acoustics/Components.h"
#include "acoustics/FieldGrid.h"
#include "electromagnetics/EmFullWave.h"
#include "electromagnetics/Touchstone.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace resonant;
using namespace resonant::em;

namespace {

constexpr double kF = 1e9;
const double kLambda = wavelength(kF);

std::unique_ptr<Project> newProject(const std::string& slug, const std::string& name)
{
    auto project = Project::createNew(name);
    project->id = Id("ex-em-" + slug);
    project->createdUtc = project->modifiedUtc = "2026-11-26T00:00:00Z";
    return project;
}

SceneObject* add(Project& project, const std::string& id, const char* type, const std::string& name, ComponentPtr component,
                 const Vec3& position = Vec3::Zero(), std::optional<Geometry> geometry = {}, const Id& material = {}, const Vec3& rotation = Vec3::Zero())
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

ComponentPtr antenna(double frequency = kF, double current = 1.0)
{
    AntennaSettings s;
    s.frequencyHz = frequency;
    s.feedCurrent = current;
    return std::make_shared<AntennaComponent>(s);
}

std::unique_ptr<Project> checkedExample(const std::string& file, const Project& built)
{
    const std::filesystem::path path = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "electromagnetics" / file;
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

SampleGrid gridOfRegion(const Project& project, const char* id)
{
    const SceneObject* o = project.scene.find(Id(id));
    const auto& region = dynamic_cast<const acoustics::RegionComponent&>(*o->state.component).settings;
    const acoustics::GridFrame f = acoustics::gridOf(region, project.scene.worldMatrix(*o));
    SampleGrid g;
    g.origin = f.origin;
    g.axisU = f.axisU;
    g.axisV = f.axisV;
    g.axisW = f.axisW;
    g.nu = f.nu;
    g.nv = f.nv;
    g.nw = f.nw;
    return g;
}

ComponentPtr fieldPlane(double size, int samples)
{
    acoustics::RegionSettings r;
    r.kind = acoustics::RegionKind::PlaneXZ;
    r.size = Vec3(size, 0.1, size);
    r.countU = r.countV = samples;
    return std::make_shared<acoustics::RegionComponent>(r);
}

Complex hankel2(int n, double x)
{
    return {std::cyl_bessel_j(static_cast<double>(n), x), -std::cyl_neumann(static_cast<double>(n), x)};
}

// --- examples -----------------------------------------------------------------------------

std::unique_ptr<Project> buildDipole()
{
    auto project = newProject("a", "half-wave-dipole");
    add(*project, "ex-ea-dipole", em::object_types::Antenna, "Half-wave dipole (copper)", antenna(), Vec3::Zero(),
        shape("antenna.dipole", {{"length", 0.5 * kLambda}, {"wireRadius", 0.001}}), Id("mat-copper"));
    return project;
}

std::unique_ptr<Project> buildMonopole()
{
    auto project = newProject("b", "quarter-wave-monopole");
    add(*project, "ex-eb-monopole", em::object_types::Antenna, "Quarter-wave monopole", antenna(), Vec3::Zero(),
        shape("antenna.monopole", {{"height", 0.25 * kLambda}, {"wireRadius", 0.001}, {"groundRadius", 0.3}}), Id("mat-aluminum"));
    return project;
}

std::unique_ptr<Project> buildLinearArray()
{
    auto project = newProject("c", "linear-array");
    EmArraySettings s;
    s.layout = EmArrayLayout::Linear;
    s.countX = 16;
    s.spacingX = 0.5 * kLambda;
    s.pattern = ElementPattern::HalfWaveDipole;
    s.frequencyHz = kF;
    const std::vector<double> w = chebyshevWeights(16, 30.0);
    for (int i = 0; i < 16; ++i)
        s.overrides[i].amplitude = std::round(w[static_cast<size_t>(i)] * 1e9) / 1e9;
    add(*project, "ex-ec-array", em::object_types::Array, "16 half-wave dipoles, Chebyshev 30 dB", std::make_shared<EmArrayComponent>(s));
    EmSettings settings;
    settings.chebyshevSidelobeDb = 30.0;
    storeEmSettings(*project, settings);
    return project;
}

std::unique_ptr<Project> buildSteering()
{
    auto project = newProject("d", "phased-array-steering");
    EmArraySettings s;
    s.layout = EmArrayLayout::Planar;
    s.countX = s.countY = 8;
    s.spacingX = s.spacingY = 0.5 * kLambda;
    s.pattern = ElementPattern::CosineHemisphere;
    s.frequencyHz = kF;
    ArrayModel model = arrayModelOf(s);
    steer(model, directionOf(30.0 * kPi / 180.0, 45.0 * kPi / 180.0), kF, SteeringMethod::Phase);
    EmArraySettings steered = withExcitation(s, model);
    for (auto& [index, o] : steered.overrides)
        o.phase = std::round(*o.phase * 1e9) / 1e9;
    add(*project, "ex-ed-array", em::object_types::Array, "8 x 8 planar array steered to (30, 45) deg", std::make_shared<EmArrayComponent>(steered));
    EmSettings settings;
    settings.steerThetaDeg = 30.0;
    settings.steerPhiDeg = 45.0;
    storeEmSettings(*project, settings);
    return project;
}

std::unique_ptr<Project> buildNearField()
{
    auto project = buildDipole();
    project->id = Id("ex-em-e");
    project->name = "near-field";
    add(*project, "ex-ee-region", acoustics::object_types::Region, "Field plane (XZ)", fieldPlane(2.0 * kLambda, 81), Vec3(0, 0, 0));
    return project;
}

std::unique_ptr<Project> buildWaveguide()
{
    auto project = newProject("f", "rectangular-waveguide");
    GuideSettings s;
    s.frequencyHz = 10e9;
    add(*project, "ex-ef-guide", em::object_types::Guide, "WR-90 waveguide", std::make_shared<GuideComponent>(s), Vec3::Zero(),
        shape("guide.rectangular", {{"a", 22.86e-3}, {"b", 10.16e-3}, {"length", 0.1}}));
    return project;
}

constexpr double kCylinderRadius = 0.5, kSourceDistance = 1.5;  // wavelengths
std::unique_ptr<Project> buildFullWave()
{
    auto project = newProject("g", "full-wave-scattering");
    add(*project, "ex-eg-cylinder", resonant::object_types::Geometry, "Copper cylinder", nullptr, Vec3::Zero(),
        shape("cylinder", {{"radius", kCylinderRadius * kLambda}, {"height", 0.5}}), Id("mat-copper"));
    add(*project, "ex-eg-source", em::object_types::Antenna, "Line source (dipole along the normal)", antenna(), Vec3(kSourceDistance * kLambda, 0, 0),
        shape("antenna.dipole", {{"length", 0.1}, {"wireRadius", 0.001}}));
    acoustics::RegionSettings r;
    r.kind = acoustics::RegionKind::PlaneXY;
    r.size = Vec3(5 * kLambda, 5 * kLambda, 0.1);
    add(*project, "ex-eg-region", acoustics::object_types::Region, "Simulation plane (XY)", std::make_shared<acoustics::RegionComponent>(r));
    EmFullWaveSettings fw;
    fw.cellsPerWavelength = 30.0;
    storeEmFullWaveSettings(*project, fw);
    return project;
}

DomainBox domainOfRegion(const Project& project, const char* id)
{
    const SampleGrid g = gridOfRegion(project, id);
    DomainBox d;
    d.corner = g.origin;
    d.axis = {g.axisU.normalized(), g.axisV.normalized(), g.nw > 1 ? Vec3(g.axisW.normalized()) : Vec3(g.axisU.normalized().cross(g.axisV.normalized()))};
    d.size = Vec3(g.axisU.norm(), g.axisV.norm(), g.axisW.norm());
    d.volume = g.nw > 1;
    return d;
}

}  // namespace

TEST(EmExamples, A_HalfWaveDipoleImpedancePatternGainAndEstimates)
{
    const auto project = checkedExample("a-half-wave-dipole.resonant", *buildDipole());
    ASSERT_NE(project, nullptr);
    const AntennaReport r = analyzeAntenna(*project, Id("ex-ea-dipole"));
    ASSERT_TRUE(r.analytical);
    EXPECT_NEAR(r.impedance->input.real(), 73.08, 0.05);
    EXPECT_NEAR(r.impedance->input.imag(), 42.51, 0.05);
    EXPECT_NEAR(r.directivity, 1.641, 0.002);
    // Copper loss at 1 GHz is tiny but not zero; realized gain includes the 50 ohm mismatch.
    EXPECT_GT(r.radiationEfficiency, 0.99);
    EXPECT_LT(r.radiationEfficiency, 1.0);
    EXPECT_NEAR(r.gain, r.directivity * r.radiationEfficiency, 1e-12);
    EXPECT_NEAR(r.realizedGain, r.gain * r.matching->mismatchEfficiency, 1e-12);
    EXPECT_LT(r.realizedGain, r.gain);
    EXPECT_NEAR(r.farFieldDistance, 3 * kLambda, 1e-9);  // for a half-wave dipole 3 lambda exceeds 2 D^2 / lambda
    EXPECT_NE(r.polarization.find("linear"), std::string::npos);
    ASSERT_GE(r.estimates.size(), 1u);
    EXPECT_NE(r.estimates[0].find("resonant length"), std::string::npos);
    const auto pattern = patternOf(*project, Id("ex-ea-dipole"), 1.0);
    ASSERT_TRUE(pattern.ok());
    EXPECT_NEAR(pattern.value().directivity, 1.641, 0.005);
    EXPECT_NEAR(pattern.value().value(90, 0), 1.0, 1e-9);                                    // broadside maximum
    EXPECT_NEAR(pattern.value().value(60, 0), std::abs(thinDipolePattern(kPi / 3, 0.5 * kLambda, kF)), 1e-9);
    // An antenna without a field model gives estimates only.
    auto patch = newProject("x", "x");
    add(*patch, "p", em::object_types::Antenna, "Patch", antenna(2.45e9), Vec3::Zero(),
        shape("antenna.patch", {{"patchLength", 0.0285}, {"patchWidth", 0.0373}, {"substrateThickness", 0.0016}, {"substrateSize", 0.06}}), Id("mat-polycarbonate"));
    const AntennaReport p = analyzeAntenna(*patch, Id("p"));
    EXPECT_FALSE(p.analytical);
    EXPECT_FALSE(p.impedance.has_value());
    EXPECT_NE(p.estimates[0].find("Resonant frequency"), std::string::npos);
    EXPECT_FALSE(patternOf(*patch, Id("p"), 1.0).ok());
}

TEST(EmExamples, B_MonopoleIsHalfTheImpedanceTwiceTheDirectivityAndSilentBelowGround)
{
    const auto project = checkedExample("b-quarter-wave-monopole.resonant", *buildMonopole());
    ASSERT_NE(project, nullptr);
    const AntennaReport r = analyzeAntenna(*project, Id("ex-eb-monopole"));
    EXPECT_NEAR(r.impedance->input.real(), 0.5 * 73.08, 0.03);
    EXPECT_NEAR(r.directivity, 2 * 1.641, 0.004);
    const auto pattern = patternOf(*project, Id("ex-eb-monopole"), 1.0);
    ASSERT_TRUE(pattern.ok());
    EXPECT_NEAR(pattern.value().directivity, 2 * 1.641, 0.01);
    EXPECT_DOUBLE_EQ(pattern.value().value(120, 0), 0.0);
    EXPECT_NE(r.model.find("INFINITE"), std::string::npos);
    // The impedance sweep crosses resonance just below the quarter wave.
    const auto sweep = impedanceSweep(*project, Id("ex-eb-monopole"), 0.8e9, 1.2e9, 81);
    bool crosses = false;
    for (size_t i = 1; i < sweep.size(); ++i)
        crosses = crosses || (sweep[i - 1].second.input.imag() < 0 && sweep[i].second.input.imag() >= 0);
    EXPECT_TRUE(crosses);
}

TEST(EmExamples, C_ChebyshevTaperedArrayHasThirtyDecibelSidelobes)
{
    const auto project = checkedExample("c-linear-array.resonant", *buildLinearArray());
    ASSERT_NE(project, nullptr);
    const auto* array = dynamic_cast<const EmArrayComponent*>(project->scene.find(Id("ex-ec-array"))->state.component.get());
    const ArrayModel model = arrayModelOf(array->settings);
    const PatternAnalysis a = analyzePattern(model, kF, 0.5);
    EXPECT_NEAR(a.sidelobeDb, -30.0, 0.3);
    EXPECT_NEAR(std::abs(a.mainLobe.y()), 1.0, 1e-3);  // broadside, perpendicular to the array and to the dipoles
    ASSERT_EQ(a.equalMaxima.size(), 1u);                 // the mirror lobe on the other side of the array
    EXPECT_NEAR(a.equalMaxima[0].y(), -a.mainLobe.y(), 1e-3);
    // Uniform weights for comparison: -13 dB.
    EmArraySettings uniform = array->settings;
    uniform.overrides.clear();
    EXPECT_NEAR(analyzePattern(arrayModelOf(uniform), kF, 0.5).sidelobeDb, -13.15, 0.3);
    EXPECT_GT(analyzePattern(arrayModelOf(uniform), kF, 0.5).directivity, a.directivity);  // tapering costs directivity
}

TEST(EmExamples, D_SteeredPlanarArrayPointsWhereAsked)
{
    const auto project = checkedExample("d-phased-array-steering.resonant", *buildSteering());
    ASSERT_NE(project, nullptr);
    const auto* array = dynamic_cast<const EmArrayComponent*>(project->scene.find(Id("ex-ed-array"))->state.component.get());
    const PatternAnalysis a = analyzePattern(arrayModelOf(array->settings), kF, 0.25);
    EXPECT_NEAR(a.mainTheta * 180 / kPi, 30.0, 1.0);
    EXPECT_NEAR(a.mainPhi * 180 / kPi, 45.0, 1.0);
    EXPECT_TRUE(predictedGratingLobes({Vec3(0.5 * kLambda, 0, 0), Vec3(0, 0.5 * kLambda, 0)}, a.mainLobe, kF).empty());
    // At 0.8 lambda spacing the same steering brings a grating lobe into view.
    EXPECT_FALSE(predictedGratingLobes({Vec3(0.8 * kLambda, 0, 0), Vec3(0, 0.8 * kLambda, 0)}, a.mainLobe, kF).empty());
    const auto pattern = patternOf(*project, Id("ex-ed-array"), 1.0);
    ASSERT_TRUE(pattern.ok());
    EXPECT_NE(pattern.value().model.find("no mutual coupling"), std::string::npos);
}

TEST(EmExamples, E_NearFieldOfTheDipoleCarriesExactlyItsRadiatedPower)
{
    const auto project = checkedExample("e-near-field.resonant", *buildNearField());
    ASSERT_NE(project, nullptr);
    const auto field = evaluateFields(*project, gridOfRegion(*project, "ex-ee-region"), kF, "near field");
    ASSERT_TRUE(field.ok()) << field.message();
    const EmFieldResult& f = *field.value();
    // Samples on the wire are excluded; the rest is finite.
    size_t excluded = 0;
    for (const auto e : f.excluded)
        excluded += e;
    EXPECT_GT(excluded, 0u);
    EXPECT_EQ(f.metadata.solverId, "em.analytical.fields");
    EXPECT_GT(f.largest(FieldComponent::EMagnitude), 0.0);
    // Far out the wave impedance is eta0 (corner of the plane, 1.4 lambda away).
    const size_t corner = f.grid.index(0, 0, 0);
    EXPECT_NEAR(std::abs(f.component(FieldComponent::EMagnitude, corner)) / std::abs(f.component(FieldComponent::HMagnitude, corner)), kEta0, 0.06 * kEta0);
    // The Poynting flux through a sphere around the dipole equals 1/2 |I|^2 R_in,
    // with R_in from the induced-EMF formula: the two calculations agree.
    double power = 0.0;
    const int n = 720;
    const double r = 0.8 * kLambda;
    for (int i = 0; i < n; ++i) {
        const double t = (i + 0.5) * kPi / n;
        const Vec3 p = r * Vec3(std::sin(t), 0, std::cos(t));
        const FieldPoint fp = thinDipoleField(p, 0.5 * kLambda, Complex(1, 0), kF);
        power += (0.5 * fp.e.cross(fp.h.conjugate())).real().dot(p / r) * 2 * kPi * r * r * std::sin(t) * kPi / n;
    }
    const AntennaReport report = analyzeAntenna(*project, Id("ex-ea-dipole"));
    ASSERT_TRUE(report.impedance.has_value());
    EXPECT_NEAR(power, 0.5 * report.impedance->input.real(), 1e-4 * power);
}

TEST(EmExamples, F_WR90ModesAndTheTE10CrossSection)
{
    const auto project = checkedExample("f-rectangular-waveguide.resonant", *buildWaveguide());
    ASSERT_NE(project, nullptr);
    const GuideReport g = analyzeGuide(*project, Id("ex-ef-guide"));
    ASSERT_TRUE(g.valid);
    EXPECT_EQ(g.modes[0].label(), "TE10");
    EXPECT_NEAR(g.modes[0].cutoffHz / 1e9, 6.557, 0.001);
    ASSERT_TRUE(g.propagation[0].propagating);
    EXPECT_FALSE(g.propagation[1].propagating);  // TE20 at 13.1 GHz: only TE10 propagates at 10 GHz
    EXPECT_NEAR(g.propagation[0].guideWavelength * 1e3, 39.7, 0.1);
    const auto field = te10CrossSection(*project, Id("ex-ef-guide"));
    ASSERT_TRUE(field.ok()) << field.message();
    const EmFieldResult& f = *field.value();
    const int mid = f.grid.nu / 2;
    EXPECT_NEAR(std::abs(f.e[1][f.grid.index(mid, 2, 0)]), 1.0, 1e-6);
    EXPECT_NEAR(std::abs(f.e[1][f.grid.index(0, 2, 0)]), 0.0, 1e-6);
    // Below cut-off it is refused.
    auto low = buildWaveguide();
    ObjectState state = low->scene.find(Id("ex-ef-guide"))->state;
    GuideSettings s;
    s.frequencyHz = 5e9;
    state.component = std::make_shared<GuideComponent>(s);
    ASSERT_TRUE(low->scene.setState(Id("ex-ef-guide"), state).ok());
    EXPECT_FALSE(te10CrossSection(*low, Id("ex-ef-guide")).ok());
}

TEST(EmExamples, G_FullWaveScatteringByACopperCylinderMatchesTheSeriesSolutionAndConverges)
{
    const auto project = checkedExample("g-full-wave-scattering.resonant", *buildFullWave());
    ASSERT_NE(project, nullptr);
    // The relative error of the full-wave E_z against the exact series at
    // points one wavelength from the axis, at a given resolution.
    const auto errorAt = [&](double cellsPerWavelength) -> double {
        EmFullWaveSettings settings = emFullWaveSettingsOf(*project);
        settings.cellsPerWavelength = cellsPerWavelength;
        EmPrepared prepared = prepareFullWave(*project, domainOfRegion(*project, "ex-eg-region"), settings);
        EXPECT_TRUE(prepared.plan.allowed) << prepared.plan.refusal;
        if (!prepared.plan.allowed)
            return 1.0;
        EXPECT_EQ(prepared.plan.dimensions, 2);
        EXPECT_GT(prepared.plan.pecCells, 0u);
        std::string objects;
        for (const std::string& o : prepared.plan.objects)
            objects += o + "\n";
        EXPECT_NE(objects.find("good conductor"), std::string::npos) << objects;
        EXPECT_NE(objects.find("infinite line current"), std::string::npos);
        const SampleGrid grid = prepared.plan.grid;
        const double cell = prepared.plan.cell;
        const auto result = simulateFullWave("scattering", std::move(prepared), settings);
        EXPECT_TRUE(result.ok()) << result.message();
        if (!result.ok() || !result.value()->harmonicField)
            return 1.0;
        EXPECT_EQ(result.value()->metadata.solverId, kEmFullWaveSolverId);
        const double k = wavenumber(kF), omega = 2 * kPi * kF, a = kCylinderRadius * kLambda;
        const double d = grid.origin.x() + cell * std::lround((kSourceDistance * kLambda - grid.origin.x()) / cell);  // the source node
        double error = 0.0, reference = 0.0;
        for (int angle = 30; angle <= 330; angle += 30) {
            const double t = angle * kPi / 180.0;
            const int u = static_cast<int>(std::lround((kLambda * std::cos(t) - grid.origin.x()) / cell));
            const int v = static_cast<int>(std::lround((kLambda * std::sin(t) - grid.origin.y()) / cell));
            const Vec3 p = grid.origin + cell * Vec3(u, v, 0);
            const double rho = std::hypot(p.x(), p.y()), phi = std::atan2(p.y(), p.x());
            Complex exact = -(omega * kMu0 / 4.0) * hankel2(0, k * (p - Vec3(d, 0, 0)).norm());
            for (int n = 0; n <= 40; ++n)
                exact += (omega * kMu0 / 4.0) * (n == 0 ? 1.0 : 2.0 * std::cos(n * phi)) * std::cyl_bessel_j(n, k * a) / hankel2(n, k * a) *
                         hankel2(n, k * d) * hankel2(n, k * rho);
            const Complex got = result.value()->harmonicField->e[2][grid.index(u, v, 0)];
            error += std::norm(got - exact);
            reference += std::norm(exact);
        }
        return std::sqrt(error / reference);
    };
    // Mesh convergence: the project's 30 cells per wavelength against 15.
    const double coarse = errorAt(15.0), fine = errorAt(emFullWaveSettingsOf(*project).cellsPerWavelength);
    EXPECT_LT(fine, 0.06);
    EXPECT_LT(fine, 0.75 * coarse) << "coarse " << coarse << ", fine " << fine;
    std::printf("[ example G ] relative error vs series: %.4f at 15 cells/lambda, %.4f at 30\n", coarse, fine);
}

// --- further scene checks ------------------------------------------------------------------

TEST(EmFullWaveScene, HalfWaveDipoleIn3DHasTheDipolePatternAndRefusesWhatItCannotDo)
{
    auto project = newProject("y", "y");
    add(*project, "d", em::object_types::Antenna, "Dipole", antenna(), Vec3::Zero(), shape("antenna.dipole", {{"length", 0.5 * kLambda}, {"wireRadius", 0.001}}),
        Id("mat-copper"));
    DomainBox box;
    box.corner = Vec3(-0.8 * kLambda, -0.8 * kLambda, -0.8 * kLambda);
    box.size = Vec3(1.6, 1.6, 1.6) * kLambda;
    EmFullWaveSettings settings;
    settings.cellsPerWavelength = 16.0;
    settings.absorbingCells = 8;
    settings.harmonicPeriods = 5.0;
    settings.snapshotCount = 0;
    EmPrepared prepared = prepareFullWave(*project, box, settings);
    ASSERT_TRUE(prepared.plan.allowed) << prepared.plan.refusal;
    EXPECT_EQ(prepared.plan.dimensions, 3);
    EXPECT_GT(prepared.plan.wireEdges, 4u);
    const SampleGrid grid = prepared.plan.grid;
    const auto result = simulateFullWave("dipole", std::move(prepared), settings);
    ASSERT_TRUE(result.ok()) << result.message();
    const EmFieldResult& f = *result.value()->harmonicField;
    // |E| at 0.6 lambda: broadside against 45 degrees, compared with the analytical thin dipole.
    const auto sampleAt = [&](const Vec3& p) {
        const Vec3 d = p - grid.origin;
        const auto index = [&](const Vec3& axis, int n) { return static_cast<int>(std::lround(d.dot(axis) / axis.squaredNorm() * (n - 1))); };
        return std::abs(f.component(FieldComponent::EMagnitude, grid.index(index(grid.axisU, grid.nu), index(grid.axisV, grid.nv), index(grid.axisW, grid.nw))));
    };
    const double r = 0.6 * kLambda;
    const Vec3 broadside(r, 0, 0), oblique(r * std::sqrt(0.5), 0, r * std::sqrt(0.5));
    const double fdtdRatio = sampleAt(oblique) / sampleAt(broadside);
    const double exactRatio = thinDipoleField(oblique, 0.5 * kLambda, Complex(1, 0), kF).e.norm() /
                              thinDipoleField(broadside, 0.5 * kLambda, Complex(1, 0), kF).e.norm();
    EXPECT_NEAR(fdtdRatio, exactRatio, 0.15 * exactRatio);
    // Not aligned with the grid: left out with a warning; loops are not represented.
    auto skew = newProject("z", "z");
    add(*skew, "d", em::object_types::Antenna, "Tilted dipole", antenna(), Vec3::Zero(), shape("antenna.dipole", {{"length", 0.5 * kLambda}, {"wireRadius", 0.001}}),
        {}, Vec3(0.3, 0, 0));
    add(*skew, "l", em::object_types::Antenna, "Loop", antenna(), Vec3::Zero(), shape("antenna.loop", {{"loopRadius", 0.01}, {"wireRadius", 0.001}}));
    const EmPrepared refused = prepareFullWave(*skew, box, settings);
    EXPECT_FALSE(refused.plan.allowed);
    std::string warnings;
    for (const std::string& w : refused.plan.warnings)
        warnings += w + "\n";
    EXPECT_NE(warnings.find("not aligned"), std::string::npos) << warnings;
    EXPECT_NE(warnings.find("not represented"), std::string::npos);
    // Too fine for the memory limit: refused, not coarsened.
    settings.cellsPerWavelength = 200.0;
    EXPECT_NE(prepareFullWave(*project, box, settings).plan.refusal.find("not lowered automatically"), std::string::npos);
}

TEST(EmScene, TouchstoneRoundTripSettingsAndPersistence)
{
    OnePortData d;
    d.frequencyHz = {1e9, 1.5e9, 2e9};
    d.reflection = {Complex(0.1, -0.2), Complex(0.0, 0.0), Complex(-0.5, 0.3)};
    d.referenceImpedance = 75.0;
    d.comments = "calculated by Resonant";
    const auto back = parseTouchstone1(writeTouchstone1(d));
    ASSERT_TRUE(back.ok()) << back.message();
    EXPECT_DOUBLE_EQ(back.value().referenceImpedance, 75.0);
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_NEAR(back.value().frequencyHz[i], d.frequencyHz[i], 1e-3);
        EXPECT_NEAR(std::abs(back.value().reflection[i] - d.reflection[i]), 0.0, 1e-12);
    }
    EXPECT_NEAR(std::abs(back.value().impedance()[1] - Complex(75, 0)), 0.0, 1e-9);
    const auto ma = parseTouchstone1("! measured\n# MHz S MA R 50\n100 0.5 -90\n200 1.0 180\n");
    ASSERT_TRUE(ma.ok());
    EXPECT_NEAR(ma.value().reflection[0].imag(), -0.5, 1e-12);
    EXPECT_DOUBLE_EQ(ma.value().frequencyHz[1], 2e8);
    const auto db = parseTouchstone1("# GHz S DB R 50\n1 -20 0\n");
    EXPECT_NEAR(std::abs(db.value().reflection[0]), 0.1, 1e-12);
    EXPECT_FALSE(parseTouchstone1("# GHz S RI R 50\n1 0.1 0.2 0.3 0.4\n").ok());  // two-port data
    EXPECT_FALSE(parseTouchstone1("# GHz Z RI R 50\n1 0.1 0.2\n").ok());
    EXPECT_FALSE(parseTouchstone1("").ok());

    EmSettings s;
    s.beams = {{0, 0, 0, false}, {20, 0, -200, true}};
    s.trueTimeDelay = true;
    auto project = buildSteering();
    storeEmSettings(*project, s);
    EmFullWaveSettings fw;
    fw.boundary[0] = YeeBoundary::PEC;
    storeEmFullWaveSettings(*project, fw);
    auto reloaded = deserializeProject(serializeProject(*project));
    ASSERT_TRUE(reloaded.ok()) << reloaded.message();
    EXPECT_EQ(emSettingsOf(*reloaded.value().project), s);
    EXPECT_EQ(emFullWaveSettingsOf(*reloaded.value().project), fw);
    EXPECT_EQ(serializeProject(*reloaded.value().project), serializeProject(*project));
}
