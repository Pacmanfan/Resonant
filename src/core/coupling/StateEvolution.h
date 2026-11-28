#pragma once

// Time-stepped evolution of a physical system (RES-DMS-002, -009, -010, ADR-000225): every
// step is a run of the Coupled Energy-Matter Engine whose transient thermal participant
// starts from the previous step's temperature field. Within a step the engine's own scheme
// converges the coupling (temperature-dependent properties at the step's temperature); the
// steps are committed in order, a step that fails is reported and ends the run. The state
// recorded per step: the temperature field, each region's mean temperature and its laws'
// property values there, the mechanical stress and thickness where the system has a
// mechanical domain, and the step's energy audit; the run's energy account sums the steps.

#include "foundation/Result.h"
#include "materials/Material.h"
#include "multiphysics/PhysicalSystem.h"

#include <nlohmann/json.hpp>

namespace resonant {
class JobContext;
}

namespace resonant::coupling {

struct EvolutionSettings {
    double duration = 0.0;  // s
    int steps = 20;         // committed steps (records)
    int substeps = 4;       // implicit thermal steps inside each
    Status validate() const;
};

// The record document (format resonant.material-state-evolution).
Result<nlohmann::json> evolveSystem(const multiphysics::PhysicalSystem& system, const MaterialLibrary& materials, const EvolutionSettings& settings,
                                    JobContext* context);

}  // namespace resonant::coupling
