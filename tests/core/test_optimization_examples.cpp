// The example optimization projects (REQ-CHAT-20261126-009, RES-OPT-024):
// every example holds a scene and an optimization problem, and each problem
// runs with the real adapters and physics and reaches what the example
// claims.
//
// To regenerate the example files after an intended change:
//   RESONANT_WRITE_EXAMPLES=1 resonant_core_tests --gtest_filter='OptimizationExamples.*'
#include "OptimizationFixtures.h"

#include "electromagnetics/ArrayFactor.h"
#include "optimization/Engine.h"
#include "optimization/MultiFidelity.h"
#include "optimization/Robust.h"
#include "optimization/Persistence.h"
#include "optimization/SceneBindings.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numbers>

using namespace resonant;
using namespace resonant::optimization;
using namespace optimization_fixtures;

namespace {

constexpr double kPi = std::numbers::pi;

OptimizationProblem fromTemplate(const Project& project, const std::string& id)
{
    auto j = AdapterRegistry::instance().findTemplate(id)->create(project);
    EXPECT_TRUE(j.ok()) << j.message();
    auto p = problemFromJson(j.value());
    EXPECT_TRUE(p.ok()) << p.message();
    return p.take();
}

void store(Project& project, OptimizationProblem problem, const std::string& id, const std::string& description)
{
    problem.id = id;
    problem.description = description;
    OptimizationWorkspace w = workspaceOf(project);
    w.problems.push_back(std::move(problem));
    storeWorkspace(project, w);
}

// Writes (when asked) and reads back an example; the file must match the builder.
std::unique_ptr<Project> checked(const std::string& file, const Project& built)
{
    const auto path = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "optimization" / file;
    const nlohmann::json expected = serializeProject(built);
    if (std::getenv("RESONANT_WRITE_EXAMPLES")) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << expected.dump(2) << "\n";
    }
    auto loaded = loadProjectFile(path);
    EXPECT_TRUE(loaded.ok()) << loaded.message();
    if (!loaded.ok())
        return nullptr;
    EXPECT_EQ(serializeProject(*loaded.value().project).dump(), nlohmann::json::parse(expected.dump()).dump())
        << file << " is stale; regenerate it with RESONANT_WRITE_EXAMPLES=1";
    return std::move(loaded.value().project);
}

RunRecord run(const Project& project, std::optional<int> budget = {})
{
    OptimizationProblem p = workspaceOf(project).problems.front();
    if (budget)
        p.algorithm.maxEvaluations = *budget;
    auto bound = AdapterRegistry::instance().bind(p.adapterId, designSnapshot(project), p.adapterConfig);
    EXPECT_TRUE(bound.ok()) << bound.message();
    auto r = runOptimization(p, bound.take());
    EXPECT_TRUE(r.ok()) << r.message();
    return r.take();
}

const Candidate& best(const RunRecord& r)
{
    EXPECT_TRUE(r.best.has_value()) << r.termination.reason;
    return r.candidates[*r.best];
}

// --- builders -------------------------------------------------------------------------------------------------

std::unique_ptr<Project> exampleA()
{
    auto project = acousticArray("ex-opt-a", 8, true);
    project->name = "A - acoustic focal array";
    OptimizationProblem p = fromTemplate(*project, "acoustic.array-phase-focus");
    for (DesignVariable& v : p.space.variables)
        v.label = v.label;  // phases from the template
    // Amplitudes too, limited to the elements' 1 Pa at 1 m.
    auto ev = AdapterRegistry::instance().bind(p.adapterId, designSnapshot(*project), p.adapterConfig).take();
    for (int i = 0; i < 8; ++i) {
        DesignVariable v = *ev->capabilities().variable(objectBinding("ex-opt-a-array", "array/element/" + std::to_string(i) + "/amplitude"));
        v.upper = 1.0;
        p.space.variables.push_back(v);
    }
    p.name = "Focus the array: element phases and amplitudes";
    p.algorithm.maxEvaluations = 1500;
    store(*project, p, "ex-opt-a-focus",
          "Maximize the pressure at the focus by the phase and amplitude of each element (at most 1 Pa at 1 m each); the initial "
          "design is the uniform excitation (equal amplitude, zero phase). The analytical optimum is every element at full "
          "amplitude, in phase at the focus: the coherent sum of A_i / r_i.");
    return project;
}

