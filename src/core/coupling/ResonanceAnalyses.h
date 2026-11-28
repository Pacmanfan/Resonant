#pragma once

// The resonance analyses of functional structures (RES-RSN-005, -006, -008,
// -009): the Helmholtz resonator, the cantilever as a resonator, coupled
// cantilevers and coupled Helmholtz resonators. Registered with the other
// functional analyses (coupling/FunctionalAnalyses.cpp).

#include "functional/Analysis.h"

namespace resonant::coupling {

inline constexpr const char* kHelmholtzAnalysis = "acoustic.helmholtz";
inline constexpr const char* kCantileverResonatorAnalysis = "mechanics.cantilever-resonator";
inline constexpr const char* kCoupledCantileversAnalysis = "mechanics.coupled-cantilevers";
inline constexpr const char* kCoupledHelmholtzAnalysis = "acoustic.coupled-helmholtz";

void registerResonanceAnalyses(functional::AnalysisRegistry& registry);

}  // namespace resonant::coupling
