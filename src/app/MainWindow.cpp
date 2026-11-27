#include "MainWindow.h"

#include "AboutDialog.h"
#include "AcousticController.h"
#include "AcousticEditors.h"
#include "AcousticMarkers.h"
#include "AcousticsPanel.h"
#include "FieldAnalysisPanel.h"
#include "FdtdController.h"
#include "FdtdPanel.h"
#include "ManipulatorController.h"
#include "MechanicalEditor.h"
#include "MechanicsController.h"
#include "MechanicsMarkers.h"
#include "MechanicsPanel.h"
#include "TimeSignalPanel.h"
#include "ManipulatorEditor.h"
#include "AnalysisPanel.h"
#include "DisplayUnits.h"
#include "DocumentController.h"
#include "MaterialsDialog.h"
#include "PropertiesPanel.h"
#include "SceneTreePanel.h"
#include "ViewportWidget.h"
#include "foundation/Log.h"
#include "project/ProjectSerializer.h"
#include "ui_MainWindow.h"

#include <QActionGroup>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>

using namespace resonant;

namespace {
QString projectFilter()
{
    return QStringLiteral("Resonant projects (*.%1);;All files (*)").arg(QLatin1String(kProjectFileExtension));
}
}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui_(std::make_unique<Ui::MainWindow>()), controller_(new DocumentController(this)),
      units_(new DisplayUnits(this)), acoustics_(new AcousticController(controller_, this))
{
    ui_->setupUi(this);

    // Standard icons where the platform style has them; the rest stay text.
    ui_->actionNew->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    ui_->actionOpen->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    ui_->actionSave->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    ui_->actionUndo->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    ui_->actionRedo->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    ui_->fileToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    ui_->menuPanels->addAction(ui_->sceneDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->propertiesDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->analysisDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->acousticsDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->solverDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->mechanicsDock->toggleViewAction());
    ui_->menuPanels->addSeparator();
    ui_->menuPanels->addAction(ui_->fileToolBar->toggleViewAction());
    ui_->menuPanels->addAction(ui_->geometryToolBar->toggleViewAction());
    ui_->menuPanels->addAction(ui_->viewToolBar->toggleViewAction());
    ui_->menuPanels->addAction(ui_->acousticsToolBar->toggleViewAction());

    unitGroup_ = new QActionGroup(this);
    unitGroup_->addAction(ui_->actionUnitsMeters)->setData(static_cast<int>(units::LengthUnit::Meter));
    unitGroup_->addAction(ui_->actionUnitsCentimeters)->setData(static_cast<int>(units::LengthUnit::Centimeter));
    unitGroup_->addAction(ui_->actionUnitsMillimeters)->setData(static_cast<int>(units::LengthUnit::Millimeter));
    unitGroup_->addAction(ui_->actionUnitsInches)->setData(static_cast<int>(units::LengthUnit::Inch));
    ui_->actionUnitsMeters->setChecked(true);
    ui_->actionShowGrid->setChecked(true);
    ui_->actionShowAxes->setChecked(true);

    ui_->viewport->setController(controller_);
    ui_->scenePanel->setController(controller_);
    ui_->propertiesPanel->setController(controller_, units_);
    ui_->analysisPanel->setController(controller_);

    // The scene tree's context menu offers the same actions as the menus.
    auto* contextMenu = new QMenu(this);
    contextMenu->addMenu(ui_->menuGeometry);
    contextMenu->addAction(ui_->actionNewGroup);
    contextMenu->addSeparator();
    contextMenu->addAction(ui_->actionRename);
    contextMenu->addAction(ui_->actionDuplicate);
    contextMenu->addAction(ui_->actionDelete);
    contextMenu->addSeparator();
    contextMenu->addAction(ui_->actionToggleVisibility);
    contextMenu->addAction(ui_->actionMoveToTopLevel);
    contextMenu->addMenu(ui_->menuAssignMaterial);
    contextMenu->addAction(ui_->actionFrameSelection);
    ui_->scenePanel->setContextMenu(contextMenu);

    connectActions();
    connectAcoustics();
    connectManipulators();
    connectFullWave();
    connectMechanics();

    unsavedChangesPrompt = [this] {
        return QMessageBox::warning(this, QStringLiteral("Unsaved changes"),
                                    QStringLiteral("\"%1\" has unsaved changes.").arg(controller_->displayName()),
                                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    };
    saveFileNamePrompt = [this] {
        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Project As"),
                                                    defaultDirectory() + QLatin1Char('/') + controller_->displayName(),
                                                    projectFilter());
        if (!path.isEmpty() && QFileInfo(path).suffix().isEmpty())
            path += QLatin1Char('.') + QLatin1String(kProjectFileExtension);
        return path;
    };

    // A workspace where the viewport dominates.
    tabifyDockWidget(ui_->propertiesDock, ui_->acousticsDock);
    tabifyDockWidget(ui_->acousticsDock, ui_->solverDock);
    tabifyDockWidget(ui_->solverDock, ui_->mechanicsDock);
    ui_->propertiesDock->raise();
    resizeDocks({ui_->sceneDock, ui_->propertiesDock}, {280, 380}, Qt::Horizontal);
    resizeDocks({ui_->analysisDock}, {220}, Qt::Vertical);
    defaultLayout_ = saveState();
    updateDocumentState();
    updateSelectionState();
    statusBar()->showMessage(QStringLiteral("Ready"));
}

