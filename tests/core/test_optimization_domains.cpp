// The domain optimization adapters against known answers
// (REQ-CHAT-20261126-009, RES-OPT-009, RES-OPT-010, RES-OPT-012, RES-OPT-014,
// RES-OPT-020, RES-OPT-025): phase focusing of an acoustic array against the
// coherent sum, acoustic inverse design against the acoustic module's own
// solver, reflector geometry against the mirror equation, invalid geometry,
// full-wave re-evaluation, beam resonance tuning against Euler-Bernoulli,
// vibration targeting against the mechanics module's solver, beam steering
// against phase steering, sidelobe metrics against a Dolph-Chebyshev taper,
// null placement, dipole resonance against the induced-EMF resonant length,
// and the original project left untouched.
#include "OptimizationFixtures.h"

#include "acoustics/InverseDesign.h"
#include "acoustics/Propagation.h"
#include "electromagnetics/Antennas.h"
#include "electromagnetics/ArrayFactor.h"
#include "manipulators/Analytics.h"
#include "mechanics/Analyses.h"
#include "mechanics/MechanicalOptimization.h"
#include "mechanics/StructuralModel.h"
#include "mechanics/Targeting.h"
#include "optimization/Engine.h"
#include "optimization/LinearInverse.h"
#include "optimization/MultiFidelity.h"
#include "optimization/Persistence.h"
#include "optimization/SceneBindings.h"
#include "project/ProjectSerializer.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

using namespace resonant;
using namespace resonant::optimization;
using namespace optimization_fixtures;

namespace {

constexpr double kPi = std::numbers::pi;
using Bindings = std::map<std::string, double, std::less<>>;

std::shared_ptr<const DesignEvaluator> bindAdapter(const Project& project, const std::string& adapter, const nlohmann::json& configuration)
{
    auto bound = AdapterRegistry::instance().bind(adapter, designSnapshot(project), configuration);
    EXPECT_TRUE(bound.ok()) << bound.message();
    return bound.ok() ? bound.take() : nullptr;
}

OptimizationProblem fromTemplate(const Project& project, const std::string& id)
{
    const ProblemTemplate* t = AdapterRegistry::instance().findTemplate(id);
    EXPECT_NE(t, nullptr) << id;
    auto j = t->create(project);
    EXPECT_TRUE(j.ok()) << j.message();
    auto p = problemFromJson(j.value());
    EXPECT_TRUE(p.ok()) << p.message();
    return p.take();
}

double quantity(const Evaluation& e, const std::string& key)
{
    const auto it = e.quantities.find(key);
    EXPECT_NE(it, e.quantities.end()) << key << " (" << e.message << ")";
    return it == e.quantities.end() ? NAN : it->second;
}

const Candidate& best(const RunRecord& r)
{
    EXPECT_TRUE(r.best.has_value());
    return r.candidates[*r.best];
}

}  // namespace

// --- acoustic -------------------------------------------------------------------------------------------------