std::unique_ptr<Project> exampleB()
{
    auto project = acousticReflector("ex-opt-b", 0.06, 0.08, 2.0);
    project->name = "B - acoustic reflector";
    OptimizationProblem p = fromTemplate(*project, "acoustic.reflector-focus");
    for (DesignVariable& v : p.space.variables)
        if (v.free()) {
            v.lower = 0.05;
            v.upper = 0.11;
        }
    p.algorithm.pointsPerVariable = 31;
    store(*project, p, "ex-opt-b-reflector",
          "Sweep the focal length of a parabolic reflector (aperture 200 mm) lit by a source 2 m away for the largest pressure at a "
          "target 80 mm in front of its vertex, with the Kirchhoff reflector model. The mirror equation 1/f = 1/2 m + 1/0.08 m "
          "predicts f = 76.9 mm; the candidates show how sharply the focus depends on it.");
    return project;
}

std::unique_ptr<Project> exampleC()
{
    auto project = plateWithActuators("ex-opt-c");
    project->name = "C - mechanical vibration suppression";
    OptimizationProblem p = fromTemplate(*project, "mechanical.suppress-vibration");
    store(*project, p, "ex-opt-c-suppression",
          "Two actuators on a simply supported aluminium plate at 120 Hz: choose their force amplitudes and phases so that a quiet "
          "point does not move and a second point moves 2 um. The linear inverse solution of the same problem is available.");
    return project;
}

std::unique_ptr<Project> exampleD()
{
    auto project = cantilever("ex-opt-d");
    project->name = "D - mechanical resonator";
    auto ev = AdapterRegistry::instance().bind("mechanical", designSnapshot(*project), {{"structure", "ex-opt-d-beam"}}).take();
    OptimizationProblem p;
    p.name = "Tune the resonator to 25 Hz";
    p.adapterId = "mechanical";
    p.adapterConfig = {{"structure", "ex-opt-d-beam"}};
    p.modelId = "mechanical.analytical";
    p.validationModelId = "mechanical.fe";
    for (const char* key : {"geometry/width", "geometry/height", "material"}) {
        DesignVariable v = *ev->capabilities().variable(objectBinding("ex-opt-d-beam", key));
        if (std::string(key) == "geometry/width") {
            v.lower = 0.3;
            v.upper = 0.8;
        } else if (std::string(key) == "geometry/height") {
            v.lower = 0.003;
            v.upper = 0.01;
            v.step = 0.0005;  // stock thicknesses in 0.5 mm steps
        }
        p.space.variables.push_back(v);
    }
    Objective f1;
    f1.id = "f1";
    f1.label = "First natural frequency";
    f1.quantity = "modal.f1";
    f1.sense = Sense::Target;
    f1.target = 25.0;
    f1.scale = 0.25;
    p.objectives.push_back(f1);
    DesignConstraint mass;
    mass.id = "mass";
    mass.label = "Mass";
    mass.quantity = "mass";
    mass.relation = Relation::AtMost;
    mass.limit = 0.25;
    mass.scale = 0.01;
    mass.category = ConstraintCategory::Physical;
    p.constraints.push_back(mass);
    DesignConstraint thick;
    thick.id = "thickness";
    thick.label = "Thickness for the clamp";
    thick.quantity = "expr:[object/ex-opt-d-beam/geometry/height]";
    thick.relation = Relation::AtLeast;
    thick.limit = 0.004;
    thick.scale = 0.0005;
    thick.category = ConstraintCategory::Manufacturing;
    p.constraints.push_back(thick);
    p.algorithm.algorithm = "differential-evolution";
    p.algorithm.maxEvaluations = 1200;
    p.algorithm.seed = 4;
    store(*project, p, "ex-opt-d-resonator",
          "A cantilever whose first natural frequency must be 25 Hz: length, a thickness from 0.5 mm stock steps and a material "
          "from the library, with at most 250 g and at least 4 mm thickness for the clamp. Screened with the Euler-Bernoulli "
          "formulas, re-evaluated with finite elements.");
    return project;
}

std::unique_ptr<Project> planarArray(const std::string& id)
{
    auto project = newProject(id, "Planar array");
    em::EmArraySettings s;
    s.layout = em::EmArrayLayout::Planar;
    s.countX = 4;
    s.countY = 4;
    s.frequencyHz = 1e9;
    s.spacingX = s.spacingY = 0.5 * 299792458.0 / 1e9;
    s.pattern = em::ElementPattern::Isotropic;
    add(*project, id + "-array", em::object_types::Array, "4 x 4 planar array", std::make_shared<em::EmArrayComponent>(s));
    return project;
}

