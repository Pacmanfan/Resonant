// The Functional Structures panel in the real main window (RES-FUNC-015):
// an example's structure and parameters, an analysis run as a job with its
// metrics, modes and model, a parameter edit that changes the scene undoably
// and makes the result stale, a sweep plotted, and curves switched without a
// new computation.
#include "DocumentController.h"
#include "FunctionalController.h"
#include "FunctionalPanel.h"
#include "MainWindow.h"

#include "project/Project.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

#include <filesystem>

using namespace resonant;

namespace {

class FunctionalPanelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window_ = std::make_unique<MainWindow>();
        window_->unsavedChangesPrompt = [] { return QMessageBox::Discard; };
        window_->resize(1400, 900);
        window_->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_.get()));
    }
    void TearDown() override
    {
        window_->close();
        window_.reset();
    }
    void open(const char* file)
    {
        const auto path = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "functional" / file;
        ASSERT_TRUE(window_->openProjectFile(QString::fromStdString(path.string())).ok());
        QTest::qWait(20);
    }
    FunctionalPanel& panel() { return window_->functionalPanel(); }
    bool analyzeAndWait()
    {
        QSignalSpy ready(&window_->functional(), &FunctionalController::resultReady);
        panel().analyzeButton()->click();
        return ready.wait(30000) || ready.count() > 0;
    }
    std::unique_ptr<MainWindow> window_;
};

}  // namespace

TEST_F(FunctionalPanelTest, ShowsAStructureAnalyzesItAndMarksTheResultStaleAfterAnEdit)
{
    open("a-microwave-cavity.resonant");
    ASSERT_EQ(panel().structureCount(), 1);
    EXPECT_EQ(panel().currentStructure(), "fs-microwave-cavity");
    ASSERT_EQ(panel().parameterRowCount(), 5);
    EXPECT_EQ(panel().parameterId(0), "a");
    ASSERT_TRUE(analyzeAndWait());
    EXPECT_TRUE(panel().metricText("cavity.f1").endsWith(" Hz")) << panel().metricText("cavity.f1").toStdString();
    EXPECT_GE(panel().modeRowCount(), 3);
    EXPECT_EQ(panel().modeLabel(0), "TE101");
    EXPECT_EQ(panel().modeCell(0, 1), "natural");
    EXPECT_TRUE(panel().modeCell(0, 4).contains("wall conduction")) << panel().modeCell(0, 4).toStdString();
    EXPECT_TRUE(panel().modeCell(0, 4).contains("port"));
    EXPECT_TRUE(panel().statusText().contains("current")) << panel().statusText().toStdString();
    EXPECT_TRUE(panel().detailsText().contains("Closed-form modes")) << panel().detailsText().toStdString();
    // Editing a parameter changes the scene object (undoably) and makes the shown result stale.
    const Id cavity("ex-fs-cavity");
    panel().editParameter(2, 0.034);  // d
    QTest::qWait(20);
    EXPECT_DOUBLE_EQ(window_->controller().project().scene.find(cavity)->state.geometry->parameter("d"), 0.034);
    EXPECT_TRUE(panel().statusText().contains("stale")) << panel().statusText().toStdString();
    window_->controller().undo();
    QTest::qWait(20);
    EXPECT_DOUBLE_EQ(window_->controller().project().scene.find(cavity)->state.geometry->parameter("d"), 0.030);
    EXPECT_TRUE(panel().statusText().contains("current")) << panel().statusText().toStdString();
    // A derived parameter cannot be edited: the service refuses it and the panel says why.
    panel().editParameter(4, 3.0);
    QTest::qWait(20);
    EXPECT_TRUE(panel().statusText().contains("derived")) << panel().statusText().toStdString();
    EXPECT_TRUE(window_->controller().commands().canRedo());  // nothing new was done (the undo above is still redoable)
}

TEST_F(FunctionalPanelTest, CurvesSwitchWithoutRecomputingAndASweepIsPlotted)
{
    open("c-acoustic-horn.resonant");
    ASSERT_TRUE(analyzeAndWait());
    const QStringList curves = panel().curveChoices();
    ASSERT_GE(curves.size(), 3);
    EXPECT_TRUE(curves.contains("resistance"));
    // Switching the curve is a view change: no job starts.
    QSignalSpy jobs(&window_->controller(), &DocumentController::jobChanged);
    panel().showCurve(1);
    QTest::qWait(30);
    EXPECT_EQ(jobs.count(), 0);
    EXPECT_GT(panel().plottedPoints(), 10);
    // A sweep of the length, plotted for the mean transmission.
    QSignalSpy ready(&window_->functional(), &FunctionalController::resultReady);
    panel().startSweep("length", 0.3, 0.7, 5, "horn.meanTransmission");
    ASSERT_TRUE(ready.wait(30000) || ready.count() > 0);
    EXPECT_EQ(panel().plottedPoints(), 5);
    EXPECT_TRUE(panel().statusText().contains("Sweep finished")) << panel().statusText().toStdString();
}
