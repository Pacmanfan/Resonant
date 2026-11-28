// The main window: the engineering workspace. Its layout (menus, toolbars,
// docks, central viewport) is defined in forms/MainWindow.ui; this class
// connects the actions to the DocumentController and keeps the window's
// state (title, enabled actions, status bar) in step with the document.
#pragma once

#include "foundation/Id.h"
#include "foundation/Result.h"

#include <QJsonArray>
#include <QMainWindow>
#include <QMessageBox>

#include <functional>
#include <memory>

class AcousticController;
class AcousticsPanel;
class AnalysisPanel;
class FieldAnalysisPanel;
class ManipulatorController;
class FdtdController;
class MechanicsController;
class MechanicsPanel;
class EmController;
class ExperimentsController;
class ExperimentsPanel;
class OptimizationController;
class OptimizationPanel;
class FunctionalController;
class FunctionalPanel;
class MaterialScalesPanel;
class MaterialStatePanel;
class AccountingPanel;
class LaboratoryPanel;
class LaboratoryController;
class AccountingController;
class MaterialStateController;
class MultiscaleController;
class EmPanel;
class FdtdPanel;
class TimeSignalPanel;
class DisplayUnits;
class DocumentController;
class PropertiesPanel;
class SceneTreePanel;
class ViewportWidget;
class PythonConsole;
class PythonSession;
class ScriptEditor;
class HelpWindow;
class PluginsDialog;
class AutomationServer;
class AutomationServerDialog;
class WorkflowsDialog;
class DemoController;
class DemoStudioPanel;
class PresenterOverlay;
class QActionGroup;
class QDialog;
class QDockWidget;
namespace resonant {
class Project;
namespace plugins {
class PluginManager;
}
}  // namespace resonant
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    DocumentController& controller() { return *controller_; }
    DisplayUnits& displayUnits() { return *units_; }
    ViewportWidget& viewport();
    SceneTreePanel& sceneTree();
    PropertiesPanel& properties();
    AnalysisPanel& analysis();
    AcousticController& acoustics() { return *acoustics_; }
    AcousticsPanel& acousticsPanel();
    FieldAnalysisPanel& fieldAnalysis() { return *fieldAnalysis_; }
    ManipulatorController& manipulators() { return *manipulators_; }
    FdtdController& fullWave() { return *fdtd_; }
    MechanicsController& mechanics() { return *mechanics_; }
    MechanicsPanel& mechanicsPanel();
    EmController& electromagnetics() { return *em_; }
    EmPanel& emPanel();
    ExperimentsController& experiments() { return *experiments_; }
    ExperimentsPanel& experimentsPanel();
    OptimizationController& optimization() { return *optimization_; }
    OptimizationPanel& optimizationPanel();
    FunctionalPanel& functionalPanel();
    FunctionalController& functional() { return *functional_; }
    MaterialScalesPanel& materialScalesPanel();
    MultiscaleController& multiscale() { return *multiscale_; }
    MaterialStatePanel& materialStatePanel();
    MaterialStateController& materialState() { return *materialState_; }
    AccountingPanel& accountingPanel();
    AccountingController& accounting() { return *accounting_; }
    LaboratoryPanel& laboratoryPanel();
    LaboratoryController& laboratory() { return *laboratory_; }
    FdtdPanel& fullWavePanel();
    TimeSignalPanel& timeSignals() { return *timeSignals_; }
    QAction* action(const QString& objectName) const;

    // Python (ADR-000215): one worker process shared by the console and the editor.
    PythonSession& pythonSession() { return *python_; }
    PythonConsole& pythonConsole();
    void showPythonConsole();
    bool startPython();  // starts the worker; the console gets the project when it is ready
    ScriptEditor& scriptEditor();  // created on first use
    // Applies a changed copy of the project (a script's, a plugin's or a
    // workflow's result) as ONE undoable change.
    resonant::Status applyProjectCopy(const resonant::Project& changed, const QString& description);

    // Help: the window (created on first use), a topic, the topic of the widget with focus or under the mouse.
    HelpWindow& helpWindow();
    void showHelp(const QString& topic);
    void showContextHelp();

    // Plugins and workflows.
    resonant::plugins::PluginManager& plugins() { return *plugins_; }
    void reloadPlugins();
    PluginsDialog* showPluginsDialog();      // non-modal; closes with the window
    WorkflowsDialog* showWorkflowsDialog();  // non-modal, kept while the window lives
    QDialog* showMaterialsDialog();          // non-modal view of the library (screenshots)

    // The automation server (REQ-CHAT-20261126-012): off until started from its dialog.
    AutomationServer& automationServer();
    AutomationServerDialog* showAutomationServerDialog();  // non-modal, kept while the window lives

    // Demo Studio and presenter mode.
    DemoController& demos() { return *demos_; }
    DemoStudioPanel& demoStudioPanel();
    PresenterOverlay& presenterOverlay();
    void setPresenterMode(bool on);
    bool presenterMode() const { return presenter_; }

    // File operations without dialogs (the dialog-driven actions call these).
    resonant::Status openProjectFile(const QString& path);
    resonant::Status saveProjectToFile(const QString& path);

    // Returns true when it is safe to discard the current project: there
    // are no unsaved changes, or the user saved them or chose to discard.
    bool confirmDiscardChanges();

    // What to ask when there are unsaved changes. The default shows a
    // Save / Discard / Cancel message box; tests replace it.
    std::function<QMessageBox::StandardButton()> unsavedChangesPrompt;
    // Where a Save As dialog would be shown; tests replace it.
    std::function<QString()> saveFileNamePrompt;

    // Workspace persistence (window geometry, dock layout, view options).
    void restoreWorkspace();
    void saveWorkspace() const;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void connectActions();
    void connectAcoustics();
    void connectManipulators();
    void connectFullWave();
    void connectMechanics();
    void connectElectromagnetics();
    void connectExperiments();
    void connectOptimization();
    void connectFunctional();
    void connectMultiscale();
    void connectMaterialState();
    void connectAccounting();
    void connectLaboratory();
    void connectAutomation();
    void connectHelp();
    void connectDemos();
    void fillExtensionsMenu();
    void callExtension(const QString& pluginId, const QString& extensionId, const QString& kind);
    void updateDocumentState();
    void updateSelectionState();
    void fillAssignMaterialMenu();
    void showMaterialLibrary();
    void report(const resonant::Status& status);
    void showFailure(const QString& message);
    bool saveInteractive();
    bool saveAsInteractive();
    QString defaultDirectory() const;

    std::unique_ptr<Ui::MainWindow> ui_;
    DocumentController* controller_ = nullptr;
    FunctionalController* functional_ = nullptr;
    MultiscaleController* multiscale_ = nullptr;
    MaterialStateController* materialState_ = nullptr;
    AccountingController* accounting_ = nullptr;
    LaboratoryController* laboratory_ = nullptr;
    AutomationServer* automationServer_ = nullptr;  // deleted before the document it serves
    AutomationServerDialog* automationDialog_ = nullptr;
    DisplayUnits* units_ = nullptr;
    AcousticController* acoustics_ = nullptr;
    FieldAnalysisPanel* fieldAnalysis_ = nullptr;
    ManipulatorController* manipulators_ = nullptr;
    FdtdController* fdtd_ = nullptr;
    MechanicsController* mechanics_ = nullptr;
    EmController* em_ = nullptr;
    ExperimentsController* experiments_ = nullptr;
    OptimizationController* optimization_ = nullptr;
    TimeSignalPanel* timeSignals_ = nullptr;
    QActionGroup* unitGroup_ = nullptr;
    PythonSession* python_ = nullptr;
    ScriptEditor* scriptEditor_ = nullptr;
    HelpWindow* helpWindow_ = nullptr;
    WorkflowsDialog* workflowsDialog_ = nullptr;
    std::unique_ptr<resonant::plugins::PluginManager> plugins_;
    QJsonArray pythonExtensions_;
    DemoController* demos_ = nullptr;
    bool presenter_ = false;
    QByteArray presenterSavedState_;
    bool presenterMenuVisible_ = true;
    QByteArray defaultLayout_;
    QString graphicsDescription_ = QStringLiteral("not initialized yet");
};
