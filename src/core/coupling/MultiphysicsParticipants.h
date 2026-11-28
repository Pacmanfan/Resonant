#pragma once

// The participants of the Coupled Energy-Matter Engine for layered-slab
// systems (REQ-CHAT-20261126-016, docs/architecture/COUPLED_ENGINE_ARCHITECTURE.md):
// adapters of the domain solvers to multiphysics::Participant.
//
//   em.layered.transfer-matrix   electromagnetics/LayeredMedia: a plane wave at
//                                normal incidence on the regions, each split into
//                                its cells (sublayers) with properties at the
//                                sublayer's temperature; works in the CURRENT
//                                geometry; reports the absorbed power per cell
//                                of the REFERENCE configuration (power per
//                                material volume, so transfers to the reference
//                                grids are exact).
//   thermal.conduction.fv        thermal/HeatConduction on a column grid (one cell
//                                across, the regions' cells along z), lateral
//                                faces adiabatic, -z and +z as configured; steady
//                                or transient; reference configuration.
//   mechanics.fe.solid-column    mechanics finite elements: a column of hexahedra
//                                (one across, one per thermal cell along z) with
//                                thermal loads; lateral faces free (symmetry
//                                constraints) or constrained, the -z face supported;
//                                reports the deformed layer thicknesses.
#include "materials/Material.h"
#include "multiphysics/Orchestrator.h"

#include <memory>

namespace resonant::coupling {

Result<std::unique_ptr<multiphysics::Participant>> makeParticipant(const multiphysics::PhysicalSystem& system,
                                                                   const multiphysics::DomainDef& domain, const MaterialLibrary& materials);

}  // namespace resonant::coupling
