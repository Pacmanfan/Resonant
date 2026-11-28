#pragma once

// Participants of `rectangular-cavity` physical systems (RES-RSN-011): a
// rectangular cavity driven near resonance through a port, heated by its own
// losses, its fill's permittivity a function of temperature and its walls
// expanding.
//
//  em.cavity.mode  (electromagnetic): the lowest mode of the current (expanded)
//      cavity with the fill at its present state; the port's external Q is
//      fixed by the coupling at the reference state; drive: an incident power at
//      a fixed frequency. Delivered power P = P_inc (1 - |Gamma(f_drive)|^2),
//      all of it lost in the walls and the fill and passed on as heat.
//  thermal.lumped  (thermal): one temperature for the cavity, T = T_amb + P / G;
//      the walls' linear expansion scales a, b, d (geometry update).

#include "foundation/Result.h"
#include "multiphysics/Orchestrator.h"

#include <memory>

namespace resonant {
class MaterialLibrary;
}

namespace resonant::coupling {

inline constexpr const char* kCavityModeSolver = "em.cavity.mode";
inline constexpr const char* kLumpedThermalSolver = "thermal.lumped";

// nullptr when the domain is not one of these participants.
std::unique_ptr<multiphysics::Participant> makeCavityParticipant(const multiphysics::PhysicalSystem& system, const multiphysics::DomainDef& domain,
                                                                 const MaterialLibrary& materials);

}  // namespace resonant::coupling
