// The acoustic workflow driven through the real main window.
#include "AcousticController.h"
#include "AcousticsPanel.h"
#include "AnalysisPanel.h"
#include "DisplayUnits.h"
#include "DocumentController.h"
#include "FieldAnalysisPanel.h"
#include "MainWindow.h"
#include "PropertiesPanel.h"
#include "SceneTreePanel.h"
#include "ViewportWidget.h"
#include "foundation/Units.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QTest>
#include <QTreeWidgetItem>

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::acoustics;

namespace {

class AcousticsUiTest : public ::testing::Test {
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
    AcousticController& acoustics() { return window_->acoustics(); }
    template <typename T>
    const T* component(const Id& id)
    {
        const SceneObject* object = doc().scene().find(id);
        return object ? componentOf<T>(*object) : nullptr;
    }
    template <typename T>
    T* child(QWidget& parent, const char* name)
    {
        T* widget = parent.findChild<T*>(QString::fromLatin1(name));
        EXPECT_NE(widget, nullptr) << name;
        return widget;
    }
    // Adds the standard set-up: a planar array, a target and a field plane.
    void standardScene()
    {
        trigger("actionAddPlanarArray");
        array_ = doc().primarySelection();
        trigger("actionAddTarget");
        target_ = doc().primarySelection();
        trigger("actionAddFieldPlane");
        region_ = doc().primarySelection();
    }
    bool waitForResults(size_t count)
    {
        return QTest::qWaitFor([&] { return acoustics().results().size() >= count; }, 30000);
    }

    std::unique_ptr<MainWindow> window_;
    Id array_, target_, region_;
};

}  // namespace

TEST_F(AcousticsUiTest, AcousticObjectsAreCreatedShownAndPickable)
{
    trigger("actionAddEmitter");
    const Id emitter = doc().primarySelection();
    ASSERT_NE(component<EmitterComponent>(emitter), nullptr);
    EXPECT_EQ(window_->sceneTree().itemFor(emitter)->text(1), QStringLiteral("Acoustic emitter"));
    // The emitter starts at the simulation frequency.
    EXPECT_DOUBLE_EQ(component<EmitterComponent>(emitter)->settings.frequencyHz, acoustics().settings().frequencyHz);

    const char* arrays[] = {"actionAddLinearArray", "actionAddPlanarArray", "actionAddCircularArray",
                            "actionAddCylindricalArray", "actionAddSphericalArray", "actionAddCustomArray"};
    const ArrayLayout layouts[] = {ArrayLayout::Linear, ArrayLayout::RectangularPlanar, ArrayLayout::Circular,
                                   ArrayLayout::Cylindrical, ArrayLayout::Spherical, ArrayLayout::Custom};
    for (int i = 0; i < 6; ++i) {
        trigger(arrays[i]);
        const auto* array = component<ArrayComponent>(doc().primarySelection());
        ASSERT_NE(array, nullptr) << arrays[i];
        EXPECT_EQ(array->settings.layout, layouts[i]);
        EXPECT_TRUE(array->settings.validate().ok());
        ASSERT_TRUE(doc().deleteSelection().ok());
    }
    trigger("actionAddTarget");
    EXPECT_NE(component<TargetComponent>(doc().primarySelection()), nullptr);
    trigger("actionAddProbe");
    EXPECT_NE(component<ProbeComponent>(doc().primarySelection()), nullptr);
    trigger("actionAddFieldPlane");
    EXPECT_FALSE(component<RegionComponent>(doc().primarySelection())->settings.isVolume());
    trigger("actionAddFieldVolume");
    EXPECT_TRUE(component<RegionComponent>(doc().primarySelection())->settings.isVolume());
    EXPECT_EQ(window_->sceneTree().itemFor(doc().primarySelection())->text(1), QStringLiteral("Field volume"));

    // The emitter's marker can be clicked in the viewport.
    doc().setSelection({emitter});
    trigger("actionFrameSelection");
    doc().clearSelection();
    QCoreApplication::processEvents();
    EXPECT_EQ(window_->viewport().objectAt(window_->viewport().rect().center()), emitter);

    // Creating objects is undoable like everything else.
    const size_t count = doc().scene().objectCount();
    trigger("actionUndo");
    EXPECT_EQ(doc().scene().objectCount(), count - 1);
}

