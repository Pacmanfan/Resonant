#pragma once

// Multiscale coupling contracts (RES-MSC-012): what each coupling strategy transfers
// between which scales, under which assumptions, how it converges and fails, and
// whether it is implemented.

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant::multiscale {

struct CouplingContract {
    std::string id, name;
    std::string strategy;  // sequential, iterative, hierarchical, concurrent
    std::string status;    // implemented, planned
    std::string sourceScale, targetScale;
    std::vector<std::string> transferred;
    std::string coordinates, assumptions, boundaryConditions, updateFrequency, convergence, failureHandling, uncertainty, provenance;
    std::vector<std::string> requirements{};  // for planned couplings: what they need
    nlohmann::json toJson() const;
};

const std::vector<CouplingContract>& couplingContracts();

}  // namespace resonant::multiscale
