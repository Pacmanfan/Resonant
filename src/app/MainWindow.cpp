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
#include "EmController.h"
#include "EmEditor.h"
#include "EmMarkers.h"
#include "EmPanel.h"
#include "ExperimentsController.h"
#include "ExperimentsPanel.h"
#include "OptimizationController.h"
#include "OptimizationPanel.h"
#include "FunctionalController.h"
#include "FunctionalPanel.h"
#include "MaterialScalesPanel.h"
#include "AccountingController.h"
#include "AccountingPanel.h"
#include "MaterialStateController.h"
#include "MaterialStatePanel.h"
#include "MultiscaleController.h"
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
#include "DemoController.h"
#include "DemoStudioPanel.h"
#include "HelpTopics.h"
#include "HelpWindow.h"
#include "PluginsDialog.h"
#include "AutomationServer.h"
#include "AutomationServerDialog.h"
#include "PresenterOverlay.h"
#include "PythonConsole.h"
#include "PythonSession.h"
#include "ResourcePaths.h"
#include "ScriptEditor.h"
#include "WorkflowsDialog.h"
#include "commands/ProjectChange.h"
#include "foundation/Log.h"
#include "foundation/Version.h"
#include "plugins/PluginManager.h"
#include "project/ProjectSerializer.h"
#include "ui_MainWindow.h"

#include <QApplication>
#include <QCursor>
#include <QDockWidget>
#include <QInputDialog>
#include <QJsonDocument>
#include <QMenuBar>
#include <QToolBar>
#include <QTimer>

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
    ui_->menuPanels->addAction(ui_->emDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->experimentsDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->optimizationDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->functionalDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->materialScalesDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->materialStateDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->accountingDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->pythonConsoleDock->toggleViewAction());
    ui_->menuPanels->addAction(ui_->demoStudioDock->toggleViewAction());
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
    connectElectromagnetics();
    connectExperiments();
    connectOptimization();
    connectFunctional();
    connectMultiscale();
    connectMaterialState();
    connectAccounting();
    connectAutomation();
    connectHelp();
    connectDemos();

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
    tabifyDockWidget(ui_->mechanicsDock, ui_->emDock);
    tabifyDockWidget(ui_->emDock, ui_->experimentsDock);
    tabifyDockWidget(ui_->experimentsDock, ui_->optimizationDock);
    tabifyDockWidget(ui_->optimizationDock, ui_->functionalDock);
    tabifyDockWidget(ui_->functionalDock, ui_->materialScalesDock);
    tabifyDockWidget(ui_->materialScalesDock, ui_->materialStateDock);
    tabifyDockWidget(ui_->materialStateDock, ui_->accountingDock);
    tabifyDockWidget(ui_->accountingDock, ui_->demoStudioDock);
    tabifyDockWidget(ui_->analysisDock, ui_->pythonConsoleDock);
    ui_->demoStudioDock->hide();
    ui_->presenterDock->hide();
    ui_->propertiesDock->raise();
    ui_->analysisDock->raise();
    resizeDocks({ui_->sceneDock, ui_->propertiesDock}, {280, 380}, Qt::Horizontal);
    resizeDocks({ui_->analysisDock}, {220}, Qt::Vertical);
    defaultLayout_ = saveState();
    updateDocumentState();
    updateSelectionState();
    statusBar()->showMessage(QStringLiteral("Ready"));
}

MainWindow::~MainWindow()
{
    // Remote calls run on this thread against the document: end them before the document goes.
    delete automationDialog_;
    automationDialog_ = nullptr;
    delete automationServer_;
    automationServer_ = nullptr;
}

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

EmPanel& MainWindow::emPanel()
{
    return *ui_->emPanel;
}

ExperimentsPanel& MainWindow::experimentsPanel()
{
    return *ui_->experimentsPanel;
}

