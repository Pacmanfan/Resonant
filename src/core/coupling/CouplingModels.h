#pragma once

// The coupling models the Coupled Energy-Matter Engine knows (RES-CEME-013,
// docs/physics/COUPLING_ARCHITECTURE.md): which quantity goes from which
// domain to which, how, by which physical mechanism and equations, and how
// far each is implemented: "validated" (implemented, with the reference
// tests named), "experimental", "external" (implemented and validated in the
// application but not available to the engine yet) or "planned" (equations
// documented, not implemented: a system using one is refused).
#include "multiphysics/PhysicalSystem.h"

#include <utility>
#include <vector>

namespace resonant::coupling {

const std::vector<multiphysics::CouplingModelDescriptor>& couplingModels();

// The participants the engine can build: (domain kind, solver id).
const std::vector<std::pair<std::string, std::string>>& participantSolvers();

}  // namespace resonant::coupling
