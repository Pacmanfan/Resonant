// The Optimization Studio driven through the real main window
// (REQ-CHAT-20261126-009, RES-OPT-018, RES-OPT-019, RES-OPT-023): templates,
// background runs with live candidates, cancellation and resume from a
// checkpoint, the candidate browser, re-evaluation with another model,
// inverse design, sensitivity, promotion as an undoable revision that leaves
// the design untouched until then, export, and the topology proof of
// concept.
#include "DocumentController.h"
#include "MainWindow.h"
#include "OptimizationController.h"
#include "OptimizationPanel.h"

#include "acoustics/Components.h"
#include "optimization/Persistence.h"
#include "project/ProjectSerializer.h"

#include <QAction>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace resonant;
using namespace resonant::optimization;

namespace {

class OptimizationUiTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window_ = std::make_unique<MainWindow>();
        window_->unsavedChangesPrompt = [] { return QMessageBox::Discard; };
        window_->resize(1400, 900);
        window_->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_.get()));
        QTest::qWait(30);
    }
    void TearDown() override
    {
        optimization().stop();
        optimization().waitUntilIdle(20000);
        window_->close();
        window_.reset();
    }
    OptimizationController& optimization() { return window_->optimization(); }
    OptimizationPanel& panel() { return window_->optimizationPanel(); }
    void openExample(const char* file, bool save = false)
    {
        const auto source = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "optimization" / file;
        ASSERT_TRUE(window_->openProjectFile(QString::fromStdString(source.string())).ok());
        if (save)
            ASSERT_TRUE(window_->saveProjectToFile(dir_.filePath(QString::fromLatin1(file))).ok());
    }
    nlohmann::json design() { return designSnapshot(window_->controller().project())["scene"]; }
    std::unique_ptr<MainWindow> window_;
    QTemporaryDir dir_;
};

}  // namespace

TEST_F(OptimizationUiTest, TheStudioIsADockWithAMenuAndOffersTemplatesOfEveryDomain)
{
    QAction* show = window_->findChild<QAction*>(QStringLiteral("actionShowOptimization"));
    ASSERT_NE(show, nullptr);
    show->trigger();
    EXPECT_TRUE(panel().isVisible());
    std::set<std::string> domains;
    for (const ProblemTemplate* t : optimization().templates())
        domains.insert(t->adapterId);
    EXPECT_EQ(domains.size(), 3u);
    // A template that needs something the project lacks says so.
    EXPECT_FALSE(panel().addFromTemplate("acoustic.array-phase-focus"));
    EXPECT_FALSE(panel().statusText().isEmpty());
}

TEST_F(OptimizationUiTest, ARunIsABackgroundJobAndTheDesignIsUnchangedUntilACandidateIsPromoted)
{
    openExample("a-acoustic-focal-array.resonant");
    const nlohmann::json before = design();
    const std::string problem = optimization().workspace().problems.front().id;
    panel().selectProblem(problem);
    ASSERT_TRUE(panel().start()) << panel().statusText().toStdString();
    EXPECT_TRUE(optimization().running());
    QSignalSpy finished(&optimization(), &OptimizationController::runFinished);
    ASSERT_TRUE(finished.wait(120000));
    ASSERT_FALSE(optimization().workspace().runs.empty());
    const RunRecord run = optimization().workspace().runs.back();
    EXPECT_TRUE(run.complete);
    EXPECT_GT(panel().candidateRows(), 10);
    EXPECT_EQ(design(), before);  // nothing was written into the design
    ASSERT_TRUE(run.best.has_value());
    // Promotion writes the best candidate as one undoable step, recorded as a revision.
    panel().selectRun(run.runId);
    panel().selectCandidateRow(static_cast<int>(*run.best));
    EXPECT_NE(panel().detailsText().toStdString().find("acoustic.freefield.analytical"), std::string::npos);
    ASSERT_TRUE(panel().promoteSelected()) << panel().statusText().toStdString();
    EXPECT_NE(design(), before);
    const auto revisions = optimization().workspace().revisions;
    ASSERT_EQ(revisions.size(), 1u);
    EXPECT_EQ(revisions.front().candidateId, run.candidates[*run.best].id);
    EXPECT_FALSE(revisions.front().validated);  // the free-field model is not a validation
    window_->controller().undo();
    EXPECT_EQ(design(), before);
    EXPECT_TRUE(optimization().workspace().revisions.empty());
}