void MainWindow::connectExperiments()
{
    experiments_ = new ExperimentsController(controller_, this);
    ui_->experimentsPanel->setController(experiments_);
    const auto showPanel = [this] {
        ui_->experimentsDock->show();
        ui_->experimentsDock->raise();
    };
    connect(ui_->actionShowExperiments, &QAction::triggered, this, showPanel);
    connect(ui_->actionDiscoverDevices, &QAction::triggered, this, [this, showPanel] {
        showPanel();
        experiments_->discoverDevices();
    });
    connect(ui_->actionEmergencyStop, &QAction::triggered, this, [this] { ui_->experimentsPanel->emergencyStop(); });
    // Sensors, actuators, scan positions and measured points of the selected experiment.
    const auto updateMarkers = [this] {
        ui_->viewport->setExtraItems(experiments_->markers(ui_->experimentsPanel->selectedExperiment()));
    };
    connect(experiments_, &ExperimentsController::markersChanged, this, updateMarkers);
    connect(ui_->experimentsPanel, &ExperimentsPanel::selectedExperimentChanged, this, updateMarkers);
    connect(experiments_, &ExperimentsController::runFinished, this, [this](const QString& message) {
        statusBar()->showMessage(tr("Experiment: %1").arg(message), 8000);
    });
}

OptimizationPanel& MainWindow::optimizationPanel()
{
    return *ui_->optimizationPanel;
}

AccountingPanel& MainWindow::accountingPanel()
{
    return *ui_->accountingPanel;
}

void MainWindow::connectAccounting()
{
    accounting_ = new AccountingController(controller_, this);
    ui_->accountingPanel->setController(accounting_);
    connect(accounting_, &AccountingController::runFailed, this,
            [this](const QString& message) { statusBar()->showMessage(tr("Accounting: %1").arg(message), 8000); });
}

MaterialStatePanel& MainWindow::materialStatePanel()
{
    return *ui_->materialStatePanel;
}

void MainWindow::connectMaterialState()
{
    materialState_ = new MaterialStateController(controller_, this);
    ui_->materialStatePanel->setController(materialState_);
    connect(materialState_, &MaterialStateController::runFailed, this,
            [this](const QString& message) { statusBar()->showMessage(tr("Material state: %1").arg(message), 8000); });
}

MaterialScalesPanel& MainWindow::materialScalesPanel()
{
    return *ui_->materialScalesPanel;
}

void MainWindow::connectMultiscale()
{
    multiscale_ = new MultiscaleController(controller_, this);
    ui_->materialScalesPanel->setController(multiscale_);
    connect(multiscale_, &MultiscaleController::derivationFailed, this,
            [this](const QString& message) { statusBar()->showMessage(tr("Multiscale derivation: %1").arg(message), 8000); });
}

FunctionalPanel& MainWindow::functionalPanel()
{
    return *ui_->functionalPanel;
}

void MainWindow::connectFunctional()
{
    functional_ = new FunctionalController(controller_, this);
    ui_->functionalPanel->setController(functional_);
    connect(functional_, &FunctionalController::studyFailed, this,
            [this](const QString& message) { statusBar()->showMessage(tr("Functional structure: %1").arg(message), 8000); });
}

void MainWindow::connectOptimization()
{
    optimization_ = new OptimizationController(controller_, this);
    ui_->optimizationPanel->setController(optimization_);
    connect(ui_->actionShowOptimization, &QAction::triggered, this, [this] {
        ui_->optimizationDock->show();
        ui_->optimizationDock->raise();
    });
    connect(ui_->actionStopOptimization, &QAction::triggered, this, [this] { optimization_->stop(); });
    connect(optimization_, &OptimizationController::runFinished, this, [this](const QString& message) {
        statusBar()->showMessage(tr("Optimization: %1").arg(message), 8000);
    });
}