std::unique_ptr<Project> exampleE()
{
    auto project = planarArray("ex-opt-e");
    project->name = "E - electromagnetic beam steering";
    OptimizationProblem p = fromTemplate(*project, "em.beam-steering");
    p.adapterConfig["directions"] = {{{"label", "target"}, {"thetaDeg", 30.0}, {"phiDeg", 45.0}}};
    p.adapterConfig["resolutionDeg"] = 2.0;
    p.algorithm.maxEvaluations = 1500;
    store(*project, p, "ex-opt-e-steering",
          "Steer a 4 x 4 planar array (half-wavelength spacing, 1 GHz) to theta 30, phi 45 degrees by its 16 element phases; "
          "compare the optimized pattern with the analytical phase steering.");
    return project;
}

std::unique_ptr<Project> linearTaperProblem(const std::string& id, int count, OptimizationProblem& p, double thetaDeg)
{
    auto project = emArray(id, count);
    auto ev = AdapterRegistry::instance().bind("electromagnetic", designSnapshot(*project), {{"directions", {{{"label", "target"}, {"thetaDeg", thetaDeg}, {"phiDeg", 0.0}}}}}).take();
    p.adapterId = "electromagnetic";
    p.adapterConfig = {{"object", id + "-array"}, {"directions", {{{"label", "target"}, {"thetaDeg", thetaDeg}, {"phiDeg", 0.0}}}}, {"resolutionDeg", 1.0}};
    p.modelId = "em.arrayfactor";
    // A symmetric taper: elements 0 .. n/2-1 are free, the others mirror them (derived variables).
    for (int i = 0; i < count / 2; ++i) {
        DesignVariable v = *ev->capabilities().variable(objectBinding(id + "-array", "array/element/" + std::to_string(i) + "/amplitude"));
        v.id = "a" + std::to_string(i);
        v.label = "Amplitude of elements " + std::to_string(i) + " and " + std::to_string(count - 1 - i);
        v.lower = 0.05;
        v.upper = 1.0;
        v.group = "Taper";
        p.space.variables.push_back(v);
        p.space.derived.push_back({"a" + std::to_string(count - 1 - i), "mirror of a" + std::to_string(i),
                                   objectBinding(id + "-array", "array/element/" + std::to_string(count - 1 - i) + "/amplitude"), "",
                                   "a" + std::to_string(i)});
    }
    return project;
}

std::unique_ptr<Project> exampleF()
{
    OptimizationProblem p;
    auto project = linearTaperProblem("ex-opt-f", 10, p, 90.0);
    project->name = "F - sidelobe reduction";
    p.name = "Directivity against sidelobes";
    p.adapterConfig["directions"] = {{{"label", "target"}, {"thetaDeg", 90.0}, {"phiDeg", 90.0}}};
    Objective d;
    d.id = "directivity";
    d.label = "Directivity";
    d.quantity = "pattern.directivityDbi";
    d.sense = Sense::Maximize;
    Objective sll;
    sll.id = "sidelobes";
    sll.label = "Peak sidelobe level";
    sll.quantity = "pattern.sidelobeDb";
    sll.sense = Sense::Minimize;
    p.objectives = {d, sll};
    p.algorithm.algorithm = "nsga2";
    p.algorithm.maxEvaluations = 1600;
    p.algorithm.populationSize = 40;
    store(*project, p, "ex-opt-f-sidelobes",
          "A 10-element linear array with a symmetric amplitude taper (five free amplitudes, the other five derived): NSGA-II "
          "approximates the trade-off between directivity and peak sidelobe level; compare the front with Dolph-Chebyshev "
          "tapers. No single design is optimal for both.");
    return project;
}

