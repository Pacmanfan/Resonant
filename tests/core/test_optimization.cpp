// The physics-independent optimization framework (REQ-CHAT-20261126-009,
// RES-OPT-001..019, RES-OPT-025): design variables, objectives and
// constraints, algorithms on problems with known solutions, linear inverse
// design against closed forms, surrogates, sensitivities, robust
// statistics against known distributions, caching, parallel determinism,
// cancellation, checkpoint and restart, persistence, mesh checks and the
// topology-optimization proof of concept.
#include "geometry/Geometry.h"
#include "geometry/MeshChecks.h"
#include "mechanics/TopologyOptimization.h"
#include "optimization/Algorithms.h"
#include "optimization/Engine.h"
#include "optimization/Expression.h"
#include "optimization/LinearInverse.h"
#include "optimization/MultiFidelity.h"
#include "optimization/Robust.h"
#include "optimization/Persistence.h"
#include "optimization/Random.h"
#include "optimization/Sensitivity.h"
#include "optimization/Surrogate.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "simulation/Job.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <thread>

using namespace resonant;
using namespace resonant::optimization;

namespace {

using Bindings = std::map<std::string, double, std::less<>>;

// A synthetic adapter with closed-form "physics": every quantity is a known
// function of x0..x3 (bindings "x0".."x3") and of an optional perturbation
// "p". Two models: "analytic" and "analytic.hi" (the same plus 0.01, a
// stand-in for a more complete model).
class AnalyticEvaluator final : public DesignEvaluator {
public:
    explicit AnalyticEvaluator(std::string key = "snapshot-1", int delayMs = 0) : key_(std::move(key)), delayMs_(delayMs)
    {
        caps_.adapterId = "test.analytic";
        caps_.domain = "test";
        caps_.version = "1";
        for (int i = 0; i < 4; ++i) {
            DesignVariable v;
            v.id = v.binding = "x" + std::to_string(i);
            v.label = v.id;
            v.lower = -2.0;
            v.upper = 2.0;
            v.initial = -1.2;
            caps_.variables.push_back(v);
        }
        DesignVariable p;
        p.id = p.binding = "p";
        p.lower = -1.0;
        p.upper = 1.0;
        p.initial = 0.0;
        caps_.variables.push_back(p);
        for (const char* q : {"rosenbrock", "sphere", "shifted", "rastrigin", "linear", "kink", "zdt.f1", "zdt.f2", "sum", "x0value", "plusp"})
            caps_.quantities.push_back({q, q, "", "test function", {}, {}, false});
        caps_.models = {{"analytic", "Analytic", "1", Fidelity::Analytical, {"closed form"}, true, {}, 0.0},
                        {"analytic.hi", "Analytic, higher", "1", Fidelity::HighFidelity, {"closed form + 0.01"}, true, {}, 0.0}};
        caps_.constants = {{"two", 2.0}};
    }
    const Capabilities& capabilities() const override { return caps_; }
    const std::string& snapshotKey() const override { return key_; }
    Evaluation evaluate(const Bindings& b, const std::string& model, JobContext* context) const override
    {
        ++calls;
        if (delayMs_ > 0) {
            for (int i = 0; i < delayMs_; ++i) {
                if (context && context->cancelRequested()) {
                    Evaluation e;
                    e.status = Evaluation::Status::Cancelled;
                    return e;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        const auto v = [&](const char* k) {
            const auto it = b.find(k);
            return it == b.end() ? 0.0 : it->second;
        };
        const double x0 = v("x0"), x1 = v("x1"), x2 = v("x2"), x3 = v("x3"), p = v("p");
        Evaluation e;
        e.model.modelId = model;
        e.model.fidelity = model == "analytic.hi" ? Fidelity::HighFidelity : Fidelity::Analytical;
        const double shift = model == "analytic.hi" ? 0.01 : 0.0;
        if (x0 > 1.95 && x1 > 1.95) {  // an "invalid geometry" corner
            e.status = Evaluation::Status::InvalidDesign;
            e.message = "invalid corner";
            return e;
        }
        e.quantities["rosenbrock"] = 100.0 * std::pow(x1 - x0 * x0, 2) + std::pow(1.0 - x0, 2) + shift;
        e.quantities["sphere"] = x0 * x0 + x1 * x1 + x2 * x2 + x3 * x3 + shift;
        e.quantities["shifted"] = std::pow(x0 - 0.3, 2) + std::pow(x1 + 0.7, 2) + std::pow(x2 - 1.1, 2) + std::pow(x3 + 0.2, 2);
        e.quantities["rastrigin"] = 20.0 + x0 * x0 - 10.0 * std::cos(2 * std::numbers::pi * x0) + x1 * x1 - 10.0 * std::cos(2 * std::numbers::pi * x1);
        e.quantities["linear"] = 3.0 * x0 + 0.001 * x1;
        e.quantities["kink"] = std::abs(x0) + x1;
        // ZDT1 on x0, x1 mapped from [-2, 2] to [0, 1].
        const double z0 = (x0 + 2.0) / 4.0, z1 = (x1 + 2.0) / 4.0;
        const double g = 1.0 + 9.0 * z1;
        e.quantities["zdt.f1"] = z0;
        e.quantities["zdt.f2"] = g * (1.0 - std::sqrt(z0 / g));
        e.quantities["sum"] = x0 + x1;
        e.quantities["x0value"] = x0;
        e.quantities["plusp"] = x0 + p + shift;
        return e;
    }
    Status apply(const Bindings&, Project&) const override { return Status::success(); }
    mutable std::atomic<int> calls{0};

private:
    Capabilities caps_;
    std::string key_;
    int delayMs_ = 0;
};

OptimizationProblem problem(std::vector<std::string> variables, std::vector<Objective> objectives, std::string algorithm, int budget)
{
    OptimizationProblem p;
    p.id = "test";
    p.name = "test";
    p.adapterId = "test.analytic";
    p.modelId = "analytic";
    for (const std::string& id : variables) {
        DesignVariable v;
        v.id = v.binding = id;
        v.lower = -2.0;
        v.upper = 2.0;
        v.initial = -1.2;
        p.space.variables.push_back(v);
    }
    p.objectives = std::move(objectives);
    p.algorithm.algorithm = std::move(algorithm);
    p.algorithm.maxEvaluations = budget;
    p.algorithm.parallel = 2;
    return p;
}

Objective minimize(const std::string& quantity, double scale = 1.0)
{
    Objective o;
    o.id = quantity;
    o.quantity = quantity;
    o.sense = Sense::Minimize;
    o.scale = scale;
    return o;
}

double value(const Candidate& c, const std::string& id) { return c.point.values.at(id); }

struct CancelAfter final : JobContext {
    explicit CancelAfter(std::function<bool()> when) : test(std::move(when)) {}
    bool cancelRequested() const override { return test(); }
    void reportProgress(double, std::string_view) override {}
    std::function<bool()> test;
};

}  // namespace

// --- design space -----------------------------------------------------------------------------------

TEST(OptimizationDesign, ExpressionsParseEvaluateAndReportErrors)
{
    auto e = Expression::parse("0.5 * wavelength + max(a, 2) ^ 2 - sqrt(b/4)");
    ASSERT_TRUE(e.ok()) << e.message();
    EXPECT_EQ(e.value().names(), (std::vector<std::string>{"a", "b", "wavelength"}));
    auto v = e.value().evaluate({{"wavelength", 2.0}, {"a", 3.0}, {"b", 16.0}});
    ASSERT_TRUE(v.ok());
    EXPECT_DOUBLE_EQ(v.value(), 1.0 + 9.0 - 2.0);
    EXPECT_FALSE(e.value().evaluate({{"a", 1.0}}).ok());  // unknown names
    EXPECT_FALSE(Expression::parse("2 * (a + ").ok());
    EXPECT_FALSE(Expression::parse("foo(1)").ok());
    EXPECT_FALSE(Expression::parse("1/0").value().evaluate({}).ok());  // not finite
    EXPECT_NEAR(Expression::parse("-pi/2").value().evaluate({}).value(), -std::numbers::pi / 2, 1e-15);
    EXPECT_DOUBLE_EQ(Expression::parse("a > 1 && !(b == 2) || 0").value().evaluate({{"a", 2.0}, {"b", 3.0}}).value(), 1.0);
    EXPECT_DOUBLE_EQ(Expression::parse("a <= 1").value().evaluate({{"a", 2.0}}).value(), 0.0);
    EXPECT_DOUBLE_EQ(Expression::parse("a != 2").value().evaluate({{"a", 2.0}}).value(), 0.0);
    auto bracketed = Expression::parse("[object/a1/geometry/aperture] - 4 * [object/a1/geometry/focalLength]");
    ASSERT_TRUE(bracketed.ok()) << bracketed.message();
    EXPECT_EQ(bracketed.value().names().size(), 2u);
    EXPECT_DOUBLE_EQ(bracketed.value().evaluate({{"object/a1/geometry/aperture", 0.2}, {"object/a1/geometry/focalLength", 0.04}}).value(), 0.04);
}

TEST(OptimizationDesign, VariablesQuantizeAndValidate)
{
    DesignVariable v;
    v.id = "n";
    v.kind = VariableKind::Integer;
    v.lower = 2;
    v.upper = 9;
    v.initial = 4;
    EXPECT_TRUE(v.validate().ok());
    EXPECT_EQ(v.quantize(5.6), 6.0);
    EXPECT_EQ(v.quantize(100.0), 9.0);
    DesignVariable s;
    s.id = "s";
    s.lower = 0.0;
    s.upper = 1.0;
    s.step = 0.25;
    s.initial = 0.5;
    EXPECT_DOUBLE_EQ(s.quantize(0.6), 0.5);
    EXPECT_DOUBLE_EQ(s.quantize(0.9), 1.0);
    DesignVariable c;
    c.id = "c";
    c.kind = VariableKind::Categorical;
    c.choices = {"a", "b", "c"};
    c.initial = 1;
    EXPECT_TRUE(c.validate().ok());
    c.initial = 3;
    EXPECT_FALSE(c.validate().ok());
    DesignVariable bad;
    bad.id = "bad";
    bad.lower = 1.0;
    bad.upper = 0.0;
    EXPECT_FALSE(bad.validate().ok());
}

TEST(OptimizationDesign, UnitCubeMappingKeepsLockedDerivedAndDiscreteValues)
{
    DesignSpace space;
    DesignVariable a;
    a.id = "a";
    a.lower = 1.0;
    a.upper = 3.0;
    a.initial = 2.0;
    DesignVariable locked = a;
    locked.id = "locked";
    locked.locked = true;
    locked.initial = 2.5;
    DesignVariable n;
    n.id = "n";
    n.kind = VariableKind::Integer;
    n.lower = 1;
    n.upper = 4;
    n.initial = 2;
    DesignVariable m;
    m.id = "m";
    m.kind = VariableKind::Categorical;
    m.choices = {"steel", "aluminium"};
    m.initial = 0;
    space.variables = {a, locked, n, m};
    space.derived.push_back({"half", "half", "", "", "a / 2 + two"});
    ASSERT_TRUE(space.validate({{"two", 2.0}}).ok());
    EXPECT_EQ(space.dimension(), 3u);
    EXPECT_TRUE(space.hasDiscrete());
    auto p = space.fromUnit({1.0, 0.99, 0.6}, {{"two", 2.0}});
    ASSERT_TRUE(p.ok());
    EXPECT_DOUBLE_EQ(p.value().values.at("a"), 3.0);
    EXPECT_DOUBLE_EQ(p.value().values.at("locked"), 2.5);
    EXPECT_DOUBLE_EQ(p.value().values.at("n"), 4.0);
    EXPECT_DOUBLE_EQ(p.value().values.at("m"), 1.0);
    EXPECT_DOUBLE_EQ(p.value().values.at("half"), 3.5);
    const auto back = space.fromUnit(space.toUnit(p.value()), {{"two", 2.0}});
    EXPECT_EQ(back.value().values, p.value().values);
    EXPECT_TRUE(space.boundViolations(p.value()).empty());
    DesignPoint outside = p.value();
    outside.values["a"] = 7.0;
    EXPECT_FALSE(space.boundViolations(outside).empty());
    // A derived variable that refers to itself is refused.
    DesignSpace cyclic = space;
    cyclic.derived = {{"x", "x", "", "", "y + 1"}, {"y", "y", "", "", "x * 2"}};
    EXPECT_FALSE(cyclic.validate().ok());
}

TEST(OptimizationDesign, ObjectivesScoreAndConstraintsMeasureTheirViolation)
{
    Objective o = minimize("q", 2.0);
    EXPECT_DOUBLE_EQ(o.score(4.0), 2.0);
    o.sense = Sense::Maximize;
    EXPECT_DOUBLE_EQ(o.score(4.0), -2.0);
    o.sense = Sense::Target;
    o.target = 3.0;
    EXPECT_DOUBLE_EQ(o.score(4.0), 0.5);
    o.sense = Sense::AtMost;
    EXPECT_DOUBLE_EQ(o.score(2.0), 0.0);
    EXPECT_DOUBLE_EQ(o.score(5.0), 1.0);
    DesignConstraint c;
    c.id = "c";
    c.quantity = "q";
    c.relation = Relation::AtLeast;
    c.limit = 1.0;
    EXPECT_DOUBLE_EQ(c.violation(0.25), 0.75);
    EXPECT_DOUBLE_EQ(c.violation(2.0), 0.0);
    c.relation = Relation::Equal;
    c.tolerance = 0.1;
    EXPECT_NEAR(c.violation(1.3), 0.2, 1e-15);
}

TEST(OptimizationDesign, ProblemsRoundTripAndAreCheckedAgainstTheAdapter)
{
    AnalyticEvaluator ev;
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("rosenbrock")}, "nelder-mead", 100);
    DesignConstraint c;
    c.id = "relation";
    c.quantity = "expr:x0 - two * x1";
    c.limit = 0.0;
    p.constraints.push_back(c);
    ASSERT_TRUE(p.validateAgainst(ev.capabilities()).ok());
    const nlohmann::json j = toJson(p);
    auto back = problemFromJson(j);
    ASSERT_TRUE(back.ok()) << back.message();
    EXPECT_EQ(toJson(back.value()), j);
    OptimizationProblem unknown = p;
    unknown.objectives[0].quantity = "nonexistent";
    EXPECT_FALSE(unknown.validateAgainst(ev.capabilities()).ok());
    OptimizationProblem binding = p;
    binding.space.variables[0].binding = "elsewhere";
    EXPECT_FALSE(binding.validateAgainst(ev.capabilities()).ok());
    OptimizationProblem model = p;
    model.modelId = "nope";
    EXPECT_FALSE(model.validateAgainst(ev.capabilities()).ok());
    // Robust optimization never assumes a distribution: the source is required.
    OptimizationProblem robust = p;
    robust.robust.enabled = true;
    robust.robust.parameters.push_back({"p", "p", Distribution::Normal, 0.1, false, 3.0, ""});
    EXPECT_FALSE(robust.validate().ok());
    robust.robust.parameters[0].source = "test tolerance";
    EXPECT_TRUE(robust.validate().ok());
    auto newer = j;
    newer["version"] = 99;
    EXPECT_FALSE(problemFromJson(newer).ok());
}

// --- algorithms --------------------------------------------------------------------------------------

TEST(OptimizationAlgorithms, NelderMeadSolvesRosenbrock)
{
    auto ev = std::make_shared<AnalyticEvaluator>();
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("rosenbrock")}, "nelder-mead", 600);
    p.algorithm.tolerance = 1e-12;
    auto r = runOptimization(p, ev);
    ASSERT_TRUE(r.ok()) << r.message();
    ASSERT_TRUE(r.value().best.has_value());
    const Candidate& best = r.value().candidates[*r.value().best];
    EXPECT_NEAR(value(best, "x0"), 1.0, 2e-3);
    EXPECT_NEAR(value(best, "x1"), 1.0, 4e-3);
    EXPECT_LE(r.value().candidates.size(), 600u);
    EXPECT_FALSE(r.value().termination.reason.empty());
}

