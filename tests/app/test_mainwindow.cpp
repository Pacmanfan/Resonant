// Drives the real main window: actions, panels, viewport, files.
#include "AnalysisPanel.h"
#include "DisplayUnits.h"
#include "DocumentController.h"
#include "MainWindow.h"
#include "MaterialsDialog.h"
#include "PropertiesPanel.h"
#include "SceneTreePanel.h"
#include "ViewportWidget.h"
#include "foundation/Units.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMenu>
#include <QSettings>
#include <QTest>
#include <QTreeWidgetItem>

#include <gtest/gtest.h>

using namespace resonant;

namespace {

QString scratchPath(const QString& name)
{
    const QString dir = QDir::tempPath() + QStringLiteral("/resonant-app-tests");
    QDir().mkpath(dir);
    return dir + QLatin1Char('/') + name;
}

class MainWindowTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        window_ = std::make_unique<MainWindow>();
        // Never block on a dialog: a test sets the answers it expects.
        window_->unsavedChangesPrompt = [this] {
            ++prompts_;
            return answer_;
        };
        window_->saveFileNamePrompt = [this] { return saveName_; };
        window_->resize(1200, 800);
        window_->show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(window_.get()));
        QTest::qWait(50);
    }

    void TearDown() override
    {
        answer_ = QMessageBox::Discard;
        window_->close();
        window_.reset();
    }

    void trigger(const char* actionName)
    {
        QAction* action = window_->action(QString::fromLatin1(actionName));
        ASSERT_NE(action, nullptr) << actionName;
        ASSERT_TRUE(action->isEnabled()) << actionName << " is disabled";
        action->trigger();
        QCoreApplication::processEvents();
    }

    DocumentController& doc() { return window_->controller(); }
    const SceneObject* selected() { return doc().scene().find(doc().primarySelection()); }

    template <typename T>
    T* child(QWidget& parent, const char* name)
    {
        T* widget = parent.findChild<T*>(QString::fromLatin1(name));
        EXPECT_NE(widget, nullptr) << name;
        return widget;
    }

    std::unique_ptr<MainWindow> window_;
    QMessageBox::StandardButton answer_ = QMessageBox::Discard;
    QString saveName_;
    int prompts_ = 0;
};

}  // namespace

TEST_F(MainWindowTest, StartsWithAnEmptyUnmodifiedProjectAndAWorkingViewport)
{
    EXPECT_EQ(doc().scene().objectCount(), 0u);
    EXPECT_FALSE(doc().isModified());
    EXPECT_FALSE(window_->isWindowModified());
    EXPECT_TRUE(window_->windowTitle().contains(QStringLiteral("Untitled")));
    EXPECT_TRUE(window_->viewport().graphicsReady()) << window_->viewport().graphicsError().toStdString();
    EXPECT_FALSE(window_->action(QStringLiteral("actionUndo"))->isEnabled());
    EXPECT_FALSE(window_->action(QStringLiteral("actionDelete"))->isEnabled());
}

TEST_F(MainWindowTest, EveryMenuActionExistsAndTheThreeDocksAreThere)
{
    for (const char* menu : {"menuFile", "menuEdit", "menuView", "menuScene", "menuGeometry", "menuMaterials",
                             "menuSimulation", "menuTools", "menuHelp"})
        EXPECT_NE(window_->findChild<QMenu*>(QString::fromLatin1(menu)), nullptr) << menu;
    for (const char* dock : {"sceneDock", "propertiesDock", "analysisDock"}) {
        auto* widget = window_->findChild<QDockWidget*>(QString::fromLatin1(dock));
        ASSERT_NE(widget, nullptr) << dock;
        EXPECT_TRUE(widget->isVisible());
    }
}