TEST(AcousticOptimization, PhaseFocusingReachesTheCoherentSumAndThePhaseFocus)
{
    auto project = acousticArray("t-focus", 8);
    const nlohmann::json before = serializeProject(*project);
    OptimizationProblem p = fromTemplate(*project, "acoustic.array-phase-focus");
    p.algorithm.maxEvaluations = 900;
    p.algorithm.tolerance = 1e-10;
    auto ev = bindAdapter(*project, p.adapterId, p.adapterConfig);
    auto run = runOptimization(p, ev);
    ASSERT_TRUE(run.ok()) << run.message();
    // The largest |p| at the focus is sum A_i / r_i with every element in phase.
    const auto sources = acoustics::collectSources(project->scene);
    const Vec3 focus(0.02, 0.0, 0.08);
    const double k = acoustics::airAtTemperature(293.15).wavenumber(kAcousticFrequency);
    double coherent = 0.0;
    for (const auto& s : sources)
        coherent += 1.0 / (focus - s.position).norm();
    const Candidate& b = best(run.value());
    EXPECT_NEAR(b.objectiveValues[0] / coherent, 1.0, 1e-5);
    EXPECT_LT(run.value().candidates.front().objectiveValues[0], 0.5 * coherent);  // the uniform start is far from it
    // The phases are the phase focus k r_i, up to one common phase.
    const std::string arrayId = "t-focus-array";
    const double phi0 = b.point.values.at(objectBinding(arrayId, "array/element/0/phase"));
    for (size_t i = 0; i < sources.size(); ++i) {
        const double phi = b.point.values.at(objectBinding(arrayId, "array/element/" + std::to_string(i) + "/phase"));
        const double expected = k * ((focus - sources[i].position).norm() - (focus - sources[0].position).norm());
        EXPECT_NEAR(std::remainder(phi - phi0 - expected, 2 * kPi), 0.0, 2e-3) << i;
    }
    // Every candidate records the model that evaluated it, and the project is untouched.
    EXPECT_EQ(b.evaluation.model.modelId, "acoustic.freefield.analytical");
    EXPECT_EQ(b.evaluation.model.fidelity, Fidelity::Analytical);
    EXPECT_EQ(serializeProject(*project), before);
}

TEST(AcousticOptimization, InverseDesignAgreesWithTheAcousticSolverAndTheEvaluation)
{
    auto project = acousticArray("t-inverse", 8, false, true);
    auto ev = bindAdapter(*project, "acoustic", {{"focus", "t-inverse-focus"}});
    auto system = ev->linearSystem({}, "acoustic.freefield.analytical");
    ASSERT_TRUE(system.ok()) << system.message();
    EXPECT_EQ(system.value().response.rows(), 2);
    EXPECT_EQ(system.value().response.cols(), 8);
    LinearInverseOptions o;
    o.regularization = 1e-6;
    auto solved = solveLinearInverse(system.value(), o);
    ASSERT_TRUE(solved.ok());
    // The acoustic module's own inverse solver on the same problem.
    const auto sources = acoustics::collectSources(project->scene);
    const auto targets = acoustics::collectTargets(project->scene);
    acoustics::InverseOptions io;
    io.regularization = 1e-6;
    const acoustics::FreeFieldModel model;
    const auto reference = acoustics::solveInverse(sources, targets.points, acoustics::airAtTemperature(293.15), model, io);
    ASSERT_TRUE(reference.solved);
    EXPECT_LT((solved.value().weights - reference.weights).norm(), 1e-9 * reference.weights.norm());
    // Applied to the design, the evaluated target error is the solver's residual.
    const auto bindings = weightBindings(system.value(), solved.value().weights);
    const Evaluation e = ev->evaluate(bindings, "acoustic.freefield.analytical");
    ASSERT_TRUE(e.ok()) << e.message;
    EXPECT_NEAR(quantity(e, "targets.error"), solved.value().relativeResidual, 1e-9);
    EXPECT_NEAR(quantity(e, "focus.pressure"), 5.0, 1e-3);
    EXPECT_LT(quantity(e, "suppression.maxPressure"), 1e-3);
}

TEST(AcousticOptimization, ReflectorGeometryFocusesWhereTheMirrorEquationSays)
{
    const double target = 0.08, source = 2.0;
    auto project = acousticReflector("t-reflector", 0.06, target, source);
    OptimizationProblem p = fromTemplate(*project, "acoustic.reflector-focus");
    ASSERT_EQ(p.space.dimension(), 1u);
    for (DesignVariable& v : p.space.variables)
        if (v.free()) {
            v.lower = 0.05;
            v.upper = 0.11;
        }
    p.algorithm.pointsPerVariable = 41;
    auto run = runOptimization(p, bindAdapter(*project, p.adapterId, p.adapterConfig));
    ASSERT_TRUE(run.ok()) << run.message();
    // 1/f = 1/d_source + 1/d_image with the image at the target.
    const double expected = 1.0 / (1.0 / source + 1.0 / target);
    const Candidate& b = best(run.value());
    const double found = b.point.values.at(p.space.freeVariables().front()->id);
    EXPECT_NEAR(found / expected, 1.0, 0.05) << "found " << found << " m, mirror equation " << expected << " m";
    EXPECT_EQ(b.evaluation.model.modelId, "acoustic.reflector.kirchhoff");
    EXPECT_GT(quantity(b.evaluation, "focus.gain"), 5.0);  // the reflector, not the direct field, makes the focus
}

