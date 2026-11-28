#pragma once

// The analyses of functional structures (RES-FUNC-005..010, RES-FUNC-013):
// adapters from the functional registry to the domains' models, registered
// here because coupling is where the domains meet. Defines
// functional::AnalysisRegistry::instance().

#include "functional/Analysis.h"

namespace resonant::coupling {

inline constexpr const char* kCavityAnalysis = "em.cavity.rectangular";
inline constexpr const char* kOpticalResonatorAnalysis = "optics.resonator.two-mirror";
inline constexpr const char* kHornAnalysis = "acoustic.horn";
inline constexpr const char* kMechanicalModalAnalysis = "mechanics.modal";
inline constexpr const char* kPhotonicCellAnalysis = "em.periodic.layers";
inline constexpr const char* kPeriodicRodAnalysis = "mechanics.periodic-rod";
inline constexpr const char* kCoupledSlabAnalysis = "multiphysics.coupled-slab";

void registerFunctionalAnalyses(functional::AnalysisRegistry& registry);

}  // namespace resonant::coupling
