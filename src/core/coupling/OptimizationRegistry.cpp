#include "coupling/OptimizationRegistry.h"

#include "acoustic_optimization/AcousticOptimization.h"
#include "electromagnetics/EmOptimization.h"
#include "mechanics/MechanicalOptimization.h"

namespace resonant::coupling {

void registerOptimizationAdapters(optimization::AdapterRegistry& registry)
{
    acoustics::registerAcousticOptimization(registry);
    mechanics::registerMechanicalOptimization(registry);
    em::registerEmOptimization(registry);
}

}  // namespace resonant::coupling

namespace resonant::optimization {

AdapterRegistry& AdapterRegistry::instance()
{
    static AdapterRegistry registry = [] {
        AdapterRegistry created;
        coupling::registerOptimizationAdapters(created);
        return created;
    }();
    return registry;
}

}  // namespace resonant::optimization