void MainWindow::connectElectromagnetics()
{
    using resonant::em::EmArrayLayout;
    using resonant::em::SourceKind;
    em_ = new EmController(controller_, this);
    ui_->emPanel->setController(em_);
    ui_->viewport->bridge().addDecorator(makeEmDecorator());
    ui_->propertiesPanel->addComponentEditorFactory(makeEmEditorFactory(*acoustics_, *em_));

    const auto showPanel = [this] {
        ui_->emDock->show();
        ui_->emDock->raise();
    };
    // New objects are selected and shown in the panel.
    const auto created = [this, showPanel](const resonant::Result<resonant::Id>& result) {
        if (!result) {
            showFailure(QString::fromStdString(result.message()));
            return;
        }
        controller_->select(result.value());
        showPanel();
        ui_->emPanel->selectObject(result.value());
    };
    const std::pair<QAction*, const char*> antennas[] = {{ui_->actionAddDipoleAntenna, "antenna.dipole"},   {ui_->actionAddMonopoleAntenna, "antenna.monopole"},
                                                         {ui_->actionAddLoopAntenna, "antenna.loop"},       {ui_->actionAddPatchAntenna, "antenna.patch"},
                                                         {ui_->actionAddHornAntenna, "antenna.horn"},       {ui_->actionAddHelixAntenna, "antenna.helix"}};
    for (const auto& [action, kind] : antennas)
        connect(action, &QAction::triggered, this, [this, created, kind = QString::fromLatin1(kind)] { created(em_->addAntenna(kind)); });
    const std::pair<QAction*, SourceKind> sources[] = {{ui_->actionAddElectricDipoleSource, SourceKind::ElectricDipole},
                                                       {ui_->actionAddMagneticDipoleSource, SourceKind::MagneticDipole},
                                                       {ui_->actionAddPlaneWaveSource, SourceKind::PlaneWave}};
    for (const auto& [action, kind] : sources)
        connect(action, &QAction::triggered, this, [this, created, kind] { created(em_->addSource(kind)); });
    const std::pair<QAction*, EmArrayLayout> arrays[] = {{ui_->actionAddLinearRfArray, EmArrayLayout::Linear},
                                                         {ui_->actionAddPlanarRfArray, EmArrayLayout::Planar},
                                                         {ui_->actionAddCircularRfArray, EmArrayLayout::Circular}};
    for (const auto& [action, layout] : arrays)
        connect(action, &QAction::triggered, this, [this, created, layout] { created(em_->addArray(layout)); });
    const std::pair<QAction*, const char*> guides[] = {{ui_->actionAddEmRectangularGuide, "guide.rectangular"},
                                                       {ui_->actionAddEmCircularGuide, "guide.circular"},
                                                       {ui_->actionAddCoaxialLine, "guide.coaxial"},
                                                       {ui_->actionAddEmCavity, "cavity.rectangular.em"}};
    for (const auto& [action, kind] : guides)
        connect(action, &QAction::triggered, this, [this, created, kind = QString::fromLatin1(kind)] { created(em_->addGuide(kind)); });
    connect(ui_->actionAnalyseEm, &QAction::triggered, this, [this, showPanel] {
        showPanel();
        ui_->emPanel->selectObject(controller_->primarySelection());
    });
    connect(ui_->actionEvaluateEmField, &QAction::triggered, this, [this, showPanel] {
        showPanel();
        ui_->emPanel->showTab(QStringLiteral("fieldsTab"));
        ui_->emPanel->evaluateRegion();
    });
    connect(ui_->actionRunEmFullWave, &QAction::triggered, this, [this, showPanel] {
        showPanel();
        ui_->emPanel->showTab(QStringLiteral("fieldsTab"));
        if (ui_->emPanel->runFullWave()) {
            ui_->analysisDock->show();
            ui_->analysisPanel->showSimulationStatus();
        }
    });
    // The panel follows the selection when an electromagnetic object is selected.
    connect(controller_, &DocumentController::selectionChanged, this, [this] {
        const resonant::Id id = controller_->primarySelection();
        if (em_->kindOf(id) != EmController::Kind::None && id != ui_->emPanel->selectedObject())
            ui_->emPanel->selectObject(id);
    });
    connect(controller_, &DocumentController::sceneChanged, this, [this](const resonant::SceneEvent& event) {
        if (event.id == ui_->emPanel->selectedObject() && em_->kindOf(event.id) != EmController::Kind::None)
            ui_->emPanel->analyse();
    });
    // One field at a time in the viewport: the one chosen last (acoustic or electromagnetic).
    connect(em_, &EmController::displayChanged, this, [this] { ui_->viewport->setField(em_->displayGrid(), em_->fieldDisplay()); });
    connect(em_, &EmController::patternChanged, this, [this] { ui_->viewport->setScalarMesh(em_->patternMesh(), em_->patternLines()); });
    connect(em_, &EmController::message, this, &MainWindow::showFailure);
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

// --- Python, plugins and workflows (ADR-000215) ------------------------------

PythonConsole& MainWindow::pythonConsole() { return *ui_->pythonConsole; }
DemoStudioPanel& MainWindow::demoStudioPanel() { return *ui_->demoStudioPanel; }
PresenterOverlay& MainWindow::presenterOverlay() { return *ui_->presenterOverlay; }

namespace {
QJsonObject projectAsJson(const Project& project)
{
    return QJsonDocument::fromJson(QByteArray::fromStdString(serializeProject(project).dump())).object();
}
}  // namespace

void MainWindow::connectAutomation()
{
    python_ = new PythonSession(this);
    ui_->pythonConsole->setSession(python_);
    ui_->pythonConsole->setProjectProvider([this] { return projectAsJson(controller_->project()); });
    connect(python_, &PythonSession::applyRequested, this, [this](int, const QJsonObject& project, const QString& description) {
        auto loaded = deserializeProject(nlohmann::json::parse(QJsonDocument(project).toJson(QJsonDocument::Compact).toStdString()));
        if (!loaded) {
            ui_->pythonConsole->appendOutput(QStringLiteral("app.apply: the project could not be read: %1\n").arg(QString::fromStdString(loaded.message())),
                                             QStringLiteral("error"));
            return;
        }
        const Status s = applyProjectCopy(*loaded.value().project, description.isEmpty() ? QStringLiteral("Script changes") : description);
        ui_->pythonConsole->appendOutput(s ? QStringLiteral("(applied to the project: \"%1\"; Edit > Undo reverts it)\n").arg(description)
                                           : QStringLiteral("app.apply failed: %1\n").arg(QString::fromStdString(s.message())),
                                         s ? QStringLiteral("info") : QStringLiteral("error"));
    });
    connect(python_, &PythonSession::pullRequested, ui_->pythonConsole, [this] { ui_->pythonConsole->sendProject(true); });
    connect(python_, &PythonSession::statusMessage, this, [this](const QString& text) { statusBar()->showMessage(text, 8000); });
    connect(python_, &PythonSession::readyChanged, this, [this](bool ready) {
        if (!ready || !plugins_)
            return;
        // Enabled Python plugins are loaded into the worker when it starts.
        QJsonArray dirs;
        for (const auto& d : plugins_->readyPythonPlugins())
            dirs.append(QString::fromStdString(d.string()));
        if (!dirs.isEmpty())
            python_->request({{QStringLiteral("op"), QStringLiteral("load_plugins")}, {QStringLiteral("plugins"), dirs}});
    });
    connect(python_, &PythonSession::received, this, [this](int, const QJsonObject& m) {
        if (m.value(QStringLiteral("type")).toString() != QLatin1String("plugins"))
            return;
        pythonExtensions_ = {};
        for (const auto& v : m.value(QStringLiteral("plugins")).toArray()) {
            const QJsonObject p = v.toObject();
            // The worker names a plugin by its directory; find its manifest.
            std::string id;
            for (const auto& r : plugins_->plugins())
                if (QString::fromStdString(r.manifest.directory.string()) == p.value(QStringLiteral("directory")).toString())
                    id = r.manifest.id;
            if (id.empty())
                continue;
            if (p.value(QStringLiteral("ok")).toBool()) {
                std::vector<std::string> names;
                for (const auto& e : p.value(QStringLiteral("extensions")).toArray()) {
                    QJsonObject ext = e.toObject();
                    names.push_back(ext.value(QStringLiteral("id")).toString().toStdString());
                    ext.insert(QStringLiteral("plugin"), QString::fromStdString(id));
                    pythonExtensions_.append(ext);
                }
                plugins_->markLoaded(id, names);
            } else {
                plugins_->markFailed(id, p.value(QStringLiteral("error")).toString().toStdString());
                logError("plugins", id + ": " + p.value(QStringLiteral("error")).toString().toStdString());
            }
        }
        fillExtensionsMenu();
    });
    connect(ui_->actionShowPythonConsole, &QAction::triggered, this, &MainWindow::showPythonConsole);
    connect(ui_->actionScriptEditor, &QAction::triggered, this, [this] {
        scriptEditor().show();
        scriptEditor().raise();
        scriptEditor().activateWindow();
    });
    connect(ui_->actionWorkflows, &QAction::triggered, this, &MainWindow::showWorkflowsDialog);
    connect(ui_->actionPlugins, &QAction::triggered, this, &MainWindow::showPluginsDialog);
    connect(ui_->actionAutomationServer, &QAction::triggered, this, &MainWindow::showAutomationServerDialog);

    const ResourcePaths paths = ResourcePaths::locate();
    plugins_ = std::make_unique<plugins::PluginManager>(
        plugins::PluginManager::standardSearchPaths(paths.resourceRoot.isEmpty() ? std::filesystem::path()
                                                                                : std::filesystem::path(paths.resourceRoot.toStdString()) / "plugins" / "examples"),
        plugins::PluginManager::standardConfigFile(), kApplicationVersion);
    reloadPlugins();
}

bool MainWindow::startPython()
{
    if (python_->isRunning())
        return true;
    if (!python_->start()) {
        ui_->pythonConsole->appendOutput(python_->startupError() + QLatin1Char('\n'), QStringLiteral("error"));
        return false;
    }
    return true;
}

void MainWindow::showPythonConsole()
{
    ui_->pythonConsoleDock->show();
    ui_->pythonConsoleDock->raise();
    startPython();
    ui_->pythonConsole->setFocus();
}

ScriptEditor& MainWindow::scriptEditor()
{
    if (!scriptEditor_) {
        scriptEditor_ = new ScriptEditor(this);
        scriptEditor_->setWindowFlag(Qt::Window);
        scriptEditor_->setSession(python_);
        scriptEditor_->setDocument(controller_);
        scriptEditor_->setStarter([this] { return startPython(); });
        connect(scriptEditor_, &ScriptEditor::helpRequested, this, &MainWindow::showHelp);
    }
    return *scriptEditor_;
}

Status MainWindow::applyProjectCopy(const Project& changed, const QString& description)
{
    auto change = projectChangeCommand(controller_->project(), changed, description.toStdString());
    if (!change)
        return Status::failure(change.message());
    for (const std::string& n : change.value().notApplied)
        logWarning("automation", description.toStdString() + ": not applied: " + n);
    if (change.value().empty()) {
        statusBar()->showMessage(QStringLiteral("%1: nothing changed").arg(description), 6000);
        return Status::success();
    }
    const ProjectChange& c = change.value();
    const QString summary = QStringLiteral("%1: %2 added, %3 removed, %4 changed, %5 moved%6%7")
                                .arg(description)
                                .arg(c.added)
                                .arg(c.removed)
                                .arg(c.changed)
                                .arg(c.moved)
                                .arg(c.materials ? QStringLiteral(", materials") : QString())
                                .arg(c.extensions ? QStringLiteral(", %1 setting(s)").arg(c.extensions) : QString());
    const bool settings = c.extensions > 0;
    if (Status s = controller_->pushCommand(std::move(change.value().command)); !s)
        return s;
    if (settings) {
        // Project-level settings changed: the panels re-read them.
        emit acoustics_->settingsChanged();
        emit mechanics_->settingsChanged();
        emit em_->settingsChanged();
        emit fdtd_->settingsChanged();
        emit optimization_->problemsChanged();
        emit experiments_->experimentsChanged();
    }
    statusBar()->showMessage(summary + QStringLiteral(" (undoable)"), 8000);
    logInfo("automation", summary.toStdString());
    return Status::success();
}

void MainWindow::reloadPlugins()
{
    std::vector<std::string> capabilities;
    if (!python_->interpreter().isEmpty() && !ResourcePaths::locate().pythonPackage.isEmpty())
        capabilities.push_back("python");
    plugins_->discover(capabilities);
    plugins_->loadEnabled();
    for (const auto& r : plugins_->plugins())
        if (r.state == plugins::PluginState::Failed || r.state == plugins::PluginState::Invalid)
            logWarning("plugins", r.manifest.id + ": " + std::string(plugins::toString(r.state)) + " - " + r.message);
        else if (r.state == plugins::PluginState::Loaded)
            logInfo("plugins", r.manifest.id + ": " + r.message);
    pythonExtensions_ = {};
    if (!plugins_->readyPythonPlugins().empty()) {
        if (python_->isReady()) {
            QJsonArray dirs;
            for (const auto& d : plugins_->readyPythonPlugins())
                dirs.append(QString::fromStdString(d.string()));
            python_->request({{QStringLiteral("op"), QStringLiteral("load_plugins")}, {QStringLiteral("plugins"), dirs}});
        } else
            QTimer::singleShot(0, this, [this] { startPython(); });  // enabled Python plugins load in the worker
    }
    fillExtensionsMenu();
}

void MainWindow::fillExtensionsMenu()
{
    QMenu* menu = ui_->menuExtensions;
    menu->clear();
    for (const auto& v : pythonExtensions_) {
        const QJsonObject e = v.toObject();
        const QString kind = e.value(QStringLiteral("kind")).toString();
        const QString label = kind == QLatin1String("geometry-generator") ? QStringLiteral("Generate: %1")
                              : kind == QLatin1String("analysis")         ? QStringLiteral("Analyse: %1")
                                                                          : QStringLiteral("Import: %1");
        QAction* a = menu->addAction(label.arg(e.value(QStringLiteral("title")).toString()));
        a->setToolTip(QStringLiteral("From the plugin %1").arg(e.value(QStringLiteral("plugin")).toString()));
        connect(a, &QAction::triggered, this, [this, e, kind] {
            callExtension(e.value(QStringLiteral("plugin")).toString(), e.value(QStringLiteral("id")).toString(), kind);
        });
    }
    if (menu->isEmpty())
        menu->addAction(QStringLiteral("(no extensions: enable plugins in Tools > Plugins)"))->setEnabled(false);
}

void MainWindow::callExtension(const QString& pluginId, const QString& extensionId, const QString& kind)
{
    if (!python_->isReady()) {
        showFailure(QStringLiteral("The Python worker is not ready yet"));
        return;
    }
    QJsonObject params;
    if (kind == QLatin1String("measurement-importer")) {
        const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("Import Measurement"), defaultDirectory());
        if (file.isEmpty())
            return;
        params.insert(QStringLiteral("path"), file);
    } else if (kind == QLatin1String("geometry-generator")) {
        QJsonObject defaults;
        for (const auto& v : pythonExtensions_)
            if (v.toObject().value(QStringLiteral("id")).toString() == extensionId) {
                const QJsonObject declared = v.toObject().value(QStringLiteral("parameters")).toObject();
                for (auto it = declared.begin(); it != declared.end(); ++it)
                    defaults.insert(it.key(), it.value().toObject().value(QStringLiteral("default")));
            }
        bool ok = false;
        const QString text = QInputDialog::getMultiLineText(this, QStringLiteral("Generator Parameters"), QStringLiteral("Parameters (JSON):"),
                                                            QString::fromUtf8(QJsonDocument(defaults).toJson()), &ok);
        if (!ok)
            return;
        params = QJsonDocument::fromJson(text.toUtf8()).object();
    }
    showPythonConsole();
    ui_->pythonConsole->sendProject(false);
    ui_->pythonConsole->appendOutput(QStringLiteral(">>> # extension %1/%2\n").arg(pluginId, extensionId), QStringLiteral("echo"));
    const int id = python_->request({{QStringLiteral("op"), QStringLiteral("call")}, {QStringLiteral("plugin"), pluginId},
                                     {QStringLiteral("extension"), extensionId}, {QStringLiteral("params"), params}});
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(python_, &PythonSession::received, this, [this, id, connection](int rid, const QJsonObject& m) {
        if (rid != id)
            return;
        const QString type = m.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("result"))
            ui_->pythonConsole->appendOutput(QString::fromUtf8(QJsonDocument::fromVariant(m.value(QStringLiteral("value")).toVariant()).toJson()) +
                                             QLatin1Char('\n'));
        else if (type == QLatin1String("error"))
            ui_->pythonConsole->appendOutput(m.value(QStringLiteral("traceback")).toString() + m.value(QStringLiteral("message")).toString() +
                                                 QLatin1Char('\n'),
                                             QStringLiteral("error"));
        else if (type == QLatin1String("done"))
            disconnect(*connection);
    });
}

