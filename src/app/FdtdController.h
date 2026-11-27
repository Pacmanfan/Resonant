#pragma once

// The full-wave (FDTD) solver in the application: its settings, the
// preflight plan, the solver assessment, running and the results.
#include "fdtd/FdtdSetup.h"

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

class AcousticController;
class DocumentController;

class FdtdController : public QObject {
    Q_OBJECT
public:
    FdtdController(DocumentController* document, AcousticController* acoustics, QObject* parent = nullptr);

    resonant::fdtd::FdtdSettings settings() const;
    // Undoable; stored in the project.
    resonant::Status setSettings(const resonant::fdtd::FdtdSettings& settings);

    // The plan for a region with the current scene and settings. Building it
    // also builds the simulation geometry.
    resonant::fdtd::Prepared prepare(const resonant::Id& regionId) const;
    // Every solver with what it would leave out of this problem.
    QString assessment(const resonant::Id& regionId, const resonant::fdtd::FdtdPlan& plan) const;

    resonant::Result<resonant::Id> run(const resonant::Id& regionId);
    void cancel();
    bool isRunning() const { return pending_ != nullptr; }

    const std::vector<std::shared_ptr<const resonant::fdtd::FdtdResult>>& results() const { return results_; }
    // Shows one recorded instant of a result in the viewport.
    bool showSnapshot(size_t resultIndex, size_t snapshotIndex);
    // Shows a result's steady-state field again (continuous signals only).
    bool showSteadyState(size_t resultIndex);
    // Time, then one column of pressure per probe.
    resonant::Status exportProbes(size_t resultIndex, const QString& path) const;

signals:
    void settingsChanged();
    void resultsChanged();
    void runningChanged();
    void message(const QString& text);

private:
    void onJobChanged(const resonant::JobInfo& info);

    DocumentController* document_;
    AcousticController* acoustics_;
    std::vector<std::shared_ptr<const resonant::fdtd::FdtdResult>> results_;
    std::shared_ptr<resonant::fdtd::FdtdJob> pending_;
    resonant::Id pendingId_;
};
