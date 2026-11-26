// Wave manipulators driven through the real main window.
#include "AcousticController.h"
#include "AcousticsPanel.h"
#include "DisplayUnits.h"
#include "DocumentController.h"
#include "FieldAnalysisPanel.h"
#include "MainWindow.h"
#include "ManipulatorController.h"
#include "ManipulatorEditor.h"
#include "PropertiesPanel.h"
#include "SceneTreePanel.h"
#include "ViewportWidget.h"
#include "manipulators/Analytics.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTest>
#include <QTreeWidgetItem>

#include <gtest/gtest.h>

using namespace resonant;
using namespace resonant::acoustics;
using namespace resonant::manipulators;

namespace {

class ManipulatorsUiTest : public ::testing::Test {
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
    const ManipulatorComponent* component(const Id& id)
    {
        const SceneObject* object = doc().scene().find(id);
        return object ? dynamic_cast<const ManipulatorComponent*>(object->state.component.get()) : nullptr;
    }
    // The editor of the shown object. Editors of previously shown objects
    // are deleted later by the event loop; let that happen first.
    ManipulatorEditor* editor()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        return window_->properties().findChild<ManipulatorEditor*>();
    }
    template <typename T>
    T* child(QWidget& parent, const char* name)
    {
        T* widget = parent.findChild<T*>(QString::fromLatin1(name));
        EXPECT_NE(widget, nullptr) << name;
        return widget;
    }
    void selectReflectorModel(bool on)
    {
        AcousticSettings s = acoustics().settings();
        s.modelId = on ? kReflectorModelId : kFreeFieldModelId;
        ASSERT_TRUE(acoustics().setSettings(s).ok());
        QCoreApplication::processEvents();
    }
    // A parabolic reflector at the origin and an emitter on its axis.
    void reflectorWithSource(double sourceZ)
    {
        trigger("actionAddParabolicReflector");
        reflector_ = doc().primarySelection();
        trigger("actionAddEmitter");
        emitter_ = doc().primarySelection();
        ASSERT_TRUE(doc().setTransform(emitter_, Transform{Vec3(0, 0, sourceZ)}, QStringLiteral("Move")).ok());
        doc().setSelection({reflector_});
        QCoreApplication::processEvents();
    }

    std::unique_ptr<MainWindow> window_;
    Id reflector_, emitter_;
};

}  // namespace

TEST_F(ManipulatorsUiTest, EveryKindCanBeAddedAndSaysWhatIsCalculatedForIt)
{
    const std::pair<const char*, const char*> actions[] = {
        {"actionAddParabolicReflector", "reflector.parabolic"},   {"actionAddEllipticalReflector", "reflector.elliptical"},
        {"actionAddSphericalReflector", "reflector.spherical"},   {"actionAddCylindricalReflector", "reflector.cylindrical"},
        {"actionAddFlatReflector", "reflector.flat"},             {"actionAddLens", "lens.spherical"},
        {"actionAddCircularWaveguide", "waveguide.circular"},     {"actionAddRectangularWaveguide", "waveguide.rectangular"},
        {"actionAddRectangularCavity", "cavity.rectangular"},     {"actionAddCylindricalCavity", "cavity.cylindrical"},
        {"actionAddSphericalCavity", "cavity.spherical"},         {"actionAddSlitPlate", "diffraction.slit"}};
    for (const auto& [action, kind] : actions) {
        trigger(action);
        const Id id = doc().primarySelection();
        const SceneObject* object = doc().scene().find(id);
        ASSERT_NE(component(id), nullptr) << action;
        ASSERT_TRUE(object->state.geometry.has_value());
        EXPECT_EQ(object->state.geometry->kindId(), kind);
        EXPECT_EQ(window_->sceneTree().itemFor(id)->text(1), QString::fromStdString(object->state.geometry->kind().displayName));
        ASSERT_NE(editor(), nullptr) << action;
        const QString support = child<QLabel>(*editor(), "supportValue")->text();
        if (isReflectorGeometry(*object->state.geometry)) {
            EXPECT_TRUE(support.contains(QStringLiteral("Not a full-wave solution"))) << action;
            EXPECT_TRUE(child<QComboBox>(*editor(), "boundaryCombo")->isEnabled());
        } else {
            EXPECT_TRUE(support.contains(QStringLiteral("does not change any simulated field"))) << action;
            EXPECT_FALSE(child<QComboBox>(*editor(), "boundaryCombo")->isEnabled());
            EXPECT_FALSE(child<QWidget>(*editor(), "optimizeGroup")->isVisible());
        }
        EXPECT_FALSE(editor()->estimates().isEmpty());
    }
    EXPECT_EQ(doc().scene().objectCount(), 12u);
    // A new parabolic reflector records its own focus as the intended focus.
    const Id parabola = doc().scene().root().children()[0]->id();
    ASSERT_TRUE(component(parabola)->settings.intendedFocus.has_value());
    EXPECT_TRUE(component(parabola)->settings.intendedFocus->isApprox(Vec3(0, 0, 0.05)));
    trigger("actionUndo");
    EXPECT_EQ(doc().scene().objectCount(), 11u);
}

