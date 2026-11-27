// The electromagnetic domain driven through the real main window.
#include "DocumentController.h"
#include "EmController.h"
#include "EmEditor.h"
#include "EmPanel.h"
#include "MainWindow.h"
#include "ObjectLabels.h"
#include "PlotWidget.h"
#include "PropertiesPanel.h"
#include "ViewportWidget.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::em;

namespace {

class EmUiTest : public ::testing::Test {
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
        window_->close();
        window_.reset();
    }
    void trigger(const char* name)
    {
        QAction* action = window_->action(QString::fromLatin1(name));
        ASSERT_NE(action, nullptr) << name;
        action->trigger();
        QCoreApplication::processEvents();
    }
    DocumentController& doc() { return window_->controller(); }
    EmController& em() { return window_->electromagnetics(); }
    EmPanel& panel() { return window_->emPanel(); }
    template <typename T>
    T* child(QWidget& parent, const char* name)
    {
        T* widget = parent.findChild<T*>(QString::fromLatin1(name));
        EXPECT_NE(widget, nullptr) << name;
        return widget;
    }
    void open(const char* example)
    {
        const QString path = QStringLiteral(RESONANT_SOURCE_DIR) + QStringLiteral("/examples/electromagnetics/") + QString::fromLatin1(example)
                             + QStringLiteral(".resonant");
        ASSERT_TRUE(window_->openProjectFile(path).ok()) << example;
        QCoreApplication::processEvents();
    }
    void select(const char* id)
    {
        doc().setSelection({Id(id)});
        QCoreApplication::processEvents();
    }
    EmEditor* editor()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        return window_->properties().findChild<EmEditor*>();
    }

    std::unique_ptr<MainWindow> window_;
};

}  // namespace

TEST_F(EmUiTest, MenuAddsADipoleThePanelReportsItAndTheEditorChangesItUndoably)
{
    trigger("actionAddDipoleAntenna");
    const Id id = doc().primarySelection();
    ASSERT_EQ(em().kindOf(id), EmController::Kind::Antenna);
    EXPECT_EQ(panel().selectedObject(), id);
    EXPECT_TRUE(objectTypeLabel(*doc().scene().find(id)).startsWith(QStringLiteral("Antenna")));
    // A half-wave dipole at 1 GHz: about 73 + j42 ohm, with the model named.
    EXPECT_TRUE(panel().reportText().contains(QStringLiteral("Input impedance: 73."))) << panel().reportText().toStdString();
    EXPECT_TRUE(panel().reportText().contains(QStringLiteral("induced-EMF"))) << panel().reportText().toStdString();
    EXPECT_TRUE(panel().reportText().contains(QStringLiteral("realized gain")));
    // The editor: the frequency changes the report; undo restores it.
    EmEditor* e = editor();
    ASSERT_NE(e, nullptr);
    auto* frequency = child<QDoubleSpinBox>(*e, "antennaFrequencySpin");
    frequency->setValue(1200.0);
    QCoreApplication::processEvents();
    const auto* antenna = dynamic_cast<const AntennaComponent*>(doc().scene().find(id)->state.component.get());
    EXPECT_DOUBLE_EQ(antenna->settings.frequencyHz, 1.2e9);
    EXPECT_FALSE(panel().reportText().contains(QStringLiteral("Input impedance: 73.")));
    doc().undo();
    QCoreApplication::processEvents();
    antenna = dynamic_cast<const AntennaComponent*>(doc().scene().find(id)->state.component.get());
    EXPECT_DOUBLE_EQ(antenna->settings.frequencyHz, 1e9);
    // Sources, arrays and guides from the menu, each with its editor page.
    trigger("actionAddPlaneWaveSource");
    EXPECT_EQ(em().kindOf(doc().primarySelection()), EmController::Kind::Source);
    trigger("actionAddPlanarRfArray");
    EXPECT_EQ(em().kindOf(doc().primarySelection()), EmController::Kind::Array);
    ASSERT_NE(editor(), nullptr);
    EXPECT_EQ(child<QWidget>(*editor(), "pages")->findChild<QWidget*>(QStringLiteral("arrayPage"))->isVisible(), true);
    trigger("actionAddEmRectangularGuide");
    EXPECT_EQ(em().kindOf(doc().primarySelection()), EmController::Kind::Guide);
    EXPECT_TRUE(panel().reportText().contains(QStringLiteral("TE10")));
}