TEST_F(MainWindowTest, NoActionIsADecoration)
{
    // Triggering every action that needs no dialog and no selection does
    // something observable or is harmless; none may be left unconnected.
    // QObject::receivers() is protected, so ask through a subclass.
    struct Probe : QAction {
        using QAction::receivers;
    };
    for (QAction* action : window_->findChildren<QAction*>()) {
        if (!action->objectName().startsWith(QStringLiteral("action")))
            continue;
        const auto* probe = static_cast<const Probe*>(action);
        const int connections = probe->receivers(SIGNAL(triggered(bool))) + probe->receivers(SIGNAL(toggled(bool)))
                                + (action->actionGroup() ? 1 : 0);
        EXPECT_GT(connections, 0) << action->objectName().toStdString();
    }
}

TEST_F(MainWindowTest, CreatingEachPrimitiveUpdatesSceneTreePropertiesAndSelection)
{
    const char* actions[] = {"actionAddBox",  "actionAddSphere", "actionAddCylinder", "actionAddCone",
                             "actionAddPlane", "actionAddDisc",   "actionAddEllipsoid"};
    const char* names[] = {"Box", "Sphere", "Cylinder", "Cone", "Plane", "Disc", "Ellipsoid"};
    for (int i = 0; i < 7; ++i) {
        trigger(actions[i]);
        ASSERT_NE(selected(), nullptr);
        EXPECT_EQ(selected()->state.name, names[i]);
        // The tree shows it, selected.
        QTreeWidgetItem* item = window_->sceneTree().itemFor(selected()->id());
        ASSERT_NE(item, nullptr);
        EXPECT_EQ(item->text(0), QString::fromLatin1(names[i]));
        EXPECT_EQ(item->text(1), QString::fromLatin1(names[i]));
        EXPECT_TRUE(item->isSelected());
        // The properties panel shows it.
        EXPECT_EQ(window_->properties().shownObject(), selected()->id());
        EXPECT_EQ(child<QLineEdit>(window_->properties(), "nameEdit")->text(), QString::fromLatin1(names[i]));
    }
    EXPECT_EQ(doc().scene().objectCount(), 7u);
    EXPECT_EQ(window_->sceneTree().itemCount(), 7);
    EXPECT_TRUE(doc().isModified());
    EXPECT_TRUE(window_->isWindowModified());
}

TEST_F(MainWindowTest, PropertiesPanelEditsTransformInDisplayUnitsAndStoresSi)
{
    trigger("actionAddBox");
    const Id id = doc().primarySelection();
    PropertiesPanel& panel = window_->properties();

    child<QDoubleSpinBox>(panel, "positionX")->setValue(2.5);
    child<QDoubleSpinBox>(panel, "rotationZ")->setValue(90.0);
    child<QDoubleSpinBox>(panel, "scaleY")->setValue(3.0);
    const Transform& t = doc().scene().find(id)->state.local;
    EXPECT_DOUBLE_EQ(t.position.x(), 2.5);
    EXPECT_NEAR(t.rotation.z(), units::pi / 2.0, 1e-12);  // shown in degrees, stored in radians
    EXPECT_DOUBLE_EQ(t.scale.y(), 3.0);

    // Switch the display unit to millimetres: the stored value is unchanged,
    // the shown value is converted, and an edit is converted back.
    trigger("actionUnitsMillimeters");
    EXPECT_DOUBLE_EQ(doc().scene().find(id)->state.local.position.x(), 2.5);
    EXPECT_DOUBLE_EQ(child<QDoubleSpinBox>(panel, "positionX")->value(), 2500.0);
    child<QDoubleSpinBox>(panel, "positionY")->setValue(125.0);
    EXPECT_DOUBLE_EQ(doc().scene().find(id)->state.local.position.y(), 0.125);
    trigger("actionUnitsMeters");

    // A scale of zero is refused and the field returns to the stored value.
    QDoubleSpinBox* scaleZ = child<QDoubleSpinBox>(panel, "scaleZ");
    scaleZ->setMinimum(-1.0);
    scaleZ->setValue(0.0);
    EXPECT_DOUBLE_EQ(doc().scene().find(id)->state.local.scale.z(), 1.0);
    EXPECT_DOUBLE_EQ(scaleZ->value(), 1.0);
}

