// The mechanical domain driven through the real main window.
#include "AcousticController.h"
#include "DocumentController.h"
#include "MainWindow.h"
#include "MechanicalEditor.h"
#include "MechanicsController.h"
#include "MechanicsPanel.h"
#include "ObjectLabels.h"
#include "PlotWidget.h"
#include "PropertiesPanel.h"
#include "ViewportWidget.h"
#include "acoustics/Components.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::mechanics;

namespace {

class MechanicsUiTest : public ::testing::Test {
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
    MechanicsController& mech() { return window_->mechanics(); }
    MechanicsPanel& panel() { return window_->mechanicsPanel(); }
    template <typename T>
    T* child(QWidget& parent, const char* name)
    {
        T* widget = parent.findChild<T*>(QString::fromLatin1(name));
        EXPECT_NE(widget, nullptr) << name;
        return widget;
    }
    void open(const char* example)
    {
        const QString path = QStringLiteral(RESONANT_SOURCE_DIR) + QStringLiteral("/examples/mechanics/") + QString::fromLatin1(example)
                             + QStringLiteral(".resonant");
        ASSERT_TRUE(window_->openProjectFile(path).ok()) << example;
        QCoreApplication::processEvents();
    }
    bool waitForResults(size_t count, int timeoutMs = 120000)
    {
        return QTest::qWaitFor([&] { return mech().results().size() == count && !mech().isRunning(); }, timeoutMs);
    }
    MechanicalEditor* editor()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        return window_->properties().findChild<MechanicalEditor*>();
    }

    std::unique_ptr<MainWindow> window_;
};

}  // namespace

TEST_F(MechanicsUiTest, ObjectsBecomeStructuresMarkersAttachAndEditsAreUndoable)
{
    // Without a selection there is nothing to idealize.
    trigger("actionMakePlate");
    trigger("actionAddBox");
    const Id box = doc().primarySelection();
    ASSERT_TRUE(doc().setGeometryParameter(box, "width", 0.3).ok());
    ASSERT_TRUE(doc().setGeometryParameter(box, "depth", 0.2).ok());
    ASSERT_TRUE(doc().setGeometryParameter(box, "height", 0.002).ok());
    trigger("actionMakePlate");
    const auto* structure = dynamic_cast<const StructureComponent*>(doc().scene().find(box)->state.component.get());
    ASSERT_NE(structure, nullptr);
    EXPECT_EQ(structure->settings.idealization, Idealization::Plate);
    EXPECT_EQ(objectTypeLabel(*doc().scene().find(box)), QStringLiteral("Box (plate)"));
    // A visible default material, and the panel explains the idealization.
    EXPECT_EQ(doc().scene().find(box)->state.materialId, Id("mat-aluminum"));
    EXPECT_EQ(panel().selectedStructure(), box);
    EXPECT_TRUE(panel().studyText().contains(QStringLiteral("plate (Kirchhoff, ACM rectangle)"))) << panel().studyText().toStdString();
    ASSERT_NE(editor(), nullptr);
    EXPECT_TRUE(editor()->studyText().contains(QStringLiteral("Plate in the local X-Y plane, 300 x 200 mm, thickness 2 mm")));

    // Edit a support through the form; undo.
    auto* support = child<QComboBox>(*editor(), "support1Combo");
    support->setCurrentIndex(2);
    emit support->activated(2);
    structure = dynamic_cast<const StructureComponent*>(doc().scene().find(box)->state.component.get());
    EXPECT_EQ(structure->settings.supports[0], Support::Clamped);
    trigger("actionUndo");
    structure = dynamic_cast<const StructureComponent*>(doc().scene().find(box)->state.component.get());
    EXPECT_EQ(structure->settings.supports[0], Support::Pinned);

    // Markers placed on the structure's top face, attached in the study.
    doc().setSelection({box});
    trigger("actionAddActuator");
    const Id actuator = doc().primarySelection();
    EXPECT_NEAR(doc().scene().find(actuator)->state.local.position.z(), 0.001, 1e-9);
    EXPECT_EQ(objectTypeLabel(*doc().scene().find(actuator)), QStringLiteral("Actuator (point force)"));
    doc().setSelection({box});
    trigger("actionAddSensor");
    doc().setSelection({box});
    trigger("actionAddVibrationTarget");
    const StructureStudy study = mech().study(box);
    EXPECT_EQ(study.forces.size(), 1u);
    EXPECT_EQ(study.sensors.size(), 1u);
    EXPECT_EQ(study.targets.size(), 1u);
    // The actuator editor says what is and is not modelled.
    doc().setSelection({actuator});
    QCoreApplication::processEvents();
    ASSERT_NE(editor(), nullptr);
    EXPECT_TRUE(child<QLabel>(*editor(), "actuatorNote")->text().contains(QStringLiteral("not modelled")));
    auto* amplitude = child<QDoubleSpinBox>(*editor(), "amplitudeSpin");
    amplitude->setValue(4.0);
    EXPECT_DOUBLE_EQ(dynamic_cast<const ActuatorComponent*>(doc().scene().find(actuator)->state.component.get())->settings.amplitude, 4.0);
    // An object of another domain is not made structural.
    trigger("actionAddEmitter");
    trigger("actionMakeBeam");
    EXPECT_EQ(dynamic_cast<const StructureComponent*>(doc().scene().find(doc().primarySelection())->state.component.get()), nullptr);
}