MainWindow::~MainWindow() = default;

ViewportWidget& MainWindow::viewport() { return *ui_->viewport; }
SceneTreePanel& MainWindow::sceneTree() { return *ui_->scenePanel; }
PropertiesPanel& MainWindow::properties() { return *ui_->propertiesPanel; }
AnalysisPanel& MainWindow::analysis() { return *ui_->analysisPanel; }
AcousticsPanel& MainWindow::acousticsPanel() { return *ui_->acousticsPanel; }

QAction* MainWindow::action(const QString& objectName) const
{
    return findChild<QAction*>(objectName);
}

void MainWindow::showFailure(const QString& message)
{
    statusBar()->showMessage(message, 8000);
}

void MainWindow::report(const Status& status)
{
    if (!status)
        showFailure(QString::fromStdString(status.message()));
}

void MainWindow::connectActions()
{
    // --- File ---
    connect(ui_->actionNew, &QAction::triggered, this, [this] {
        if (confirmDiscardChanges())
            controller_->newProject();
    });
    connect(ui_->actionOpen, &QAction::triggered, this, [this] {
        if (!confirmDiscardChanges())
            return;
        const QString path =
            QFileDialog::getOpenFileName(this, QStringLiteral("Open Project"), defaultDirectory(), projectFilter());
        if (path.isEmpty())
            return;
        if (Status status = openProjectFile(path); !status)
            QMessageBox::critical(this, QStringLiteral("Could not open the project"),
                                  QString::fromStdString(status.message()));
    });
    connect(ui_->actionSave, &QAction::triggered, this, [this] { saveInteractive(); });
    connect(ui_->actionSaveAs, &QAction::triggered, this, [this] { saveAsInteractive(); });
    connect(ui_->actionExit, &QAction::triggered, this, &QWidget::close);

    // --- Edit ---
    connect(ui_->actionUndo, &QAction::triggered, controller_, &DocumentController::undo);
    connect(ui_->actionRedo, &QAction::triggered, controller_, &DocumentController::redo);
    connect(ui_->actionDuplicate, &QAction::triggered, this, [this] { report(controller_->duplicateSelection()); });
    connect(ui_->actionDelete, &QAction::triggered, this, [this] { report(controller_->deleteSelection()); });
    connect(ui_->actionRename, &QAction::triggered, this, [this] {
        ui_->sceneDock->show();
        ui_->scenePanel->editSelectedName();
    });
    connect(ui_->actionSelectAll, &QAction::triggered, controller_, &DocumentController::selectAll);
    connect(ui_->actionDeselect, &QAction::triggered, controller_, &DocumentController::clearSelection);

    // --- View ---
    connect(ui_->actionShowGrid, &QAction::toggled, ui_->viewport, &ViewportWidget::setGridVisible);
    connect(ui_->actionShowAxes, &QAction::toggled, ui_->viewport, &ViewportWidget::setAxesVisible);
    connect(ui_->actionFrameAll, &QAction::triggered, ui_->viewport, &ViewportWidget::frameAll);
    connect(ui_->actionFrameSelection, &QAction::triggered, ui_->viewport, &ViewportWidget::frameSelection);
    connect(ui_->actionResetCamera, &QAction::triggered, ui_->viewport, &ViewportWidget::resetCamera);
    connect(ui_->actionViewTop, &QAction::triggered, ui_->viewport, &ViewportWidget::viewTop);
    connect(ui_->actionViewFront, &QAction::triggered, ui_->viewport, &ViewportWidget::viewFront);
    connect(ui_->actionViewRight, &QAction::triggered, ui_->viewport, &ViewportWidget::viewRight);
    connect(ui_->actionResetLayout, &QAction::triggered, this, [this] { restoreState(defaultLayout_); });

    // --- Scene ---
    connect(ui_->actionNewGroup, &QAction::triggered, this, [this] {
        if (auto created = controller_->createGroup(); !created)
            showFailure(QString::fromStdString(created.message()));
    });
    connect(ui_->actionToggleVisibility, &QAction::triggered, this, [this] {
        for (const Id& id : controller_->selection())
            if (const SceneObject* object = controller_->scene().find(id))
                report(controller_->setVisible(id, !object->state.visible));
    });
    connect(ui_->actionMoveToTopLevel, &QAction::triggered, this, [this] {
        for (const Id& id : controller_->selection())
            report(controller_->moveObject(id, Id()));
    });

    // --- Geometry ---
    const std::pair<QAction*, const char*> primitives[] = {
        {ui_->actionAddBox, "box"},     {ui_->actionAddSphere, "sphere"}, {ui_->actionAddCylinder, "cylinder"},
        {ui_->actionAddCone, "cone"},   {ui_->actionAddPlane, "plane"},   {ui_->actionAddDisc, "disc"},
        {ui_->actionAddEllipsoid, "ellipsoid"}};
    for (const auto& [action, kind] : primitives) {
        connect(action, &QAction::triggered, this, [this, kind = QString::fromLatin1(kind)] {
            if (auto created = controller_->createPrimitive(kind); !created)
                showFailure(QString::fromStdString(created.message()));
        });
    }

    // --- Materials ---
    connect(ui_->actionMaterialLibrary, &QAction::triggered, this, &MainWindow::showMaterialLibrary);
    connect(ui_->menuAssignMaterial, &QMenu::aboutToShow, this, &MainWindow::fillAssignMaterialMenu);
    connect(ui_->actionClearMaterial, &QAction::triggered, this, [this] {
        for (const Id& id : controller_->selection())
            report(controller_->setMaterial(id, Id()));
    });

    // --- Simulation ---
    connect(ui_->actionRunDemonstrator, &QAction::triggered, this, [this] {
        controller_->startDemonstratorJob();
        ui_->analysisDock->show();
        ui_->analysisPanel->showSimulationStatus();
    });
    connect(ui_->actionCancelJobs, &QAction::triggered, controller_, &DocumentController::cancelAllJobs);

    // --- Tools ---
    connect(unitGroup_, &QActionGroup::triggered, this, [this](QAction* action) {
        units_->setLengthUnit(static_cast<units::LengthUnit>(action->data().toInt()));
    });
    connect(ui_->actionClearConsole, &QAction::triggered, ui_->analysisPanel, &AnalysisPanel::clearConsole);

    // --- Help ---
    connect(ui_->actionAbout, &QAction::triggered, this, [this] { AboutDialog(graphicsDescription_, this).exec(); });
    connect(ui_->actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);

    // --- state that follows the document ---
    connect(controller_, &DocumentController::documentStateChanged, this, &MainWindow::updateDocumentState);
    connect(controller_, &DocumentController::projectReplaced, this, [this] {
        updateDocumentState();
        ui_->viewport->frameAll();
    });
    connect(controller_, &DocumentController::selectionChanged, this, &MainWindow::updateSelectionState);
    connect(controller_, &DocumentController::operationFailed, this, &MainWindow::showFailure);
    connect(ui_->scenePanel, &SceneTreePanel::operationFailed, this, &MainWindow::showFailure);
    connect(ui_->propertiesPanel, &PropertiesPanel::operationFailed, this, &MainWindow::showFailure);
    connect(controller_, &DocumentController::jobChanged, this, [this](const JobInfo& info) {
        ui_->actionCancelJobs->setEnabled(controller_->jobs().hasUnfinishedJobs());
        if (isFinished(info.state))
            statusBar()->showMessage(QStringLiteral("%1: %2")
                                         .arg(QString::fromStdString(info.name),
                                              QString::fromUtf8(toString(info.state).data())),
                                     6000);
    });
    ui_->actionCancelJobs->setEnabled(false);

    connect(ui_->viewport, &ViewportWidget::graphicsInitialized, this, [this](const QString& description) {
        graphicsDescription_ = description;
        ui_->analysisPanel->setGraphicsDescription(description);
    });
    connect(ui_->viewport, &ViewportWidget::graphicsFailed, this, [this](const QString& message) {
        graphicsDescription_ = QStringLiteral("unavailable: ") + message;
        ui_->analysisPanel->setGraphicsDescription(graphicsDescription_);
        QMessageBox::critical(this, QStringLiteral("3D graphics are unavailable"),
                              message + QStringLiteral("\n\nThe viewport will stay empty. Projects can still be "
                                                       "opened, edited through the panels and saved."));
    });
}