PluginsDialog* MainWindow::showPluginsDialog()
{
    auto* dialog = new PluginsDialog(*plugins_, [this] { reloadPlugins(); }, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &PluginsDialog::helpRequested, this, &MainWindow::showHelp);
    dialog->show();
    return dialog;
}

AutomationServer& MainWindow::automationServer()
{
    if (!automationServer_)
        automationServer_ = new AutomationServer(*controller_, this);
    return *automationServer_;
}

AutomationServerDialog* MainWindow::showAutomationServerDialog()
{
    if (!automationDialog_) {
        automationDialog_ = new AutomationServerDialog(automationServer(), *controller_, this);
        connect(automationDialog_, &AutomationServerDialog::helpRequested, this, &MainWindow::showHelp);
    }
    automationDialog_->show();
    automationDialog_->raise();
    return automationDialog_;
}

WorkflowsDialog* MainWindow::showWorkflowsDialog()
{
    if (!workflowsDialog_) {
        workflowsDialog_ = new WorkflowsDialog(*controller_, [this](const Project& p, const QString& d) { return applyProjectCopy(p, d); }, this);
        connect(workflowsDialog_, &WorkflowsDialog::helpRequested, this, &MainWindow::showHelp);
    }
    workflowsDialog_->show();
    workflowsDialog_->raise();
    return workflowsDialog_;
}