TEST_F(ManipulatorsUiTest, EditorChangesBoundaryAndDesignIntentUndoably)
{
    trigger("actionAddParabolicReflector");
    const Id id = doc().primarySelection();
    ASSERT_NE(editor(), nullptr);
    auto* boundary = child<QComboBox>(*editor(), "boundaryCombo");
    boundary->setCurrentIndex(1);
    emit boundary->activated(1);
    EXPECT_EQ(component(id)->settings.boundary, BoundaryCondition::PressureRelease);

    child<QDoubleSpinBox>(*editor(), "focusZ")->setValue(0.08);
    EXPECT_NEAR(component(id)->settings.intendedFocus->z(), 0.08, 1e-12);
    auto* sourceCheck = child<QCheckBox>(*editor(), "sourceCheck");
    sourceCheck->setChecked(true);
    emit sourceCheck->clicked(true);
    ASSERT_TRUE(component(id)->settings.intendedSource.has_value());
    child<QDoubleSpinBox>(*editor(), "sourceZ")->setValue(1.5);
    EXPECT_NEAR(component(id)->settings.intendedSource->z(), 1.5, 1e-12);

    trigger("actionUndo");
    trigger("actionUndo");
    EXPECT_FALSE(component(id)->settings.intendedSource.has_value());
    trigger("actionUndo");
    EXPECT_NEAR(component(id)->settings.intendedFocus->z(), 0.05, 1e-12);

    // Geometry parameters are edited like any other geometry.
    ASSERT_TRUE(doc().setGeometryParameter(id, "focalLength", 0.07).ok());
    QCoreApplication::processEvents();
    EXPECT_TRUE(editor()->estimates().contains(QStringLiteral("Focus (world): (0, 0, 0.07)")));
    EXPECT_FALSE(doc().setGeometryParameter(id, "aperture", -1.0).ok());
}

TEST_F(ManipulatorsUiTest, LensEstimateUsesTheAssignedMaterialAndTheMedium)
{
    trigger("actionAddLens");
    const Id id = doc().primarySelection();
    EXPECT_TRUE(editor()->estimates().contains(QStringLiteral("Assign a material")));
    ASSERT_TRUE(doc().setMaterial(id, doc().project().materials.findByName("Acrylic")->id()).ok());
    AcousticSettings s = acoustics().settings();
    s.medium = waterNominal();
    ASSERT_TRUE(acoustics().setSettings(s).ok());
    QCoreApplication::processEvents();
    const QString text = editor()->estimates();
    EXPECT_TRUE(text.contains(QStringLiteral("Lens material: Acrylic (illustrative)")));
    EXPECT_TRUE(text.contains(QStringLiteral("Refractive index n = c_medium / c_lens: 0.5429")));
    // The default lens is biconvex; in a slower medium that diverges.
    EXPECT_TRUE(text.contains(QStringLiteral("DIVERGING")));
    EXPECT_TRUE(text.contains(QStringLiteral("not a field simulation")));
    EXPECT_TRUE(text.contains(QStringLiteral("NO field effect is calculated")));
}