TEST_F(AcousticsUiTest, EmitterEditorEditsInDisplayUnitsAndKeepsUntouchedFields)
{
    trigger("actionAddEmitter");
    const Id id = doc().primarySelection();
    // Give the emitter a phase with more digits than the editor shows.
    EmitterSettings precise = component<EmitterComponent>(id)->settings;
    precise.phaseRad = 0.123456789012;
    ASSERT_TRUE(acoustics().setComponent(id, std::make_shared<EmitterComponent>(precise), QStringLiteral("Edit")).ok());
    QCoreApplication::processEvents();

    QWidget* editor = window_->properties().findChild<QWidget*>(QStringLiteral("EmitterEditor"));
    ASSERT_NE(editor, nullptr);
    child<QDoubleSpinBox>(*editor, "amplitudeSpin")->setValue(7.5);
    child<QDoubleSpinBox>(*editor, "delaySpin")->setValue(12.0);  // microseconds
    const EmitterSettings& after = component<EmitterComponent>(id)->settings;
    EXPECT_DOUBLE_EQ(after.amplitudePa, 7.5);
    EXPECT_NEAR(after.delaySec, 12e-6, 1e-15);
    EXPECT_DOUBLE_EQ(after.phaseRad, 0.123456789012);  // not rounded by editing other fields
    EXPECT_NEAR(child<QDoubleSpinBox>(*editor, "phaseSpin")->value(), units::radiansToDegrees(0.123456789012), 1e-3);

    // The piston radius follows the display length unit.
    trigger("actionUnitsMillimeters");
    child<QComboBox>(*editor, "directivityCombo")->setCurrentIndex(2);
    emit child<QComboBox>(*editor, "directivityCombo")->activated(2);
    child<QDoubleSpinBox>(*editor, "pistonRadiusSpin")->setValue(8.0);
    EXPECT_EQ(component<EmitterComponent>(id)->settings.directivity.model, DirectivityModel::BaffledPiston);
    EXPECT_NEAR(component<EmitterComponent>(id)->settings.directivity.pistonRadius, 0.008, 1e-12);
    trigger("actionUnitsMeters");

    trigger("actionUndo");
    EXPECT_NEAR(component<EmitterComponent>(id)->settings.directivity.pistonRadius, 0.005, 1e-12);
}

TEST_F(AcousticsUiTest, ArrayEditorChangesLayoutAndKeepsPerElementSettings)
{
    trigger("actionAddLinearArray");
    const Id id = doc().primarySelection();
    QWidget* editor = window_->properties().findChild<QWidget*>(QStringLiteral("ArrayEditor"));
    ASSERT_NE(editor, nullptr);
    auto* table = child<QTableWidget>(*editor, "elementTable");
    ASSERT_EQ(table->rowCount(), 16);

    // Give element 3 its own phase (degrees in the table) and switch element 5 off.
    table->item(3, 5)->setText(QStringLiteral("90"));
    table = child<QTableWidget>(*editor, "elementTable");
    table->item(5, 7)->setCheckState(Qt::Unchecked);
    const auto* array = component<ArrayComponent>(id);
    EXPECT_NEAR(*array->settings.overrides.at(3).phaseRad, units::pi / 2.0, 1e-12);
    EXPECT_FALSE(*array->settings.overrides.at(5).active);
    EXPECT_EQ(collectSources(doc().scene()).size(), 15u);

    // Changing array parameters regenerates positions and keeps both.
    child<QSpinBox>(*editor, "countXSpin")->setValue(20);
    array = component<ArrayComponent>(id);
    EXPECT_EQ(array->settings.countX, 20);
    EXPECT_EQ(child<QTableWidget>(*editor, "elementTable")->rowCount(), 20);
    EXPECT_NEAR(*array->settings.overrides.at(3).phaseRad, units::pi / 2.0, 1e-12);
    EXPECT_EQ(collectSources(doc().scene()).size(), 19u);

    // A negative amplitude is refused and the table shows the real value again.
    child<QTableWidget>(*editor, "elementTable")->item(0, 4)->setText(QStringLiteral("-1"));
    EXPECT_FALSE(component<ArrayComponent>(id)->settings.overrides.count(0)
                 && component<ArrayComponent>(id)->settings.overrides.at(0).amplitudePa.has_value());
    EXPECT_EQ(child<QTableWidget>(*editor, "elementTable")->item(0, 4)->text(), QStringLiteral("1"));

    emit child<QPushButton>(*editor, "clearOverridesButton")->clicked();
    EXPECT_TRUE(component<ArrayComponent>(id)->settings.overrides.empty());
    trigger("actionUndo");
    EXPECT_FALSE(component<ArrayComponent>(id)->settings.overrides.empty());
}