TEST_F(MainWindowTest, GeometryParametersAreEditableAndValidated)
{
    trigger("actionAddCylinder");
    const Id id = doc().primarySelection();
    const auto spins = window_->properties().findChild<QWidget*>(QStringLiteral("geometryGroup"))->findChildren<QDoubleSpinBox*>();
    ASSERT_EQ(spins.size(), 2);  // radius, height
    spins[0]->setValue(0.2);
    spins[1]->setValue(1.75);
    const Geometry& geometry = *doc().scene().find(id)->state.geometry;
    EXPECT_DOUBLE_EQ(geometry.parameter("radius"), 0.2);
    EXPECT_DOUBLE_EQ(geometry.parameter("height"), 1.75);
    // The core refuses a non-positive radius whatever the widget allows.
    EXPECT_FALSE(doc().setGeometryParameter(id, "radius", 0.0).ok());
    EXPECT_FALSE(doc().setGeometryParameter(id, "nonsense", 1.0).ok());
    EXPECT_DOUBLE_EQ(doc().scene().find(id)->state.geometry->parameter("radius"), 0.2);
}

TEST_F(MainWindowTest, RenameHideDuplicateDeleteAndUndoRedo)
{
    trigger("actionAddSphere");
    const Id id = doc().primarySelection();

    // Rename through the tree item (as in-place editing does).
    window_->sceneTree().itemFor(id)->setText(0, QStringLiteral("Emitter housing"));
    EXPECT_EQ(doc().scene().find(id)->state.name, "Emitter housing");
    EXPECT_EQ(child<QLineEdit>(window_->properties(), "nameEdit")->text(), QStringLiteral("Emitter housing"));
    // An empty name is refused and the tree shows the real name again.
    window_->sceneTree().itemFor(id)->setText(0, QStringLiteral("   "));
    EXPECT_EQ(doc().scene().find(id)->state.name, "Emitter housing");
    EXPECT_EQ(window_->sceneTree().itemFor(id)->text(0), QStringLiteral("Emitter housing"));

    // Hide through the tree check box, show through the action.
    window_->sceneTree().itemFor(id)->setCheckState(0, Qt::Unchecked);
    EXPECT_FALSE(doc().scene().find(id)->state.visible);
    EXPECT_FALSE(child<QCheckBox>(window_->properties(), "visibleCheck")->isChecked());
    trigger("actionToggleVisibility");
    EXPECT_TRUE(doc().scene().find(id)->state.visible);

    trigger("actionDuplicate");
    EXPECT_EQ(doc().scene().objectCount(), 2u);
    const Id copy = doc().primarySelection();
    EXPECT_NE(copy, id);
    EXPECT_EQ(doc().scene().find(copy)->state.name, "Emitter housing copy");

    trigger("actionDelete");
    EXPECT_EQ(doc().scene().objectCount(), 1u);
    EXPECT_EQ(window_->sceneTree().itemFor(copy), nullptr);
    EXPECT_TRUE(doc().selection().isEmpty());

    trigger("actionUndo");  // delete
    EXPECT_NE(doc().scene().find(copy), nullptr);
    EXPECT_NE(window_->sceneTree().itemFor(copy), nullptr);
    trigger("actionUndo");  // duplicate
    trigger("actionUndo");  // show
    trigger("actionUndo");  // hide
    trigger("actionUndo");  // rename
    EXPECT_EQ(doc().scene().find(id)->state.name, "Sphere");
    EXPECT_EQ(window_->sceneTree().itemFor(id)->text(0), QStringLiteral("Sphere"));
    trigger("actionUndo");  // add
    EXPECT_EQ(doc().scene().objectCount(), 0u);
    EXPECT_FALSE(window_->action(QStringLiteral("actionUndo"))->isEnabled());
    EXPECT_FALSE(doc().isModified());

    trigger("actionRedo");
    trigger("actionRedo");
    EXPECT_EQ(doc().scene().find(id)->state.name, "Emitter housing");
    EXPECT_TRUE(window_->action(QStringLiteral("actionUndo"))->text().contains(QStringLiteral("Rename")));
}

