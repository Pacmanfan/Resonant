#pragma once

// Material-state analyses of functional structures (RES-DMS-006, RES-DMS-011): the
// cantilever that accumulates damage under tip-deflection cycles, whose modes come from the
// damaged stiffness. Registered with the other functional analyses.

#include "functional/Analysis.h"

namespace resonant::coupling {

inline constexpr const char* kDamagedCantileverAnalysis = "mechanics.damaged-cantilever";

void registerMaterialStateAnalyses(functional::AnalysisRegistry& registry);

}  // namespace resonant::coupling
