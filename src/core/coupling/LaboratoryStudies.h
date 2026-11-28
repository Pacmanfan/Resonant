#pragma once

// The laboratory's operations on a project (REQ-CHAT-20261126-023, RES-LAB-002, -005,
// -009..012, -014, -015, ADR-000227): each reads datasets through a loader (the service
// reads the measurement store) and models through coupling/LaboratoryModels, and returns a
// record for the project's laboratory extension.
//
//   comparison   {"id", "name", "study", "kind": response | transient | field | scalar | resonance | retrieval,
//                 "measured": "<dataset>", "predicted": {"model", "configuration", "parameters"} | {"dataset"} |
//                 {"value", "standard", "unit"} | {"grid": {...}}, "options": {...}, "link": {model link}}
//   calibration  {"id", "study", "model", "configuration", "parameters": [{"id", "initial", "lower", "upper",
//                 "prior": {"mean", "standard"}}], "datasets": [...], "axisDatasets": [...], "observable": magnitude | real-imag | value}
//   validation   a calibration record and other datasets: refused when they overlap
//   resonance    {"dataset", "method": response | ring-down, "options"}
//   explanation  the established findings of a record with their evidence, hypotheses marked as such,
//                missing information and suggested measurements - nothing invented, nothing changed
//   report       a study with its datasets, comparisons, calibrations and validations (Markdown or HTML)

#include "experiments/Processing.h"
#include "foundation/Result.h"
#include "laboratory/Laboratory.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <string>
#include <vector>

namespace resonant {
class JobContext;
class Project;
}  // namespace resonant

namespace resonant::coupling {

using DatasetLoader = std::function<Result<experiments::ProcessingItem>(const std::string& datasetId)>;

// The fingerprint of what a link points to now (empty when it is gone).
std::string currentRevision(const Project& project, const laboratory::ModelLink& link);
// Links whose content changed since they were linked.
std::vector<std::string> staleLinks(const Project& project, const laboratory::Study& study);

Result<nlohmann::json> runComparison(const nlohmann::json& spec, const Project& project, const DatasetLoader& load, JobContext* context);
Result<nlohmann::json> runCalibration(const nlohmann::json& spec, const Project& project, const DatasetLoader& load, JobContext* context);
// axisDatasets: as for the calibration, the dataset giving each validation dataset's axis (or empty).
Result<nlohmann::json> runValidation(const nlohmann::json& calibration, const std::vector<std::string>& datasets, const std::string& id,
                                     const Project& project, const DatasetLoader& load, JobContext* context,
                                     const std::vector<std::string>& axisDatasets = {});
Result<nlohmann::json> extractResonances(const nlohmann::json& spec, const DatasetLoader& load);
nlohmann::json explainRecord(const nlohmann::json& record, const laboratory::Workspace& workspace, const Project& project);
Result<std::string> laboratoryReport(const std::string& studyId, const laboratory::Workspace& workspace, const Project& project, const std::string& format);

}  // namespace resonant::coupling
