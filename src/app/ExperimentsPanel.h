#pragma once

// The Experimental Hardware workspace: devices (discovery, status,
// capabilities), experiments (templates, definition, validation),
// calibration records, the run (safety, explicit output authorization, run,
// stop, emergency stop, progress, live display) and measurements (data,
// comparison with the simulation, reports, archives). Laid out in
// forms/ExperimentsPanel.ui.
//
// The live display copies the acquisition's live view on a timer; it never
// waits for the acquisition, which never waits for it.
#include <QTimer>
#include <QWidget>

#include <memory>
#include <string>

class ExperimentsController;
class QTreeWidgetItem;

namespace Ui {
class ExperimentsPanel;
}

class ExperimentsPanel : public QWidget {
    Q_OBJECT
public:
    explicit ExperimentsPanel(QWidget* parent = nullptr);
    ~ExperimentsPanel() override;

    void setController(ExperimentsController* controller);
    std::string selectedExperiment() const;
    void selectExperiment(const std::string& id);
    std::string selectedMeasurement() const;
    QString statusText() const;
    QString definitionText() const;   // the definition tree, flattened (tests)
    int measurementCount() const;

    // The actions behind the buttons (the dialogs are skipped when `operatorName` is given).
    bool authorizeSelected(const QString& operatorName = {});
    bool runSelected();
    void stop();
    void emergencyStop();

signals:
    void selectedExperimentChanged();

private:
    void refreshDevices();
    void refreshExperiments();
    void refreshDefinition();
    void refreshCalibrations();
    void refreshRun();
    void refreshMeasurements();
    void showMeasurement();
    void refreshLive();
    void setStatus(const QString& text, bool error = false);

    std::unique_ptr<Ui::ExperimentsPanel> ui_;
    ExperimentsController* controller_ = nullptr;
    QTimer liveTimer_;
    bool updating_ = false;
};