TEST_F(AcousticsUiTest, MediumSettingsAreValidatedSavedWithTheProjectAndUndoable)
{
    AcousticsPanel& panel = window_->acousticsPanel();
    EXPECT_EQ(acoustics().settings().medium.name, "Air");
    const double coolSpeed = acoustics().settings().medium.soundSpeed;

    // Raising the air temperature raises the speed of sound (ideal-gas model).
    child<QDoubleSpinBox>(panel, "temperatureSpin")->setValue(40.0);
    EXPECT_NEAR(acoustics().settings().medium.temperatureK, 313.15, 1e-9);
    EXPECT_GT(acoustics().settings().medium.soundSpeed, coolSpeed);
    EXPECT_FALSE(child<QDoubleSpinBox>(panel, "soundSpeedSpin")->isEnabled());  // derived, not free

    auto* preset = child<QComboBox>(panel, "mediumPreset");
    preset->setCurrentIndex(1);
    emit preset->activated(1);
    EXPECT_EQ(acoustics().settings().medium.name, "Water");
    EXPECT_NEAR(acoustics().settings().medium.soundSpeed, 1482.0, 1e-9);
    child<QDoubleSpinBox>(panel, "frequencySpin")->setValue(1.0e6);
    EXPECT_DOUBLE_EQ(acoustics().settings().frequencyHz, 1.0e6);
    EXPECT_TRUE(doc().isModified());

    const QString path = QDir::tempPath() + QStringLiteral("/resonant-app-tests/medium.resonant");
    QDir().mkpath(QDir::tempPath() + QStringLiteral("/resonant-app-tests"));
    ASSERT_TRUE(window_->saveProjectToFile(path).ok());
    trigger("actionNew");
    EXPECT_EQ(acoustics().settings().medium.name, "Air");
    ASSERT_TRUE(window_->openProjectFile(path).ok());
    EXPECT_EQ(acoustics().settings().medium.name, "Water");
    EXPECT_DOUBLE_EQ(acoustics().settings().frequencyHz, 1.0e6);
    EXPECT_EQ(child<QComboBox>(panel, "mediumPreset")->currentIndex(), 1);

    // Undo steps back through the settings.
    child<QDoubleSpinBox>(panel, "frequencySpin")->setValue(2.0e6);
    trigger("actionUndo");
    EXPECT_DOUBLE_EQ(acoustics().settings().frequencyHz, 1.0e6);
    EXPECT_DOUBLE_EQ(child<QDoubleSpinBox>(panel, "frequencySpin")->value(), 1.0e6);
}