TEST(OptimizationAlgorithms, LbfgsWithFiniteDifferencesSolvesSmoothBoundedProblems)
{
    auto ev = std::make_shared<AnalyticEvaluator>();
    OptimizationProblem p = problem({"x0", "x1", "x2", "x3"}, {minimize("shifted")}, "lbfgs", 400);
    p.algorithm.tolerance = 1e-9;
    auto r = runOptimization(p, ev);
    ASSERT_TRUE(r.ok()) << r.message();
    const Candidate& best = r.value().candidates[*r.value().best];
    EXPECT_NEAR(value(best, "x0"), 0.3, 1e-4);
    EXPECT_NEAR(value(best, "x1"), -0.7, 1e-4);
    EXPECT_NEAR(value(best, "x2"), 1.1, 1e-4);
    EXPECT_NEAR(value(best, "x3"), -0.2, 1e-4);
    EXPECT_TRUE(r.value().termination.converged) << r.value().termination.reason;
    // The minimum of a linear function lies on the bounds.
    OptimizationProblem bound = problem({"x0", "x1"}, {minimize("linear")}, "lbfgs", 200);
    auto rb = runOptimization(bound, ev);
    const Candidate& b = rb.value().candidates[*rb.value().best];
    EXPECT_NEAR(value(b, "x0"), -2.0, 1e-9);
    EXPECT_NEAR(value(b, "x1"), -2.0, 1e-9);
    // A kink is reported: the gradient there is not reliable.
    OptimizationProblem kink = problem({"x0", "x1"}, {minimize("kink")}, "lbfgs", 200);
    kink.space.variables[0].initial = 0.0;
    auto rk = runOptimization(kink, ev);
    bool warned = false;
    for (const std::string& n : rk.value().notes)
        warned = warned || n.find("unreliable") != std::string::npos;
    EXPECT_TRUE(warned);
}

