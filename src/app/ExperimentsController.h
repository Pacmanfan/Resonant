#pragma once

// Experiments in the application: the device registry (simulated devices and
// the hardware drivers this build has), the project's experiments,
// calibrations and measurements, output authorization, runs as background
// jobs, measured data, comparison with the simulation, reports, archives,
// and the markers the viewport shows (sensors, actuators, measured points).
//
// Output authorizations live only in this controller's memory: they are
// never saved, and opening a project never creates one. Discovery runs on
// its own thread and never delays the application's start.
#include "experiments/ExperimentProject.h"
#include "experiments/ExperimentRunner.h"
#include "experiments/MeasurementStore.h"
#include "render/Layers.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <map>
#include <memory>
#include <optional>

class DocumentController;

class ExperimentsController : public QObject {
    Q_OBJECT
public:
    explicit ExperimentsController(DocumentController* document, QObject* parent = nullptr);
    ~ExperimentsController() override;

    // --- devices ---
    void discoverDevices();  // asynchronous; devicesChanged when done
    bool discovering() const;
    std::vector<resonant::experiments::DeviceInfo> devices() const;
    QString deviceStatus(const std::string& id) const;
    resonant::Result<resonant::experiments::DeviceCapabilities> capabilities(const std::string& id);
    QString discoveryReport() const { return discoveryReport_; }
    QString driverSummary() const;
    resonant::experiments::DeviceRegistry& registry() { return *registry_; }

    // --- experiments (undoable project changes) ---
    resonant::experiments::ExperimentWorkspace workspace() const;
    std::vector<std::pair<QString, QString>> templates() const;  // id, title
    resonant::Result<std::string> addFromTemplate(const QString& templateId);
    resonant::Status removeExperiment(const std::string& id);
    resonant::Status setExperiment(const resonant::experiments::Experiment& experiment, const QString& description);
    resonant::experiments::ValidationReport validate(const std::string& id) const;

    // --- calibration ---
    resonant::Status addCalibration(resonant::experiments::CalibrationRecord record);
    resonant::Status importCalibration(const QString& path);  // a calibration record as JSON
    resonant::Status selectCalibration(const std::string& experimentId, const std::string& sensorId, const std::string& calibrationId);

    // --- output authorization (never persisted) ---
    resonant::experiments::OutputRequest outputRequest(const std::string& id) const;
    resonant::experiments::SafetyCheck safetyCheck(const std::string& id) const;
    resonant::Status authorize(const std::string& id, const QString& operatorName, const QString& acknowledgement);
    bool isAuthorized(const std::string& id) const;
    void emergencyStop();
    void resetEmergencyStop(const QString& operatorName);
    bool emergencyStopActive() const;

    // --- runs ---
    // Starts a run as a background job. Fails (without output) when the
    // project has not been saved (raw data is stored next to it), when a run
    // is going, or when an output is needed and not authorized.
    resonant::Status run(const std::string& id);
    void stop();
    bool running() const { return runner_ != nullptr; }
    const resonant::experiments::RunProgress& progress() const { return progress_; }
    // A copy of the live acquisition (empty when nothing is acquiring).
    std::optional<resonant::experiments::LiveView::Snapshot> liveSnapshot() const;
    const resonant::experiments::RunResult* lastResult() const { return lastResult_ ? &*lastResult_ : nullptr; }

    // --- measurements ---
    std::vector<resonant::experiments::MeasurementRecord> measurements(const std::string& experimentId) const;
    resonant::Result<resonant::experiments::MeasurementData> data(const std::string& measurementId);
    resonant::Status importTouchstone(const QString& path, const std::string& experimentId);
    // A comparison of a measurement (or of the experiment's spatial data set)
    // with the simulation the project offers for it; recorded in the project.
    resonant::Result<resonant::experiments::ComparisonRecord> compareWithSimulation(const std::string& experimentId,
                                                                                    const std::string& measurementId);
    QString reportHtml(const std::string& experimentId) const;
    resonant::Status exportArchive(const std::string& experimentId, const QString& directory);
    resonant::Status importArchive(const QString& directory);

    // --- viewport ---
    std::vector<resonant::render::DrawItem> markers(const std::string& experimentId) const;

signals:
    void devicesChanged();
    void experimentsChanged();
    void runStateChanged();
    void runFinished(const QString& message);
    void markersChanged();

private:
    resonant::Status storeWorkspace(const resonant::experiments::ExperimentWorkspace& workspace, const QString& description);
    resonant::Result<resonant::experiments::MeasurementStore*> store();
    void onJobChanged(const resonant::JobInfo& info);
    void finishRun();

    DocumentController* document_;
    std::shared_ptr<resonant::experiments::DeviceRegistry> registry_;
    QString discoveryReport_;
    std::map<std::string, resonant::experiments::OutputAuthorization> authorizations_;
    std::unique_ptr<resonant::experiments::MeasurementStore> store_;
    QString storeProject_;
    std::shared_ptr<resonant::experiments::ExperimentRunner> runner_;
    std::shared_ptr<resonant::experiments::ExperimentJob> job_;
    resonant::Id jobId_;
    std::string runningExperiment_;
    std::unique_ptr<resonant::experiments::CalibrationLibrary> runCalibrations_;
    resonant::experiments::RunProgress progress_;
    std::optional<resonant::experiments::RunResult> lastResult_;
    std::map<std::string, QString> reports_;
    QTimer progressTimer_;
};