TEST_F(AcousticsUiTest, FocusWorkflowSelectArrayTargetControlCalculateApplySimulateInspect)
{
    standardScene();
    AcousticsPanel& panel = window_->acousticsPanel();
    ASSERT_TRUE(doc().setTransform(target_, Transform{Vec3(0.01, 0.0, 0.08)}, QStringLiteral("Move")).ok());
    QCoreApplication::processEvents();
    const Vec3 target(0.01, 0.0, 0.08);

    const auto evaluateAtTarget = [&] { return std::abs(acoustics().currentEvaluator()->pressure(target)); };
    const double unfocused = evaluateAtTarget();

    // Steps 1-4: array, target, control, calculate.
    panel.selectFocusArray(array_);
    panel.selectFocusTarget(target_);
    EXPECT_FALSE(child<QDoubleSpinBox>(panel, "focusX")->isEnabled());  // coordinates come from the target object
    EXPECT_NEAR(child<QDoubleSpinBox>(panel, "focusZ")->value(), 0.08, 1e-9);
    ASSERT_TRUE(panel.calculateFocus());
    auto* table = child<QTableWidget>(panel, "focusTable");
    EXPECT_EQ(table->rowCount(), 64);
    EXPECT_EQ(table->item(0, 3)->text(), QStringLiteral("-"));  // phase control: no delays
    // Calculating changes nothing in the scene.
    EXPECT_TRUE(component<ArrayComponent>(array_)->settings.overrides.empty());
    EXPECT_NEAR(evaluateAtTarget(), unfocused, 1e-12);

    // Step 5: apply.
    ASSERT_TRUE(panel.applyFocus());
    EXPECT_EQ(component<ArrayComponent>(array_)->settings.overrides.size(), 64u);
    double inPhaseSum = 0.0;
    for (const PointSource& source : acoustics().currentEvaluator()->sources())
        inPhaseSum += 1.0 / (target - source.position).norm();
    EXPECT_NEAR(evaluateAtTarget(), inPhaseSum, 1e-6 * inPhaseSum);
    EXPECT_GT(evaluateAtTarget(), unfocused);

    // Step 6: run a field simulation over the plane.
    panel.selectRegion(region_);
    EXPECT_TRUE(panel.estimateText().contains(QStringLiteral("samples")));
    EXPECT_TRUE(panel.estimateText().contains(QStringLiteral("objects in the scene are ignored")));
    trigger("actionRunField");
    ASSERT_TRUE(waitForResults(1));
    QCoreApplication::processEvents();

    // Step 7: inspect. The result is displayed, summarized and reported at the target.
    const auto result = acoustics().displayedResult();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->sources.size(), 64u);
    // Across the plane at the target's depth the field peaks at the target's
    // x. (The largest value of the whole plane is next to the array, where
    // 1/r dominates: this small aperture focuses only weakly at 80 mm.)
    const int row = 45;  // z = 0.005 m + 45 * (0.2 m / 120) = 0.08 m
    ASSERT_NEAR(result->frame.point(0, row, 0).z(), 0.08, 1e-9);
    int best = 0;
    for (int iu = 1; iu < result->frame.nu; ++iu)
        if (std::abs(result->pressure[result->frame.index(iu, row, 0)]) > std::abs(result->pressure[result->frame.index(best, row, 0)]))
            best = iu;
    EXPECT_NEAR(result->frame.point(best, row, 0).x(), 0.01, 0.002);
    FieldAnalysisPanel& analysis = window_->fieldAnalysis();
    const QStringList summary = analysis.summaryItems();
    const auto has = [&summary](const QString& prefix) {
        return std::any_of(summary.begin(), summary.end(), [&](const QString& s) { return s.startsWith(prefix); });
    };
    EXPECT_TRUE(has(QStringLiteral("Frequency=40000 Hz")));
    EXPECT_TRUE(has(QStringLiteral("Medium=Air")));
    EXPECT_TRUE(has(QStringLiteral("Sources in result=64")));
    EXPECT_TRUE(has(QStringLiteral("Maximum |P|=")));
    EXPECT_TRUE(has(QStringLiteral("Solver=acoustic.freefield.analytical")));
    EXPECT_TRUE(has(QStringLiteral("Assumption 2=Objects in the scene do NOT influence")));
    ASSERT_EQ(analysis.pointRowCount(), 1);
    EXPECT_EQ(analysis.pointCell(0, 1), QStringLiteral("Target"));
    EXPECT_NEAR(analysis.pointCell(0, 5).toDouble(), inPhaseSum, 1e-3 * inPhaseSum);

    // Undo removes the focus settings; the stored result is untouched.
    const auto stored = result->pressure;
    doc().setSelection({});
    while (doc().commands().canUndo() && !component<ArrayComponent>(array_)->settings.overrides.empty())
        trigger("actionUndo");
    EXPECT_NEAR(evaluateAtTarget(), unfocused, 1e-9);
    EXPECT_EQ(acoustics().displayedResult()->pressure, stored);
}