TEST(OptimizationAlgorithms, DifferentialEvolutionFindsTheGlobalMinimumOfAMultimodalFunction)
{
    auto ev = std::make_shared<AnalyticEvaluator>();
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("rastrigin")}, "differential-evolution", 1500);
    p.algorithm.seed = 3;
    auto r = runOptimization(p, ev);
    ASSERT_TRUE(r.ok()) << r.message();
    const Candidate& best = r.value().candidates[*r.value().best];
    EXPECT_NEAR(value(best, "x0"), 0.0, 0.02);
    EXPECT_NEAR(value(best, "x1"), 0.0, 0.02);
    // Nelder-Mead from the same start stops in a local minimum (it is local).
    OptimizationProblem local = problem({"x0", "x1"}, {minimize("rastrigin")}, "nelder-mead", 400);
    auto rl = runOptimization(local, ev);
    EXPECT_GT(rl.value().candidates[*rl.value().best].objectiveValues[0], 0.5);
}

TEST(OptimizationAlgorithms, Nsga2ApproximatesAKnownParetoFront)
{
    auto ev = std::make_shared<AnalyticEvaluator>();
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("zdt.f1"), minimize("zdt.f2")}, "nsga2", 2000);
    p.algorithm.populationSize = 40;
    auto r = runOptimization(p, ev);
    ASSERT_TRUE(r.ok()) << r.message();
    ASSERT_GE(r.value().pareto.size(), 15u);
    double deviation = 0.0;
    for (size_t i : r.value().pareto) {
        const Candidate& c = r.value().candidates[i];
        deviation += std::abs(c.objectiveValues[1] - (1.0 - std::sqrt(c.objectiveValues[0])));
        for (size_t j : r.value().pareto)
            EXPECT_FALSE(dominates(r.value().candidates[j], c));
    }
    EXPECT_LT(deviation / r.value().pareto.size(), 0.05);
    bool told = false;
    for (const std::string& n : r.value().notes)
        told = told || n.find("no single candidate is optimal") != std::string::npos;
    EXPECT_TRUE(told);
    // Priorities chosen after the run rank the front.
    const auto order = rankByPriorities(r.value().candidates, {1.0, 0.0});
    EXPECT_LE(r.value().candidates[order.front()].objectiveValues[0], 0.01);
}