TEST_F(ManipulatorsUiTest, ReflectorModelChangesTheFieldAndTheFreeFieldModelSaysItIgnoresReflectors)
{
    reflectorWithSource(1.0);
    trigger("actionAddProbe");
    const Id probe = doc().primarySelection();
    const double image = mirrorImageDistance(0.05, 1.0);
    ASSERT_TRUE(doc().setTransform(probe, Transform{Vec3(0, 0, image)}, QStringLiteral("Move")).ok());
    QCoreApplication::processEvents();

    // Free field: the reflector is ignored, and the panel says so.
    EXPECT_FALSE(acoustics().reflectorModelSelected());
    FieldAnalysisPanel& analysis = window_->fieldAnalysis();
    const double free = analysis.pointCell(0, 5).toDouble();
    EXPECT_NEAR(free, 1.0 / (1.0 - image), 1e-4);
    EXPECT_TRUE(acoustics().modelNotes().join(QLatin1Char(' ')).contains(QStringLiteral("free-field model ignores them")));
    EXPECT_TRUE(child<QLabel>(window_->acousticsPanel(), "modelNote")->text().contains(QStringLiteral("free-field model ignores them")));

    // Select the reflector model through the panel's combo box.
    auto* combo = child<QComboBox>(window_->acousticsPanel(), "modelCombo");
    combo->setCurrentIndex(1);
    emit combo->activated(1);
    QCoreApplication::processEvents();
    EXPECT_TRUE(acoustics().reflectorModelSelected());
    EXPECT_TRUE(child<QLabel>(window_->acousticsPanel(), "modelNote")->text().contains(QStringLiteral("NOT a full-wave solution")));
    const double focused = analysis.pointCell(0, 5).toDouble();
    EXPECT_GT(focused, 20.0 * free);
    EXPECT_EQ(analysis.pointCell(0, 9), QStringLiteral("not calculated"));  // no intensity from this model
    EXPECT_TRUE(analysis.pointCell(0, 11).contains(QStringLiteral("no particle velocity")));

    // Hiding the reflector takes it out of the calculation, with a note.
    ASSERT_TRUE(doc().setVisible(reflector_, false).ok());
    QCoreApplication::processEvents();
    EXPECT_NEAR(analysis.pointCell(0, 5).toDouble(), free, 1e-4);
    EXPECT_TRUE(acoustics().modelNotes().join(QLatin1Char(' ')).contains(QStringLiteral("is hidden and was left out")));
    trigger("actionUndo");

    // The model choice is a project setting: undoable, and saved with the project.
    trigger("actionUndo");
    EXPECT_FALSE(acoustics().reflectorModelSelected());
    EXPECT_EQ(combo->currentIndex(), 0);
}

TEST_F(ManipulatorsUiTest, FieldSimulationWithAReflectorIsEstimatedRunAndCompared)
{
    reflectorWithSource(1.0);
    trigger("actionAddFieldPlane");
    const Id region = doc().primarySelection();
    RegionSettings small;
    small.kind = RegionKind::PlaneXZ;
    small.size = Vec3(0.04, 0.1, 0.04);
    small.countU = small.countV = 41;
    ASSERT_TRUE(acoustics().setComponent(region, std::make_shared<RegionComponent>(small), QStringLiteral("Edit")).ok());
    ASSERT_TRUE(doc().setTransform(region, Transform{Vec3(0, 0, 0.0525)}, QStringLiteral("Move")).ok());
    QCoreApplication::processEvents();
    AcousticsPanel& panel = window_->acousticsPanel();
    panel.selectRegion(region);

    // Run once with the free field, once with the reflector model.
    ASSERT_TRUE(panel.runSimulation());
    ASSERT_TRUE(QTest::qWaitFor([&] { return acoustics().results().size() == 1; }, 60000));
    selectReflectorModel(true);
    EXPECT_TRUE(panel.estimateText().contains(QStringLiteral("surface element(s)")));
    EXPECT_TRUE(panel.estimateText().contains(QStringLiteral("Kirchhoff approximation")));
    ASSERT_TRUE(panel.runSimulation());
    ASSERT_TRUE(QTest::qWaitFor([&] { return acoustics().results().size() == 2; }, 120000));
    QCoreApplication::processEvents();

    const auto& results = acoustics().results();
    EXPECT_EQ(results[0]->metadata.solverId, "acoustic.freefield.analytical");
    EXPECT_EQ(results[1]->metadata.solverId, "acoustic.reflector.kirchhoff");
    EXPECT_GT(results[1]->statistics.maxMagnitude, 20.0 * results[0]->statistics.maxMagnitude);
    // The run record names the reflector and its surface elements.
    const auto& record = doc().project().extensions["acousticRuns"][1]["metadata"]["configuration"]["reflectors"];
    ASSERT_EQ(record.size(), 1u);
    EXPECT_EQ(record[0]["boundary"], "rigid");
    EXPECT_GT(record[0]["surfaceElements"].get<int>(), 1000);
    bool approximation = false;
    for (const std::string& assumption : results[1]->assumptions)
        approximation = approximation || assumption.find("not a full-wave solution") != std::string::npos;
    EXPECT_TRUE(approximation);

    // The comparison table puts both runs side by side with focus metrics
    // relative to the reflector's intended focus.
    FieldAnalysisPanel& analysis = window_->fieldAnalysis();
    ASSERT_EQ(analysis.comparisonRowCount(), 2);
    EXPECT_EQ(analysis.comparisonCell(0, 1), QStringLiteral("acoustic.freefield.analytical"));
    EXPECT_EQ(analysis.comparisonCell(1, 1), QStringLiteral("acoustic.reflector.kirchhoff"));
    EXPECT_GT(analysis.comparisonCell(1, 2).toDouble(), 20.0 * analysis.comparisonCell(0, 2).toDouble());
    // With the reflector the peak is at the mirror-equation image, 2.6 mm
    // beyond the intended (geometric) focus, and has a wavelength-scale width.
    EXPECT_NEAR(analysis.comparisonCell(1, 5).toDouble(), mirrorImageDistance(0.05, 1.0), 0.004);
    const QString displacement = analysis.comparisonCell(1, 9);
    EXPECT_TRUE(displacement.endsWith(QStringLiteral(" m")));
    EXPECT_LT(displacement.split(QLatin1Char(' ')).first().toDouble(), 0.006);
    EXPECT_TRUE(analysis.comparisonCell(1, 6).endsWith(QStringLiteral(" m")));  // a resolved lateral width
    EXPECT_EQ(analysis.comparisonCell(0, 6), QStringLiteral("0 (not resolved)"));  // the free field has no focus here
}

