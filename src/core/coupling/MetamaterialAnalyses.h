#pragma once

// The effective-medium analyses of functional structures (RES-META-003..010):
// a locally resonant mass-in-mass lattice, a duct loaded by Helmholtz
// resonators, a layered electromagnetic medium (anisotropic homogenization,
// oblique incidence, retrieval), a split-ring array (circuit model) and a
// layered thermal medium. Each reports its constituents, unit cell, periodic
// structure, effective description and validity separately, and, where the
// effective description holds and the library can hold it, an effective
// material (functional/EffectiveMaterial.h) in details.effectiveMaterial.
// Registered with the other functional analyses (coupling/FunctionalAnalyses.cpp).

#include "functional/Analysis.h"

namespace resonant::coupling {

inline constexpr const char* kMassInMassAnalysis = "mechanics.mass-in-mass-lattice";
inline constexpr const char* kResonatorDuctAnalysis = "acoustic.resonator-duct";
inline constexpr const char* kLayeredEffectiveMediumAnalysis = "em.layered-effective-medium";
inline constexpr const char* kSplitRingArrayAnalysis = "em.split-ring-array";
inline constexpr const char* kLayeredConductivityAnalysis = "thermal.layered-conductivity";

void registerMetamaterialAnalyses(functional::AnalysisRegistry& registry);

}  // namespace resonant::coupling