TEST_F(AcousticsUiTest, TimeDelayFocusWritesDelaysNotPhases)
{
    standardScene();
    AcousticsPanel& panel = window_->acousticsPanel();
    panel.selectFocusArray(array_);
    panel.selectFocusTarget(target_);
    child<QComboBox>(panel, "focusControlCombo")->setCurrentIndex(1);
    child<QComboBox>(panel, "focusTaperCombo")->setCurrentIndex(1);  // Hann
    ASSERT_TRUE(panel.calculateFocus());
    EXPECT_EQ(child<QTableWidget>(panel, "focusTable")->item(0, 2)->text(), QStringLiteral("-"));
    ASSERT_TRUE(panel.applyFocus());
    const ArraySettings& s = component<ArrayComponent>(array_)->settings;
    double maxDelay = 0.0;
    for (const auto& [index, o] : s.overrides) {
        EXPECT_TRUE(o.delaySec.has_value());
        EXPECT_FALSE(o.phaseRad.has_value());
        EXPECT_TRUE(o.amplitudePa.has_value());  // the taper
        EXPECT_GE(*o.delaySec, 0.0);
        maxDelay = std::max(maxDelay, *o.delaySec);
    }
    EXPECT_GT(maxDelay, 0.0);
}

TEST_F(AcousticsUiTest, InverseDesignSolvesAppliesAndReportsWhatItCouldNotAchieve)
{
    standardScene();
    AcousticsPanel& panel = window_->acousticsPanel();
    // Two targets with different wanted values and one to suppress.
    TargetSettings a;
    a.desiredAmplitudePa = 50.0;
    ASSERT_TRUE(acoustics().setComponent(target_, std::make_shared<TargetComponent>(a), QStringLiteral("Edit")).ok());
    ASSERT_TRUE(doc().setTransform(target_, Transform{Vec3(-0.015, 0, 0.08)}, QStringLiteral("Move")).ok());
    trigger("actionAddTarget");
    const Id second = doc().primarySelection();
    TargetSettings b;
    b.suppress = true;
    ASSERT_TRUE(acoustics().setComponent(second, std::make_shared<TargetComponent>(b), QStringLiteral("Edit")).ok());
    ASSERT_TRUE(doc().setTransform(second, Transform{Vec3(0.015, 0, 0.08)}, QStringLiteral("Move")).ok());
    // A target kind no solver handles is reported, not silently used.
    trigger("actionAddTarget");
    TargetSettings plane;
    plane.kind = TargetKind::Plane;
    ASSERT_TRUE(acoustics().setComponent(doc().primarySelection(), std::make_shared<TargetComponent>(plane), QStringLiteral("Edit")).ok());
    QCoreApplication::processEvents();

    panel.selectInverseArray(array_);
    ASSERT_TRUE(panel.solveInverse());
    EXPECT_TRUE(panel.inverseSummaryText().contains(QStringLiteral("Targets achieved within tolerance")));
    EXPECT_TRUE(panel.inverseSummaryText().contains(QStringLiteral("is not a point target and was not used")));
    EXPECT_EQ(child<QTableWidget>(panel, "inverseTable")->rowCount(), 2);
    ASSERT_TRUE(panel.applyInverse());
    const auto field = acoustics().currentEvaluator();
    EXPECT_NEAR(std::abs(field->pressure(Vec3(-0.015, 0, 0.08))), 50.0, 0.05);
    EXPECT_LT(std::abs(field->pressure(Vec3(0.015, 0, 0.08))), 0.05);

    // An amplitude limit far below what is needed: clipped, and said so.
    child<QLineEdit>(panel, "amplitudeLimitEdit")->setText(QStringLiteral("0.01"));
    ASSERT_TRUE(panel.solveInverse());
    EXPECT_TRUE(panel.inverseSummaryText().contains(QStringLiteral("TARGETS NOT ACHIEVED ACCURATELY")));
    EXPECT_TRUE(panel.inverseSummaryText().contains(QStringLiteral("clipped")));
    // Bad input is refused with an explanation.
    child<QLineEdit>(panel, "regularizationEdit")->setText(QStringLiteral("minus one"));
    EXPECT_FALSE(panel.solveInverse());
    EXPECT_TRUE(panel.inverseSummaryText().contains(QStringLiteral("regularization must be a number")));
}