std::unique_ptr<Project> exampleG()
{
    OptimizationProblem p;
    auto project = linearTaperProblem("ex-opt-g", 10, p, 30.0);
    project->name = "G - robust design";
    // Phase steering to 30 degrees at 1 GHz (fixed); the taper is optimized.
    auto ev = AdapterRegistry::instance().bind("electromagnetic", designSnapshot(*project), p.adapterConfig).take();
    em::ArrayModel model = em::arrayModelOf(
        std::dynamic_pointer_cast<const em::EmArrayComponent>(project->scene.find(Id("ex-opt-g-array"))->state.component)->settings);
    em::steer(model, em::directionOf(30.0 * kPi / 180.0, 0.0), 1e9, em::SteeringMethod::Phase);
    for (size_t n = 0; n < model.elements.size(); ++n) {
        DesignVariable v = *ev->capabilities().variable(objectBinding("ex-opt-g-array", "array/element/" + std::to_string(n) + "/phase"));
        v.initial = std::arg(model.elements[n].weight);
        v.locked = true;
        p.space.variables.push_back(v);
    }
    p.name = "Robust taper for a 0.9 - 1.1 GHz band";
    Objective g;
    g.id = "gain";
    g.label = "Directivity toward the target";
    g.quantity = "pattern.gainDbi/target";
    g.sense = Sense::Maximize;
    p.objectives = {g};
    p.robust.enabled = true;
    p.robust.samples = 8;
    p.robust.measure = RobustMeasure::WorstCase;
    p.robust.parameters.push_back({objectBinding("ex-opt-g-array", "array/frequency"), "Operating frequency", Distribution::Uniform, 0.1, true, 3.0,
                                   "the example's operating band, 0.9 to 1.1 GHz (a stated requirement, not a measured distribution)"});
    p.algorithm.algorithm = "differential-evolution";
    p.algorithm.maxEvaluations = 400;
    p.algorithm.seed = 2;
    store(*project, p, "ex-opt-g-robust",
          "Phase steering set for 1 GHz squints across a 0.9 - 1.1 GHz band. The robust problem maximizes the WORST directivity "
          "toward 30 degrees over the band (eight samples, the same for every design) by a symmetric amplitude taper; compare it "
          "with the nominal optimum, which maximizes the directivity at 1 GHz only.");
    return project;
}

}  // namespace

TEST(OptimizationExamples, AcousticFocalArrayReachesTheCoherentSum)
{
    auto project = checked("a-acoustic-focal-array.resonant", *exampleA());
    ASSERT_TRUE(project);
    const RunRecord r = run(*project);
    double coherent = 0.0;
    for (const auto& s : acoustics::collectSources(project->scene))
        coherent += 1.0 / (Vec3(0.02, 0.0, 0.08) - s.position).norm();
    EXPECT_NEAR(best(r).objectiveValues[0] / coherent, 1.0, 1e-3);
    // The uniform excitation (the initial design) is the comparison.
    EXPECT_LT(r.candidates.front().objectiveValues[0], 0.5 * coherent);
}

TEST(OptimizationExamples, AcousticReflectorCandidatesPeakNearTheMirrorEquation)
{
    auto project = checked("b-acoustic-reflector.resonant", *exampleB());
    ASSERT_TRUE(project);
    const RunRecord r = run(*project);
    EXPECT_EQ(r.candidates.size(), 31u);
    const OptimizationProblem p = workspaceOf(*project).problems.front();
    const double f = best(r).point.values.at(p.space.freeVariables().front()->id);
    EXPECT_NEAR(f / (1.0 / (1.0 / 2.0 + 1.0 / 0.08)), 1.0, 0.05);
    const DesignComparison cmp = compare(p, r.candidates.front(), best(r));
    EXPECT_TRUE(cmp.sameModel);
    EXPECT_GT(cmp.objectives[0].difference, 0.0);
}

TEST(OptimizationExamples, MechanicalVibrationIsSuppressed)
{
    auto project = checked("c-mechanical-vibration-suppression.resonant", *exampleC());
    ASSERT_TRUE(project);
    const RunRecord r = run(*project);
    EXPECT_LT(best(r).objectiveValues[0], 0.05 * r.candidates.front().objectiveValues[0]);
    EXPECT_EQ(best(r).evaluation.model.modelId, "mechanical.fe");
}

TEST(OptimizationExamples, MechanicalResonatorMeetsItsFrequencyWithinTheConstraints)
{
    auto project = checked("d-mechanical-resonator.resonant", *exampleD());
    ASSERT_TRUE(project);
    const RunRecord r = run(*project);
    const Candidate& b = best(r);
    EXPECT_TRUE(b.feasible);
    EXPECT_NEAR(b.objectiveValues[0], 25.0, 0.25);
    EXPECT_LE(b.point.values.at("object/ex-opt-d-beam/geometry/height"), 0.01);
    EXPECT_GE(b.point.values.at("object/ex-opt-d-beam/geometry/height"), 0.004 - 1e-12);
    // Validated with the finite elements.
    OptimizationProblem p = workspaceOf(*project).problems.front();
    auto ev = AdapterRegistry::instance().bind(p.adapterId, designSnapshot(*project), p.adapterConfig).take();
    p.fillUnits(ev->capabilities());
    EvaluationService service(p, ev, {});
    const auto report = reevaluate(service, {b}, "mechanical.fe");
    ASSERT_TRUE(report.validated.front().evaluated());
    EXPECT_NEAR(report.validated.front().objectiveValues[0], 25.0, 0.5);
    EXPECT_TRUE(isValidated(p, report.validated.front()));
}