TEST_F(EmUiTest, ExampleA_SweepPatternAnd3DPatternAndTouchstoneRoundTrip)
{
    open("a-half-wave-dipole");
    select("ex-ea-dipole");
    panel().showTab(QStringLiteral("impedanceTab"));
    ASSERT_TRUE(panel().calculateSweep());
    EXPECT_EQ(panel().sweepPlot().pointCount(), em().settings().sweepPoints);
    EXPECT_EQ(panel().sweepPlot().curveCount(), 2);  // R and X
    // Export the calculated data and import it back as "measured" data: the
    // two must coincide (and are labelled differently).
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("dipole.s1p"));
    ASSERT_TRUE(em().exportTouchstone(Id("ex-ea-dipole"), path).ok());
    ASSERT_TRUE(em().importTouchstone(path).ok());
    ASSERT_EQ(em().measurements().size(), 1u);
    auto* measured = child<QComboBox>(panel(), "measurementCombo");
    ASSERT_EQ(measured->count(), 2);
    measured->setCurrentIndex(1);
    emit measured->activated(1);
    QCoreApplication::processEvents();
    EXPECT_EQ(panel().sweepPlot().curveCount(), 4);
    const auto& model = panel().sweepPlot().yValues(0);
    const auto& data = panel().sweepPlot().yValues(2);
    ASSERT_EQ(model.size(), data.size());
    for (int i = 0; i < model.size(); ++i)
        EXPECT_NEAR(model[i], data[i], 1e-3 * std::max(1.0, std::abs(model[i])));
    doc().undo();  // the import is undoable
    QCoreApplication::processEvents();
    EXPECT_TRUE(em().measurements().empty());
    // Pattern: an E-plane cut with its directivity, and the 3D surface in the viewport.
    ASSERT_TRUE(panel().calculatePattern());
    EXPECT_TRUE(panel().patternText().contains(QStringLiteral("1.64"))) << panel().patternText().toStdString();
    EXPECT_TRUE(panel().patternText().contains(QStringLiteral("normalized")));
    ASSERT_TRUE(panel().showPattern3D());
    ASSERT_NE(window_->viewport().scalarMesh(), nullptr);
    EXPECT_FALSE(window_->viewport().scalarMesh()->empty());
}

TEST_F(EmUiTest, ExamplesCAndD_ArraysSteerTaperAndWarnAboutGratingLobes)
{
    open("c-linear-array");
    select("ex-ec-array");
    ASSERT_TRUE(panel().analyseArray());
    EXPECT_TRUE(panel().arrayText().contains(QStringLiteral("-30.0"))) << panel().arrayText().toStdString();
    EXPECT_TRUE(panel().arrayText().contains(QStringLiteral("no mutual coupling")));
    EXPECT_GT(panel().arrayPlot().pointCount(), 100);

    open("d-phased-array-steering");
    select("ex-ed-array");
    auto* theta = child<QDoubleSpinBox>(panel(), "steerThetaSpin");
    auto* phi = child<QDoubleSpinBox>(panel(), "steerPhiSpin");
    EXPECT_DOUBLE_EQ(theta->value(), 30.0);  // from the project's settings
    theta->setValue(20.0);
    phi->setValue(0.0);
    QCoreApplication::processEvents();
    ASSERT_TRUE(panel().steer());
    auto analysis = em().analyzeArray(Id("ex-ed-array"));
    ASSERT_TRUE(analysis.ok());
    EXPECT_NEAR(analysis.value().pattern.mainTheta * 180 / kPi, 20.0, 1.0);
    EXPECT_TRUE(analysis.value().predictedGratingLobes.empty());
    // Wider spacing in the editor: a grating lobe is predicted and reported.
    EmEditor* e = editor();
    ASSERT_NE(e, nullptr);
    auto* spacing = child<QDoubleSpinBox>(*e, "spacingXSpin");
    spacing->setValue(spacing->value() * 1.8);  // 0.9 wavelength
    QCoreApplication::processEvents();
    ASSERT_TRUE(panel().analyseArray());
    EXPECT_TRUE(panel().arrayText().contains(QStringLiteral("Grating lobe predicted"))) << panel().arrayText().toStdString();
    doc().undo();
    // Steering is undoable too.
    doc().undo();
    QCoreApplication::processEvents();
    analysis = em().analyzeArray(Id("ex-ed-array"));
    EXPECT_NEAR(analysis.value().pattern.mainTheta * 180 / kPi, 30.0, 1.0);
}