TEST_F(AcousticsUiTest, FieldIsDrawnFromStoredDataAndDisplayChangesDoNotRecompute)
{
    standardScene();
    AcousticsPanel& panel = window_->acousticsPanel();
    panel.selectRegion(region_);
    doc().clearSelection();
    trigger("actionFrameAll");
    ViewportWidget& viewport = window_->viewport();
    QCoreApplication::processEvents();
    const QImage before = viewport.grabFramebuffer();

    ASSERT_TRUE(panel.runSimulation());
    ASSERT_TRUE(waitForResults(1));
    QCoreApplication::processEvents();
    const QImage magnitude = viewport.grabFramebuffer();
    EXPECT_NE(before, magnitude);  // the field plane now shows colours
    const int uploads = viewport.renderer().field().uploadCount();
    EXPECT_EQ(uploads, 1);
    const size_t jobs = doc().jobs().jobs().size();

    // Quantity, normalization, camera: new pictures, no new calculation, no new upload.
    auto* quantity = child<QComboBox>(panel, "quantityCombo");
    quantity->setCurrentIndex(1);
    emit quantity->activated(1);
    QCoreApplication::processEvents();
    const QImage phase = viewport.grabFramebuffer();
    EXPECT_NE(magnitude, phase);
    EXPECT_TRUE(child<QLabel>(panel, "displayDescription")->text().contains(QStringLiteral("phase")));

    quantity->setCurrentIndex(0);
    emit quantity->activated(0);
    EXPECT_TRUE(child<QLabel>(panel, "displayDescription")->text().contains(QStringLiteral("NORMALIZED")));
    auto* normalization = child<QComboBox>(panel, "normalizationCombo");
    normalization->setCurrentIndex(1);
    emit normalization->activated(1);
    child<QDoubleSpinBox>(panel, "absoluteScaleSpin")->setValue(1e6);  // everything far below the scale top
    QCoreApplication::processEvents();
    EXPECT_TRUE(child<QLabel>(panel, "displayDescription")->text().contains(QStringLiteral("ABSOLUTE")));
    EXPECT_NE(viewport.grabFramebuffer(), magnitude);

    trigger("actionViewFront");
    QCoreApplication::processEvents();
    viewport.grabFramebuffer();
    EXPECT_EQ(viewport.renderer().field().uploadCount(), uploads);
    EXPECT_EQ(doc().jobs().jobs().size(), jobs);

    // Animation advances the phase of the instantaneous view without recomputing.
    child<QCheckBox>(panel, "animateCheck")->setChecked(true);
    EXPECT_EQ(acoustics().display().quantity, render::FieldQuantity::Instantaneous);
    const double phase0 = acoustics().display().animationPhase;
    ASSERT_TRUE(QTest::qWaitFor([&] { return acoustics().display().animationPhase != phase0; }, 2000));
    child<QCheckBox>(panel, "animateCheck")->setChecked(false);
    EXPECT_EQ(viewport.renderer().field().uploadCount(), uploads);
    EXPECT_EQ(doc().jobs().jobs().size(), jobs);

    // Hiding the field brings back the picture without it.
    child<QCheckBox>(panel, "showFieldCheck")->setChecked(false);
    trigger("actionResetCamera");
    trigger("actionFrameAll");
    QCoreApplication::processEvents();
    EXPECT_EQ(viewport.grabFramebuffer(), before);
}

