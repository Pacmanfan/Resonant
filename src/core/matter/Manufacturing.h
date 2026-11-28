#pragma once

// Manufacturing awareness (RES-FAM-015): the process kinds an architecture can
// name, and the check of an architecture against the constraints its processes
// state - layer thickness, feature size, material classes, process temperature
// against the limits of every region already made, dimensions, repeatability,
// built-in residual stress against the stated limits and the interface
// strength. Constraint values come from the architecture (with their source);
// none is invented here. A theoretical architecture is never reported feasible:
// idealized fields and missing limits are "not assessed".

#include "matter/MatterArchitecture.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace resonant::matter {

struct ProcessKind {
    std::string id, title;
    std::string category;  // additive, deposition, patterning, joining, thermal, subtractive, composite
    std::vector<std::string> constraints;  // the constraint keys that apply
    std::string description;
};
const std::vector<ProcessKind>& processKinds();
const ProcessKind* findProcessKind(const std::string& id);

// A residual stress at an interface (computed by the lamination analysis from the
// process temperature), passed in so that this module needs no mechanics.
struct ResidualStress {
    std::string interface;
    std::string layer;   // the layer made at the process temperature
    double stress = 0.0; // Pa, the largest in-plane mismatch stress next to the interface
    double temperature = 0.0;  // K, the process temperature it was built in at
    std::string basis;
};

struct ManufacturingCheck {
    std::string status;  // feasible, infeasible, not-assessed
    std::vector<Issue> violations, unassessed, notes;
    nlohmann::json toJson() const;
};

ManufacturingCheck checkManufacturing(const Architecture& architecture, const std::vector<ResidualStress>& residual = {});

}  // namespace resonant::matter
