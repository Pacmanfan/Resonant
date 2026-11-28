#pragma once

// Downscaling and iterative multiscale coupling (RES-MSC-012): the local state of a region
// of a coupled result, and the iterative coupling of a region of a physical system to an
// RVE: run the coupled engine, take the region's mean temperature, homogenize the RVE with
// its constituents at that temperature, set the region's conductivity, repeat until the
// conductivity changes less than the tolerance (contract iterative.region-rve-thermal).

#include "foundation/Result.h"
#include "multiphysics/Orchestrator.h"
#include "multiphysics/PhysicalSystem.h"
#include "multiscale/Representation.h"

#include <nlohmann/json.hpp>

namespace resonant {
class JobContext;
class Project;
}

namespace resonant::coupling {

struct LocalState {
    std::string region;
    double meanTemperature = 0.0, minTemperature = 0.0, maxTemperature = 0.0;  // K
    double meanGradient = 0.0;  // K/m, the mean |dT/dz| over the region's cells
    double zFrom = 0.0, zTo = 0.0;  // m
    int cells = 0;
    nlohmann::json toJson() const;
};

// From the coupled result's temperature field of a layered-slab system.
Result<LocalState> regionState(const multiphysics::PhysicalSystem& system, const multiphysics::CoupledResult& result, const std::string& region);
// The same from a stored coupled-result document (its fields' values and edges, its geometry).
Result<LocalState> regionState(const multiphysics::PhysicalSystem& system, const nlohmann::json& resultDocument, const std::string& region);

struct IterationSettings {
    std::string region;           // the region whose material the RVE describes
    std::string representation;   // the RVE (rve.voxel)
    nlohmann::json derive = nlohmann::json::object();  // settings of rve.thermal-conductivity
    double tolerance = 1e-4;      // relative change of the conductivity
    int maxIterations = 20;
};

// The result: {"history": [{iteration, temperatureK, conductivity}], "converged", "iterations",
// "state" (the final local state), "derivation" (the final derivation document, assignable)}.
Result<nlohmann::json> iterateRegion(const multiphysics::PhysicalSystem& system, const Project& project, const IterationSettings& settings,
                                     JobContext* context);

}  // namespace resonant::coupling