TEST(AcousticOptimization, InvalidGeometryIsDetectedAndNeverEvaluated)
{
    auto project = acousticReflector("t-sphere", 0.06, 0.08, 2.0, "reflector.spherical");
    auto ev = bindAdapter(*project, "acoustic", {{"focus", "t-sphere-focus"}});
    const std::string radius = objectBinding("t-sphere-reflector", "geometry/curvatureRadius");
    ASSERT_NE(ev->capabilities().variable(radius), nullptr);
    // An aperture of 0.2 m needs a radius of curvature of at least 0.1 m.
    const Evaluation bad = ev->evaluate({{radius, 0.08}}, "acoustic.reflector.kirchhoff");
    EXPECT_EQ(bad.status, Evaluation::Status::InvalidDesign);
    EXPECT_NE(bad.message.find("aperture"), std::string::npos) << bad.message;
    const Evaluation good = ev->evaluate({{radius, 0.2}}, "acoustic.reflector.kirchhoff");
    EXPECT_TRUE(good.ok()) << good.message;
    const Evaluation unknown = ev->evaluate({{"object/t-sphere-reflector/nonsense", 1.0}}, "acoustic.reflector.kirchhoff");
    EXPECT_EQ(unknown.status, Evaluation::Status::InvalidDesign);
}

TEST(AcousticOptimization, FullWaveReevaluationValidatesTheFocusedDesign)
{
    auto project = acousticArray("t-fullwave", 8, true);
    OptimizationProblem p = fromTemplate(*project, "acoustic.array-phase-focus");
    EXPECT_EQ(p.validationModelId, "acoustic.fdtd.staggered");
    p.algorithm.maxEvaluations = 900;
    auto ev = bindAdapter(*project, p.adapterId, p.adapterConfig);
    auto run = runOptimization(p, ev);
    ASSERT_TRUE(run.ok());
    p.fillUnits(ev->capabilities());
    ServiceOptions o;
    o.parallel = 2;
    EvaluationService service(p, ev, o);
    const Candidate uniform = run.value().candidates.front();
    const Candidate focused = best(run.value());
    const MultiFidelityReport report = reevaluate(service, {uniform, focused}, "acoustic.fdtd.staggered");
    ASSERT_EQ(report.validated.size(), 2u);
    for (const Candidate& c : report.validated)
        ASSERT_TRUE(c.evaluated()) << c.evaluation.message;
    EXPECT_EQ(report.toFidelity, Fidelity::HighFidelity);
    EXPECT_TRUE(isValidated(p, report.validated[1]));
    EXPECT_FALSE(isValidated(p, focused));
    // The full-wave model is a different (two-dimensional) problem, but the
    // focused design still concentrates more pressure at the focus.
    EXPECT_GT(report.validated[1].objectiveValues[0], 1.5 * report.validated[0].objectiveValues[0]);
    EXPECT_EQ(report.validated[1].parents, std::vector<std::string>{focused.id});
}

// --- mechanical -------------------------------------------------------------------------------------------------

