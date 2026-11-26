// The main window: the engineering workspace. Its layout (menus, toolbars,
// docks, central viewport) is defined in forms/MainWindow.ui; this class
// connects the actions to the DocumentController and keeps the window's
// state (title, enabled actions, status bar) in step with the document.
#pragma once

#include "foundation/Id.h"
#include "foundation/Result.h"

#include <QMainWindow>
#include <QMessageBox>

#include <functional>
#include <memory>

class AcousticController;
class AcousticsPanel;
class AnalysisPanel;
class FieldAnalysisPanel;
class DisplayUnits;
class DocumentController;
class PropertiesPanel;
class SceneTreePanel;
class ViewportWidget;
class QActionGroup;
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
    QAction* action(const QString& objectName) const;

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
    DisplayUnits* units_ = nullptr;
    AcousticController* acoustics_ = nullptr;
    FieldAnalysisPanel* fieldAnalysis_ = nullptr;
    QActionGroup* unitGroup_ = nullptr;
    QByteArray defaultLayout_;
    QString graphicsDescription_ = QStringLiteral("not initialized yet");
};