void MainWindow::connectAcoustics()
{
    using resonant::acoustics::ArrayLayout;
    using resonant::acoustics::RegionKind;

    ui_->viewport->bridge().addDecorator(makeAcousticDecorator());
    ui_->propertiesPanel->addComponentEditorFactory(makeAcousticEditorFactory(*acoustics_));
    ui_->acousticsPanel->setController(acoustics_, units_);
    fieldAnalysis_ = new FieldAnalysisPanel(ui_->analysisPanel);
    fieldAnalysis_->setController(acoustics_, units_);
    ui_->analysisPanel->addTab(fieldAnalysis_, QStringLiteral("Field Analysis"));

    const auto created = [this](const Result<Id>& result) {
        if (!result)
            showFailure(QString::fromStdString(result.message()));
        else
            ui_->viewport->frameAll();
    };
    connect(ui_->actionAddEmitter, &QAction::triggered, this, [this, created] { created(acoustics_->addEmitter()); });
    const std::pair<QAction*, ArrayLayout> arrays[] = {
        {ui_->actionAddLinearArray, ArrayLayout::Linear},           {ui_->actionAddPlanarArray, ArrayLayout::RectangularPlanar},
        {ui_->actionAddCircularArray, ArrayLayout::Circular},       {ui_->actionAddCylindricalArray, ArrayLayout::Cylindrical},
        {ui_->actionAddSphericalArray, ArrayLayout::Spherical},     {ui_->actionAddCustomArray, ArrayLayout::Custom}};
    for (const auto& [action, layout] : arrays)
        connect(action, &QAction::triggered, this, [this, created, layout] { created(acoustics_->addArray(layout)); });
    connect(ui_->actionAddTarget, &QAction::triggered, this, [this, created] { created(acoustics_->addTarget()); });
    connect(ui_->actionAddProbe, &QAction::triggered, this, [this, created] { created(acoustics_->addProbe()); });
    connect(ui_->actionAddFieldPlane, &QAction::triggered, this, [this, created] { created(acoustics_->addRegion(RegionKind::PlaneXZ)); });
    connect(ui_->actionAddFieldVolume, &QAction::triggered, this, [this, created] { created(acoustics_->addRegion(RegionKind::Volume)); });

    const auto showAcoustics = [this](const char* tab) {
        ui_->acousticsDock->show();
        ui_->acousticsDock->raise();
        ui_->acousticsPanel->showTab(QString::fromLatin1(tab));
    };
    connect(ui_->actionFocusArray, &QAction::triggered, this, [showAcoustics] { showAcoustics("focusTab"); });
    connect(ui_->actionSolveInverse, &QAction::triggered, this, [showAcoustics] { showAcoustics("inverseTab"); });
    connect(ui_->actionRunField, &QAction::triggered, this, [this, showAcoustics] {
        showAcoustics("simulationTab");
        if (ui_->acousticsPanel->runSimulation()) {
            ui_->analysisDock->show();
            ui_->analysisPanel->showSimulationStatus();
        }
    });

    connect(acoustics_, &AcousticController::message, this, &MainWindow::showFailure);
    connect(acoustics_, &AcousticController::displayChanged, this, [this] {
        ui_->viewport->setField(acoustics_->displayGrid(), acoustics_->display());
    });
    connect(acoustics_, &AcousticController::resultsChanged, this, [this] {
        if (acoustics_->displayedResult())
            ui_->analysisPanel->showTab(fieldAnalysis_);
    });
}

