// Simulation configuration and result metadata. These are the records that
// make a simulation reproducible: what was asked for (SimulationConfig) and
// what produced a result (ResultMetadata). Solvers live in the physics
// domain modules; this module knows none of them.
#pragma once

#include "foundation/Id.h"
#include "foundation/Types.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant {

struct SimulationConfig {
    Id id;
    std::string name;
    PhysicsDomain domain = PhysicsDomain::Acoustic;
    std::string solverId;         // which solver the configuration is for
    nlohmann::json parameters = nlohmann::json::object();  // solver-specific, SI units

    friend bool operator==(const SimulationConfig&, const SimulationConfig&) = default;
};

struct ResultMetadata {
    Id simulationId;
    Id projectId;
    PhysicsDomain domain = PhysicsDomain::Acoustic;
    std::string solverId;
    std::string solverVersion;
    std::string softwareVersion;
    std::string startedUtc;       // ISO 8601
    double elapsedSeconds = 0.0;
    std::vector<std::string> assumptions;   // what the model assumes / ignores
    nlohmann::json configuration = nlohmann::json::object();  // the inputs, as used
};

nlohmann::json toJson(const SimulationConfig& config);
nlohmann::json toJson(const ResultMetadata& metadata);

}  // namespace resonant