TEST_F(MainWindowTest, GroupsNestObjectsAndDeletingAGroupDeletesItsChildren)
{
    trigger("actionNewGroup");
    const Id group = doc().primarySelection();
    trigger("actionAddBox");
    const Id box = doc().primarySelection();
    ASSERT_TRUE(doc().moveObject(box, group).ok());
    QCoreApplication::processEvents();
    EXPECT_EQ(window_->sceneTree().itemFor(box)->parent(), window_->sceneTree().itemFor(group));

    doc().setSelection({box});
    trigger("actionMoveToTopLevel");
    EXPECT_EQ(window_->sceneTree().itemFor(box)->parent(), nullptr);
    trigger("actionUndo");
    EXPECT_EQ(window_->sceneTree().itemFor(box)->parent(), window_->sceneTree().itemFor(group));

    doc().setSelection({group, box});
    trigger("actionDelete");
    EXPECT_EQ(doc().scene().objectCount(), 0u);
    EXPECT_EQ(window_->sceneTree().itemCount(), 0);
    trigger("actionUndo");
    EXPECT_EQ(doc().scene().objectCount(), 2u);
    EXPECT_EQ(doc().scene().find(box)->parent()->id(), group);
}

TEST_F(MainWindowTest, ClickingInTheViewportSelectsTheObjectUnderTheCursor)
{
    trigger("actionAddBox");
    doc().clearSelection();
    ViewportWidget& viewport = window_->viewport();
    const QPoint centre = viewport.rect().center();
    EXPECT_FALSE(viewport.objectAt(centre).isNull());
    EXPECT_TRUE(viewport.objectAt(QPoint(3, 3)).isNull());

    QTest::mouseClick(&viewport, Qt::LeftButton, Qt::NoModifier, centre);
    EXPECT_EQ(doc().selection().size(), 1);
    EXPECT_TRUE(window_->sceneTree().itemFor(doc().primarySelection())->isSelected());
    QTest::mouseClick(&viewport, Qt::LeftButton, Qt::NoModifier, QPoint(3, 3));
    EXPECT_TRUE(doc().selection().isEmpty());

    // A hidden object cannot be picked.
    doc().selectAll();
    trigger("actionToggleVisibility");
    EXPECT_TRUE(viewport.objectAt(centre).isNull());
}

TEST_F(MainWindowTest, ViewportShowsGeometryAndHighlightsTheSelection)
{
    ViewportWidget& viewport = window_->viewport();
    QCoreApplication::processEvents();
    const QImage empty = viewport.grabFramebuffer();
    trigger("actionAddSphere");
    doc().clearSelection();
    QCoreApplication::processEvents();
    const QImage withSphere = viewport.grabFramebuffer();
    doc().selectAll();
    QCoreApplication::processEvents();
    const QImage selectedImage = viewport.grabFramebuffer();

    const QPoint centre(withSphere.width() / 2, withSphere.height() / 2);
    EXPECT_NE(empty.pixel(centre), withSphere.pixel(centre));
    EXPECT_NE(withSphere.pixel(centre), selectedImage.pixel(centre));
    // Kept for manual inspection of what the viewport draws.
    selectedImage.save(scratchPath(QStringLiteral("viewport.png")));
}