void MainWindow::connectManipulators()
{
    manipulators_ = new ManipulatorController(controller_, acoustics_, this);
    ui_->viewport->bridge().addDecorator(manipulators_->makeDecorator());
    ui_->propertiesPanel->addComponentEditorFactory(makeManipulatorEditorFactory(*acoustics_, *manipulators_));

    const std::pair<QAction*, const char*> kinds[] = {
        {ui_->actionAddParabolicReflector, "reflector.parabolic"},   {ui_->actionAddEllipticalReflector, "reflector.elliptical"},
        {ui_->actionAddSphericalReflector, "reflector.spherical"},   {ui_->actionAddCylindricalReflector, "reflector.cylindrical"},
        {ui_->actionAddFlatReflector, "reflector.flat"},             {ui_->actionAddLens, "lens.spherical"},
        {ui_->actionAddCircularWaveguide, "waveguide.circular"},     {ui_->actionAddRectangularWaveguide, "waveguide.rectangular"},
        {ui_->actionAddRectangularCavity, "cavity.rectangular"},     {ui_->actionAddCylindricalCavity, "cavity.cylindrical"},
        {ui_->actionAddSphericalCavity, "cavity.spherical"},         {ui_->actionAddSlitPlate, "diffraction.slit"}};
    for (const auto& [action, kind] : kinds) {
        connect(action, &QAction::triggered, this, [this, kind = QString::fromLatin1(kind)] {
            if (auto created = manipulators_->addManipulator(kind); !created)
                showFailure(QString::fromStdString(created.message()));
            else
                ui_->viewport->frameAll();
        });
    }
    connect(ui_->actionShowRays, &QAction::toggled, manipulators_, &ManipulatorController::setShowRays);
    connect(manipulators_, &ManipulatorController::displayChanged, ui_->viewport, &ViewportWidget::refresh);
}

