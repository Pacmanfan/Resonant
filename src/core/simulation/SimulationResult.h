// Base of every stored simulation result. A result is immutable once
// produced and carries the metadata needed to reproduce it; it is a snapshot
// of the inputs at the time of the run, so later edits to the scene do not
// change it.
#pragma once

#include "simulation/Simulation.h"

#include <memory>
#include <string>

namespace resonant {

class SimulationResult {
public:
    virtual ~SimulationResult() = default;

    // Stable type identifier ("acoustic.field").
    virtual std::string kind() const = 0;

    Id id;
    std::string name;
    ResultMetadata metadata;
};

using SimulationResultPtr = std::shared_ptr<const SimulationResult>;

}  // namespace resonant
