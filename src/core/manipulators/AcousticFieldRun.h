// The acoustic field run, shared by every caller (REQ-CHAT-20261126-012,
// ADR-000216): the GUI's Acoustics panel, the application services (and so
// gRPC, Python and MCP) choose the propagation model from the project's
// settings, estimate the cost, assemble the provenance and build the job in
// one place. Formerly inside the GUI's AcousticController.
#pragma once

#include "acoustics/Settings.h"
#include "acoustics/FieldGrid.h"
#include "foundation/Result.h"
#include "project/Project.h"

#include <memory>
#include <string>
#include <vector>

namespace resonant::acoustics {

// The propagation model the settings select (free field, or the Kirchhoff reflector model).
std::shared_ptr<const PropagationModel> fieldModelFor(const Project& project, const AcousticSettings& settings);
// An evaluator over a snapshot of the project's active sources.
std::shared_ptr<const FieldEvaluator> fieldEvaluatorFor(const Project& project, const AcousticSettings& settings);
// What the chosen model leaves out for this scene (shown with estimates and results).
std::vector<std::string> fieldModelNotes(const Project& project, const AcousticSettings& settings);
// Cost of a field on a region (points, memory, evaluations), refusal when it is too large.
Result<FieldEstimate> estimateFieldRun(const Project& project, const Id& regionId, const AcousticSettings& settings);

struct FieldRun {
    std::shared_ptr<FieldJob> job;  // ready to submit; works on a snapshot
    FieldEstimate estimate;
    std::string name;
};
// Everything needed to compute (and reproduce) the field on a region, as the project is now.
// `runNumber` only names the run ("Plane @ 40000 Hz, run 3").
Result<FieldRun> prepareFieldRun(const Project& project, const Id& regionId, int runNumber = 1);

}  // namespace resonant::acoustics