TEST_F(ManipulatorsUiTest, GeometryOptimizationNeedsTheModelImprovesTheTargetAndAppliesOnRequest)
{
    reflectorWithSource(0.5);
    ASSERT_TRUE(doc().setGeometryParameter(reflector_, "focalLength", 0.04).ok());
    ASSERT_TRUE(doc().setGeometryParameter(reflector_, "aperture", 0.16).ok());
    ManipulatorSettings settings = component(reflector_)->settings;
    settings.intendedFocus = Vec3(0, 0, 0.07);
    ASSERT_TRUE(window_->manipulators().setSettings(reflector_, settings).ok());
    QCoreApplication::processEvents();
    ASSERT_NE(editor(), nullptr);

    // With the free-field model there is nothing to optimize, and it says why.
    EXPECT_FALSE(editor()->optimize());
    EXPECT_TRUE(editor()->optimizeResultText().contains(QStringLiteral("select the reflector field model")));
    selectReflectorModel(true);

    child<QDoubleSpinBox>(*editor(), "optimizeLower")->setValue(0.03);
    child<QDoubleSpinBox>(*editor(), "optimizeUpper")->setValue(0.12);
    const size_t undoSteps = doc().commands().size();
    ASSERT_TRUE(editor()->optimize());
    // Optimizing changes nothing in the project.
    EXPECT_EQ(doc().commands().size(), undoSteps);
    EXPECT_DOUBLE_EQ(doc().scene().find(reflector_)->state.geometry->parameter("focalLength"), 0.04);
    EXPECT_TRUE(editor()->optimizeResultText().contains(QStringLiteral("Improved")));
    EXPECT_TRUE(editor()->optimizeResultText().contains(QStringLiteral("local search")));
    EXPECT_TRUE(child<QPushButton>(*editor(), "applyBestButton")->isEnabled());

    const double before = std::abs(acoustics().currentEvaluator()->pressure(Vec3(0, 0, 0.07)));
    ASSERT_TRUE(editor()->applyBest());
    const double focal = doc().scene().find(reflector_)->state.geometry->parameter("focalLength");
    EXPECT_NEAR(focal, 1.0 / (1.0 / 0.5 + 1.0 / 0.07), 0.004);  // the mirror equation's answer
    const double after = std::abs(acoustics().currentEvaluator()->pressure(Vec3(0, 0, 0.07)));
    EXPECT_GT(after, 3.0 * before);
    // One undo step brings the original design back.
    trigger("actionUndo");
    EXPECT_DOUBLE_EQ(doc().scene().find(reflector_)->state.geometry->parameter("focalLength"), 0.04);
}

