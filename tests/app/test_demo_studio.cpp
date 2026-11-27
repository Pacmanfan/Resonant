// Demo Studio in the real main window (REQ-CHAT-20261126-010, RES-DEMO-*):
// every bundled demonstration runs from a clean application state to
// completion with real results; a run is reproducible; presenter mode hides
// and restores the layout without touching the project; narration pauses wait
// for the presenter; Stop cancels the running simulation; a failing step
// fails the run (never shows an old result); a time limit fails a step;
// captures write the image and its annotated copy.
#include "DemoController.h"
#include "DemoStudioPanel.h"
#include "DocumentController.h"
#include "MainWindow.h"
#include "PresenterOverlay.h"
#include "PythonSession.h"
#include "project/ProjectSerializer.h"

#include <QDockWidget>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QMenuBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

#include <functional>

using namespace resonant::demo;

namespace {

bool waitUntil(const std::function<bool()>& condition, int ms = 120000)
{
    QElapsedTimer t;
    t.start();
    while (!condition()) {
        if (t.elapsed() > ms)
            return false;
        QTest::qWait(10);
    }
    return true;
}

class DemoStudioTest : public ::testing::Test {
protected:
    void SetUp() override { fresh(); }
    void TearDown() override { close(); }
    void fresh()
    {
        close();
        window_ = std::make_unique<MainWindow>();
        window_->unsavedChangesPrompt = [] { return QMessageBox::Discard; };
        window_->resize(1400, 900);
        window_->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_.get()));
        demos().setOutputDirectory(out_.path());
    }
    void close()
    {
        if (!window_)
            return;
        if (demos().isRunning())
            demos().stop();
        waitUntil([&] { return !demos().isRunning(); }, 30000);
        window_->pythonSession().stop();
        window_->close();
        window_.reset();
    }
    DemoController& demos() { return window_->demos(); }
    // Runs a profile unattended; returns the final status.
    QString runProfile(const QString& id)
    {
        QSignalSpy finished(&demos(), &DemoController::runFinished);
        if (!demos().startProfile(id, false))
            return QStringLiteral("not started");
        if (!waitUntil([&] { return finished.count() == 1; }, 600000))
            return QStringLiteral("timed out");
        return finished.front().at(0).toString();
    }
    nlohmann::json stepResult(const std::string& step) const
    {
        for (const StepRun& s : window_->demos().lastRun().steps)
            if (s.id == step)
                return s.result;
        return {};
    }
    QTemporaryDir out_;
    std::unique_ptr<MainWindow> window_;
};

DemoSequence sequenceOf(const nlohmann::json& steps)
{
    auto s = sequenceFromJson({{"format", kSequenceFormat}, {"version", 1}, {"id", "test"}, {"steps", steps}});
    EXPECT_TRUE(s.ok()) << s.message();
    return s.take();
}

}  // namespace

TEST_F(DemoStudioTest, EveryDemonstrationRunsFromACleanStateWithRealResults)
{
    ASSERT_EQ(window_->demoStudioPanel().profileIds().size(), 8);
    for (const QString& id : window_->demoStudioPanel().profileIds()) {
        SCOPED_TRACE(id.toStdString());
        fresh();  // a clean application for each demonstration
        if (id == QLatin1String("demo-h-python-automation") && !window_->startPython()) {
            std::printf("[ SKIPPED  ] %s: Python is not available\n", qPrintable(id));
            continue;
        }
        const QString status = runProfile(id);
        EXPECT_EQ(status, QStringLiteral("completed")) << demos().lastRun().message;
        for (const StepRun& s : demos().lastRun().steps)
            EXPECT_EQ(s.outcome, StepOutcome::Done) << s.id << ": " << s.message;
        EXPECT_TRUE(QFile::exists(demos().manifestPath()));
        EXPECT_EQ(demos().lastRun().projectSha256.size(), 64u);
    }
}

TEST_F(DemoStudioTest, ARunIsReproducibleAndRecordedInItsManifest)
{
    ASSERT_EQ(runProfile(QStringLiteral("demo-b-acoustic-focusing")), QStringLiteral("completed"));
    const nlohmann::json uniform = stepResult("uniform"), focused = stepResult("focused");
    ASSERT_FALSE(uniform.empty());
    EXPECT_NE(uniform["summary"], focused["summary"]) << "focusing must change the calculated field";
    fresh();
    ASSERT_EQ(runProfile(QStringLiteral("demo-b-acoustic-focusing")), QStringLiteral("completed"));
    // The displayed-result label counts runs of this window and the solver time is measured;
    // everything calculated is identical.
    auto calculated = [](nlohmann::json s) {
        nlohmann::json out = nlohmann::json::array();
        for (const auto& line : s["summary"])
            if (line.get<std::string>().rfind("Displayed result", 0) != 0 && line.get<std::string>().rfind("Solver time", 0) != 0)
                out.push_back(line);
        return out;
    };
    EXPECT_EQ(calculated(stepResult("uniform")), calculated(uniform));
    EXPECT_EQ(calculated(stepResult("focused")), calculated(focused));
    QFile manifest(demos().manifestPath());
    ASSERT_TRUE(manifest.open(QIODevice::ReadOnly));
    const auto j = nlohmann::json::parse(manifest.readAll().toStdString());
    EXPECT_EQ(j["profile"], "demo-b-acoustic-focusing");
    EXPECT_EQ(j["status"], "completed");
    EXPECT_EQ(j["project"]["file"], "examples/acoustics/b-linear-array.resonant");
    EXPECT_TRUE(j["simulation"].contains("acoustics") || j["simulation"].is_object());
    EXPECT_FALSE(j["notes"].empty());
}