TEST(OptimizationAlgorithms, BayesianOptimizationNeedsFewEvaluations)
{
    auto ev = std::make_shared<AnalyticEvaluator>();
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("shifted")}, "bayesian", 30);
    p.space.variables[0].initial = 1.5;
    p.algorithm.samples = 8;
    p.algorithm.tolerance = 1e-9;
    auto r = runOptimization(p, ev);
    ASSERT_TRUE(r.ok()) << r.message();
    const Candidate& best = r.value().candidates[*r.value().best];
    EXPECT_LT(best.objectiveValues[0], 1.25 + 0.05);  // x2 = x3 = 0, so the minimum is 1.1^2 + 0.2^2 = 1.25
    bool predicted = false;
    for (const Candidate& c : r.value().candidates)
        predicted = predicted || c.origin.find("a prediction, not a simulation") != std::string::npos;
    EXPECT_TRUE(predicted);
}

TEST(OptimizationAlgorithms, SamplingMethodsCoverTheSpaceAsDefined)
{
    auto ev = std::make_shared<AnalyticEvaluator>();
    OptimizationProblem sweep = problem({"x0", "x1"}, {minimize("sphere")}, "sweep", 1000);
    sweep.algorithm.pointsPerVariable = 5;
    auto rs = runOptimization(sweep, ev);
    ASSERT_EQ(rs.value().candidates.size(), 25u);
    const Candidate& best = rs.value().candidates[*rs.value().best];
    EXPECT_DOUBLE_EQ(value(best, "x0"), 0.0);
    EXPECT_DOUBLE_EQ(value(best, "x1"), 0.0);
    Random random(5);
    const auto lhs = latinHypercube(20, 3, random);
    for (int k = 0; k < 3; ++k) {
        std::vector<int> strata(20, 0);
        for (const auto& pnt : lhs)
            ++strata[static_cast<size_t>(std::floor(pnt[static_cast<size_t>(k)] * 20))];
        EXPECT_EQ(std::count(strata.begin(), strata.end(), 1), 20);
    }
    // The same seed gives the same samples.
    Random a(9), b(9);
    EXPECT_EQ(latinHypercube(10, 2, a), latinHypercube(10, 2, b));
}

TEST(OptimizationAlgorithms, DiscreteVariablesNeedASuitableMethod)
{
    auto ev = std::make_shared<AnalyticEvaluator>();
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("shifted")}, "nelder-mead", 200);
    p.space.variables[0].kind = VariableKind::Integer;
    p.space.variables[0].initial = -1;
    EXPECT_FALSE(runOptimization(p, ev).ok());
    p.algorithm.algorithm = "differential-evolution";
    p.algorithm.maxEvaluations = 600;
    auto r = runOptimization(p, ev);
    ASSERT_TRUE(r.ok()) << r.message();
    const Candidate& best = r.value().candidates[*r.value().best];
    EXPECT_DOUBLE_EQ(value(best, "x0"), 0.0);  // the nearest integer to 0.3
    EXPECT_NEAR(value(best, "x1"), -0.7, 0.02);
    const auto recommended = recommendAlgorithms(traitsOf(p, ev->capabilities()));
    EXPECT_EQ(recommended.front().algorithm, "differential-evolution");
}

TEST(OptimizationAlgorithms, HardConstraintsDecideFeasibilityAndReportTheirViolation)
{
    auto ev = std::make_shared<AnalyticEvaluator>();
    // minimize x0^2 + x1^2 subject to x0 + x1 >= 1: the optimum is (0.5, 0.5).
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("sphere")}, "nelder-mead", 800);
    p.space.variables[0].initial = 1.5;
    p.space.variables[1].initial = 1.0;
    DesignConstraint c;
    c.id = "sum";
    c.label = "x0 + x1";
    c.quantity = "sum";
    c.relation = Relation::AtLeast;
    c.limit = 1.0;
    c.scale = 0.01;
    p.constraints.push_back(c);
    p.algorithm.tolerance = 1e-12;
    auto r = runOptimization(p, ev);
    ASSERT_TRUE(r.ok()) << r.message();
    const Candidate& best = r.value().candidates[*r.value().best];
    EXPECT_TRUE(best.feasible);
    EXPECT_NEAR(value(best, "x0"), 0.5, 0.01);
    EXPECT_NEAR(value(best, "x1"), 0.5, 0.01);
    int infeasible = 0;
    for (const Candidate& cand : r.value().candidates)
        if (!cand.feasible && cand.evaluated()) {
            ++infeasible;
            EXPECT_GT(cand.constraints[0].violation, 0.0);
            EXPECT_FALSE(cand.failedConstraints(p).empty());
        }
    EXPECT_GT(infeasible, 0);
    // An invalid design is recorded as such and never treated as feasible.
    OptimizationProblem corner = problem({"x0", "x1"}, {minimize("sphere")}, "sweep", 100);
    corner.algorithm.pointsPerVariable = 2;
    auto rc = runOptimization(corner, ev);
    int invalid = 0;
    for (const Candidate& cand : rc.value().candidates)
        if (cand.evaluation.status == Evaluation::Status::InvalidDesign) {
            ++invalid;
            EXPECT_FALSE(cand.feasible);
        }
    EXPECT_EQ(invalid, 1);
}

// --- engine ---------------------------------------------------------------------------------------------

TEST(OptimizationEngine, IdenticalDependencyStatesAreReusedOthersAreNot)
{
    auto ev = std::make_shared<AnalyticEvaluator>("key-A");
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("sphere")}, "sweep", 100);
    p.algorithm.pointsPerVariable = 3;
    auto cache = std::make_shared<EvaluationCache>();
    RunOptions o;
    o.cache = cache;
    auto first = runOptimization(p, ev, o);
    const int calls = ev->calls.load();
    EXPECT_EQ(calls, 9);
    auto second = runOptimization(p, ev, o);
    EXPECT_EQ(ev->calls.load(), calls);  // every evaluation came from the cache
    EXPECT_TRUE(second.value().candidates.front().fromCache);
    EXPECT_EQ(second.value().cacheHits, 9);
    // A different snapshot (the design changed) is never served from the cache.
    auto other = std::make_shared<AnalyticEvaluator>("key-B");
    auto third = runOptimization(p, other, o);
    EXPECT_EQ(other->calls.load(), 9);
    EXPECT_FALSE(third.value().candidates.front().fromCache);
}