TEST(OptimizationExamples, ElectromagneticBeamSteeringMatchesPhaseSteering)
{
    auto project = checked("e-electromagnetic-beam-steering.resonant", *exampleE());
    ASSERT_TRUE(project);
    const RunRecord r = run(*project);
    OptimizationProblem p = workspaceOf(*project).problems.front();
    auto ev = AdapterRegistry::instance().bind(p.adapterId, designSnapshot(*project), p.adapterConfig).take();
    em::ArrayModel model = em::arrayModelOf(
        std::dynamic_pointer_cast<const em::EmArrayComponent>(project->scene.find(Id("ex-opt-e-array"))->state.component)->settings);
    em::steer(model, em::directionOf(30.0 * kPi / 180.0, 45.0 * kPi / 180.0), 1e9, em::SteeringMethod::Phase);
    std::map<std::string, double, std::less<>> steered;
    for (size_t n = 0; n < model.elements.size(); ++n)
        steered[objectBinding("ex-opt-e-array", "array/element/" + std::to_string(n) + "/phase")] = std::arg(model.elements[n].weight);
    const Evaluation reference = ev->evaluate(steered, "em.arrayfactor");
    // At least as good as phase steering. Diagonal neighbours of a half-wave
    // grid are not orthogonal radiators, so the largest directivity toward a
    // direction is not exactly the conjugate-phase design: the optimizer may
    // exceed it slightly.
    EXPECT_GE(best(r).objectiveValues[0], reference.quantities.at("pattern.gainDbi/target") - 0.05);
    EXPECT_LT(best(r).objectiveValues[0], reference.quantities.at("pattern.gainDbi/target") + 0.5);
    EXPECT_LT(best(r).evaluation.quantities.at("pattern.mainLobeErrorDeg"), 4.0);  // within one sampling cell (2 x 4 degrees)
    EXPECT_GT(best(r).objectiveValues[0], r.candidates.front().objectiveValues[0] + 3.0);
}

TEST(OptimizationExamples, SidelobeReductionGivesATradeOffFront)
{
    auto project = checked("f-sidelobe-reduction.resonant", *exampleF());
    ASSERT_TRUE(project);
    const RunRecord r = run(*project);
    ASSERT_GE(r.pareto.size(), 8u);
    double lowest = 0.0, highest = -100.0;
    for (size_t i : r.pareto) {
        lowest = std::min(lowest, r.candidates[i].objectiveValues[1]);
        highest = std::max(highest, r.candidates[i].objectiveValues[0]);
        // The derived amplitudes mirror the free ones.
        EXPECT_DOUBLE_EQ(r.candidates[i].point.values.at("a9"), r.candidates[i].point.values.at("a0"));
    }
    EXPECT_LT(lowest, -20.0);
    EXPECT_GT(highest, 10.0 * std::log10(10.0) - 0.3);  // the uniform array's directivity is 10 log10(N) = 10 dBi
}

TEST(OptimizationExamples, RobustDesignTradesNominalGainForTheWorstCaseOverTheBand)
{
    auto project = checked("g-robust-design.resonant", *exampleG());
    ASSERT_TRUE(project);
    const RunRecord robust = run(*project);
    OptimizationProblem nominal = workspaceOf(*project).problems.front();
    nominal.robust.enabled = false;
    auto ev = AdapterRegistry::instance().bind(nominal.adapterId, designSnapshot(*project), nominal.adapterConfig).take();
    auto nominalRun = runOptimization(nominal, ev);
    ASSERT_TRUE(nominalRun.ok());
    const Candidate& rb = best(robust);
    const Candidate& nb = best(nominalRun.value());
    ASSERT_TRUE(rb.robust.has_value());
    EXPECT_FALSE(nb.robust.has_value());
    // Both designs scored robustly with the same samples.
    OptimizationProblem robustProblem = workspaceOf(*project).problems.front();
    auto nominalUnderBand = assessRobustness(robustProblem, ev, nb.point);
    ASSERT_TRUE(nominalUnderBand.ok()) << nominalUnderBand.message();
    const double robustWorst = -rb.scores[0];
    const double nominalWorst = -nominalUnderBand.value().robust.scores[0];
    EXPECT_GE(robustWorst, nominalWorst - 1e-9);  // the robust design is at least as good in the worst case
    EXPECT_GE(nb.objectiveValues[0], rb.objectiveValues[0] - 1e-9);  // and the nominal design is at least as good at 1 GHz
}