TEST(MechanicalOptimization, AnalyticalAndFiniteElementBeamsAgree)
{
    auto project = cantilever("t-beam");
    auto ev = bindAdapter(*project, "mechanical", nlohmann::json::object());
    const Evaluation analytical = ev->evaluate({}, "mechanical.analytical");
    const Evaluation fe = ev->evaluate({}, "mechanical.fe");
    ASSERT_TRUE(analytical.ok()) << analytical.message;
    ASSERT_TRUE(fe.ok()) << fe.message;
    EXPECT_NEAR(quantity(fe, "modal.f1") / quantity(analytical, "modal.f1"), 1.0, 0.01);
    EXPECT_NEAR(quantity(fe, "modal.f2") / quantity(analytical, "modal.f2"), 1.0, 0.02);
    EXPECT_NEAR(quantity(fe, "mass") / quantity(analytical, "mass"), 1.0, 1e-6);
    const Evaluation coarse = ev->evaluate({}, "mechanical.fe.coarse");
    ASSERT_TRUE(coarse.ok());
    EXPECT_EQ(coarse.model.fidelity, Fidelity::Reduced);
    EXPECT_NE(coarse.model.settings["elementSize"], fe.model.settings["elementSize"]);
}

TEST(MechanicalOptimization, TuningTheBeamLengthMatchesEulerBernoulli)
{
    auto project = cantilever("t-tune");
    OptimizationProblem p = fromTemplate(*project, "mechanical.tune-frequency");
    EXPECT_EQ(p.modelId, "mechanical.analytical");
    p.algorithm.tolerance = 1e-12;
    p.algorithm.maxEvaluations = 120;
    auto ev = bindAdapter(*project, p.adapterId, p.adapterConfig);
    auto run = runOptimization(p, ev);
    ASSERT_TRUE(run.ok()) << run.message();
    const Candidate& b = best(run.value());
    const double target = p.objectives[0].target;
    // f ~ 1 / L^2: the length for 1.2 times the frequency is L0 / sqrt(1.2).
    const double length = b.point.values.at(objectBinding("t-tune-beam", "geometry/width"));
    EXPECT_NEAR(length / (0.5 / std::sqrt(1.2)), 1.0, 1e-4);
    EXPECT_NEAR(b.objectiveValues[0], target, 1e-3 * target);
    // Re-evaluated with the finite elements: close, and recorded as a different model.
    p.fillUnits(ev->capabilities());
    EvaluationService service(p, ev, {});
    const auto report = reevaluate(service, {b}, "mechanical.fe");
    ASSERT_TRUE(report.validated.front().evaluated());
    EXPECT_NEAR(report.validated.front().objectiveValues[0] / target, 1.0, 0.01);
    EXPECT_TRUE(isValidated(p, report.validated.front()));
}

TEST(MechanicalOptimization, ActuatorInverseDesignAgreesWithTheVibrationTargeting)
{
    auto project = plateWithActuators("t-plate");
    auto ev = bindAdapter(*project, "mechanical", {{"analyses", {"harmonic"}}});
    auto system = ev->linearSystem({}, "mechanical.fe");
    ASSERT_TRUE(system.ok()) << system.message();
    LinearInverseOptions o;
    o.regularization = 1e-6;  // solveTargets regularizes with (lambda sigma_max)^2: lambda = 1e-3 there
    auto solved = solveLinearInverse(system.value(), o);
    ASSERT_TRUE(solved.ok());
    // The mechanics module's own targeting on the same model.
    const mechanics::StructureStudy study = mechanics::prepareStudy(*project, Id("t-plate-plate"));
    std::vector<mechanics::TargetRequest> requests;
    for (const auto& t : study.targets)
        requests.push_back({t.label, t.point, t.suppress ? Complex(0.0, 0.0) : t.desired, t.weight});
    auto reference = mechanics::solveTargets(*study.model, mechanics::assemble(*study.model), study.forces, requests, 120.0,
                                             mechanics::mechanicsSettingsOf(*project).damping, 1e-3);
    ASSERT_TRUE(reference.ok()) << reference.message();
    for (size_t j = 0; j < reference.value().forces.size(); ++j)
        EXPECT_LT(std::abs(solved.value().weights(static_cast<Eigen::Index>(j)) - reference.value().forces[j]),
                  1e-8 * std::abs(reference.value().forces[j]));
    // The optimizer with the finite-element model reaches the same quality.
    OptimizationProblem p = fromTemplate(*project, "mechanical.suppress-vibration");
    p.algorithm.tolerance = 1e-12;
    auto run = runOptimization(p, bindAdapter(*project, p.adapterId, p.adapterConfig));
    ASSERT_TRUE(run.ok()) << run.message();
    const double start = run.value().candidates.front().objectiveValues[0];
    EXPECT_LT(best(run.value()).objectiveValues[0], 0.05 * start);
    // Two actuators, two complex targets: exactly determined, so both the
    // regularized inverse and the optimizer come close to zero error.
    EXPECT_LT(solved.value().relativeResidual, 0.05);
    EXPECT_LT(best(run.value()).objectiveValues[0], 0.05);
}

