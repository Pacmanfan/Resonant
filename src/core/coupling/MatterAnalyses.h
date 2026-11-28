#pragma once

// Analyses of architectures by the existing solvers and closed forms
// (REQ-CHAT-20261127-001, RES-FAM-010, -011, ADR-000228). Each analysis declares
// its capability; a stack is checked against it before it runs, and what the
// declaration does not cover is refused with the reason.
//
//   laminate         classical lamination theory (mechanics/Laminate): ABD, moduli,
//                    neutral surface, reduced bending stiffness, thermal curvature and
//                    the mismatch stresses next to every interface
//   modes            cantilevered beam or simply supported plate modes of the coated
//                    and of the bare structure
//   harvester        the piezoelectric cantilever with a resistive load
//                    (mechanics/PiezoelectricBeam), with its energy balance checked
//                    by the accounting framework
//   thermal          through-thickness conduction: series resistances with contact
//                    resistances, the temperature profile with its jumps
//   electrical       through-thickness resistance with contact resistances, and the
//                    series capacitance
//   electromagnetic  the layered plane-wave solver (em::LayeredSolution)
//   interfaces       derived quantities of each interface: acoustic transmission,
//                    the thermal-mismatch stress, the temperature jump under a flux
//   layered-solid    the stack as a three-dimensional body of 20-node hexahedra
//                    (mechanics/LayeredSolid) on a flat, cylindrical, spherical or
//                    revolved reference surface: modes, or a static response with
//                    the interfaces' tractions, states and initiation indices
//                    (REQ-CHAT-20261128-004, ADR-000232)
//   layered-solid-thermal  steady three-dimensional conduction of the same body,
//                    with the interfaces' contact resistances

#include "foundation/Result.h"
#include "matter/MatterArchitecture.h"
#include "matter/SolverCapability.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant {
class Project;
class MaterialLibrary;
}  // namespace resonant

namespace resonant::coupling {

const std::vector<matter::SolverCapability>& matterCapabilities();
const matter::SolverCapability* findMatterCapability(const std::string& id);

// The compatibility of a stack with an analysis (the "interfaces" analysis has no
// capability of its own: it reads what each interface states).
Result<matter::Compatibility> checkMatterStack(const matter::Architecture& architecture, const std::string& stackId, const std::string& analysis,
                                               const MaterialLibrary& library);

// Runs an analysis. The result:
//   {"analysis", "architecture", "revision", "stack", "representation", "compatibility",
//    "metrics" {name: value}, "result" {...}, "assumptions" [..], "warnings" [..]}
// Fails with the refusals when the stack is not compatible.
Result<nlohmann::json> analyzeStack(const matter::Architecture& architecture, const std::string& stackId, const std::string& analysis,
                                    const nlohmann::json& options, const MaterialLibrary& library);

// The metric keys each analysis reports (for the optimizer's quantities).
struct MatterMetric {
    std::string key, label, unit, definition;
};
const std::vector<MatterMetric>& matterMetrics(const std::string& analysis);

// The resolved properties of a region (every library property, plus its anisotropic
// sets), at the middle of its extent: what the analyses use.
nlohmann::json effectiveRegionProperties(const matter::Architecture& architecture, const std::string& regionId, const MaterialLibrary& library);

// The mismatch stresses built in when the layer made by a process at its temperature
// cools to the reference temperature (for the manufacturing check).
std::vector<nlohmann::json> processResidualStresses(const matter::Architecture& architecture, const MaterialLibrary& library, double reference = 293.15);

}  // namespace resonant::coupling