TEST_F(ManipulatorsUiTest, HybridFocusingThroughThePanelUsesTheReflectedPaths)
{
    trigger("actionAddParabolicReflector");
    trigger("actionAddPlanarArray");
    const Id array = doc().primarySelection();
    Transform facingDown;
    facingDown.position = Vec3(0, 0, 0.3);
    facingDown.rotation = Vec3(units::pi, 0, 0);
    ASSERT_TRUE(doc().setTransform(array, facingDown, QStringLiteral("Move")).ok());
    trigger("actionAddTarget");
    const Id target = doc().primarySelection();
    const Vec3 point(0.004, 0.0, 0.055);
    ASSERT_TRUE(doc().setTransform(target, Transform{point}, QStringLiteral("Move")).ok());
    selectReflectorModel(true);

    AcousticsPanel& panel = window_->acousticsPanel();
    panel.selectFocusArray(array);
    panel.selectFocusTarget(target);
    ASSERT_TRUE(panel.calculateFocus());
    EXPECT_TRUE(child<QLabel>(panel, "focusNote")->text().contains(QStringLiteral("Hybrid focusing")));
    ASSERT_TRUE(panel.applyFocus());

    // With the reflector model, every element's direct + reflected response
    // now arrives in phase: |sum| equals the sum of the magnitudes.
    const auto model = acoustics().currentModel();
    const AcousticSettings s = acoustics().settings();
    Complex sum(0.0, 0.0);
    double bound = 0.0;
    for (const PointSource& source : collectSources(doc().scene(), {array})) {
        const Complex h = model->transfer(source, point, s.medium).value;
        sum += source.amplitude * h;
        bound += std::abs(h);
    }
    EXPECT_NEAR(std::abs(sum), bound, 1e-6 * bound);
    EXPECT_NEAR(std::abs(acoustics().currentEvaluator()->pressure(point)), bound, 1e-6 * bound);

    // Time-delay focusing does not compensate reflected paths, and says so.
    child<QComboBox>(panel, "focusControlCombo")->setCurrentIndex(1);
    ASSERT_TRUE(panel.calculateFocus());
    EXPECT_TRUE(child<QLabel>(panel, "focusNote")->text().contains(QStringLiteral("Reflected paths are NOT compensated")));
}

TEST_F(ManipulatorsUiTest, FocalMarkersAndRayPathsAreDrawn)
{
    reflectorWithSource(0.4);
    trigger("actionFrameAll");
    ViewportWidget& viewport = window_->viewport();
    QCoreApplication::processEvents();
    const QImage withoutRays = viewport.grabFramebuffer();
    trigger("actionShowRays");
    EXPECT_TRUE(window_->manipulators().showRays());
    QCoreApplication::processEvents();
    const QImage withRays = viewport.grabFramebuffer();
    EXPECT_NE(withoutRays, withRays);
    // Rays are drawn for the selected reflector only.
    doc().clearSelection();
    QCoreApplication::processEvents();
    EXPECT_NE(viewport.grabFramebuffer(), withRays);
    trigger("actionShowRays");
    EXPECT_FALSE(window_->manipulators().showRays());
}

TEST_F(ManipulatorsUiTest, ExampleProjectsOpenWithTheirModelAndSimulate)
{
    const std::pair<const char*, bool> examples[] = {{"a-parabolic-reflector", true}, {"c-acoustic-lens", false}, {"d-hybrid-focusing", true}};
    for (const auto& [name, reflectorModel] : examples) {
        const QString path = QStringLiteral(RESONANT_SOURCE_DIR) + QStringLiteral("/examples/manipulators/") + QString::fromLatin1(name)
                             + QStringLiteral(".resonant");
        ASSERT_TRUE(window_->openProjectFile(path).ok()) << name;
        EXPECT_EQ(acoustics().reflectorModelSelected(), reflectorModel) << name;
        ASSERT_TRUE(window_->acousticsPanel().runSimulation()) << name;
        ASSERT_TRUE(QTest::qWaitFor([&] { return acoustics().results().size() == 1; }, 180000)) << name;
        QCoreApplication::processEvents();
        EXPECT_GT(acoustics().displayedResult()->statistics.maxMagnitude, 0.0);
    }
    // The lens example states that the lens is not simulated.
    ASSERT_TRUE(window_->openProjectFile(QStringLiteral(RESONANT_SOURCE_DIR) + QStringLiteral("/examples/manipulators/c-acoustic-lens.resonant")).ok());
    EXPECT_TRUE(acoustics().modelNotes().join(QLatin1Char(' ')).contains(QStringLiteral("free-field model ignores them")));
}
