#pragma once

// The mechanical optimization adapter (RES-OPT-020): structural design
// variables, quantities and models for one structure object.
//
// Models, each recorded with every candidate:
//   mechanical.analytical   Analytical    Euler-Bernoulli beam (box or cylinder, clamped /
//                                         pinned / free ends) and simply supported Kirchhoff
//                                         plate (box, every edge pinned) natural frequencies
//                                         and mass; unavailable for other structures
//   mechanical.fe.coarse    Reduced       the finite-element study with elements twice the
//                                         size of the study's
//   mechanical.fe           HighFidelity  the finite-element study as configured
//
// Variables: the structure's geometry parameters, thickness, supports and
// material (a library material), HYPOTHETICAL scalings of Young's modulus
// and density (labelled as such in every result), the loss factor, the
// harmonic frequency, and every actuator's amplitude, phase, delay,
// activation and position.
//
// Quantities: natural frequencies f1..fK, their clearance from a frequency to
// avoid, modal mass fractions, mass; at the harmonic frequency the response
// at every sensor and vibration target, the target error and the largest
// suppressed response; static largest displacement, von Mises stress,
// stress concentration and compliance; actuator effort.
#include "foundation/Id.h"
#include "optimization/Evaluator.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <vector>

namespace resonant {
class Project;
}

namespace resonant::mechanics {

inline constexpr const char* kMechanicalAdapterId = "mechanical";

// Configuration:
//   structure        id of the structure object (default: the first)
//   modes            natural frequencies reported (default 6)
//   frequency        Hz, the harmonic frequency (default: the mechanics settings' target frequency)
//   avoidFrequency   Hz, for modal.clearance (default: the harmonic frequency)
//   analyses         ["modal", "harmonic", "static"] (default: modal, plus harmonic and static when there are actuators)
Result<std::shared_ptr<const optimization::DesignEvaluator>> bindMechanicalAdapter(const nlohmann::json& snapshot,
                                                                                  const nlohmann::json& configuration);

// Natural frequencies of the analytical model of a structure, or the reason
// there is none (exposed for validation).
Result<std::vector<double>> analyticalFrequencies(const Project& project, const Id& structureId, int count);

void registerMechanicalOptimization(optimization::AdapterRegistry& registry);

}  // namespace resonant::mechanics
