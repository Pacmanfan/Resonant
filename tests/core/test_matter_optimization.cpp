// Inverse design of architectures (RES-FAM-014, -020): a thickness moves the
// frequency the expected way, a material choice changes the result, an
// architecture that cannot be made is identified (constrained or rejected), and
// two competing objectives (harvesting against the stiffness a load-bearing
// function needs) give a Pareto set whose trade-off is kept.
#include "MatterFixture.h"

#include "coupling/MatterAdapter.h"
#include "optimization/Engine.h"
#include "optimization/Persistence.h"

#include <gtest/gtest.h>

#include <algorithm>

using namespace matter_test;
using namespace resonant::optimization;

namespace {

std::shared_ptr<const DesignEvaluator> matterAdapter(const Project& p, const json& config)
{
    auto ev = resonant::coupling::bindMatterAdapter(designSnapshot(p), config);
    EXPECT_TRUE(ev.ok()) << ev.message();
    return ev ? ev.value() : nullptr;
}

}  // namespace

TEST(MatterOptimization, AThicknessVariableMovesTheFrequencyTheExpectedWay)
{
    auto p = example("b-energy-converter.resonant");
    auto ev = matterAdapter(*p, {{"architecture", "arch-bonded"}, {"stack", "st-beam"}, {"analyses", json::array({{{"analysis", "modes"}}})}});
    ASSERT_TRUE(ev);
    const std::string t = "matter/layer/st-beam/0/thickness";
    ASSERT_NE(ev->capabilities().variable(t), nullptr);
    ASSERT_NE(ev->capabilities().quantity("modes.f1"), nullptr);
    const Evaluation thin = ev->evaluate({{t, 4e-4}}, "matter.analyses");
    const Evaluation thick = ev->evaluate({{t, 8e-4}}, "matter.analyses");
    ASSERT_TRUE(thin.ok() && thick.ok()) << thin.message << thick.message;
    EXPECT_GT(thick.quantities.at("modes.f1"), thin.quantities.at("modes.f1"));
    EXPECT_NE(std::find(thin.warnings.begin(), thin.warnings.end(), "an optimized architecture is a prediction, not a validated design"), thin.warnings.end());
    const Evaluation bad = ev->evaluate({{"matter/layer/st-beam/9/thickness", 1e-4}}, "matter.analyses");
    EXPECT_EQ(bad.status, Evaluation::Status::InvalidDesign);
}

TEST(MatterOptimization, AMaterialChoiceChangesTheResult)
{
    auto p = example("b-energy-converter.resonant");
    auto ev = matterAdapter(*p, {{"architecture", "arch-bonded"},
                        {"stack", "st-beam"},
                        {"analyses", json::array({{{"analysis", "laminate"}}})},
                        {"materialChoices", {{"encapsulation", json::array({"mat-fam-parylene", "mat-fam-epoxy"})}}}});
    ASSERT_TRUE(ev);
    const std::string m = "matter/region/encapsulation/material";
    ASSERT_NE(ev->capabilities().variable(m), nullptr);
    EXPECT_EQ(ev->capabilities().variable(m)->kind, VariableKind::Categorical);
    const Evaluation parylene = ev->evaluate({{m, 0.0}}, "matter.analyses");
    const Evaluation epoxy = ev->evaluate({{m, 1.0}}, "matter.analyses");
    ASSERT_TRUE(parylene.ok() && epoxy.ok());
    EXPECT_NE(parylene.quantities.at("laminate.massPerArea"), epoxy.quantities.at("laminate.massPerArea"));
}

TEST(MatterOptimization, ArchitecturesThatCannotBeMadeAreIdentified)
{
    auto p = example("b-energy-converter.resonant");
    const json config{{"architecture", "arch-bonded"}, {"stack", "st-beam"}, {"analyses", json::array({{{"analysis", "modes"}}})}};
    const std::string t = "matter/layer/st-beam/0/thickness";
    auto constrained = matterAdapter(*p, config);
    ASSERT_TRUE(constrained);
    const Evaluation thin = constrained->evaluate({{t, 2e-4}}, "matter.analyses");  // below the printable 0.3 mm
    ASSERT_TRUE(thin.ok()) << thin.message;
    EXPECT_GE(thin.quantities.at("manufacturing.violations"), 1.0);
    EXPECT_DOUBLE_EQ(thin.quantities.at("manufacturing.feasible"), 0.0);
    json reject = config;
    reject["manufacturing"] = "reject";
    auto rejecting = matterAdapter(*p, reject);
    const Evaluation refused = rejecting->evaluate({{t, 2e-4}}, "matter.analyses");
    EXPECT_EQ(refused.status, Evaluation::Status::InvalidDesign);
    EXPECT_NE(refused.message.find("cannot be manufactured"), std::string::npos) << refused.message;
    EXPECT_TRUE(rejecting->evaluate({{t, 6e-4}}, "matter.analyses").ok());
}

TEST(MatterOptimization, CompetingObjectivesGiveAParetoSet)
{
    auto p = example("g-thickness-tradeoff.resonant");
    const OptimizationWorkspace w = resonant::optimization::workspaceOf(*p);
    ASSERT_EQ(w.problems.size(), 1u);
    OptimizationProblem problem = w.problems.front();
    problem.algorithm.maxEvaluations = 160;
    auto ev = matterAdapter(*p, problem.adapterConfig);
    ASSERT_TRUE(ev);
    ASSERT_TRUE(problem.validateAgainst(ev->capabilities()).ok()) << problem.validateAgainst(ev->capabilities()).message();
    auto run = runOptimization(problem, ev);
    ASSERT_TRUE(run.ok()) << run.message();
    const auto& r = run.value();
    ASSERT_GE(r.pareto.size(), 3u) << "the two objectives are not collapsed into one score";
    std::vector<std::pair<double, double>> front;
    for (size_t i : r.pareto)
        front.emplace_back(r.candidates[i].evaluation.quantities.at("laminate.bendingStiffness"), r.candidates[i].evaluation.quantities.at("harvester.power"));
    std::sort(front.begin(), front.end());
    for (size_t i = 1; i < front.size(); ++i)
        EXPECT_LE(front[i].second, front[i - 1].second) << "on the front, more stiffness costs power";
    EXPECT_LT(front.back().second, front.front().second);
    for (size_t i : r.pareto)
        EXPECT_DOUBLE_EQ(r.candidates[i].evaluation.quantities.at("manufacturing.violations"), 0.0);
}