TEST_F(MechanicsUiTest, ModalResultIsShownDeformedWithItsVisualScaleReportedApart)
{
    open("a-vibrating-beam");
    const Id beam("ex-me-beam");
    panel().selectStructure(beam);
    trigger("actionRunModal");
    ASSERT_TRUE(waitForResults(1));
    QCoreApplication::processEvents();
    EXPECT_EQ(panel().modeRowCount(), 10);
    EXPECT_NEAR(panel().modeCell(0, 1).toDouble(), 16.4, 0.3);  // first cantilever mode of the 500 x 30 x 5 mm aluminium beam
    // The deformed surface replaces the object's own mesh in the viewport.
    ASSERT_NE(window_->viewport().scalarMesh(), nullptr);
    EXPECT_FALSE(window_->viewport().scalarMesh()->empty());
    const auto frame = window_->viewport().bridge().build(doc().scene(), {});
    bool ownMesh = false;
    for (const auto& pickable : window_->viewport().bridge().pickables())
        ownMesh = ownMesh || pickable.id == beam;
    EXPECT_FALSE(ownMesh);
    (void)frame;
    const QString description = panel().displayText();
    EXPECT_TRUE(description.contains(QStringLiteral("amplitude is arbitrary"))) << description.toStdString();
    EXPECT_TRUE(description.contains(QStringLiteral("magnified x")));
    // A manual magnification is used as given and reported.
    child<QCheckBox>(panel(), "autoScaleCheck")->click();
    child<QDoubleSpinBox>(panel(), "scaleSpin")->setValue(2.5);
    EXPECT_DOUBLE_EQ(mech().scaleFactor(), 2.5);
    EXPECT_TRUE(panel().displayText().contains(QStringLiteral("magnified x2.5")));
    // Other modes; stress is not offered for a modal result.
    auto* item = child<QComboBox>(panel(), "itemCombo");
    item->setCurrentIndex(3);
    emit item->activated(3);
    EXPECT_EQ(mech().displayedItem(), 3);
    auto* quantity = child<QComboBox>(panel(), "quantityCombo");
    const auto* model = qobject_cast<QStandardItemModel*>(quantity->model());
    EXPECT_FALSE(model->item(static_cast<int>(MechanicsController::Quantity::VonMises))->isEnabled());
    EXPECT_FALSE(model->item(static_cast<int>(MechanicsController::Quantity::VelocityMagnitude))->isEnabled());
    EXPECT_TRUE(model->item(static_cast<int>(MechanicsController::Quantity::Z))->isEnabled());
    quantity->setCurrentIndex(static_cast<int>(MechanicsController::Quantity::Z));
    emit quantity->activated(static_cast<int>(MechanicsController::Quantity::Z));
    EXPECT_TRUE(window_->viewport().scalarMesh()->diverging);
    // Animation moves the surface and the colour scale stays fixed.
    const float before = window_->viewport().scalarMesh()->scale;
    child<QCheckBox>(panel(), "animateCheck")->click();
    EXPECT_TRUE(mech().isAnimating());
    QTest::qWait(150);
    EXPECT_FLOAT_EQ(window_->viewport().scalarMesh()->scale, before);
    child<QCheckBox>(panel(), "animateCheck")->click();
    EXPECT_FALSE(mech().isAnimating());
    // The run is recorded in the project.
    EXPECT_EQ(doc().project().extensions["mechanicsRuns"][0]["metadata"]["solver"], "mechanical.fem.modal");
    // Removing the result restores the object's own mesh.
    child<QPushButton>(panel(), "removeResultButton")->click();
    EXPECT_TRUE(mech().results().empty());
    EXPECT_EQ(window_->viewport().scalarMesh(), nullptr);
}