FdtdPanel& MainWindow::fullWavePanel()
{
    return *ui_->fdtdPanel;
}

void MainWindow::connectFullWave()
{
    fdtd_ = new FdtdController(controller_, acoustics_, this);
    ui_->fdtdPanel->setControllers(acoustics_, fdtd_);
    timeSignals_ = new TimeSignalPanel(ui_->analysisPanel);
    timeSignals_->setController(fdtd_);
    ui_->analysisPanel->addTab(timeSignals_, QStringLiteral("Time Signals"));

    connect(ui_->actionRunFullWave, &QAction::triggered, this, [this] {
        ui_->solverDock->show();
        ui_->solverDock->raise();
        if (ui_->fdtdPanel->run()) {
            ui_->analysisDock->show();
            ui_->analysisPanel->showSimulationStatus();
        }
    });
    connect(fdtd_, &FdtdController::message, this, &MainWindow::showFailure);
    connect(fdtd_, &FdtdController::resultsChanged, this, [this] {
        if (!fdtd_->results().empty())
            ui_->analysisPanel->showTab(timeSignals_);
    });
}

MechanicsPanel& MainWindow::mechanicsPanel()
{
    return *ui_->mechanicsPanel;
}

void MainWindow::connectMechanics()
{
    using resonant::mechanics::Analysis;
    using resonant::mechanics::Idealization;
    mechanics_ = new MechanicsController(controller_, acoustics_, this);
    ui_->mechanicsPanel->setControllers(mechanics_, acoustics_);
    ui_->viewport->bridge().addDecorator(makeMechanicsDecorator());
    ui_->viewport->bridge().setReplaced([this](const resonant::Id& id) { return id == mechanics_->displayedStructure(); });
    ui_->propertiesPanel->addComponentEditorFactory(makeMechanicalEditorFactory(*acoustics_, *mechanics_));

    const auto showPanel = [this] {
        ui_->mechanicsDock->show();
        ui_->mechanicsDock->raise();
    };
    const std::pair<QAction*, Idealization> idealizations[] = {{ui_->actionMakeBeam, Idealization::Beam},
                                                               {ui_->actionMakePlate, Idealization::Plate},
                                                               {ui_->actionMakeMembrane, Idealization::Membrane},
                                                               {ui_->actionMakeSolid, Idealization::Solid}};
    for (const auto& [action, idealization] : idealizations)
        connect(action, &QAction::triggered, this, [this, showPanel, idealization] {
            const resonant::Id id = controller_->primarySelection();
            if (const resonant::Status status = mechanics_->makeStructural(id, idealization); !status) {
                showFailure(QString::fromStdString(status.message()));
                return;
            }
            showPanel();
            ui_->mechanicsPanel->selectStructure(id);
            ui_->mechanicsPanel->check();
        });
    // New markers go on the selected structure, or the one chosen in the panel.
    const auto structureForMarker = [this] {
        const resonant::Id selected = controller_->primarySelection();
        const resonant::SceneObject* object = controller_->scene().find(selected);
        if (object && dynamic_cast<const resonant::mechanics::StructureComponent*>(object->state.component.get()))
            return selected;
        return ui_->mechanicsPanel->selectedStructure();
    };
    const auto created = [this](const resonant::Result<resonant::Id>& result) {
        if (!result)
            showFailure(QString::fromStdString(result.message()));
    };
    connect(ui_->actionAddActuator, &QAction::triggered, this, [this, created, structureForMarker] { created(mechanics_->addActuator(structureForMarker())); });
    connect(ui_->actionAddSensor, &QAction::triggered, this, [this, created, structureForMarker] { created(mechanics_->addSensor(structureForMarker())); });
    connect(ui_->actionAddVibrationTarget, &QAction::triggered, this,
            [this, created, structureForMarker] { created(mechanics_->addTarget(structureForMarker())); });
    connect(ui_->actionAddHorn, &QAction::triggered, this, [this, created] {
        created(mechanics_->addRod("rod.horn"));
        ui_->viewport->frameAll();
    });
    connect(ui_->actionAddPeriodicRod, &QAction::triggered, this, [this, created] {
        created(mechanics_->addRod("rod.periodic"));
        ui_->viewport->frameAll();
    });
    const std::pair<QAction*, const char*> runs[] = {{ui_->actionRunModal, "modesTab"},
                                                     {ui_->actionRunHarmonic, "harmonicTab"},
                                                     {ui_->actionRunTransient, "transientTab"},
                                                     {ui_->actionRunStatic, "staticTab"}};
    for (const auto& [action, tab] : runs)
        connect(action, &QAction::triggered, this, [this, showPanel, structureForMarker, tab = QString::fromLatin1(tab)] {
            showPanel();
            if (const resonant::Id id = structureForMarker(); !id.isNull())
                ui_->mechanicsPanel->selectStructure(id);
            ui_->mechanicsPanel->showTab(tab);
            const bool started = tab == QLatin1String("modesTab")      ? ui_->mechanicsPanel->runModal()
                                 : tab == QLatin1String("harmonicTab") ? ui_->mechanicsPanel->runHarmonic()
                                 : tab == QLatin1String("transientTab") ? ui_->mechanicsPanel->runTransient()
                                                                        : ui_->mechanicsPanel->runStatic();
            if (started) {
                ui_->analysisDock->show();
                ui_->analysisPanel->showSimulationStatus();
            }
        });
    connect(mechanics_, &MechanicsController::displayChanged, this, [this] {
        ui_->viewport->setScalarMesh(mechanics_->displayMesh(), mechanics_->displayLines());
    });
    connect(mechanics_, &MechanicsController::message, this, &MainWindow::showFailure);
}