TEST(MechanicalOptimization, HypotheticalMaterialsAreLabelledAndOriginalsKept)
{
    auto project = cantilever("t-hypo");
    auto ev = bindAdapter(*project, "mechanical", nlohmann::json::object());
    const std::string scale = objectBinding("t-hypo-beam", "material/youngsModulusScale");
    const Evaluation stiff = ev->evaluate({{scale, 1.44}}, "mechanical.analytical");
    const Evaluation nominal = ev->evaluate({}, "mechanical.analytical");
    ASSERT_TRUE(stiff.ok()) << stiff.message;
    EXPECT_NEAR(quantity(stiff, "modal.f1") / quantity(nominal, "modal.f1"), 1.2, 1e-9);  // f ~ sqrt(E)
    bool labelled = false;
    for (const std::string& n : stiff.notes)
        labelled = labelled || n.find("HYPOTHETICAL") != std::string::npos;
    EXPECT_TRUE(labelled);
    EXPECT_EQ(project->materials.materials().size(), createExampleMaterialLibrary().materials().size());
}

// --- electromagnetic --------------------------------------------------------------------------------------------

TEST(EmOptimization, SteeringByPhasesMatchesPhaseSteering)
{
    auto project = emArray("t-steer", 8);
    OptimizationProblem p = fromTemplate(*project, "em.beam-steering");
    p.adapterConfig["directions"] = {{{"label", "target"}, {"thetaDeg", 30.0}, {"phiDeg", 0.0}}};
    p.algorithm.maxEvaluations = 900;
    p.algorithm.tolerance = 1e-10;
    auto ev = bindAdapter(*project, p.adapterId, p.adapterConfig);
    auto run = runOptimization(p, ev);
    ASSERT_TRUE(run.ok()) << run.message();
    const Candidate& b = best(run.value());
    // The analytical steering of the same array.
    em::ArrayModel model = em::arrayModelOf(std::dynamic_pointer_cast<const em::EmArrayComponent>(
                                                project->scene.find(Id("t-steer-array"))->state.component)
                                                ->settings);
    const Vec3 direction = em::directionOf(30.0 * kPi / 180.0, 0.0);
    em::steer(model, direction, 1e9, em::SteeringMethod::Phase);
    Bindings steered;
    for (size_t n = 0; n < model.elements.size(); ++n)
        steered[objectBinding("t-steer-array", "array/element/" + std::to_string(n) + "/phase")] = std::arg(model.elements[n].weight);
    const Evaluation reference = ev->evaluate(steered, "em.arrayfactor");
    EXPECT_NEAR(b.objectiveValues[0], quantity(reference, "pattern.gainDbi/target"), 0.02);
    EXPECT_LT(quantity(b.evaluation, "pattern.mainLobeErrorDeg"), 1.01);  // the sampling resolution
    const double k = 2.0 * kPi * 1e9 / 299792458.0, d = 0.5 * 299792458.0 / 1e9;
    for (size_t n = 1; n < 8; ++n) {
        const double step = b.point.values.at(objectBinding("t-steer-array", "array/element/" + std::to_string(n) + "/phase")) -
                            b.point.values.at(objectBinding("t-steer-array", "array/element/" + std::to_string(n - 1) + "/phase"));
        EXPECT_NEAR(std::remainder(step + k * d * std::sin(30.0 * kPi / 180.0), 2 * kPi), 0.0, 0.02) << n;
    }
}