TEST_F(OptimizationUiTest, StoppingKeepsACheckpointAndResumingFinishesTheRun)
{
    openExample("d-mechanical-resonator.resonant", true);
    const std::string problem = optimization().workspace().problems.front().id;
    panel().selectProblem(problem);
    QSignalSpy changed(&optimization(), &OptimizationController::candidatesChanged);
    ASSERT_TRUE(panel().start());
    const std::string runId = optimization().liveRunId();
    // Stop once some candidates exist (live candidates are visible meanwhile).
    for (int i = 0; i < 400 && optimization().liveCandidates().size() < 30; ++i)
        QTest::qWait(5);
    optimization().stop();
    QSignalSpy finished(&optimization(), &OptimizationController::runFinished);
    ASSERT_TRUE(!optimization().running() || finished.wait(30000));
    const OptimizationWorkspace w = optimization().workspace();
    const RunRecord* stopped = w.findRun(runId);
    ASSERT_NE(stopped, nullptr);
    if (stopped->complete)
        GTEST_SKIP() << "the run finished before it could be stopped";
    EXPECT_TRUE(stopped->cancelled);
    EXPECT_TRUE(optimization().canResume(runId));
    // The checkpoint is a file next to the saved project.
    const auto checkpoint = std::filesystem::path(dir_.path().toStdString()) / "d-mechanical-resonator.optimization" / (runId + ".checkpoint.json");
    EXPECT_TRUE(std::filesystem::exists(checkpoint)) << checkpoint;
    ASSERT_TRUE(optimization().resume(runId).ok());
    ASSERT_TRUE(finished.wait(120000));
    const OptimizationWorkspace after = optimization().workspace();
    const RunRecord* resumed = after.findRun(runId);
    ASSERT_NE(resumed, nullptr);
    EXPECT_TRUE(resumed->complete);
    EXPECT_TRUE(resumed->resumed);
    EXPECT_GT(resumed->candidates.size(), stopped->candidates.size());
    EXPECT_TRUE(resumed->candidates.front().replayed);
}

TEST_F(OptimizationUiTest, InverseDesignReevaluationAndSensitivityAreAvailableFromTheStudio)
{
    openExample("a-acoustic-focal-array.resonant");
    panel().selectProblem(optimization().workspace().problems.front().id);
    ASSERT_TRUE(panel().solveInverse());
    OptimizationWorkspace w = optimization().workspace();
    ASSERT_EQ(w.runs.size(), 1u);
    EXPECT_EQ(w.runs.front().algorithm, "linear-inverse");
    ASSERT_EQ(w.runs.front().candidates.size(), 1u);
    EXPECT_TRUE(w.runs.front().candidates.front().evaluated());
    panel().selectRun(w.runs.front().runId);
    panel().selectCandidateRow(0);
    ASSERT_TRUE(panel().analyzeSensitivity()) << panel().statusText().toStdString();
    // Re-evaluation with the full-wave model: a new run record whose candidate names its parent.
    ASSERT_TRUE(panel().reevaluateSelected("acoustic.fdtd.staggered")) << panel().statusText().toStdString();
    w = optimization().workspace();
    ASSERT_EQ(w.runs.size(), 2u);
    const Candidate& validated = w.runs.back().candidates.front();
    EXPECT_EQ(validated.evaluation.model.fidelity, Fidelity::HighFidelity);
    EXPECT_EQ(validated.parents, std::vector<std::string>{w.runs.front().candidates.front().id});
    EXPECT_TRUE(validated.evaluated()) << validated.evaluation.message;
}

TEST_F(OptimizationUiTest, ExportWritesTheCandidateRecordAndItsGeometryMeshes)
{
    openExample("b-acoustic-reflector.resonant");
    const OptimizationProblem problem = optimization().workspace().problems.front();
    panel().selectProblem(problem.id);
    ASSERT_TRUE(panel().start());
    QSignalSpy finished(&optimization(), &OptimizationController::runFinished);
    ASSERT_TRUE(finished.wait(120000));
    const RunRecord run = optimization().workspace().runs.back();
    ASSERT_TRUE(run.best.has_value());
    const Candidate& best = run.candidates[*run.best];
    ASSERT_TRUE(optimization().exportCandidate(run.runId, best.id, dir_.path()).ok());
    const std::filesystem::path dir(dir_.path().toStdString());
    EXPECT_TRUE(std::filesystem::exists(dir / (best.id + ".json")));
    bool stl = false;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
        stl = stl || entry.path().extension() == ".stl";
    EXPECT_TRUE(stl);
    ASSERT_TRUE(optimization().exportRun(run.runId, dir_.filePath(QStringLiteral("run.csv"))).ok());
    ASSERT_TRUE(optimization().exportRun(run.runId, dir_.filePath(QStringLiteral("run.json"))).ok());
    EXPECT_TRUE(runRecordFromJson(nlohmann::json::parse(std::ifstream(dir / "run.json"))).ok());
}

TEST_F(OptimizationUiTest, TheTopologyProofOfConceptRunsInTheBackground)
{
    panel().showTab(QStringLiteral("topologyTab"));
    QSignalSpy finished(&optimization(), &OptimizationController::topologyFinished);
    ASSERT_TRUE(panel().runTopology());
    ASSERT_TRUE(finished.wait(120000));
    ASSERT_TRUE(optimization().topologyResult().has_value());
    EXPECT_LT(optimization().topologyResult()->compliance.back(), optimization().topologyResult()->initialCompliance);
    ASSERT_TRUE(optimization().exportTopology(dir_.filePath(QStringLiteral("topology.stl"))).ok());
}