TEST(OptimizationEngine, ParallelEvaluationGivesTheSameCandidatesAsSequential)
{
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("rosenbrock")}, "differential-evolution", 300);
    p.algorithm.parallel = 1;
    auto a = runOptimization(p, std::make_shared<AnalyticEvaluator>());
    p.algorithm.parallel = 4;
    auto b = runOptimization(p, std::make_shared<AnalyticEvaluator>());
    ASSERT_EQ(a.value().candidates.size(), b.value().candidates.size());
    for (size_t i = 0; i < a.value().candidates.size(); ++i) {
        EXPECT_EQ(a.value().candidates[i].point, b.value().candidates[i].point);
        EXPECT_EQ(a.value().candidates[i].objectiveValues, b.value().candidates[i].objectiveValues);
    }
}

TEST(OptimizationEngine, CancellationStopsTheJobAndStoresOnlyCompleteCandidates)
{
    auto ev = std::make_shared<AnalyticEvaluator>("slow", 20);
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("sphere")}, "random", 500);
    p.algorithm.samples = 500;
    p.algorithm.parallel = 2;
    JobManager jobs(1);
    auto job = std::make_shared<OptimizationJob>(p, ev);
    const Id id = jobs.submit(job);
    while (job->candidates().size() < 4)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    jobs.cancel(id);
    const auto info = jobs.wait(id, std::chrono::seconds(20));
    ASSERT_TRUE(info);
    EXPECT_EQ(info->state, JobState::Cancelled);
    const std::optional<RunRecord> result = job->result();
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->cancelled);
    EXPECT_LT(result->candidates.size(), 500u);
    for (const Candidate& c : result->candidates)
        EXPECT_NE(c.evaluation.status, Evaluation::Status::Cancelled);
}

TEST(OptimizationEngine, CheckpointAndRestartReproduceTheUninterruptedRun)
{
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("rosenbrock")}, "nelder-mead", 160);
    p.algorithm.checkpointEvery = 10;
    p.algorithm.tolerance = 1e-14;
    auto reference = std::make_shared<AnalyticEvaluator>();
    auto full = runOptimization(p, reference);
    ASSERT_TRUE(full.ok());
    // Interrupted after 70 candidates, with checkpoints along the way.
    auto ev = std::make_shared<AnalyticEvaluator>();
    nlohmann::json checkpoint;
    RunOptions o;
    o.runId = "OPT-TEST";
    o.checkpoint = [&](const nlohmann::json& j) { checkpoint = j; };
    std::atomic<int> stored{0};
    o.onCandidate = [&](const Candidate&) { ++stored; };
    CancelAfter context([&] { return stored.load() >= 70; });
    auto interrupted = runOptimization(p, ev, o, &context);
    ASSERT_TRUE(interrupted.ok());
    EXPECT_TRUE(interrupted.value().cancelled);
    ASSERT_FALSE(checkpoint.is_null());
    EXPECT_FALSE(checkpoint.value("complete", true));
    // Restart from the checkpoint (as after a crash: written to disk and read back).
    const auto path = std::filesystem::temp_directory_path() / "resonant-optimization-checkpoint.json";
    std::ofstream(path) << checkpoint.dump();
    auto state = resumeFromCheckpoint(nlohmann::json::parse(std::ifstream(path)));
    std::filesystem::remove(path);
    ASSERT_TRUE(state.ok()) << state.message();
    RunOptions resume;
    resume.runId = state.value().runId;
    resume.cache = state.value().cache;
    resume.resumed = true;
    const int before = ev->calls.load();
    auto finished = runOptimization(state.value().problem, ev, resume);
    ASSERT_TRUE(finished.ok());
    ASSERT_EQ(finished.value().candidates.size(), full.value().candidates.size());
    for (size_t i = 0; i < full.value().candidates.size(); ++i) {
        EXPECT_EQ(finished.value().candidates[i].point, full.value().candidates[i].point) << i;
        EXPECT_EQ(finished.value().candidates[i].objectiveValues, full.value().candidates[i].objectiveValues);
    }
    EXPECT_TRUE(finished.value().candidates.front().replayed);
    // Only the designs after the checkpoint were evaluated again.
    EXPECT_LT(ev->calls.load() - before, reference->calls.load() - 50);
    EXPECT_LE(ev->calls.load(), reference->calls.load() + 10);
    EXPECT_TRUE(finished.value().resumed);
}

TEST(OptimizationEngine, RunRecordsRoundTripWithProvenance)
{
    auto ev = std::make_shared<AnalyticEvaluator>("snap");
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("sphere")}, "latin-hypercube", 12);
    p.algorithm.samples = 12;
    auto r = runOptimization(p, ev);
    const nlohmann::json j = toJson(r.value());
    auto back = runRecordFromJson(j);
    ASSERT_TRUE(back.ok()) << back.message();
    EXPECT_EQ(toJson(back.value()), j);
    for (const Candidate& c : back.value().candidates) {
        EXPECT_EQ(c.evaluation.model.modelId, "analytic");
        EXPECT_EQ(c.evaluation.model.snapshotKey, "snap");
        EXPECT_EQ(c.evaluation.model.adapterId, "test.analytic");
        EXPECT_FALSE(c.id.empty());
    }
    // CSV for scientific tools.
    const std::string csv = candidatesCsv(p, r.value().candidates);
    EXPECT_EQ(std::count(csv.begin(), csv.end(), '\n'), 13);
}

// --- linear inverse design -------------------------------------------------------------------------------

namespace {
LinearSystem randomSystem(int m, int n, unsigned seed)
{
    Random r(seed);
    LinearSystem s;
    s.response.resize(m, n);
    s.desired.resize(m);
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j)
            s.response(i, j) = Complex(r.normal(), r.normal());
        s.desired(i) = Complex(r.normal(), r.normal());
    }
    s.rowWeights = Eigen::VectorXd::Ones(m);
    s.model = "random";
    return s;
}
}  // namespace

TEST(OptimizationInverse, SquareSystemsAreSolvedExactly)
{
    const LinearSystem s = randomSystem(6, 6, 1);
    LinearInverseOptions o;
    o.regularization = 0.0;
    auto r = solveLinearInverse(s, o);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(r.value().exact);
    EXPECT_LT(r.value().relativeResidual, 1e-10);
    EXPECT_NE(r.value().classification.find("exact"), std::string::npos);
}