TEST(EmOptimization, SidelobeMetricReproducesDolphChebyshevAndTheFrontTradesItAgainstDirectivity)
{
    auto project = emArray("t-sidelobe", 8);
    auto ev = bindAdapter(*project, "electromagnetic", {{"directions", {{{"label", "broadside"}, {"thetaDeg", 90.0}, {"phiDeg", 90.0}}}}});
    const Evaluation uniform = ev->evaluate({}, "em.arrayfactor");
    EXPECT_NEAR(quantity(uniform, "pattern.sidelobeDb"), -12.8, 0.4);
    const auto weights = em::chebyshevWeights(8, 30.0);
    Bindings cheb;
    for (size_t n = 0; n < weights.size(); ++n)
        cheb[objectBinding("t-sidelobe-array", "array/element/" + std::to_string(n) + "/amplitude")] = weights[n];
    const Evaluation chebyshev = ev->evaluate(cheb, "em.arrayfactor");
    EXPECT_NEAR(quantity(chebyshev, "pattern.sidelobeDb"), -30.0, 0.6);
    EXPECT_LT(quantity(chebyshev, "pattern.directivityDbi"), quantity(uniform, "pattern.directivityDbi"));
    OptimizationProblem p = fromTemplate(*project, "em.sidelobe-tradeoff");
    p.algorithm.maxEvaluations = 800;
    auto run = runOptimization(p, bindAdapter(*project, p.adapterId, p.adapterConfig));
    ASSERT_TRUE(run.ok()) << run.message();
    ASSERT_GE(run.value().pareto.size(), 5u);
    double lowest = 0.0;
    for (size_t i : run.value().pareto)
        lowest = std::min(lowest, run.value().candidates[i].objectiveValues[1]);
    EXPECT_LT(lowest, -20.0);
}

TEST(EmOptimization, ANullIsPlacedExactlyByTheLinearInverseAndCheckedByTheEvaluation)
{
    auto project = emArray("t-null", 8);
    const nlohmann::json configuration{{"directions",
                                        {{{"label", "target"}, {"thetaDeg", 20.0}, {"phiDeg", 0.0}, {"levelDb", 0.0}},
                                         {{"label", "null"}, {"thetaDeg", 55.0}, {"phiDeg", 0.0}, {"null", true}}}}};
    auto ev = bindAdapter(*project, "electromagnetic", configuration);
    auto system = ev->linearSystem({}, "em.arrayfactor");
    ASSERT_TRUE(system.ok()) << system.message();
    LinearInverseOptions o;
    o.method = LinearInverseOptions::Method::MinimumNorm;
    auto solved = solveLinearInverse(system.value(), o);
    ASSERT_TRUE(solved.ok());
    EXPECT_TRUE(solved.value().exact);
    const Evaluation e = ev->evaluate(weightBindings(system.value(), solved.value().weights), "em.arrayfactor");
    ASSERT_TRUE(e.ok()) << e.message;
    EXPECT_LT(quantity(e, "pattern.levelDb/null"), -100.0);
}

TEST(EmOptimization, DipoleLengthForZeroReactanceMatchesTheResonantLength)
{
    auto project = dipole("t-dipole");
    OptimizationProblem p = fromTemplate(*project, "em.dipole-reactance");
    auto run = runOptimization(p, bindAdapter(*project, p.adapterId, p.adapterConfig));
    ASSERT_TRUE(run.ok()) << run.message();
    const double found = best(run.value()).point.values.at(objectBinding("t-dipole-dipole", "geometry/length"));
    EXPECT_NEAR(found / em::resonantDipoleLength(0.001, 1e9), 1.0, 1e-4);
    EXPECT_LT(std::abs(quantity(best(run.value()).evaluation, "antenna.inputReactance")), 0.05);
}

