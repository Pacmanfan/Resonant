// The Laboratory panel in the real main window (RES-LAB-017): a study's files imported with
// their report, a dataset plotted, a comparison overlaid with its residual, a calibration
// job with estimates and identifiability, kept and validated on the study's other run, the
// report and an evidence summary.
#include "DocumentController.h"
#include "LaboratoryController.h"
#include "LaboratoryPanel.h"
#include "MainWindow.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

#include <filesystem>
#include <random>

using namespace resonant;
namespace fs = std::filesystem;

namespace {

class LaboratoryPanelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::random_device rd;
        dir_ = fs::temp_directory_path() / ("resonant-labui-" + std::to_string(rd()));
        fs::create_directories(dir_);
        const fs::path source = fs::path(RESONANT_SOURCE_DIR) / "examples" / "laboratory";
        fs::copy_file(source / "a-resonator-frf.resonant", dir_ / "a-resonator-frf.resonant");
        fs::copy(source / "data", dir_ / "data", fs::copy_options::recursive);
        window_ = std::make_unique<MainWindow>();
        window_->unsavedChangesPrompt = [] { return QMessageBox::Discard; };
        window_->resize(1400, 900);
        window_->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_.get()));
        ASSERT_TRUE(window_->openProjectFile(QString::fromStdString((dir_ / "a-resonator-frf.resonant").string())).ok());
        QTest::qWait(20);
    }
    void TearDown() override
    {
        window_->close();
        window_.reset();
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }
    LaboratoryPanel& panel() { return window_->laboratoryPanel(); }
    fs::path dir_;
    std::unique_ptr<MainWindow> window_;
};

}  // namespace

TEST_F(LaboratoryPanelTest, ImportCompareCalibrateValidateAndReport)
{
    ASSERT_EQ(panel().studyCount(), 1);
    EXPECT_TRUE(panel().studyText().contains("free length")) << panel().studyText().toStdString();
    panel().importStudyButton()->click();
    QTest::qWait(20);
    EXPECT_TRUE(panel().statusText().contains("accepted")) << panel().statusText().toStdString();
    ASSERT_EQ(panel().datasetCount(), 2);
    panel().selectDataset(0);
    EXPECT_EQ(panel().plottedPoints(), 161);
    EXPECT_EQ(panel().issueRows(), 0);  // a complete file: its import report lists no issue
    // Compare with the nominal model.
    panel().compareButton()->click();
    QTest::qWait(20);
    EXPECT_TRUE(panel().verdictText().contains("inconsistent")) << panel().verdictText().toStdString();
    EXPECT_EQ(panel().comparisonCurves(), 2);
    EXPECT_GT(panel().residualPoints(), 100);
    // Calibrate mass, stiffness and damping.
    for (const char* p : {"mass", "stiffness", "damping"})
        panel().setParameterEnabled(p, true);
    QSignalSpy ready(&window_->laboratory(), &LaboratoryController::calibrationReady);
    panel().calibrateButton()->click();
    ASSERT_TRUE(ready.wait(60000) || ready.count() > 0) << panel().calibrationText().toStdString();
    EXPECT_EQ(panel().estimateRows(), 3);
    EXPECT_TRUE(panel().estimate("stiffness").contains("identified")) << panel().estimate("stiffness").toStdString();
    EXPECT_TRUE(panel().calibrationText().contains("calibrated"));
    ASSERT_TRUE(panel().keepButton()->isEnabled());
    panel().keepButton()->click();
    QTest::qWait(20);
    ASSERT_TRUE(panel().validateButton()->isEnabled());
    panel().validateButton()->click();
    QTest::qWait(20);
    EXPECT_TRUE(panel().statusText().contains("validated within uncertainty")) << panel().statusText().toStdString();
    panel().reportButton()->click();
    QTest::qWait(20);
    EXPECT_TRUE(panel().reportText().contains("Identify the resonator")) << panel().reportText().toStdString();
    panel().explainButton()->click();
    QTest::qWait(20);
    EXPECT_TRUE(panel().reportText().contains("no model or parameter was changed"));
}