TEST(OptimizationInverse, OverdeterminedSystemsMatchTheNormalEquations)
{
    const LinearSystem s = randomSystem(12, 4, 2);
    LinearInverseOptions o;
    o.regularization = 0.0;
    auto r = solveLinearInverse(s, o);
    const Eigen::VectorXcd normal = (s.response.adjoint() * s.response).ldlt().solve(s.response.adjoint() * s.desired);
    EXPECT_LT((r.value().weights - normal).norm(), 1e-10 * normal.norm());
    EXPECT_FALSE(r.value().exact);
    EXPECT_NE(r.value().classification.find("approximate"), std::string::npos);
    // Tikhonov against its closed form (A^H A + lambda sigma_max^2 I)^-1 A^H b.
    o.regularization = 1e-2;
    auto t = solveLinearInverse(s, o);
    Eigen::JacobiSVD<Eigen::MatrixXcd> svd(s.response);
    const double sigma = svd.singularValues()(0);
    const Eigen::MatrixXcd lhs = s.response.adjoint() * s.response + 1e-2 * sigma * sigma * Eigen::MatrixXcd::Identity(4, 4);
    const Eigen::VectorXcd closed = lhs.ldlt().solve(s.response.adjoint() * s.desired);
    EXPECT_LT((t.value().weights - closed).norm(), 1e-10 * closed.norm());
}

TEST(OptimizationInverse, UnderdeterminedSystemsGiveTheMinimumNormSolution)
{
    const LinearSystem s = randomSystem(3, 8, 3);
    LinearInverseOptions o;
    o.method = LinearInverseOptions::Method::MinimumNorm;
    auto r = solveLinearInverse(s, o);
    const Eigen::VectorXcd pinv = s.response.completeOrthogonalDecomposition().pseudoInverse() * s.desired;
    EXPECT_LT((r.value().weights - pinv).norm(), 1e-10 * pinv.norm());
    EXPECT_TRUE(r.value().exact);
    EXPECT_NE(r.value().classification.find("smallest norm"), std::string::npos);
}

TEST(OptimizationInverse, BoundedWeightsSatisfyTheOptimalityConditions)
{
    LinearSystem s = randomSystem(10, 5, 4);
    LinearInverseOptions o;
    o.regularization = 1e-4;
    auto free = solveLinearInverse(s, o);
    const double bound = 0.5 * free.value().weights.cwiseAbs().maxCoeff();
    o.maxMagnitude = bound;
    auto r = solveLinearInverse(s, o);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(r.value().boundLimited);
    EXPECT_LE(r.value().weights.cwiseAbs().maxCoeff(), bound * (1.0 + 1e-9));
    // KKT: the projected gradient step is zero at the optimum of the convex problem.
    Eigen::JacobiSVD<Eigen::MatrixXcd> svd(s.response);
    const double lambda = 1e-4 * std::pow(svd.singularValues()(0), 2);
    const Eigen::VectorXcd w = r.value().weights;
    const Eigen::VectorXcd g = 2.0 * (s.response.adjoint() * (s.response * w - s.desired)) + 2.0 * lambda * w;
    Eigen::VectorXcd stepped = w - 1e-3 * g;
    for (Eigen::Index i = 0; i < stepped.size(); ++i)
        if (std::abs(stepped(i)) > bound)
            stepped(i) *= bound / std::abs(stepped(i));
    EXPECT_LT((stepped - w).norm(), 1e-6 * (1.0 + w.norm()));
    EXPECT_GT(r.value().relativeResidual, free.value().relativeResidual);
}

TEST(OptimizationInverse, SparseExcitationRecoversAPlantedSupport)
{
    LinearSystem s = randomSystem(20, 10, 5);
    Eigen::VectorXcd planted = Eigen::VectorXcd::Zero(10);
    planted(2) = Complex(1.0, 0.5);
    planted(7) = Complex(-0.8, 0.2);
    s.desired = s.response * planted;
    LinearInverseOptions o;
    o.regularization = 0.0;
    o.maxActive = 2;
    auto r = solveLinearInverse(s, o);
    EXPECT_EQ(r.value().activeSources, (std::vector<int>{2, 7}));
    EXPECT_LT((r.value().weights - planted).norm(), 1e-9);
    s.amplitudeBindings.assign(10, "");
    s.phaseBindings.assign(10, "");
    s.amplitudeBindings[2] = "a2";
    s.phaseBindings[2] = "p2";
    const auto bindings = weightBindings(s, r.value().weights);
    EXPECT_NEAR(bindings.at("a2"), std::abs(planted(2)), 1e-9);
    EXPECT_NEAR(bindings.at("p2"), std::arg(planted(2)), 1e-9);
}

// --- surrogates, sensitivity, robustness, fidelity ---------------------------------------------------------

TEST(OptimizationSurrogates, ModelsReproduceKnownFunctionsAndReportTheirErrors)
{
    std::vector<std::vector<double>> x;
    std::vector<double> y;
    Random r(11);
    for (const auto& p : latinHypercube(30, 2, r)) {
        x.push_back(p);
        y.push_back(1.0 + 2.0 * p[0] - p[1] + 3.0 * p[0] * p[0] + 0.5 * p[0] * p[1]);
    }
    auto q = makeSurrogate("quadratic");
    ASSERT_TRUE(q->fit(x, y).ok());
    EXPECT_NEAR(q->predict({0.3, 0.6}).mean, 1.0 + 0.6 - 0.6 + 0.27 + 0.09, 1e-9);
    EXPECT_FALSE(q->predict({0.3, 0.6}).hasUncertainty);
    auto gp = makeSurrogate("gaussian-process");
    ASSERT_TRUE(gp->fit(x, y).ok());
    const Prediction atData = gp->predict(x[3]);
    EXPECT_NEAR(atData.mean, y[3], 1e-2);
    EXPECT_TRUE(atData.hasUncertainty);
    EXPECT_GT(gp->predict({3.0, 3.0}).stddev, 10.0 * atData.stddev);  // far from the data it is uncertain
    auto rbf = makeSurrogate("rbf");
    ASSERT_TRUE(rbf->fit(x, y).ok());
    EXPECT_NEAR(rbf->predict(x[5]).mean, y[5], 1e-6);
    EXPECT_NEAR(expectedImprovement({0.0, 1.0, true}, 0.0), 1.0 / std::sqrt(2.0 * std::numbers::pi), 1e-12);
    EXPECT_DOUBLE_EQ(expectedImprovement({2.0, 0.0, false}, 1.0), 0.0);

    // Through the framework: trained on candidates, errors recorded.
    auto ev = std::make_shared<AnalyticEvaluator>();
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("sphere")}, "latin-hypercube", 25);
    p.algorithm.samples = 25;
    auto run = runOptimization(p, ev);
    auto study = fitSurrogate(p, run.value().candidates, 0, "quadratic");
    ASSERT_TRUE(study.ok()) << study.message();
    EXPECT_EQ(study.value().info.trainingPoints, 25u);
    EXPECT_LT(study.value().info.validationRmse, 1e-8);
    EXPECT_FALSE(study.value().info.notes.empty());
    Random pr(1);
    EXPECT_EQ(mostInformative(study.value(), uniformSamples(50, 2, pr), 3).size(), 3u);
}