TEST_F(MainWindowTest, CameraNavigationAndViewActions)
{
    ViewportWidget& viewport = window_->viewport();
    const double yaw = viewport.camera().yaw;
    const double distance = viewport.camera().distance;
    const QPoint start = viewport.rect().center();

    QTest::mousePress(&viewport, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&viewport, start + QPoint(80, 0));
    QTest::mouseMove(&viewport, start + QPoint(120, 0));
    QTest::mouseRelease(&viewport, Qt::LeftButton, Qt::NoModifier, start + QPoint(120, 0));
    EXPECT_NE(viewport.camera().yaw, yaw);
    EXPECT_DOUBLE_EQ(viewport.camera().distance, distance);
    EXPECT_TRUE(doc().selection().isEmpty());  // a drag is not a click

    trigger("actionViewTop");
    EXPECT_GT(viewport.camera().pitch, units::degreesToRadians(80.0));
    trigger("actionResetCamera");
    EXPECT_DOUBLE_EQ(viewport.camera().yaw, yaw);

    trigger("actionAddBox");
    ASSERT_TRUE(doc().setTransform(doc().primarySelection(), Transform{Vec3(50.0, 0.0, 0.0)}, QStringLiteral("Move")).ok());
    trigger("actionFrameSelection");
    EXPECT_NEAR(viewport.camera().target.x(), 50.0, 1e-9);

    trigger("actionShowGrid");
    EXPECT_FALSE(viewport.gridVisible());
    trigger("actionShowGrid");
    EXPECT_TRUE(viewport.gridVisible());
}

TEST_F(MainWindowTest, MaterialsCanBeCreatedEditedAndAssigned)
{
    trigger("actionAddBox");
    const Id box = doc().primarySelection();

    MaterialsDialog dialog(doc().project().materials);
    const Id created = dialog.addMaterial();
    ASSERT_FALSE(created.isNull());
    EXPECT_EQ(dialog.currentMaterialId(), created);
    child<QLineEdit>(dialog, "nameEdit")->setText(QStringLiteral("Test foam"));
    emit child<QLineEdit>(dialog, "nameEdit")->editingFinished();
    child<QCheckBox>(dialog, "domainApplies")->setChecked(true);
    EXPECT_TRUE(dialog.setPropertyText(QStringLiteral("density"), QStringLiteral("32.5")));
    EXPECT_TRUE(dialog.setPropertyText(QStringLiteral("soundSpeed"), QStringLiteral("210")));
    // Bad input is refused with a reason and changes nothing.
    EXPECT_FALSE(dialog.setPropertyText(QStringLiteral("density"), QStringLiteral("-4")));
    EXPECT_FALSE(dialog.errorText().isEmpty());
    EXPECT_FALSE(dialog.setPropertyText(QStringLiteral("density"), QStringLiteral("heavy")));
    EXPECT_DOUBLE_EQ(dialog.library().find(created)->property(PhysicsDomain::Acoustic, "density")->value, 32.5);

    ASSERT_TRUE(doc().setMaterialLibrary(dialog.library(), QStringLiteral("Edit materials")).ok());
    const Material* material = doc().project().materials.find(created);
    ASSERT_NE(material, nullptr);
    EXPECT_EQ(material->name(), "Test foam");
    EXPECT_EQ(material->property(PhysicsDomain::Acoustic, "soundSpeed")->provenance, Provenance::Measured);

    // Assign through the properties panel's combo box.
    QComboBox* combo = child<QComboBox>(window_->properties(), "materialCombo");
    const int index = combo->findText(QStringLiteral("Test foam"));
    ASSERT_GE(index, 0);
    combo->setCurrentIndex(index);
    emit combo->activated(index);
    EXPECT_EQ(doc().scene().find(box)->state.materialId, created);

    // Deleting the material clears the assignment in the same undo step.
    MaterialLibrary without = doc().project().materials;
    without.remove(created);
    ASSERT_TRUE(doc().setMaterialLibrary(without, QStringLiteral("Delete material")).ok());
    EXPECT_TRUE(doc().scene().find(box)->state.materialId.isNull());
    trigger("actionUndo");
    EXPECT_EQ(doc().scene().find(box)->state.materialId, created);
    EXPECT_NE(doc().project().materials.find(created), nullptr);
}