TEST_F(DemoStudioTest, PresenterModeHidesAndRestoresTheLayoutAndKeepsTheProject)
{
    ASSERT_TRUE(window_->openProjectFile(QStringLiteral(RESONANT_SOURCE_DIR "/examples/acoustics/b-linear-array.resonant")).ok());
    const nlohmann::json project = resonant::serializeProject(window_->controller().project());
    QDockWidget* scene = window_->findChild<QDockWidget*>(QStringLiteral("sceneDock"));
    ASSERT_TRUE(scene->isVisible());
    window_->setPresenterMode(true);
    QTest::qWait(20);
    EXPECT_FALSE(scene->isVisible());
    EXPECT_FALSE(window_->menuBar()->isVisible());
    EXPECT_TRUE(window_->findChild<QDockWidget*>(QStringLiteral("presenterDock"))->isVisible());
    window_->setPresenterMode(false);
    QTest::qWait(20);
    EXPECT_TRUE(scene->isVisible());
    EXPECT_TRUE(window_->menuBar()->isVisible());
    EXPECT_FALSE(window_->findChild<QDockWidget*>(QStringLiteral("presenterDock"))->isVisible());
    EXPECT_EQ(resonant::serializeProject(window_->controller().project())["scene"], project["scene"]);
}

TEST_F(DemoStudioTest, NarrationPausesWaitForThePresenterAndRestartRunsAgain)
{
    window_->setPresenterMode(true);
    const DemoSequence s = sequenceOf({{{"id", "open"}, {"action", "open-project"}, {"description", "Open"}, {"params", {{"path", "examples/acoustics/b-linear-array.resonant"}}}},
                                       {{"id", "say"}, {"action", "pause"}, {"description", "Explain the array"}, {"notes", "16 elements"}},
                                       {{"id", "select"}, {"action", "select"}, {"params", {{"object", "ex-b-array"}}}}});
    QSignalSpy waiting(&demos(), &DemoController::waitingForPresenter);
    QSignalSpy finished(&demos(), &DemoController::runFinished);
    ASSERT_TRUE(demos().start(s, nullptr, true));
    ASSERT_TRUE(waitUntil([&] { return waiting.count() == 1; }));
    EXPECT_TRUE(demos().isWaitingForPresenter());
    QTest::qWait(200);  // it keeps waiting: there is no time-out on a narration pause
    EXPECT_TRUE(demos().isWaitingForPresenter());
    EXPECT_EQ(finished.count(), 0);
    EXPECT_TRUE(window_->presenterOverlay().stepText().contains(QStringLiteral("Step")));
    demos().next();
    ASSERT_TRUE(waitUntil([&] { return finished.count() == 1; }));
    EXPECT_EQ(finished.front().at(0).toString(), QStringLiteral("completed"));
    EXPECT_EQ(window_->controller().primarySelection(), resonant::Id("ex-b-array"));
    ASSERT_TRUE(demos().restart());
    ASSERT_TRUE(waitUntil([&] { return waiting.count() == 2; }));
    demos().stop();
    ASSERT_TRUE(waitUntil([&] { return finished.count() == 2; }));
    EXPECT_EQ(finished.back().at(0).toString(), QStringLiteral("cancelled"));
    window_->setPresenterMode(false);
}

TEST_F(DemoStudioTest, StopCancelsTheRunningSimulation)
{
    QSignalSpy started(&demos(), &DemoController::stepStarted);
    QSignalSpy finished(&demos(), &DemoController::runFinished);
    ASSERT_TRUE(demos().startProfile(QStringLiteral("demo-f-inverse-design")));
    // Stop while the optimization runs.
    ASSERT_TRUE(waitUntil([&] { return !started.isEmpty() && demos().currentStep() >= 0 && demos().sequence().steps[static_cast<size_t>(demos().currentStep())].id == "run"; }));
    QTest::qWait(100);
    demos().stop();
    ASSERT_TRUE(waitUntil([&] { return finished.count() == 1; }));
    EXPECT_EQ(finished.front().at(0).toString(), QStringLiteral("cancelled"));
    const auto& steps = demos().lastRun().steps;
    auto promote = std::find_if(steps.begin(), steps.end(), [](const StepRun& r) { return r.id == "promote"; });
    ASSERT_NE(promote, steps.end());
    EXPECT_EQ(promote->outcome, StepOutcome::Skipped);
    EXPECT_TRUE(waitUntil([&] { return !window_->controller().jobs().hasUnfinishedJobs(); }, 20000)) << "the cancelled optimization job ends";
}