TEST(EmOptimization, ExactDipoleFieldsAgreeWithTheArrayFactorFarAway)
{
    auto project = emArray("t-exact", 6, em::ElementPattern::HalfWaveDipole);
    auto ev = bindAdapter(*project, "electromagnetic", {{"directions", {{{"label", "x"}, {"thetaDeg", 60.0}, {"phiDeg", 0.0}}}}});
    const Evaluation af = ev->evaluate({}, "em.arrayfactor");
    const Evaluation exact = ev->evaluate({}, "em.dipoles.exact");
    ASSERT_TRUE(af.ok() && exact.ok()) << exact.message;
    EXPECT_EQ(exact.model.fidelity, Fidelity::Reduced);
    EXPECT_NEAR(quantity(exact, "pattern.directivityDbi"), quantity(af, "pattern.directivityDbi"), 0.3);
    EXPECT_NEAR(quantity(exact, "pattern.levelDb/x"), quantity(af, "pattern.levelDb/x"), 0.5);
    // Isotropic elements have no exact-field model.
    auto iso = bindAdapter(*emArray("t-iso", 4), "electromagnetic", nlohmann::json::object());
    EXPECT_FALSE(iso->capabilities().model("em.dipoles.exact")->available);
    EXPECT_EQ(iso->evaluate({}, "em.dipoles.exact").status, Evaluation::Status::Refused);
}

// --- every adapter -------------------------------------------------------------------------------------------------

TEST(OptimizationAdapters, CapabilitiesAreExplicitAndPromotionWritesOnlyTheBoundValues)
{
    auto acoustic = acousticArray("t-caps", 4);
    auto ev = bindAdapter(*acoustic, "acoustic", nlohmann::json::object());
    const Capabilities& c = ev->capabilities();
    EXPECT_EQ(c.domain, "acoustic");
    EXPECT_FALSE(c.limitations.empty());
    for (const QuantityInfo& q : c.quantities)
        EXPECT_FALSE(q.definition.empty()) << q.key;
    EXPECT_NE(c.variable(objectBinding("t-caps-array", "array/element/3/phase")), nullptr);
    EXPECT_EQ(c.variable(objectBinding("t-caps-array", "array/element/4/phase")), nullptr);
    // apply() changes the bound object and nothing else.
    auto copy = projectFromSnapshot(designSnapshot(*acoustic)).take();
    ASSERT_TRUE(ev->apply({{objectBinding("t-caps-array", "array/element/1/phase"), 0.7}}, *copy).ok());
    const auto* a = dynamic_cast<const acoustics::ArrayComponent*>(copy->scene.find(Id("t-caps-array"))->state.component.get());
    EXPECT_DOUBLE_EQ(*a->settings.overrides.at(1).phaseRad, 0.7);
    EXPECT_EQ(serializeProject(*copy)["scene"]["objects"][1], serializeProject(*acoustic)["scene"]["objects"][1]);
    // The snapshot key changes with the design and not with the save time.
    auto again = bindAdapter(*acoustic, "acoustic", nlohmann::json::object());
    EXPECT_EQ(again->snapshotKey(), ev->snapshotKey());
    acoustic->modifiedUtc = "2031-01-01T00:00:00Z";
    EXPECT_EQ(bindAdapter(*acoustic, "acoustic", nlohmann::json::object())->snapshotKey(), ev->snapshotKey());
    EXPECT_NE(bindAdapter(*copy, "acoustic", nlohmann::json::object())->snapshotKey(), ev->snapshotKey());
    // Every domain has adapters and templates.
    std::set<std::string> domains;
    for (const ProblemTemplate& t : AdapterRegistry::instance().templates())
        domains.insert(t.adapterId);
    EXPECT_EQ(domains, (std::set<std::string>{"acoustic", "mechanical", "electromagnetic"}));
}
