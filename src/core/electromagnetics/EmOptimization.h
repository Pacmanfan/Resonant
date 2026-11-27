#pragma once

// The electromagnetic optimization adapter (RES-OPT-020) for one array or
// one antenna object.
//
// Models, each recorded with every candidate:
//   em.arrayfactor        Analytical  element pattern x array factor in the far field
//                                     (identical, isolated elements)
//   em.dipoles.exact      Reduced     the superposed exact fields of the elements' assumed
//                                     sinusoidal currents (thin dipoles) at a finite
//                                     distance: near-field and element-model effects
//                                     included, mutual coupling still NOT included
//   em.antenna.analytical Analytical  induced-EMF impedance and the exact pattern of the
//                                     assumed current (dipole, monopole, small loop)
//
// Pattern metrics are computed the same way for both array models from a
// sampled sphere in the object's local frame (theta from +Z, phi from +X):
// directivity 4 pi U_max / integral U, the main lobe, the half-power
// beamwidths along two great circles through it, the peak sidelobe level
// outside the main lobe (the samples reachable from the maximum without
// climbing), and the level and gain toward each configured direction.
#include "optimization/Evaluator.h"

#include <nlohmann/json.hpp>

#include <memory>

namespace resonant::em {

inline constexpr const char* kEmAdapterId = "electromagnetic";

// Configuration:
//   object        id of the array or antenna object (default: the first array, else the first antenna)
//   directions    [{label, thetaDeg, phiDeg, levelDb?, null?}] in the object's local frame; the
//                 first is the target direction; levelDb / null define wanted levels for inverse design
//   resolutionDeg pattern sampling (default 1 for the array factor; 2 for exact fields)
//   distance      m, observation distance of the exact-field model (default 100 wavelengths)
Result<std::shared_ptr<const optimization::DesignEvaluator>> bindEmAdapter(const nlohmann::json& snapshot, const nlohmann::json& configuration);

void registerEmOptimization(optimization::AdapterRegistry& registry);

}  // namespace resonant::em