void MainWindow::updateDocumentState()
{
    setWindowTitle(QStringLiteral("%1[*] - Resonant").arg(controller_->displayName()));
    setWindowModified(controller_->isModified());

    const CommandStack& commands = controller_->commands();
    ui_->actionUndo->setEnabled(commands.canUndo());
    ui_->actionRedo->setEnabled(commands.canRedo());
    ui_->actionUndo->setText(commands.canUndo()
                                 ? QStringLiteral("&Undo %1").arg(QString::fromStdString(commands.undoText()))
                                 : QStringLiteral("&Undo"));
    ui_->actionRedo->setText(commands.canRedo()
                                 ? QStringLiteral("&Redo %1").arg(QString::fromStdString(commands.redoText()))
                                 : QStringLiteral("&Redo"));
}

void MainWindow::updateSelectionState()
{
    const bool any = !controller_->selection().isEmpty();
    for (QAction* action : {ui_->actionDuplicate, ui_->actionDelete, ui_->actionToggleVisibility,
                            ui_->actionMoveToTopLevel, ui_->actionClearMaterial, ui_->actionFrameSelection,
                            ui_->actionDeselect})
        action->setEnabled(any);
    ui_->actionRename->setEnabled(controller_->selection().size() == 1);
    ui_->menuAssignMaterial->setEnabled(any);

    if (!any) {
        statusBar()->showMessage(QStringLiteral("Nothing selected"), 3000);
    } else if (controller_->selection().size() == 1) {
        const SceneObject* object = controller_->scene().find(controller_->primarySelection());
        statusBar()->showMessage(QStringLiteral("Selected: %1").arg(QString::fromStdString(object->state.name)), 3000);
    } else {
        statusBar()->showMessage(QStringLiteral("%1 objects selected").arg(controller_->selection().size()), 3000);
    }
}

