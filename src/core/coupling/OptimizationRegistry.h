#pragma once

// Where the optimization framework meets the physics domains: the adapter
// registry of every domain (ADR-000214). The optimization core includes no
// domain; this file, in the cross-domain coupling module, registers the
// acoustic, mechanical and electromagnetic adapters and their templates.
#include "optimization/Evaluator.h"

namespace resonant::coupling {

// Registers every domain adapter and template (called once by
// optimization::AdapterRegistry::instance()).
void registerOptimizationAdapters(optimization::AdapterRegistry& registry);

}  // namespace resonant::coupling
