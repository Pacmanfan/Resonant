// The Material State panel in the real main window (RES-DMS-014): a project's studies and a
// model's card (with what is not implemented), a run as a job with its curve, history and
// spatial profiles, a record saved undoably and an evolved state committed to a material.
#include "DocumentController.h"
#include "MainWindow.h"
#include "MaterialStateController.h"
#include "MaterialStatePanel.h"

#include "project/Project.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

#include <filesystem>

using namespace resonant;

namespace {

class MaterialStatePanelTest : public ::testing::Test {
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
        const auto path = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "materialstate" / file;
        ASSERT_TRUE(window_->openProjectFile(QString::fromStdString(path.string())).ok());
        QTest::qWait(20);
    }
    bool runAndWait(QPushButton* button)
    {
        QSignalSpy ready(&window_->materialState(), &MaterialStateController::resultReady);
        button->click();
        return ready.wait(60000) || ready.count() > 0;
    }
    MaterialStatePanel& panel() { return window_->materialStatePanel(); }
    std::unique_ptr<MainWindow> window_;
};

}  // namespace

TEST_F(MaterialStatePanelTest, RunsAStudyAndShowsItsCurveHistoryAndProfiles)
{
    open("e-phase-change.resonant");
    ASSERT_EQ(panel().studyCount(), 1);
    panel().selectModel("mechanics.j2.point");
    EXPECT_TRUE(panel().cardText().contains("Plastic strain")) << panel().cardText().toStdString();
    EXPECT_TRUE(panel().cardText().contains("irreversible"));
    EXPECT_TRUE(panel().cardText().contains("Not implemented:"));
    EXPECT_TRUE(panel().cardText().contains("Fatigue"));
    ASSERT_TRUE(runAndWait(panel().runStudyButton()));
    EXPECT_TRUE(panel().statusText().contains("reliable")) << panel().statusText().toStdString();
    ASSERT_TRUE(panel().curveChoices().contains("front"));
    EXPECT_GT(panel().plottedPoints(), 5);
    EXPECT_GT(panel().historyRowCount(), 5);
    // The spatial state: the liquid fraction along the slab at a chosen snapshot.
    const int profile = static_cast<int>(panel().curveChoices().indexOf("profile"));
    ASSERT_GE(profile, 0);
    panel().showCurve(profile);
    EXPECT_EQ(panel().plottedPoints(), 400);
    EXPECT_GT(panel().snapshotCount(), 5);
}

TEST_F(MaterialStatePanelTest, SavesARecordAndCommitsTheStateUndoably)
{
    open("d-damage-resonance.resonant");
    panel().selectStudy("st-damage-point");
    ASSERT_TRUE(runAndWait(panel().runStudyButton()));
    ASSERT_TRUE(panel().saveButton()->isEnabled());
    panel().saveButton()->click();
    QTest::qWait(20);
    ASSERT_EQ(panel().recordCount(), 1);
    ASSERT_TRUE(panel().commitButton()->isEnabled());
    panel().chooseTarget("mat-ex-laminate");
    panel().commitButton()->click();
    QTest::qWait(20);
    const PropertyValue* E = window_->controller().project().materials.find(Id("mat-ex-laminate"))->property(PhysicsDomain::Mechanical, "youngsModulus");
    ASSERT_NE(E, nullptr);
    EXPECT_EQ(E->provenance, Provenance::Computed);
    EXPECT_LT(E->value, 25e9);
    EXPECT_TRUE(panel().statusText().contains("Committed")) << panel().statusText().toStdString();
    window_->controller().undo();
    QTest::qWait(20);
    EXPECT_EQ(window_->controller().project().materials.find(Id("mat-ex-laminate"))->property(PhysicsDomain::Mechanical, "youngsModulus")->provenance,
              Provenance::Illustrative);
}