TEST_F(AcousticsUiTest, VolumeSimulationRunsWithSlicesAndSeveralResultsCanBeCompared)
{
    trigger("actionAddPlanarArray");
    trigger("actionAddFieldVolume");
    const Id volume = doc().primarySelection();
    RegionSettings small = component<RegionComponent>(volume)->settings;
    small.countU = small.countV = small.countW = 25;
    ASSERT_TRUE(acoustics().setComponent(volume, std::make_shared<RegionComponent>(small), QStringLiteral("Edit")).ok());
    QCoreApplication::processEvents();

    AcousticsPanel& panel = window_->acousticsPanel();
    panel.selectRegion(volume);
    ASSERT_TRUE(panel.runSimulation());
    ASSERT_TRUE(waitForResults(1));
    QCoreApplication::processEvents();
    const auto first = acoustics().displayedResult();
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(first->frame.isVolume());
    EXPECT_EQ(first->pressure.size(), 25u * 25u * 25u);
    EXPECT_TRUE(child<QWidget>(panel, "sliceGroup")->isEnabled());
    child<QSlider>(panel, "sliceWSlider")->setValue(100);
    EXPECT_NEAR(acoustics().display().sliceW, 0.1, 1e-9);
    child<QCheckBox>(panel, "sliceUCheck")->setChecked(false);
    EXPECT_FALSE(acoustics().display().showSliceU);

    // A second run with a different configuration is a second, separate result.
    AcousticSettings settings = acoustics().settings();
    settings.frequencyHz = 20000.0;
    ASSERT_TRUE(acoustics().setSettings(settings).ok());
    const auto* array = component<ArrayComponent>(acoustics().objectsOfType(acoustics::object_types::Array).front().first);
    ArraySettings retuned = array->settings;
    retuned.element.frequencyHz = 20000.0;
    ASSERT_TRUE(acoustics().setComponent(acoustics().objectsOfType(acoustics::object_types::Array).front().first,
                                         std::make_shared<ArrayComponent>(retuned), QStringLiteral("Edit")).ok());
    ASSERT_TRUE(panel.runSimulation());
    ASSERT_TRUE(waitForResults(2));
    QCoreApplication::processEvents();
    ASSERT_EQ(acoustics().results().size(), 2u);
    EXPECT_DOUBLE_EQ(acoustics().results()[0]->frequencyHz, 40000.0);
    EXPECT_DOUBLE_EQ(acoustics().results()[1]->frequencyHz, 20000.0);
    EXPECT_NE(acoustics().results()[0]->pressure, acoustics().results()[1]->pressure);
    EXPECT_EQ(child<QListWidget>(panel, "resultList")->count(), 2);

    // Switching between them changes what is displayed and summarized.
    acoustics().displayResult(acoustics().results()[0]->id);
    QCoreApplication::processEvents();
    EXPECT_EQ(acoustics().displayedResult()->id, first->id);
    // Each run left a record with its configuration in the project.
    const auto& runs = doc().project().extensions["acousticRuns"];
    ASSERT_EQ(runs.size(), 2u);
    EXPECT_DOUBLE_EQ(runs[1]["metadata"]["configuration"]["settings"]["frequencyHz"].get<double>(), 20000.0);
    EXPECT_EQ(runs[0]["metadata"]["configuration"]["sources"].size(), 64u);
    EXPECT_EQ(runs[0]["metadata"]["projectId"], doc().project().id.str());
}

TEST_F(AcousticsUiTest, ExcessiveResolutionIsRefusedBeforeAnythingRuns)
{
    trigger("actionAddEmitter");
    trigger("actionAddFieldVolume");
    const Id volume = doc().primarySelection();
    RegionSettings huge = component<RegionComponent>(volume)->settings;
    huge.countU = huge.countV = huge.countW = 1200;  // 1.7 billion samples
    ASSERT_TRUE(acoustics().setComponent(volume, std::make_shared<RegionComponent>(huge), QStringLiteral("Edit")).ok());
    QCoreApplication::processEvents();

    AcousticsPanel& panel = window_->acousticsPanel();
    panel.selectRegion(volume);
    EXPECT_TRUE(panel.estimateText().contains(QStringLiteral("more than the limit")));
    EXPECT_FALSE(child<QPushButton>(panel, "runButton")->isEnabled());
    const size_t jobs = doc().jobs().jobs().size();
    EXPECT_FALSE(panel.runSimulation());
    EXPECT_EQ(doc().jobs().jobs().size(), jobs);
}

