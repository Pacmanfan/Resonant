#pragma once

// The optimizer's adapter for architectures (REQ-CHAT-20261127-001, RES-FAM-014):
// layer thicknesses, in-plane angles and electrode coverage, the material of a
// region (among stated choices) and interface parameters are design variables;
// the metrics of the stack analyses and the manufacturing check are quantities.
// Every candidate is evaluated on its own copy of the design. An architecture that
// violates its manufacturing constraints is rejected (an invalid design) or
// constrained (manufacturing.violations), as configured. An optimized architecture
// is a prediction, not a validated design: every evaluation says so.
//
//   matter/layer/<stack>/<index>/thickness   m
//   matter/layer/<stack>/<index>/angle       rad
//   matter/layer/<stack>/<index>/coverage    the electrode span's end (segmented pattern)
//   matter/region/<id>/material              categorical: index into the stated choices
//   matter/interface/<id>/<model>/<param>    the interface model's parameter (its unit)

#include "foundation/Result.h"

#include <nlohmann/json.hpp>

#include <memory>

namespace resonant::optimization {
class AdapterRegistry;
class DesignEvaluator;
}  // namespace resonant::optimization

namespace resonant::coupling {

inline constexpr const char* kMatterAdapterId = "matter";

// configuration: {"architecture", "stack", "analyses": [{"analysis", "options"}],
//                 "materialChoices": {"<region>": ["<material id>", ...]},
//                 "variables": ["<binding>", ...] (default: thicknesses, interface resistances,
//                 angles of anisotropic layers, coverages of transduction layers, choices),
//                 "manufacturing": "constrain" (default), "reject" or "ignore"}
Result<std::shared_ptr<const optimization::DesignEvaluator>> bindMatterAdapter(const nlohmann::json& snapshot, const nlohmann::json& configuration);
void registerMatterOptimization(optimization::AdapterRegistry& registry);

}  // namespace resonant::coupling