void MainWindow::fillAssignMaterialMenu()
{
    ui_->menuAssignMaterial->clear();
    for (const Material& material : controller_->project().materials.materials()) {
        const Id materialId = material.id();
        ui_->menuAssignMaterial->addAction(QString::fromStdString(material.name()), this, [this, materialId] {
            for (const Id& id : controller_->selection())
                report(controller_->setMaterial(id, materialId));
        });
    }
    if (ui_->menuAssignMaterial->isEmpty())
        ui_->menuAssignMaterial->addAction(QStringLiteral("(the library is empty)"))->setEnabled(false);
}

void MainWindow::showMaterialLibrary()
{
    MaterialsDialog dialog(controller_->project().materials, this);
    if (dialog.exec() == QDialog::Accepted)
        report(controller_->setMaterialLibrary(dialog.library(), QStringLiteral("Edit materials")));
}

QString MainWindow::defaultDirectory() const
{
    if (!controller_->filePath().isEmpty())
        return QFileInfo(controller_->filePath()).absolutePath();
    const QString last = QSettings().value(QStringLiteral("files/lastDirectory")).toString();
    return last.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : last;
}

Status MainWindow::openProjectFile(const QString& path)
{
    QStringList warnings;
    if (Status status = controller_->openProject(path, &warnings); !status)
        return status;
    QSettings().setValue(QStringLiteral("files/lastDirectory"), QFileInfo(path).absolutePath());
    statusBar()->showMessage(warnings.isEmpty()
                                 ? QStringLiteral("Opened %1").arg(path)
                                 : QStringLiteral("Opened %1 with %2 warning(s); see the Console").arg(path).arg(warnings.size()),
                             8000);
    return Status::success();
}

Status MainWindow::saveProjectToFile(const QString& path)
{
    if (Status status = controller_->saveProjectAs(path); !status)
        return status;
    QSettings().setValue(QStringLiteral("files/lastDirectory"), QFileInfo(path).absolutePath());
    statusBar()->showMessage(QStringLiteral("Saved %1").arg(path), 5000);
    return Status::success();
}

bool MainWindow::saveInteractive()
{
    if (controller_->filePath().isEmpty())
        return saveAsInteractive();
    if (Status status = saveProjectToFile(controller_->filePath()); !status) {
        QMessageBox::critical(this, QStringLiteral("Could not save the project"), QString::fromStdString(status.message()));
        return false;
    }
    return true;
}

bool MainWindow::saveAsInteractive()
{
    const QString path = saveFileNamePrompt();
    if (path.isEmpty())
        return false;
    if (Status status = saveProjectToFile(path); !status) {
        QMessageBox::critical(this, QStringLiteral("Could not save the project"), QString::fromStdString(status.message()));
        return false;
    }
    return true;
}

bool MainWindow::confirmDiscardChanges()
{
    if (!controller_->isModified())
        return true;
    switch (unsavedChangesPrompt()) {
    case QMessageBox::Save: return saveInteractive();
    case QMessageBox::Discard: return true;
    default: return false;
    }
}

void MainWindow::restoreWorkspace()
{
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("workspace/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("workspace/state")).toByteArray());
    ui_->actionShowGrid->setChecked(settings.value(QStringLiteral("view/grid"), true).toBool());
    ui_->actionShowAxes->setChecked(settings.value(QStringLiteral("view/axes"), true).toBool());
    const int unit = settings.value(QStringLiteral("units/length"), static_cast<int>(units::LengthUnit::Meter)).toInt();
    for (QAction* action : unitGroup_->actions()) {
        if (action->data().toInt() == unit) {
            action->setChecked(true);
            units_->setLengthUnit(static_cast<units::LengthUnit>(unit));
        }
    }
}

void MainWindow::saveWorkspace() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("workspace/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("workspace/state"), saveState());
    settings.setValue(QStringLiteral("view/grid"), ui_->actionShowGrid->isChecked());
    settings.setValue(QStringLiteral("view/axes"), ui_->actionShowAxes->isChecked());
    settings.setValue(QStringLiteral("units/length"), static_cast<int>(units_->lengthUnit()));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!confirmDiscardChanges()) {
        event->ignore();
        return;
    }
    saveWorkspace();
    controller_->cancelAllJobs();
    event->accept();
}
