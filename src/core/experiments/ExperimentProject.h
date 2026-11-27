#pragma once

// The experiments of a project, kept in the project file under
// extensions["experiments"]: experiment definitions, the calibration
// library, the measurement records (metadata only: the data is in the
// measurement store next to the project), spatial data sets and comparison
// results. Projects without experiments are unchanged; the key is absent.
//
// Output authorizations are never part of a project: opening a project or
// loading an experiment cannot start a physical output.
#include "experiments/Calibration.h"
#include "experiments/Experiment.h"
#include "experiments/MeasurementRecord.h"
#include "experiments/Spatial.h"
#include "project/Project.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant::experiments {

inline constexpr const char* kExperimentsKey = "experiments";
inline constexpr int kExperimentsVersion = 1;

struct ComparisonRecord {
    std::string id;
    std::string experimentId;
    std::string measurementId;   // the measured data set
    std::string simulatedLabel;  // what it was compared with (model, solver, run)
    std::string kind;            // frequency-response, spatial, modal, s-parameter, pattern, ...
    nlohmann::json metrics = nlohmann::json::object();
    std::string createdUtc;
    std::string note;
};

struct ExperimentWorkspace {
    std::vector<Experiment> experiments;
    CalibrationLibrary calibrations;
    std::vector<MeasurementRecord> measurements;
    std::vector<SpatialDataset> spatial;
    std::vector<ComparisonRecord> comparisons;

    Experiment* experiment(const std::string& id);
    const Experiment* experiment(const std::string& id) const;
    const MeasurementRecord* measurement(const std::string& id) const;
    std::vector<const MeasurementRecord*> measurementsOf(const std::string& experimentId) const;
    // Records are added once and never replaced.
    Status addMeasurement(MeasurementRecord record);
    Status addExperiment(Experiment experiment);
};

nlohmann::json toJson(const ExperimentWorkspace& workspace);
Result<ExperimentWorkspace> experimentWorkspaceFromJson(const nlohmann::json& value);

// An empty workspace when the project has none.
Result<ExperimentWorkspace> loadExperiments(const Project& project);
// Removes the key when the workspace is empty.
void storeExperiments(Project& project, const ExperimentWorkspace& workspace);

}  // namespace resonant::experiments