TEST_F(AcousticsUiTest, ProbesReportAndExport)
{
    trigger("actionAddEmitter");
    trigger("actionAddProbe");
    const Id probe = doc().primarySelection();
    ASSERT_TRUE(doc().setTransform(probe, Transform{Vec3(0, 0, 0.5)}, QStringLiteral("Move")).ok());
    QCoreApplication::processEvents();

    FieldAnalysisPanel& analysis = window_->fieldAnalysis();
    ASSERT_EQ(analysis.pointRowCount(), 1);
    EXPECT_EQ(analysis.pointCell(0, 1), QStringLiteral("Probe"));
    EXPECT_NEAR(analysis.pointCell(0, 5).toDouble(), 2.0, 1e-5);   // 1 Pa at 1 m -> 2 Pa at 0.5 m
    EXPECT_NEAR(analysis.pointCell(0, 7).toDouble(), std::sqrt(2.0), 1e-5);
    EXPECT_TRUE(analysis.pointCell(0, 8).contains(QStringLiteral("re 20 uPa")));

    // Moving the probe updates the report.
    ASSERT_TRUE(doc().setTransform(probe, Transform{Vec3(0, 0, 0.25)}, QStringLiteral("Move")).ok());
    QCoreApplication::processEvents();
    EXPECT_NEAR(analysis.pointCell(0, 5).toDouble(), 4.0, 1e-5);

    const QString dir = QDir::tempPath() + QStringLiteral("/resonant-app-tests");
    QDir().mkpath(dir);
    ASSERT_TRUE(acoustics().exportPointReports(dir + QStringLiteral("/points.csv")).ok());
    ASSERT_TRUE(acoustics().exportPointReports(dir + QStringLiteral("/points.json")).ok());
    QFile csv(dir + QStringLiteral("/points.csv"));
    ASSERT_TRUE(csv.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(csv.readAll());
    EXPECT_TRUE(text.startsWith(QStringLiteral("name,x_m,y_m,z_m,")));
    EXPECT_TRUE(text.contains(QStringLiteral("Probe,0,0,0.25,")));
    QFile json(dir + QStringLiteral("/points.json"));
    ASSERT_TRUE(json.open(QIODevice::ReadOnly));
    EXPECT_TRUE(json.readAll().contains("resonant-point-report"));
    EXPECT_FALSE(acoustics().exportPointReports(QStringLiteral("/no-such-directory/points.csv")).ok());
}

TEST_F(AcousticsUiTest, ExampleProjectsOpenAndSimulate)
{
    for (const char* name : {"a-two-source-interference", "b-linear-array", "c-focused-planar-array", "d-multiple-target-points"}) {
        const QString path = QStringLiteral(RESONANT_SOURCE_DIR) + QStringLiteral("/examples/acoustics/") + QString::fromLatin1(name)
                             + QStringLiteral(".resonant");
        ASSERT_TRUE(window_->openProjectFile(path).ok()) << name;
        EXPECT_TRUE(acoustics().results().empty());  // results belong to the project that produced them
        ASSERT_TRUE(window_->acousticsPanel().runSimulation()) << name;
        ASSERT_TRUE(waitForResults(1)) << name;
        QCoreApplication::processEvents();
        EXPECT_GT(acoustics().displayedResult()->statistics.maxMagnitude, 0.0) << name;
        EXPECT_FALSE(acoustics().displayedResult()->assumptions.empty());
    }
}

TEST_F(AcousticsUiTest, FoundationStillWorksAlongsideAcoustics)
{
    // Geometry may be present; the free-field model ignores it and says so.
    trigger("actionAddBox");
    trigger("actionAddEmitter");
    const double alone = std::abs(acoustics().currentEvaluator()->pressure(Vec3(0, 0, 1)));
    EXPECT_NEAR(alone, 1.0, 1e-9);
    const auto assumptions = acoustics().currentEvaluator()->model().assumptions();
    EXPECT_NE(assumptions[1].find("do NOT influence"), std::string::npos);
    EXPECT_EQ(doc().scene().objectCount(), 2u);
    trigger("actionUndo");
    trigger("actionUndo");
    EXPECT_EQ(doc().scene().objectCount(), 0u);
}
