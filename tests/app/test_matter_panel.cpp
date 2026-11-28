// The Matter panel in the real main window (RES-FAM-019): the energy converter's regions and
// stack drawn as an idealized model with its interfaces, the harvester analysed and plotted,
// the film found infeasible and explored (findings apart from hypotheses), mechanisms between
// domains, an alternative duplicated and undone, the architectures compared, and a stack
// refused by the coupled engine; a barrier stack added to it.
#include "DocumentController.h"
#include "MainWindow.h"
#include "MatterController.h"
#include "MatterPanel.h"
#include "StackView.h"

#include <QPushButton>
#include <QTest>

#include <gtest/gtest.h>

#include <filesystem>
#include <random>

namespace fs = std::filesystem;

namespace {

class MatterPanelTest : public ::testing::Test {
protected:
    void open(const std::string& file)
    {
        std::random_device rd;
        dir_ = fs::temp_directory_path() / ("resonant-matterui-" + std::to_string(rd()));
        fs::create_directories(dir_);
        fs::copy_file(fs::path(RESONANT_SOURCE_DIR) / "examples" / "matter" / file, dir_ / file);
        window_ = std::make_unique<MainWindow>();
        window_->unsavedChangesPrompt = [] { return QMessageBox::Discard; };
        window_->resize(1400, 900);
        window_->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_.get()));
        ASSERT_TRUE(window_->openProjectFile(QString::fromStdString((dir_ / file).string())).ok());
        QTest::qWait(20);
    }
    void TearDown() override
    {
        if (window_)
            window_->close();
        window_.reset();
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }
    MatterPanel& panel() { return window_->matterPanel(); }
    fs::path dir_;
    std::unique_ptr<MainWindow> window_;
};

}  // namespace

TEST_F(MatterPanelTest, TheEnergyConverterFromRegionsToAlternatives)
{
    open("b-energy-converter.resonant");
    ASSERT_EQ(panel().architectureCount(), 2);
    panel().selectArchitecture("arch-bonded");
    EXPECT_EQ(panel().regionRows(), 6);
    EXPECT_EQ(panel().stackView()->layerCount(), 6);
    EXPECT_EQ(panel().stackView()->interfaceCount(), 5);
    EXPECT_EQ(panel().interfaceRows(), 5);
    EXPECT_TRUE(panel().stackView()->caption().startsWith("Idealized model")) << panel().stackView()->caption().toStdString();
    // The harvester, plotted over frequency.
    panel().selectAnalysis("harvester");
    panel().setOptions("{\"tipMass\": 0.002}");
    panel().runButton()->click();
    EXPECT_TRUE(panel().resultText().contains("power =")) << panel().resultText().toStdString();
    EXPECT_TRUE(panel().resultText().contains("Assumptions"));
    EXPECT_EQ(panel().plottedPoints(), 201);
    // An analysis that refuses says why.
    panel().selectAnalysis("electromagnetic");
    panel().setOptions("");
    panel().runButton()->click();
    EXPECT_TRUE(panel().resultText().contains("Layered plane-wave solver") || panel().resultText().contains("absorptance")) << panel().resultText().toStdString();
    // The film: infeasible, and explored.
    panel().selectArchitecture("arch-film");
    panel().manufactureButton()->click();
    EXPECT_TRUE(panel().manufactureStatus().startsWith("Infeasible")) << panel().manufactureStatus().toStdString();
    EXPECT_GT(panel().manufactureRows(), 0);
    panel().exploreButton()->click();
    EXPECT_TRUE(panel().exploreText().contains("Findings")) << panel().exploreText().toStdString();
    EXPECT_TRUE(panel().exploreText().contains("Hypotheses"));
    EXPECT_TRUE(panel().exploreText().contains("delamination"));
    panel().setMechanismDomains("thermal", "electrical");
    panel().mechanismsButton()->click();
    EXPECT_TRUE(panel().exploreText().contains("Thermoelectric")) << panel().exploreText().toStdString();
    // An alternative, compared, then undone.
    panel().selectArchitecture("arch-bonded");
    panel().duplicateButton()->click();
    QTest::qWait(20);
    EXPECT_EQ(panel().architectureCount(), 3);
    panel().compareButton()->click();
    EXPECT_GT(panel().compareRows(), 5);
    EXPECT_EQ(panel().compareColumns(), 4);
    window_->controller().undo();
    QTest::qWait(20);
    EXPECT_EQ(panel().architectureCount(), 2);
    // The segmented electrode is refused by the coupled engine, with the reason.
    panel().selectArchitecture("arch-bonded");
    panel().systemButton()->click();
    EXPECT_TRUE(panel().statusText().contains("segmented")) << panel().statusText().toStdString();
}

TEST_F(MatterPanelTest, ABarrierStackGoesToTheCoupledEngine)
{
    open("c-thermal-barrier.resonant");
    ASSERT_EQ(panel().architectureCount(), 1);
    panel().selectAnalysis("thermal");
    panel().setOptions("{\"temperatures\": [1400, 900]}");
    panel().runButton()->click();
    EXPECT_TRUE(panel().resultText().contains("resistance =")) << panel().resultText().toStdString();
    EXPECT_EQ(panel().plottedPoints(), 5);  // four boundaries and the jump at the contact resistance
    panel().systemButton()->click();
    EXPECT_TRUE(panel().statusText().contains("Added the physical system")) << panel().statusText().toStdString();
}

TEST_F(MatterPanelTest, TheLayeredStackAsAThreeDimensionalBody)
{
    // The nickel-alumina step as a 3D body of 20-node elements (REQ-CHAT-20261128-004):
    // its modes plotted, and the energy converter's cohesive zones refused with the reason.
    open("d-graded-metal-ceramic.resonant");
    panel().selectArchitecture("arch-step");
    panel().selectAnalysis("layered-solid");
    panel().setOptions("{\"mesh\": {\"elementsU\": 6, \"elementsV\": 2}, \"modes\": 3}");
    panel().runButton()->click();
    EXPECT_TRUE(panel().resultText().contains("f1 =")) << panel().resultText().toStdString();
    EXPECT_TRUE(panel().resultText().contains("20-node")) << panel().resultText().toStdString();
    EXPECT_EQ(panel().plottedPoints(), 3);
}

TEST_F(MatterPanelTest, TheThreeDimensionalSolidRefusesACohesiveZone)
{
    open("b-energy-converter.resonant");
    panel().selectArchitecture("arch-bonded");
    panel().selectAnalysis("layered-solid");
    panel().setOptions("{}");
    panel().runButton()->click();
    EXPECT_TRUE(panel().resultText().contains("cohesive-zone")) << panel().resultText().toStdString();
}