QDialog* MainWindow::showMaterialsDialog()
{
    auto* dialog = new MaterialsDialog(controller_->project().materials, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    return dialog;
}

// --- help --------------------------------------------------------------------

void MainWindow::connectHelp()
{
    connect(ui_->actionContextHelp, &QAction::triggered, this, &MainWindow::showContextHelp);
    connect(ui_->actionHelpContents, &QAction::triggered, this, [this] { showHelp(QString::fromLatin1(HelpTopics::kHome)); });
    connect(ui_->actionKeyboardShortcuts, &QAction::triggered, this, [this] { showHelp(QStringLiteral("ui.shortcuts")); });
    connect(ui_->actionSearchHelp, &QAction::triggered, this, [this] {
        HelpWindow& help = helpWindow();
        if (help.currentTopic().isEmpty())
            help.home();
        help.show();
        help.raise();
        help.search(QString());
        if (auto* edit = help.findChild<QLineEdit*>(QStringLiteral("searchEdit"))) {
            if (auto* tabs = help.findChild<QTabWidget*>(QStringLiteral("navTabs")))
                tabs->setCurrentIndex(1);
            edit->setFocus();
        }
    });
    // F1 also works in the floating windows that belong to this one.
    ui_->actionContextHelp->setShortcutContext(Qt::ApplicationShortcut);
}

HelpWindow& MainWindow::helpWindow()
{
    if (!helpWindow_) {
        helpWindow_ = new HelpWindow(this);
        helpWindow_->resize(1100, 760);
        helpWindow_->load(ResourcePaths::locate().help);
    }
    return *helpWindow_;
}

void MainWindow::showHelp(const QString& topic)
{
    HelpWindow& help = helpWindow();
    if (help.isLoaded())
        help.showTopic(topic.isEmpty() ? QString::fromLatin1(HelpTopics::kHome) : topic);
    help.show();
    help.raise();
    help.activateWindow();
}

void MainWindow::showContextHelp()
{
    QWidget* w = QApplication::focusWidget();
    if (!w)
        w = focusWidget();  // the window's focus child, also when the window is not active
    if (!w || w == this || (helpWindow_ && helpWindow_->isAncestorOf(w)))
        w = QApplication::widgetAt(QCursor::pos());
    if (w && helpWindow_ && (w == helpWindow_ || helpWindow_->isAncestorOf(w)))
        w = nullptr;
    showHelp(w ? HelpTopics::topicFor(w) : QString::fromLatin1(HelpTopics::kHome));
}

// --- Demo Studio and presenter mode -------------------------------------------

void MainWindow::connectDemos()
{
    demos_ = new DemoController(*this, this);
    demos_->setResourceRoot(ResourcePaths::locate().resourceRoot);
    ui_->demoStudioPanel->setController(demos_);
    ui_->demoStudioPanel->setConfirmation([this] { return confirmDiscardChanges(); });
    ui_->demoStudioPanel->setPresenterSwitch([this](bool on) { setPresenterMode(on); });
    connect(ui_->actionShowDemoStudio, &QAction::triggered, this, [this] {
        ui_->demoStudioDock->show();
        ui_->demoStudioDock->raise();
    });
    connect(ui_->actionPresenterMode, &QAction::toggled, this, [this](bool on) {
        if (on != presenter_)
            setPresenterMode(on);
    });
    PresenterOverlay* overlay = ui_->presenterOverlay;
    connect(overlay, &PresenterOverlay::nextRequested, demos_, &DemoController::next);
    connect(overlay, &PresenterOverlay::pauseToggled, this, [this](bool pause) {
        if (pause)
            demos_->pause();
        else
            demos_->resume();
    });
    connect(overlay, &PresenterOverlay::restartRequested, demos_, &DemoController::restart);
    connect(overlay, &PresenterOverlay::exitRequested, this, [this] { setPresenterMode(false); });
    connect(demos_, &DemoController::runStarted, this, [this, overlay] {
        overlay->setTitle(QString::fromStdString(demos_->profile() ? demos_->profile()->name : demos_->sequence().name));
    });
    connect(demos_, &DemoController::stepStarted, this, [this, overlay](int index) {
        const auto& step = demos_->sequence().steps[static_cast<size_t>(index)];
        overlay->setStep(index, demos_->stepCount(), QString::fromStdString(step.description), QString::fromStdString(step.notes));
    });
    connect(demos_, &DemoController::waitingForPresenter, this, [overlay](int, const QString& text) {
        overlay->setWaiting(true);
        if (!text.isEmpty())
            overlay->setStep(-1, 0, text, {});
    });
    connect(demos_, &DemoController::stateChanged, this, [this, overlay] { overlay->setPaused(demos_->isPaused()); });
    connect(demos_, &DemoController::runFinished, this, [overlay](const QString& status, const QString& message) { overlay->setFinished(status, message); });
}

void MainWindow::setPresenterMode(bool on)
{
    if (on == presenter_)
        return;
    presenter_ = on;
    if (on) {
        // A clean layout: the viewport and the presenter bar; nothing is closed or lost.
        presenterSavedState_ = saveState();
        presenterMenuVisible_ = menuBar()->isVisible();
        for (QDockWidget* d : findChildren<QDockWidget*>())
            if (d != ui_->presenterDock)
                d->hide();
        for (QToolBar* t : findChildren<QToolBar*>())
            t->hide();
        menuBar()->hide();
        statusBar()->hide();
        ui_->presenterDock->show();
        // The bar takes only the height it needs; the viewport gets the rest.
        QTimer::singleShot(0, this, [this] {
            resizeDocks({ui_->presenterDock}, {ui_->presenterOverlay->sizeHint().height() + 24}, Qt::Vertical);
        });
        logInfo("demo", "presenter mode on (Esc or Exit Presenter Mode returns to the normal layout)");
    } else {
        ui_->presenterDock->hide();
        restoreState(presenterSavedState_);
        ui_->presenterDock->hide();
        menuBar()->setVisible(presenterMenuVisible_);
        statusBar()->show();
        logInfo("demo", "presenter mode off");
    }
    const QSignalBlocker block(ui_->actionPresenterMode);
    ui_->actionPresenterMode->setChecked(on);
    // The F11 / presenter actions still work without the menu bar.
    if (on) {
        addAction(ui_->actionPresenterMode);
        addAction(ui_->actionContextHelp);
    }
}