TEST_F(MechanicsUiTest, FrequencyResponseIsPlottedWithModesAndAMeasuredCurve)
{
    open("c-forced-resonance");
    panel().selectStructure(Id("ex-me-beam"));
    ASSERT_TRUE(panel().runModal());
    ASSERT_TRUE(waitForResults(1));
    ASSERT_TRUE(panel().runHarmonic());
    ASSERT_TRUE(waitForResults(2));
    QCoreApplication::processEvents();
    PlotWidget& plot = panel().frfPlot();
    EXPECT_EQ(plot.pointCount(), 800);
    EXPECT_EQ(plot.markers().size(), 10);  // natural frequencies of the modal result
    EXPECT_TRUE(panel().frfNote().contains(QStringLiteral("direct solution")));
    // The displayed frequency is the strongest response; velocity is now offered.
    const auto* model = qobject_cast<QStandardItemModel*>(child<QComboBox>(panel(), "quantityCombo")->model());
    EXPECT_TRUE(model->item(static_cast<int>(MechanicsController::Quantity::VelocityMagnitude))->isEnabled());
    EXPECT_TRUE(panel().displayText().contains(QStringLiteral("(peak amplitude)")));

    // A measured response is imported (stored in the project, undoable) and overlaid.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("bench.csv"));
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("# sensor: A1\n# quantity: displacement\nfrequency_Hz,magnitude,phase_deg\n10,0.002,0\n16,0.05,-90\n40,0.0005,-180\n");
    }
    ASSERT_TRUE(mech().importMeasurement(path).ok());
    QCoreApplication::processEvents();
    EXPECT_EQ(plot.curveCount(), 2);
    EXPECT_TRUE(panel().frfNote().contains(QStringLiteral("RMS level difference"))) << panel().frfNote().toStdString();
    trigger("actionUndo");
    EXPECT_TRUE(mech().measurements().empty());
    EXPECT_FALSE(mech().importMeasurement(QStringLiteral("/nonexistent.csv")).ok());
}

TEST_F(MechanicsUiTest, TransientAndStaticResults)
{
    open("e-wave-propagation");
    panel().selectStructure(Id("ex-mf-rod"));
    ASSERT_TRUE(panel().runTransient());
    ASSERT_TRUE(waitForResults(1));
    QCoreApplication::processEvents();
    EXPECT_GT(panel().transientPlot().pointCount(), 500);
    EXPECT_EQ(mech().itemCount(0), 80);  // recorded instants
    EXPECT_TRUE(mech().itemLabel(0, 0).startsWith(QStringLiteral("t = ")));
    auto* quantity = child<QComboBox>(panel(), "transientQuantityCombo");
    quantity->setCurrentIndex(3);
    emit quantity->activated(3);
    EXPECT_EQ(panel().transientPlot().curveCount(), 2);  // energy and the work done

    open("c-forced-resonance");
    panel().selectStructure(Id("ex-me-beam"));
    ASSERT_TRUE(panel().runStatic());
    ASSERT_TRUE(waitForResults(1));
    QCoreApplication::processEvents();
    const QString text = child<QPlainTextEdit>(panel(), "staticText")->toPlainText();
    EXPECT_TRUE(text.contains(QStringLiteral("Largest stress"))) << text.toStdString();
    const auto* model = qobject_cast<QStandardItemModel*>(child<QComboBox>(panel(), "quantityCombo")->model());
    EXPECT_TRUE(model->item(static_cast<int>(MechanicsController::Quantity::VonMises))->isEnabled());
    // A structure without an actuator cannot be loaded statically.
    open("a-vibrating-beam");
    panel().selectStructure(Id("ex-me-beam"));
    EXPECT_FALSE(panel().runStatic());
    EXPECT_TRUE(panel().statusText().contains(QStringLiteral("actuator")));
}