TEST(OptimizationSensitivity, DerivativesRankingsAndReliabilityAreReported)
{
    auto ev = std::make_shared<AnalyticEvaluator>();
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("linear"), minimize("kink")}, "sweep", 10);
    p.fillUnits(ev->capabilities());
    ServiceOptions o;
    EvaluationService service(p, ev, o);
    DesignPoint at;
    at.values = {{"x0", 0.0}, {"x1", 0.5}};
    const SensitivityReport s = localSensitivity(service, at, 0.01);
    ASSERT_EQ(s.entries.size(), 4u);
    EXPECT_NEAR(s.entries[0].derivative, 3.0, 1e-9);   // d linear / d x0
    EXPECT_TRUE(s.entries[0].reliable);
    EXPECT_FALSE(s.entries[1].reliable);                // d kink / d x0 at the kink
    EXPECT_NEAR(s.entries[2].derivative, 0.001, 1e-9);  // d linear / d x1
    EXPECT_EQ(s.ranking.front(), "x0");
    EXPECT_TRUE(s.negligible.empty() || s.negligible.front() != "x0");
    EXPECT_NEAR(s.heatmap[0][0], 3.0 * 4.0, 1e-6);       // normalized by the range 4
    auto r = runOptimization(problem({"x0", "x1"}, {minimize("linear")}, "latin-hypercube", 40), ev);
    const auto corr = correlations(problem({"x0", "x1"}, {minimize("linear")}, "latin-hypercube", 40), r.value().candidates);
    EXPECT_GT(corr[0].pearson, 0.999);
    EXPECT_GT(corr[0].spearman, 0.99);
    const auto curves = responseCurves(service, at, 5);
    ASSERT_EQ(curves.size(), 4u);
    EXPECT_EQ(curves[0].x.size(), 5u);
    EXPECT_NEAR(curves[0].y.back(), 3.0 * 2.0 + 0.0005, 1e-12);
}

TEST(OptimizationRobust, StatisticsMatchKnownDistributions)
{
    auto ev = std::make_shared<AnalyticEvaluator>();
    OptimizationProblem p = problem({"x0"}, {minimize("plusp")}, "sweep", 10);
    p.robust.enabled = true;
    p.robust.samples = 200;
    p.robust.parameters.push_back({"p", "perturbation", Distribution::Uniform, 0.3, false, 3.0, "test tolerance +/- 0.3"});
    DesignPoint at;
    at.values = {{"x0", 0.5}};
    auto uniform = assessRobustness(p, ev, at);
    ASSERT_TRUE(uniform.ok()) << uniform.message();
    const RobustStatistics& u = *uniform.value().robust.robust;
    EXPECT_NEAR(u.mean[0], 0.5, 1e-3);
    EXPECT_NEAR(u.stddev[0], 0.3 / std::sqrt(3.0), 0.01);
    EXPECT_NEAR(u.maximum[0], 0.8, 0.01);
    EXPECT_NEAR(uniform.value().nominal.objectiveValues[0], 0.5, 1e-12);
    p.robust.parameters[0].distribution = Distribution::Normal;
    p.robust.parameters[0].width = 0.1;
    auto normal = assessRobustness(p, ev, at);
    EXPECT_NEAR(normal.value().robust.robust->stddev[0], 0.1, 0.008);
    // A hard constraint met in a known share of the samples.
    DesignConstraint c;
    c.id = "c";
    c.quantity = "plusp";
    c.relation = Relation::AtMost;
    c.limit = 0.5;
    p.constraints.push_back(c);
    p.robust.requiredProbability = 0.9;
    auto prob = assessRobustness(p, ev, at);
    EXPECT_NEAR(prob.value().robust.robust->constraintProbability[0], 0.5, 0.05);
    EXPECT_FALSE(prob.value().robust.feasible);
    EXPECT_TRUE(prob.value().nominal.feasible);  // nominal and robust are distinguished
}

TEST(OptimizationFidelity, ReevaluationCreatesLinkedCandidatesAndNeverMergesModels)
{
    auto ev = std::make_shared<AnalyticEvaluator>();
    OptimizationProblem p = problem({"x0", "x1"}, {minimize("sphere")}, "latin-hypercube", 10);
    p.validationModelId = "analytic.hi";
    auto run = runOptimization(p, ev);
    ServiceOptions o;
    p.fillUnits(ev->capabilities());
    EvaluationService service(p, ev, o);
    const auto chosen = selectForValidation(p, run.value().candidates, 3);
    ASSERT_EQ(chosen.size(), 3u);
    const MultiFidelityReport report = reevaluate(service, chosen, "analytic.hi");
    ASSERT_EQ(report.validated.size(), 3u);
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(report.validated[i].parents, std::vector<std::string>{chosen[i].id});
        EXPECT_EQ(report.validated[i].evaluation.model.fidelity, Fidelity::HighFidelity);
        EXPECT_NEAR(report.rows[i].difference[0], 0.01, 1e-12);
        EXPECT_TRUE(isValidated(p, report.validated[i]));
        EXPECT_FALSE(isValidated(p, chosen[i]));
    }
    EXPECT_NEAR(report.rankAgreement, 1.0, 1e-12);
    const DesignComparison cmp = compare(p, chosen[0], report.validated[0]);
    EXPECT_FALSE(cmp.sameModel);
    EXPECT_FALSE(cmp.notes.empty());
    // Pareto ranking never mixes models.
    std::vector<Candidate> mixed = run.value().candidates;
    mixed.insert(mixed.end(), report.validated.begin(), report.validated.end());
    for (size_t i : paretoFront(mixed, "analytic"))
        EXPECT_EQ(mixed[i].evaluation.model.modelId, "analytic");
}

// --- persistence -----------------------------------------------------------------------------------------------

