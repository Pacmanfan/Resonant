#pragma once

// Executes an experiment:
//
//    1 validate the definition            8 arm triggers
//    2 verify device availability         9 execute acquisition and authorized outputs
//    3 verify hardware capabilities      10 record data
//    4 verify calibration status         11 verify data integrity
//    5 verify synchronization            12 process results
//    6 check safety limits / authorization 13 generate a report
//    7 configure instruments
//
// through the states Draft -> Validating -> Ready -> Armed -> Running ->
// Stopping -> Completed | Failed | Cancelled. A failed check stops the run
// before anything is configured or output; a device error or a lost
// connection during acquisition ends it (Failed) without processing the
// partial data; cancellation stops outputs at once and closes the recorded
// data sets as incomplete (they stay readable).
//
// Physical output needs an OutputAuthorization granted for this experiment
// (Safety.h); the run consumes it when its outputs start and releases it at
// the end. Without one, an experiment with outputs does not run.
#include "experiments/Acquisition.h"
#include "experiments/Calibration.h"
#include "experiments/Experiment.h"
#include "experiments/MeasurementStore.h"
#include "experiments/Processing.h"
#include "simulation/Job.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

namespace resonant::experiments {

// What an experiment would output: the request an operator authorizes.
OutputRequest outputRequestFor(const Experiment& experiment);

struct RunProgress {
    ExperimentState state = ExperimentState::Draft;
    int step = 0;
    std::string stepName;
    double fraction = 0.0;
    std::string message;
    AcquisitionStats stats;
};

struct RunOptions {
    std::optional<OutputAuthorization> authorization;
    std::string operatorName;
    bool keepInMemory = true;
    std::function<void(const RunProgress&)> progress;  // called on the running thread
};

struct RunResult {
    ExperimentState state = ExperimentState::Draft;
    ExecutionRecord record;
    std::vector<ProcessingItem> raw;
    std::vector<ProcessingItem> derived;
    std::vector<SpatialDataset> spatial;
    DataQuality quality;
    std::vector<std::string> warnings;
    std::string reportHtml;
};

class ExperimentRunner {
public:
    ExperimentRunner(DeviceRegistry& registry, const CalibrationLibrary& calibrations, MeasurementStore* store);

    // Synchronous; run it on a worker (ExperimentJob) to keep a user
    // interface responsive. Never throws.
    RunResult run(const Experiment& experiment, const RunOptions& options);

    // Thread-safe.
    void cancel();
    bool cancelRequested() const { return cancel_.load(); }
    // The live view of the acquisition running now (or null).
    std::shared_ptr<const AcquisitionEngine> activeAcquisition() const;

private:
    DeviceRegistry& registry_;
    const CalibrationLibrary& calibrations_;
    MeasurementStore* store_;
    std::atomic<bool> cancel_{false};
    mutable std::mutex mutex_;
    std::shared_ptr<AcquisitionEngine> active_;
};

// The runner as a background job of the JobManager.
class ExperimentJob final : public Job {
public:
    ExperimentJob(std::shared_ptr<ExperimentRunner> runner, Experiment experiment, RunOptions options);
    std::string name() const override { return "Experiment: " + experiment_.name; }
    JobEnd run(JobContext& context) override;
    // After the job finished.
    const RunResult& result() const { return result_; }
    ExperimentRunner& runner() { return *runner_; }

private:
    std::shared_ptr<ExperimentRunner> runner_;
    Experiment experiment_;
    RunOptions options_;
    RunResult result_;
};

}  // namespace resonant::experiments