TEST_F(DemoStudioTest, AFailingSimulationFailsItsStepAndATimeLimitIsEnforced)
{
    // The full-wave solver refuses a region with no emitter in its plane: the step fails with
    // the solver's reason, and the steps after it are skipped (nothing old is shown as new).
    const DemoSequence s = sequenceOf({{{"id", "open"}, {"action", "open-project"}, {"params", {{"path", "examples/foundation/primitives.resonant"}}}},
                                       {{"id", "plane"}, {"action", "project-action"}, {"params", {{"id", "scene.add"}, {"params", {{"type", "region"}, {"id", "r"}, {"name", "Plane"}}}}}},
                                       {{"id", "field"}, {"action", "run"}, {"params", {{"what", "full-wave"}, {"region", "r"}}}},
                                       {{"id", "never"}, {"action", "select"}, {"params", {{"object", "r"}}}}});
    QSignalSpy finished(&demos(), &DemoController::runFinished);
    ASSERT_TRUE(demos().start(s));
    ASSERT_TRUE(waitUntil([&] { return finished.count() == 1; }));
    EXPECT_EQ(finished.front().at(0).toString(), QStringLiteral("failed"));
    EXPECT_EQ(demos().lastRun().steps[2].outcome, StepOutcome::Failed);
    EXPECT_NE(demos().lastRun().steps[2].message.find("refused"), std::string::npos) << demos().lastRun().steps[2].message;
    EXPECT_EQ(demos().lastRun().steps[3].outcome, StepOutcome::Skipped);
    // A time limit shorter than the optimization: the step fails and the run is cancelled.
    const DemoSequence slow = sequenceOf({{{"id", "open"}, {"action", "open-project"}, {"params", {{"path", "examples/optimization/a-acoustic-focal-array.resonant"}, {"copyToRunDirectory", true}}}},
                                          {{"id", "run"}, {"action", "run"}, {"params", {{"what", "optimization"}}}, {"timeoutSeconds", 0.2}}});
    ASSERT_TRUE(demos().start(slow));
    ASSERT_TRUE(waitUntil([&] { return finished.count() == 2; }));
    EXPECT_EQ(finished.back().at(0).toString(), QStringLiteral("failed"));
    EXPECT_NE(demos().lastRun().steps[1].message.find("timed out"), std::string::npos) << demos().lastRun().steps[1].message;
    EXPECT_TRUE(waitUntil([&] { return !window_->controller().jobs().hasUnfinishedJobs(); }, 30000));
}

TEST_F(DemoStudioTest, ACaptureWritesTheImageAndItsAnnotatedCopy)
{
    const DemoSequence s = sequenceOf({{{"id", "open"}, {"action", "open-project"}, {"params", {{"path", "examples/acoustics/c-focused-planar-array.resonant"}}}},
                                       {{"id", "field"}, {"action", "run"}, {"params", {{"what", "acoustics.field"}, {"region", "ex-c-region"}}}},
                                       {{"id", "shot"}, {"action", "capture"},
                                        {"params", {{"name", "test-shot.png"}, {"target", "window"}, {"annotations", {{{"x", 0.5}, {"y", 0.5}, {"text", "Viewport"}}}}}}}});
    QSignalSpy finished(&demos(), &DemoController::runFinished);
    ASSERT_TRUE(demos().start(s));
    ASSERT_TRUE(waitUntil([&] { return finished.count() == 1; }));
    ASSERT_EQ(finished.front().at(0).toString(), QStringLiteral("completed")) << demos().lastRun().message;
    const QString dir = QFileInfo(demos().manifestPath()).absolutePath();
    const QImage shot(dir + QStringLiteral("/test-shot.png")), annotated(dir + QStringLiteral("/test-shot-annotated.png"));
    ASSERT_FALSE(shot.isNull());
    ASSERT_FALSE(annotated.isNull());
    EXPECT_EQ(shot.size(), window_->size());
    EXPECT_NE(shot.convertToFormat(QImage::Format_ARGB32), annotated.convertToFormat(QImage::Format_ARGB32));
    EXPECT_EQ(demos().lastRun().screenshots, (std::vector<std::string>{"test-shot.png", "test-shot-annotated.png"}));
}
