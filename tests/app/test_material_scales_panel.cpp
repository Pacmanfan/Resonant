// The Material Scales panel in the real main window (RES-MSC-014): an example's
// representations by scale with their model card and applicable methods, a derivation
// run as a job and assigned to a library material (undoable), and the lineage table
// tracing the material back to the RVE, stale after the RVE changes.
#include "DocumentController.h"
#include "MainWindow.h"
#include "MaterialScalesPanel.h"
#include "MultiscaleController.h"

#include "project/Project.h"
#include "services/Services.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

#include <filesystem>

using namespace resonant;

namespace {

class MaterialScalesPanelTest : public ::testing::Test {
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
        const auto path = std::filesystem::path(RESONANT_SOURCE_DIR) / "examples" / "multiscale" / file;
        ASSERT_TRUE(window_->openProjectFile(QString::fromStdString(path.string())).ok());
        QTest::qWait(20);
    }
    MaterialScalesPanel& panel() { return window_->materialScalesPanel(); }
    std::unique_ptr<MainWindow> window_;
};

}  // namespace

TEST_F(MaterialScalesPanelTest, DerivesAssignsAndTracesAMaterial)
{
    open("c-porous-composite.resonant");
    ASSERT_EQ(panel().representationCount(), 2);
    panel().selectRepresentation("rve-porous-alumina");
    EXPECT_EQ(panel().currentRepresentation(), "rve-porous-alumina");
    EXPECT_TRUE(panel().detailsText().contains("microstructure scale")) << panel().detailsText().toStdString();
    EXPECT_TRUE(panel().detailsText().contains("Model card: Voxel representative volume element"));
    EXPECT_TRUE(panel().detailsText().contains("Not applicable: atomistic.elastic-constants"));
    ASSERT_TRUE(panel().methodChoices().contains("rve.thermal-conductivity"));
    panel().chooseMethod("rve.thermal-conductivity");
    QSignalSpy ready(&window_->multiscale(), &MultiscaleController::resultReady);
    panel().deriveButton()->click();
    ASSERT_TRUE(ready.wait(60000) || ready.count() > 0);
    EXPECT_TRUE(panel().statusText().contains("thermal.thermalConductivity (mean)")) << panel().statusText().toStdString();
    ASSERT_TRUE(panel().assignButton()->isEnabled());
    panel().chooseTarget("mat-ex-porous-alumina");
    panel().assignButton()->click();
    QTest::qWait(20);
    const Material* m = window_->controller().project().materials.find(Id("mat-ex-porous-alumina"));
    ASSERT_NE(m->property(PhysicsDomain::Thermal, "thermalConductivity"), nullptr);
    panel().showLineage("mat-ex-porous-alumina");
    ASSERT_GE(panel().lineageRowCount(), 1);
    bool traced = false;
    for (int r = 0; r < panel().lineageRowCount(); ++r)
        if (panel().lineageCell(r, 0) == "thermal.thermalConductivity")
            traced = panel().lineageCell(r, 2) == "homogenized" && panel().lineageCell(r, 3).contains("Alumina with 20 % spherical pores") &&
                     panel().lineageCell(r, 4) == "current";
    EXPECT_TRUE(traced) << panel().lineageCell(0, 0).toStdString() << " | " << panel().lineageCell(0, 2).toStdString() << " | "
                        << panel().lineageCell(0, 3).toStdString() << " | " << panel().lineageCell(0, 4).toStdString();
    // Changing the RVE makes the derived value stale in the table.
    auto rep = window_->controller().services().multiscale.representation(DocumentController::guiContext(), "rve-porous-alumina");
    ASSERT_TRUE(rep);
    nlohmann::json changed = rep.value();
    changed["data"]["rve"]["seed"] = 7;
    ASSERT_TRUE(window_->controller().services().multiscale.setRepresentation(DocumentController::guiContext(), changed));
    QTest::qWait(20);
    panel().showLineage("mat-ex-porous-alumina");
    bool stale = false;
    for (int r = 0; r < panel().lineageRowCount(); ++r)
        if (panel().lineageCell(r, 0) == "thermal.thermalConductivity")
            stale = panel().lineageCell(r, 4).startsWith("stale");
    EXPECT_TRUE(stale);
    EXPECT_FALSE(panel().assignButton()->isEnabled());
    // Undo both steps: the material loses the derived value again.
    window_->controller().undo();
    window_->controller().undo();
    QTest::qWait(20);
    EXPECT_EQ(window_->controller().project().materials.find(Id("mat-ex-porous-alumina"))->property(PhysicsDomain::Thermal, "thermalConductivity"), nullptr);
}
