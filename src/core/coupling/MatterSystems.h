#pragma once

// Architectures in the Coupled Energy-Matter Engine (REQ-CHAT-20261127-001,
// RES-FAM-013): a stack becomes a layered-slab physical system with one region
// per layer bound to the stack's object. Graded layers become sublayers whose
// constant laws carry the field's values; effective (composition), anisotropic
// (the through-thickness conductivity) and region-stated properties become
// constant laws carrying their method and provenance; thermal contact
// resistances go to the thermal participant's interfaces. What the engine cannot
// represent is refused (capability "coupled-engine").

#include "foundation/Result.h"
#include "matter/MatterArchitecture.h"
#include "multiphysics/PhysicalSystem.h"

#include <nlohmann/json.hpp>

#include <string>

namespace resonant {
class MaterialLibrary;
}

namespace resonant::coupling {

struct MatterSystem {
    multiphysics::PhysicalSystem system;
    nlohmann::json notes = nlohmann::json::array();  // the defaults applied and the laws written, with why
};

// options: {"id", "name", "lateralArea" (m^2; default the stack's length x width, else 0.01),
//           "ambientTemperature" (K), "cellsPerLayer" (10), "sublayers" (graded layers, 10),
//           "domains": [DomainDef as JSON] (default: a thermal domain, both faces convective
//           with h = 10 W/(m^2 K) - stated in the notes), "couplings", "scheme"}
Result<MatterSystem> stackToPhysicalSystem(const matter::Architecture& architecture, const std::string& stackId, const nlohmann::json& options,
                                           const MaterialLibrary& library);

}  // namespace resonant::coupling