TEST_F(MainWindowTest, SaveAndReopenPreservesTheScene)
{
    trigger("actionNewGroup");
    const Id group = doc().primarySelection();
    trigger("actionAddCone");
    const Id cone = doc().primarySelection();
    ASSERT_TRUE(doc().moveObject(cone, group).ok());
    Transform transform;
    transform.position = Vec3(1.0, 2.0, 3.0);
    transform.rotation = Vec3(0.1, 0.2, 0.3);
    transform.scale = Vec3(1.0, 2.0, 0.5);
    ASSERT_TRUE(doc().setTransform(cone, transform, QStringLiteral("Move")).ok());
    ASSERT_TRUE(doc().setGeometryParameter(cone, "height", 2.25).ok());
    ASSERT_TRUE(doc().setMaterial(cone, doc().project().materials.findByName("Glass")->id()).ok());
    ASSERT_TRUE(doc().setVisible(cone, false).ok());
    const Id projectId = doc().project().id;

    const QString path = scratchPath(QStringLiteral("saved.resonant"));
    ASSERT_TRUE(window_->saveProjectToFile(path).ok());
    EXPECT_FALSE(doc().isModified());
    EXPECT_TRUE(window_->windowTitle().startsWith(QStringLiteral("saved.resonant")));

    trigger("actionNew");
    EXPECT_EQ(doc().scene().objectCount(), 0u);
    EXPECT_EQ(prompts_, 0);  // nothing was unsaved

    ASSERT_TRUE(window_->openProjectFile(path).ok());
    EXPECT_EQ(doc().project().id, projectId);
    const SceneObject* reopened = doc().scene().find(cone);
    ASSERT_NE(reopened, nullptr);
    EXPECT_EQ(reopened->parent()->id(), group);
    EXPECT_EQ(reopened->state.local, transform);
    EXPECT_DOUBLE_EQ(reopened->state.geometry->parameter("height"), 2.25);
    EXPECT_EQ(doc().project().materials.find(reopened->state.materialId)->name(), "Glass");
    EXPECT_FALSE(reopened->state.visible);
    EXPECT_EQ(window_->sceneTree().itemCount(), 2);
    EXPECT_FALSE(doc().isModified());
    EXPECT_FALSE(window_->action(QStringLiteral("actionUndo"))->isEnabled());
}

TEST_F(MainWindowTest, UnsavedChangesArePromptedForAndEachAnswerIsHonoured)
{
    trigger("actionAddBox");
    ASSERT_TRUE(doc().isModified());

    answer_ = QMessageBox::Cancel;
    trigger("actionNew");
    EXPECT_EQ(prompts_, 1);
    EXPECT_EQ(doc().scene().objectCount(), 1u);  // still there

    // Save, with no file name yet: Save As is asked; an empty name cancels.
    answer_ = QMessageBox::Save;
    saveName_.clear();
    trigger("actionNew");
    EXPECT_EQ(prompts_, 2);
    EXPECT_EQ(doc().scene().objectCount(), 1u);

    saveName_ = scratchPath(QStringLiteral("prompted.resonant"));
    QFile::remove(saveName_);
    trigger("actionNew");
    EXPECT_EQ(prompts_, 3);
    EXPECT_TRUE(QFile::exists(saveName_));
    EXPECT_EQ(doc().scene().objectCount(), 0u);

    trigger("actionAddSphere");
    answer_ = QMessageBox::Discard;
    trigger("actionNew");
    EXPECT_EQ(prompts_, 4);
    EXPECT_EQ(doc().scene().objectCount(), 0u);

    // Closing the window asks as well, and Cancel keeps it open.
    trigger("actionAddSphere");
    answer_ = QMessageBox::Cancel;
    EXPECT_FALSE(window_->close());
    EXPECT_TRUE(window_->isVisible());
    EXPECT_EQ(prompts_, 5);
}