TEST_F(MechanicsUiTest, TargetingSolvesAppliesAndUndoes)
{
    open("f-vibration-concentration");
    const Id plate("ex-mg-plate");
    panel().selectStructure(plate);
    // Reset the actuators to a plain drive, then solve.
    for (int i = 1; i <= 4; ++i) {
        const Id id("ex-mg-a" + std::to_string(i));
        ActuatorSettings s = dynamic_cast<const ActuatorComponent*>(doc().scene().find(id)->state.component.get())->settings;
        s.amplitude = 1.0;
        s.phase = 0.0;
        ASSERT_TRUE(mech().setComponent(id, std::make_shared<ActuatorComponent>(s), QStringLiteral("Reset")).ok());
    }
    ASSERT_TRUE(panel().solveTargets());
    EXPECT_TRUE(panel().targetText().contains(QStringLiteral("relative residual")));
    EXPECT_TRUE(child<QPushButton>(panel(), "applyTargetsButton")->isEnabled());
    ASSERT_TRUE(panel().applyTargets());
    const double applied = dynamic_cast<const ActuatorComponent*>(doc().scene().find(Id("ex-mg-a1"))->state.component.get())->settings.amplitude;
    EXPECT_NE(applied, 1.0);
    // One undo step restores all four.
    trigger("actionUndo");
    for (int i = 1; i <= 4; ++i)
        EXPECT_DOUBLE_EQ(dynamic_cast<const ActuatorComponent*>(doc().scene().find(Id("ex-mg-a" + std::to_string(i)))->state.component.get())->settings.amplitude,
                         1.0);
}

TEST_F(MechanicsUiTest, BandStructureAndSoundRadiatedByAPlate)
{
    open("g-phononic-rod");
    panel().selectStructure(Id("ex-mh-rod"));
    ASSERT_TRUE(panel().showBandStructure());
    EXPECT_EQ(panel().bandPlot().curveCount(), 4);
    EXPECT_TRUE(child<QLabel>(panel(), "bandNote")->text().contains(QStringLiteral("Stop bands")));
    // Not a periodic rod: refused with the reason.
    open("b-vibrating-plate");
    panel().selectStructure(Id("ex-mb-plate"));
    EXPECT_FALSE(panel().showBandStructure());

    // A plate's frequency response radiating onto an acoustic plane above it.
    trigger("actionAddFieldPlane");
    const Id region = doc().primarySelection();
    acoustics::RegionSettings plane;
    plane.kind = acoustics::RegionKind::PlaneXZ;
    plane.size = Vec3(0.4, 0.1, 0.3);
    plane.countU = plane.countV = 21;
    ASSERT_TRUE(window_->acoustics().setComponent(region, std::make_shared<acoustics::RegionComponent>(plane), QStringLiteral("Edit")).ok());
    ASSERT_TRUE(doc().setTransform(region, Transform{Vec3(0, 0, 0.2)}, QStringLiteral("Move")).ok());
    doc().setSelection({Id("ex-mb-plate")});
    trigger("actionAddActuator");
    MechanicsSettings s = mech().settings();
    s.frequencyMin = 50.0;
    s.frequencyMax = 60.0;
    s.frequencyPoints = 3;
    ASSERT_TRUE(mech().setSettings(s).ok());
    ASSERT_TRUE(panel().runHarmonic());
    ASSERT_TRUE(waitForResults(1));
    QCoreApplication::processEvents();
    auto* regions = child<QComboBox>(panel(), "radiateRegionCombo");
    regions->setCurrentIndex(regions->findText(QString::fromStdString(doc().scene().find(region)->state.name)));
    ASSERT_TRUE(panel().radiate()) << panel().statusText().toStdString();
    ASSERT_EQ(window_->acoustics().results().size(), 1u);
    EXPECT_EQ(window_->acoustics().results()[0]->metadata.solverId, "coupling.structure-to-acoustic.rayleigh");
    EXPECT_GT(window_->acoustics().results()[0]->statistics.maxMagnitude, 0.0);
}

TEST_F(MechanicsUiTest, AnalysisSettingsAreUndoableAndSaved)
{
    open("a-vibrating-beam");
    auto* modes = child<QSpinBox>(panel(), "modeCountSpin");
    modes->setValue(5);
    EXPECT_EQ(mech().settings().modeCount, 5);
    auto* damping = child<QComboBox>(panel(), "dampingCombo");
    damping->setCurrentIndex(1);
    emit damping->activated(1);
    EXPECT_EQ(mech().settings().damping.kind, DampingKind::Rayleigh);
    EXPECT_TRUE(child<QDoubleSpinBox>(panel(), "alphaSpin")->isEnabled());
    EXPECT_FALSE(child<QDoubleSpinBox>(panel(), "etaSpin")->isEnabled());
    trigger("actionUndo");
    EXPECT_EQ(mech().settings().damping.kind, DampingKind::LossFactor);
    trigger("actionUndo");
    EXPECT_EQ(mech().settings().modeCount, 10);
    EXPECT_TRUE(doc().project().extensions.contains("mechanics"));
}
