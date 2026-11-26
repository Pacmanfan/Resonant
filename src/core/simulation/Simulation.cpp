#include "simulation/Simulation.h"

namespace resonant {

nlohmann::json toJson(const SimulationConfig& config)
{
    return {{"id", config.id.str()},
            {"name", config.name},
            {"domain", std::string(toString(config.domain))},
            {"solver", config.solverId},
            {"parameters", config.parameters}};
}

nlohmann::json toJson(const ResultMetadata& metadata)
{
    return {{"simulationId", metadata.simulationId.str()},
            {"projectId", metadata.projectId.str()},
            {"domain", std::string(toString(metadata.domain))},
            {"solver", metadata.solverId},
            {"solverVersion", metadata.solverVersion},
            {"softwareVersion", metadata.softwareVersion},
            {"startedUtc", metadata.startedUtc},
            {"elapsedSeconds", metadata.elapsedSeconds},
            {"assumptions", metadata.assumptions},
            {"configuration", metadata.configuration}};
}

}  // namespace resonant