TEST_F(MainWindowTest, OpeningABadFileFailsCleanlyAndKeepsTheCurrentProject)
{
    trigger("actionAddBox");
    const Id projectId = doc().project().id;

    EXPECT_FALSE(window_->openProjectFile(scratchPath(QStringLiteral("missing.resonant"))).ok());
    const QString broken = scratchPath(QStringLiteral("broken.resonant"));
    QFile file(broken);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("{\"format\": \"resonant-project\", \"schemaVersion\": 99}");
    file.close();
    const Status status = window_->openProjectFile(broken);
    EXPECT_FALSE(status.ok());
    EXPECT_NE(status.message().find("newer version"), std::string::npos);

    EXPECT_EQ(doc().project().id, projectId);
    EXPECT_EQ(doc().scene().objectCount(), 1u);
    EXPECT_TRUE(doc().isModified());
    // The failure is in the console.
    QCoreApplication::processEvents();
    EXPECT_TRUE(window_->analysis().consoleText().contains(QStringLiteral("[error] project:")));
}

TEST_F(MainWindowTest, DemonstratorJobRunsReportsProgressAndCancels)
{
    trigger("actionRunDemonstrator");
    AnalysisPanel& analysis = window_->analysis();
    ASSERT_TRUE(QTest::qWaitFor([&] { return analysis.jobRowCount() == 1 && analysis.jobState(0) == QLatin1String("running"); }, 5000));
    EXPECT_TRUE(window_->action(QStringLiteral("actionCancelJobs"))->isEnabled());

    // The window stays responsive while the job runs.
    trigger("actionAddBox");
    EXPECT_EQ(doc().scene().objectCount(), 1u);

    trigger("actionCancelJobs");
    ASSERT_TRUE(QTest::qWaitFor([&] { return analysis.jobState(0) == QLatin1String("cancelled"); }, 5000));
    EXPECT_FALSE(doc().jobs().hasUnfinishedJobs());
    EXPECT_FALSE(window_->action(QStringLiteral("actionCancelJobs"))->isEnabled());
    EXPECT_TRUE(analysis.consoleText().contains(QStringLiteral("job cancelled")));
}

TEST_F(MainWindowTest, DiagnosticsReportTheEnvironmentAndTheProject)
{
    trigger("actionAddBox");
    const QStringList items = window_->analysis().diagnosticsItems();
    const auto has = [&items](const QString& prefix) {
        return std::any_of(items.begin(), items.end(), [&](const QString& item) { return item.startsWith(prefix); });
    };
    EXPECT_TRUE(has(QStringLiteral("Application/Version=Resonant")));
    EXPECT_TRUE(has(QStringLiteral("Graphics/Context=OpenGL")));
    EXPECT_TRUE(has(QStringLiteral("Project/Objects=1")));
    EXPECT_TRUE(has(QStringLiteral("Project/Unsaved changes=yes")));
    EXPECT_TRUE(has(QStringLiteral("Conventions/Internal units=SI")));
    EXPECT_TRUE(window_->analysis().consoleText().contains(QStringLiteral("Add Box")));
}

TEST_F(MainWindowTest, WorkspaceLayoutPersistsBetweenSessions)
{
    auto* dock = window_->findChild<QDockWidget*>(QStringLiteral("propertiesDock"));
    dock->hide();
    trigger("actionShowAxes");
    trigger("actionUnitsMillimeters");
    window_->saveWorkspace();

    MainWindow second;
    second.unsavedChangesPrompt = [] { return QMessageBox::Discard; };
    second.restoreWorkspace();
    second.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&second));
    EXPECT_FALSE(second.findChild<QDockWidget*>(QStringLiteral("propertiesDock"))->isVisible());
    EXPECT_FALSE(second.action(QStringLiteral("actionShowAxes"))->isChecked());
    EXPECT_EQ(second.displayUnits().lengthUnit(), units::LengthUnit::Millimeter);

    // "Reset Layout" brings the hidden panel back.
    second.action(QStringLiteral("actionResetLayout"))->trigger();
    EXPECT_TRUE(second.findChild<QDockWidget*>(QStringLiteral("propertiesDock"))->isVisible());
    second.close();
    QSettings().clear();
}
