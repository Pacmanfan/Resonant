#pragma once

// The catalogue of material-state models and their runners (RES-DMS-005..008, -013,
// -014): a model card per constitutive model - implemented ones with their variables,
// parameters (with defaults), validity and loading presets, and the models that are not
// implemented, with why - and a runner that turns a study {"model", "parameters",
// "loading"} into a run document (format resonant.material-state-run): the history, the
// curves and spatial profiles, energies, convergence, what can be committed to a library
// material, and whether the run is reliable.

#include "foundation/Result.h"
#include "materialstate/StateModel.h"

#include <nlohmann/json.hpp>

#include <vector>

namespace resonant {
class JobContext;
}

namespace resonant::coupling {

inline constexpr const char* kStateRunFormat = "resonant.material-state-run";

const std::vector<materialstate::ModelCard>& materialStateModels();
const materialstate::ModelCard* findMaterialStateModel(std::string_view id);

Result<nlohmann::json> runMaterialStateStudy(const nlohmann::json& study, JobContext* context);

}  // namespace resonant::coupling
