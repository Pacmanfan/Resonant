#pragma once

// Experiment archives: everything needed to inspect and re-analyse an
// experiment without the original hardware or project.
//
//   <archive>/experiment.json   the experiment, the calibrations it used (with
//                               their history), its measurement records, spatial
//                               data sets and comparisons
//   <archive>/data/...          the measurement data sets (copied from the store)
//
// reproduceProcessing re-runs the recorded processing step of every derived
// data set on its sources and compares the result with what was stored.
#include "experiments/ExperimentProject.h"
#include "experiments/MeasurementStore.h"

#include <filesystem>
#include <memory>

namespace resonant::experiments {

inline constexpr const char* kArchiveFormat = "resonant-experiment-archive";
inline constexpr int kArchiveVersion = 1;

struct ArchiveReport {
    std::filesystem::path directory;
    size_t dataSets = 0;
    std::vector<std::string> missing;  // records whose data was not found in the store
};

// The directory must not exist yet.
Result<ArchiveReport> exportExperiment(const ExperimentWorkspace& workspace, const std::string& experimentId,
                                       const MeasurementStore& store, const std::filesystem::path& directory);

struct ImportedArchive {
    ExperimentWorkspace workspace;
    std::unique_ptr<MeasurementStore> store;  // reads the archive's data
    std::filesystem::path directory;
};

Result<ImportedArchive> importExperiment(const std::filesystem::path& directory);

// Adds an imported experiment to a workspace and copies its data into the
// target store. Fails (changing nothing) when the experiment or a record id
// exists already.
Status mergeArchive(ExperimentWorkspace& target, MeasurementStore& targetStore, const ImportedArchive& archive);

struct ReproductionReport {
    size_t compared = 0;
    size_t reproduced = 0;
    double maxRelativeDifference = 0.0;
    std::vector<std::string> problems;
    bool ok() const { return compared == reproduced && problems.empty(); }
};

Result<ReproductionReport> reproduceProcessing(const ExperimentWorkspace& workspace, const std::string& experimentId,
                                               const MeasurementStore& store);

}  // namespace resonant::experiments
