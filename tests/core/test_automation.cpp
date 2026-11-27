// The workflow engine (REQ-CHAT-20261126-010, RES-AUTO-001..009): workflow
// definitions, parameters and references, conditions, repetition,
// dependencies, failure handling, cancellation, history, jobs on snapshots,
// and every built-in template run with the real actions.
#include "automation/WorkflowEngine.h"
#include "coupling/WorkflowActions.h"
#include "optimization/Persistence.h"
#include "project/Project.h"
#include "project/ProjectSerializer.h"
#include "scene/Scene.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace resonant;
using namespace resonant::automation;
using nlohmann::json;

namespace {

Workflow parse(const json& j)
{
    auto w = workflowFromJson(j);
    EXPECT_TRUE(w.ok()) << w.message();
    return w.ok() ? w.take() : Workflow{};
}

json minimal(json steps, json parameters = json::array())
{
    return {{"format", "resonant.workflow"}, {"version", 1}, {"id", "test"}, {"name", "test"}, {"parameters", parameters}, {"steps", steps}};
}

std::filesystem::path outputDir(const std::string& name)
{
    const auto dir = std::filesystem::temp_directory_path() / ("resonant-workflow-" + name);
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

struct Cancelling final : JobContext {
    bool cancelRequested() const override { return cancel; }
    void reportProgress(double, std::string_view) override {}
    bool cancel = false;
};

}  // namespace

TEST(Automation, WorkflowsRoundTripAndInvalidOnesAreRefused)
{
    const json j = minimal({{{"id", "a"}, {"action", "scene.add"}, {"params", {{"type", "geometry"}}}},
                            {{"id", "b"}, {"action", "scene.set"}, {"params", {{"object", "${steps.a.id}"}}}, {"dependsOn", {"a"}}, {"when", "x > 1"}}},
                           {{{"id", "x"}, {"type", "number"}, {"default", 2.0}}});
    const Workflow w = parse(j);
    EXPECT_EQ(toJson(parse(toJson(w))), toJson(w));
    json duplicate = j;
    duplicate["steps"][1]["id"] = "a";
    EXPECT_FALSE(workflowFromJson(duplicate).ok());
    json forward = j;
    forward["steps"][0]["dependsOn"] = {"b"};
    EXPECT_FALSE(workflowFromJson(forward).ok());
    json badCondition = j;
    badCondition["steps"][1]["when"] = "x >";
    EXPECT_FALSE(workflowFromJson(badCondition).ok());
    json newer = j;
    newer["version"] = 9;
    EXPECT_FALSE(workflowFromJson(newer).ok());
    json unknownAction = j;
    unknownAction["steps"][0]["action"] = "no.such.action";
    EXPECT_FALSE(checkRequirements(parse(unknownAction)).ok());
}

TEST(Automation, ParametersAndReferencesResolveWithTheirTypes)
{
    const Workflow w = parse(minimal({{{"id", "a"}, {"action", "workflow.check"}, {"params", {{"value", 1}}}}},
                                     {{{"id", "f"}, {"type", "number"}, {"default", 40000}}, {{"id", "n"}, {"type", "integer"}, {"default", 8}}}));
    auto p = resolveParameters(w, {{"n", 12}});
    ASSERT_TRUE(p.ok());
    EXPECT_EQ(p.value()["f"], 40000);
    EXPECT_EQ(p.value()["n"], 12);
    EXPECT_FALSE(resolveParameters(w, {{"n", "twelve"}}).ok());
    EXPECT_FALSE(resolveParameters(w, {{"unknown", 1}}).ok());
    const json outputs{{"field", {{"maxPressure", 3.5}, {"maxPoint", {0.1, 0.2, 0.3}}}}};
    auto s = substitute({{"a", "${f}"}, {"b", "f = ${f} Hz"}, {"c", "${steps.field.maxPoint.2}"}, {"d", {"${n}", 1}}}, p.value(), outputs);
    ASSERT_TRUE(s.ok()) << s.message();
    EXPECT_EQ(s.value()["a"], 40000);  // the whole string: the value keeps its type
    EXPECT_EQ(s.value()["b"], "f = 40000 Hz");
    EXPECT_DOUBLE_EQ(s.value()["c"].get<double>(), 0.3);
    EXPECT_EQ(s.value()["d"][0], 12);
    EXPECT_FALSE(substitute(json("${missing}"), p.value(), outputs).ok());
}

TEST(Automation, ConditionsDependenciesRepetitionAndFailuresAreHonoured)
{
    auto project = Project::createNew("t");
    const Workflow w = parse(minimal(
        {{{"id", "box"}, {"action", "scene.add"}, {"params", {{"type", "geometry"}, {"id", "box-1"}, {"name", "Box"}}}},
         {{"id", "never"}, {"action", "scene.add"}, {"params", {{"type", "geometry"}}}, {"when", "size > 10"}},
         {{"id", "sizes"}, {"action", "scene.set"}, {"params", {{"object", "box-1"}, {"parameters", {{"width", "${size}"}}}}},
          {"repeat", {{"parameter", "size"}, {"values", {0.1, 0.2, 0.3}}}}},
         {{"id", "bad"}, {"action", "scene.set"}, {"params", {{"object", "nothing-here"}}}, {"continueOnError", true}},
         {{"id", "afterBad"}, {"action", "scene.add"}, {"params", {{"type", "geometry"}}}, {"dependsOn", {"bad"}}},
         {{"id", "stop"}, {"action", "workflow.check"}, {"params", {{"value", "${size}"}, {"max", 0.5}, {"message", "too large"}}}},
         {{"id", "last"}, {"action", "scene.add"}, {"params", {{"type", "geometry"}, {"name", "Last"}}}}},
        {{{"id", "size"}, {"type", "number"}, {"default", 1.0}}}));
    auto run = runWorkflow(*project, w);
    ASSERT_TRUE(run.ok()) << run.message();
    const WorkflowRun& r = run.value();
    EXPECT_EQ(r.status, RunStatus::Failed);  // "stop" fails, so "last" never runs
    EXPECT_NE(r.message.find("too large"), std::string::npos);
    std::map<std::string, StepStatus> status;
    int repeats = 0;
    for (const StepRecord& s : r.steps) {
        status[s.stepId] = s.status;
        repeats += s.stepId == "sizes" ? 1 : 0;
    }
    EXPECT_EQ(status["never"], StepStatus::Skipped);
    EXPECT_EQ(repeats, 3);
    EXPECT_EQ(r.outputs["sizes"].size(), 3u);
    EXPECT_EQ(status["bad"], StepStatus::Failed);
    EXPECT_EQ(status["afterBad"], StepStatus::Skipped);
    EXPECT_EQ(status["stop"], StepStatus::Failed);
    EXPECT_FALSE(status.contains("last"));
    EXPECT_DOUBLE_EQ(project->scene.find(Id("box-1"))->state.geometry->parameter("width"), 0.3);
    EXPECT_EQ(project->scene.objectCount(), 1u);
}

TEST(Automation, CancellationStopsBetweenSteps)
{
    auto project = Project::createNew("t");
    Cancelling context;
    context.cancel = true;
    auto run = runWorkflow(*project, parse(minimal({{{"id", "a"}, {"action", "scene.add"}, {"params", {{"type", "geometry"}}}}})), {}, &context);
    ASSERT_TRUE(run.ok());
    EXPECT_EQ(run.value().status, RunStatus::Cancelled);
    EXPECT_EQ(project->scene.objectCount(), 0u);
}

TEST(Automation, AJobWorksOnItsOwnSnapshotAndHistoryIsKept)
{
    auto project = Project::createNew("t");
    const json before = serializeProject(*project);
    const Workflow w = *WorkflowLibrary::instance().find("em.dipole-pattern");
    WorkflowJob job(before, w, json::object(), outputDir("job"));
    struct Context final : JobContext {
        bool cancelRequested() const override { return false; }
        void reportProgress(double, std::string_view) override {}
    } context;
    const JobEnd end = job.run(context);
    EXPECT_EQ(end.kind, JobEnd::Kind::Completed) << end.message;
    EXPECT_EQ(serializeProject(*project), before);  // the original project is untouched
    ASSERT_TRUE(job.result());
    EXPECT_FALSE(job.resultingProject()["scene"]["objects"].empty());
    project->extensions[kAutomationExtensionKey] = appendHistory(*project, *job.result(), 2);
    project->extensions[kAutomationExtensionKey] = appendHistory(*project, *job.result(), 2);
    project->extensions[kAutomationExtensionKey] = appendHistory(*project, *job.result(), 2);
    const auto history = workflowHistory(*project);
    ASSERT_EQ(history.size(), 2u);
    EXPECT_EQ(history.back().workflowId, "em.dipole-pattern");
    EXPECT_EQ(toJson(workflowRunFromJson(toJson(history.back()))), toJson(history.back()));
}

TEST(Automation, TheAcousticTemplateFocusesTheArrayWithRealFields)
{
    auto project = Project::createNew("t");
    const auto dir = outputDir("acoustic");
    auto run = runWorkflow(*project, *WorkflowLibrary::instance().find("acoustic.linear-array-focus"), {{"elements", 10}}, nullptr, dir);
    ASSERT_TRUE(run.ok()) << run.message();
    ASSERT_EQ(run.value().status, RunStatus::Completed) << run.value().message;
    const json& o = run.value().outputs;
    EXPECT_GT(o["focusedAtFocus"]["values"][0]["magnitude"].get<double>(), 2.0 * o["uniformAtFocus"]["values"][0]["magnitude"].get<double>());
    EXPECT_EQ(o["focused"]["model"], "acoustic.freefield.analytical");
    EXPECT_TRUE(std::filesystem::exists(dir / "linear-array-focus.html"));
    EXPECT_NE(project->scene.find(Id("wf-array")), nullptr);
}

TEST(Automation, TheMechanicalTemplateComparesDesignVariants)
{
    auto project = Project::createNew("t");
    const auto dir = outputDir("mechanical");
    auto run = runWorkflow(*project, *WorkflowLibrary::instance().find("mechanical.modal-and-forced"), {}, nullptr, dir);
    ASSERT_TRUE(run.ok()) << run.message();
    ASSERT_EQ(run.value().status, RunStatus::Completed) << run.value().message;
    const json& variants = run.value().outputs["variants"];
    ASSERT_EQ(variants.size(), 3u);
    // Bending frequencies of a beam grow in proportion to its thickness.
    const double f4 = variants[0]["frequencies"][0].get<double>(), f6 = variants[2]["frequencies"][0].get<double>();
    EXPECT_NEAR(f6 / f4, 6.0 / 4.0, 0.03);
    EXPECT_TRUE(std::filesystem::exists(dir / "cantilever-modes.csv"));
    EXPECT_FALSE(run.value().outputs["forced"]["peaks"].empty());
}

TEST(Automation, TheElectromagneticTemplatesReportPatternsAndLowerSidelobes)
{
    auto project = Project::createNew("t");
    const auto dir = outputDir("em");
    auto dipole = runWorkflow(*project, *WorkflowLibrary::instance().find("em.dipole-pattern"), {}, nullptr, dir);
    ASSERT_TRUE(dipole.ok()) << dipole.message();
    ASSERT_EQ(dipole.value().status, RunStatus::Completed) << dipole.value().message;
    EXPECT_NEAR(dipole.value().outputs["antenna"]["directivityDbi"].get<double>(), 2.15, 0.1);
    EXPECT_TRUE(std::filesystem::exists(dir / "dipole-pattern.csv"));
    auto array = runWorkflow(*project, *WorkflowLibrary::instance().find("em.array-steering"), {}, nullptr, dir);
    ASSERT_TRUE(array.ok()) << array.message();
    ASSERT_EQ(array.value().status, RunStatus::Completed) << array.value().message;
    const json& o = array.value().outputs;
    EXPECT_LT(o["tapered"]["sidelobeDb"].get<double>(), o["steered"]["sidelobeDb"].get<double>() - 10.0);
    EXPECT_NEAR(o["steered"]["mainPhiDeg"].get<double>(), 70.0, 1.0);
}

TEST(Automation, TheReflectorTemplateRunsAnOptimizationAndRecordsIt)
{
    auto project = Project::createNew("t");
    auto run = runWorkflow(*project, *WorkflowLibrary::instance().find("acoustic.reflector-optimization"), {{"evaluations", 15}}, nullptr,
                           outputDir("reflector"));
    ASSERT_TRUE(run.ok()) << run.message();
    ASSERT_EQ(run.value().status, RunStatus::Completed) << run.value().message;
    EXPECT_TRUE(run.value().outputs["optimize"].contains("best"));
    EXPECT_EQ(optimization::workspaceOf(*project).runs.size(), 1u);
}

TEST(Automation, SceneObjectsOfEveryTypeCanBeAddedAndChanged)
{
    auto project = Project::createNew("t");
    for (const std::string& type : coupling::sceneObjectTypes()) {
        auto id = coupling::addSceneObject(*project, {{"type", type}, {"name", "obj " + type}});
        EXPECT_TRUE(id.ok()) << type << ": " << id.message();
    }
    EXPECT_FALSE(coupling::addSceneObject(*project, {{"type", "spaceship"}}).ok());
    EXPECT_FALSE(coupling::addSceneObject(*project, {{"type", "emitter"}, {"component", {{"amplitudePa", -1.0}}}}).ok());
    ASSERT_TRUE(coupling::changeSceneObject(*project, "obj geometry", {{"parameters", {{"width", 0.5}}}, {"position", {1, 2, 3}}}).ok());
    const SceneObject* box = coupling::findSceneObject(*project, "obj geometry");
    EXPECT_DOUBLE_EQ(box->state.geometry->parameter("width"), 0.5);
    EXPECT_DOUBLE_EQ(box->state.local.position.z(), 3.0);
    EXPECT_FALSE(coupling::changeSceneObject(*project, "obj geometry", {{"parameters", {{"width", -1.0}}}}).ok());
    // The project still saves and loads with every type in it.
    EXPECT_TRUE(deserializeProject(serializeProject(*project)).ok());
}