TEST_F(EmUiTest, ExamplesEAndF_FieldsOnARegionProbesAndTheGuideMode)
{
    open("e-near-field");
    panel().selectRegion(Id("ex-ee-region"));
    ASSERT_TRUE(panel().evaluateRegion());
    ASSERT_EQ(em().results().size(), 1u);
    EXPECT_EQ(em().displayedIndex(), 0);
    EXPECT_TRUE(panel().displayText().contains(QStringLiteral("analytical"))) << panel().displayText().toStdString();
    EXPECT_TRUE(panel().displayText().contains(QStringLiteral("V/m")));
    EXPECT_TRUE(panel().displayText().contains(QStringLiteral("NORMALIZED")));
    auto* component = child<QComboBox>(panel(), "componentCombo");
    component->setCurrentIndex(4);  // |H|
    emit component->activated(4);
    QCoreApplication::processEvents();
    EXPECT_TRUE(panel().displayText().contains(QStringLiteral("A/m")));
    // A probe half a wavelength from the dipole.
    child<QDoubleSpinBox>(panel(), "probeXSpin")->setValue(0.15);
    ASSERT_TRUE(panel().probe());
    EXPECT_TRUE(panel().probeText().contains(QStringLiteral("Ez ="))) << panel().probeText().toStdString();
    EXPECT_TRUE(panel().probeText().contains(QStringLiteral("power density")));

    open("f-rectangular-waveguide");
    EXPECT_TRUE(em().results().empty());  // results belong to the project that made them
    select("ex-ef-guide");
    EXPECT_TRUE(panel().reportText().contains(QStringLiteral("TE10: cut-off 6.557"))) << panel().reportText().toStdString();
    ASSERT_TRUE(panel().showGuideField());
    ASSERT_EQ(em().results().size(), 1u);
}

TEST_F(EmUiTest, ExampleG_FullWaveRunFromThePanelIsLabelledNumerical)
{
    open("g-full-wave-scattering");
    panel().selectRegion(Id("ex-eg-region"));
    child<QDoubleSpinBox>(panel(), "cellsSpin")->setValue(12.0);  // coarse, to keep the test quick
    QCoreApplication::processEvents();
    ASSERT_TRUE(panel().planFullWave());
    EXPECT_TRUE(panel().planText().contains(QStringLiteral("2D Yee grid"))) << panel().planText().toStdString();
    EXPECT_TRUE(panel().planText().contains(QStringLiteral("good conductor")));
    ASSERT_TRUE(panel().runFullWave());
    EXPECT_TRUE(em().isRunning());
    ASSERT_TRUE(QTest::qWaitFor([&] { return !em().isRunning(); }, 300000));
    ASSERT_EQ(em().results().size(), 1u);
    EXPECT_EQ(em().results().front()->metadata.solverId, kEmFullWaveSolverId);
    EXPECT_TRUE(panel().displayText().contains(QStringLiteral("full-wave numerical"))) << panel().displayText().toStdString();
    EXPECT_TRUE(panel().displayText().contains(QStringLiteral("Only the electric field")));
    // The run is recorded with its configuration in the project.
    EXPECT_TRUE(doc().project().extensions.contains("electromagneticRuns"));
}

TEST_F(EmUiTest, LineCalculatorGivesTheCoaxialImpedance)
{
    panel().showTab(QStringLiteral("linesTab"));
    ASSERT_TRUE(panel().calculateLine());  // 0.45 / 1.47 mm, air: eta0 / (2 pi) ln(b / a) = 70.98 ohm
    EXPECT_TRUE(panel().lineText().contains(QStringLiteral("Z0 = 70.98 ohm"))) << panel().lineText().toStdString();
}