TEST(OptimizationPersistence, WorkspacesRoundTripAndStayOutOfTheDesignSnapshot)
{
    auto project = Project::createNew("p");
    auto ev = std::make_shared<AnalyticEvaluator>();
    OptimizationProblem p = problem({"x0"}, {minimize("sphere")}, "sweep", 5);
    auto run = runOptimization(p, ev);
    OptimizationWorkspace w;
    w.problems.push_back(p);
    w.runs.push_back(run.value());
    DesignRevision rev;
    rev.id = "REV-1";
    rev.candidateId = run.value().candidates.front().id;
    rev.values = {{"x0", 1.0}};
    rev.previous = {{"x0", -1.2}};
    w.revisions.push_back(rev);
    const nlohmann::json before = designSnapshot(*project);
    storeWorkspace(*project, w);
    EXPECT_EQ(designSnapshot(*project), before);  // optimization data are not part of the design
    auto loaded = deserializeProject(serializeProject(*project));
    ASSERT_TRUE(loaded.ok());
    const OptimizationWorkspace back = workspaceOf(*loaded.value().project);
    ASSERT_EQ(back.problems.size(), 1u);
    ASSERT_EQ(back.runs.size(), 1u);
    EXPECT_EQ(back.runs[0].candidates.size(), run.value().candidates.size());
    EXPECT_EQ(back.revisions[0].previous.at("x0"), -1.2);
    nlohmann::json later = before;
    later["project"]["modifiedUtc"] = "2030-01-01T00:00:00Z";
    EXPECT_EQ(dependencyDocument(later), dependencyDocument(before));
}

// --- manufacturing checks and topology ---------------------------------------------------------------------------

TEST(OptimizationManufacturing, MeshChecksFindClosedSolidsHolesAndOverhangs)
{
    auto box = Geometry::create("box").take();
    ASSERT_TRUE(box.setParameter("width", 0.02).ok());
    ASSERT_TRUE(box.setParameter("depth", 0.03).ok());
    ASSERT_TRUE(box.setParameter("height", 0.04).ok());
    const TriangleMesh mesh = box.tessellate();
    const MeshIntegrity i = checkMesh(mesh, 1e-7);
    EXPECT_TRUE(i.closed);
    EXPECT_NEAR(i.volume, 0.02 * 0.03 * 0.04, 1e-9);
    EXPECT_NEAR(overhangFraction(mesh, Vec3::UnitZ(), 0.785), 0.0, 1e-12);
    const TriangleMesh plane = Geometry::create("plane").take().tessellate();
    EXPECT_FALSE(checkMesh(plane).closed);
    PrintabilityLimits limits;
    limits.minimumWall = 0.002;
    limits.buildVolume = Vec3(0.2, 0.2, 0.03);
    const PrintabilityReport r = checkPrintability(mesh, limits, 0.001);
    EXPECT_FALSE(r.printable());
    EXPECT_EQ(r.failures.size(), 2u);  // too thin a wall, too tall for the build volume
    const TriangleMesh sphere = Geometry::create("sphere").take().tessellate();
    EXPECT_GT(overhangFraction(sphere, Vec3::UnitZ(), 0.785), 0.05);
    const auto dir = std::filesystem::temp_directory_path() / "resonant-mesh-export";
    std::filesystem::create_directories(dir);
    EXPECT_TRUE(writeStl(mesh, dir / "box.stl").ok());
    EXPECT_TRUE(writeObj(mesh, dir / "box.obj", "candidate C1").ok());
    EXPECT_GT(std::filesystem::file_size(dir / "box.stl"), 500u);
    std::filesystem::remove_all(dir);
}

TEST(OptimizationTopology, TheFiniteElementModelMatchesBeamTheory)
{
    mechanics::TopologySettings s;
    s.nelx = 160;
    s.nely = 8;
    s.width = 0.4;  // slender: L / h = 20
    const std::vector<double> full(static_cast<size_t>(s.nelx * s.nely), 1.0);
    std::vector<double> u;
    auto c = mechanics::topologyCompliance(s, full, &u);
    ASSERT_TRUE(c.ok()) << c.message();
    // Tip deflection under P at the free end: P L^3 / (3 E I), I = t h^3 / 12.
    const double h = s.height(), inertia = s.thickness * h * h * h / 12.0;
    const double expected = s.load * std::pow(s.width, 3) / (3.0 * s.youngsModulus * inertia);
    const double tip = c.value() / s.load;  // compliance = P delta
    EXPECT_NEAR(tip / expected, 1.0, 0.06);
}

TEST(OptimizationTopology, SimpReducesComplianceMeetsTheVolumeAndIsCleanedForExport)
{
    mechanics::TopologySettings s;
    s.nelx = 48;
    s.nely = 24;
    s.maxIterations = 120;
    auto r = mechanics::optimizeTopology(s);
    ASSERT_TRUE(r.ok()) << r.message();
    const auto& t = r.value();
    EXPECT_LT(t.compliance.back(), 0.6 * t.initialCompliance);
    EXPECT_NEAR(t.volume.back(), s.volumeFraction, 2e-3);
    // The cantilever with a load at mid-height is symmetric about the mid-plane.
    double asymmetry = 0.0;
    for (int i = 0; i < s.nelx; ++i)
        for (int j = 0; j < s.nely; ++j)
            asymmetry = std::max(asymmetry, std::abs(t.density[static_cast<size_t>(i * s.nely + j)] -
                                                     t.density[static_cast<size_t>(i * s.nely + (s.nely - 1 - j))]));
    EXPECT_LT(asymmetry, 0.05);
    // Cleaned and re-analysed; the export is a closed solid of the cleaned volume.
    EXPECT_GT(t.cleanedCompliance, 0.0);
    EXPECT_FALSE(t.notes.empty());
    const TriangleMesh mesh = mechanics::extrudeTopology(t);
    const MeshIntegrity i = checkMesh(mesh, 1e-9);
    EXPECT_TRUE(i.closed);
    const double cell = s.elementSize();
    EXPECT_NEAR(i.volume, t.cleanedVolumeFraction * s.nelx * s.nely * cell * cell * s.thickness, 1e-12);
}

TEST(OptimizationTopology, AFilterRadiusInMetresKeepsTheResultMeshIndependent)
{
    mechanics::TopologySettings coarse;
    coarse.nelx = 40;
    coarse.nely = 20;
    coarse.filterRadius = 0.003;
    coarse.maxIterations = 150;
    mechanics::TopologySettings fine = coarse;
    fine.nelx = 80;
    fine.nely = 40;
    auto a = mechanics::optimizeTopology(coarse);
    auto b = mechanics::optimizeTopology(fine);
    ASSERT_TRUE(a.ok() && b.ok());
    EXPECT_NEAR(b.value().compliance.back() / a.value().compliance.back(), 1.0, 0.1);
    EXPECT_DOUBLE_EQ(a.value().minimumFeature, b.value().minimumFeature);
}
