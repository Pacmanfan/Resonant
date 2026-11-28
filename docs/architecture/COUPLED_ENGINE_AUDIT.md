# Coupled Engine Audit

The audit before the Coupled Energy–Matter Engine (REQ-CHAT-20261126-016,
`docs/requirements/coupled_engine.md`, ADR-000220), from the source code at
commit `1daad68` (after the Shape–Function–Energy architecture, ADR-000219, whose
broader audit is [SHAPE_FUNCTION_ENERGY_AUDIT.md](SHAPE_FUNCTION_ENERGY_AUDIT.md)).

## 1. Physical domains

`PhysicsDomain` had acoustic, mechanical and electromagnetic. No thermal
domain existed: temperature appeared only as a validity bound of material
values, the acoustic medium's temperature (ideal-gas air) and experiment
metadata. Fluid dynamics beyond linear acoustics does not exist.

## 2. Solvers

| Domain | Solvers | Output relevant to coupling |
|---|---|---|
| Acoustics | analytical free field, Kirchhoff reflectors, FDTD (2D/3D, `fdtd/FdtdGrid`) | complex pressure fields on grids; FDTD energy over time |
| Mechanics | finite elements (frame, plate, membrane, solid hexahedra; static, modal, harmonic, transient) | displacements, element stresses; transient kinetic/strain/external/dissipated energy |
| Electromagnetics | analytical antennas, arrays, lines, guides, cavities; Yee FDTD (`electromagnetics/Yee`, PEC/periodic/absorbing faces) | fields on the Yee grid (transient); energy over time |

The Yee solver is transient: a harmonic absorbed-power field needs a run to
steady state and a phasor extraction, for every iteration of a coupled loop.
No solver had an exact steady harmonic solution for a lossy body.

## 3. Material abstractions

Per-domain property sets with provenance, validity (frequency, temperature)
and frequency tables (`materials/Material.h`). No temperature- or
state-dependent values, no thermal set, no anisotropic tensors; elastic
materials have no expansion coefficient.

## 4. Geometry and meshes

Parametric primitives and component geometries; `geometry/SolidModel`
(inside tests for grid solvers); the finite-element model builders
(`mechanics/Meshing`, `StructuralModel`); FDTD and Yee setups. No geometry
revision, no deformed state, no mesh-to-mesh mapping.

## 5. Simulation data structures

Domain result types (`acoustics::FieldResult`, the mechanical solutions of
`mechanics/Analyses`, the Yee run's records), each with its own grid or mesh; a solver registry of
descriptors (`solvers/SolverRegistry`, acoustic phenomena only). No field
type shared between domains, no quantity descriptors.

## 6. Units and conventions

SI internally, `app/DisplayUnits` at the user boundary, `e^{+jωt}` peak
phasors in acoustics, mechanics coupling and electromagnetics (checked by the
Shape–Function–Energy audit). Consistent.

## 7. Jobs and scheduling

`simulation/Job` with a `JobContext` (cancellation, progress) on a worker
pool; services track jobs (`services/Session::trackJob`) and register their
results. Sufficient for coupled runs as one job.

## 8. Optimization

Domain-independent framework with adapters registered in
`coupling/OptimizationRegistry`. Coupled objectives are not yet an adapter.

## 9. Serialization and storage

Project JSON with schema versions, migration and `extensions` (domain
workspaces); experiments use HDF5 beside the project. Physical systems fit in
an extension.

## 10. Automation interfaces

Application services (`src/core/services`) behind the protobuf contract (16
services, 68 methods), the Python client and MCP. New operations follow
`docs/development/ADDING_AUTOMATION_OPERATIONS.md`.

## 11. Architectural gaps

Thermal domain; typed exchanged quantities; field data on a common support;
transfer operators; material state; geometry state; a physical system
document; participant interface and orchestration; an energy ledger;
thermal loads in the finite-element model; a steady electromagnetic model
of lossy bodies.

## 12. Integration risks

Layering (the engine must not include domains); breaking standalone
simulations; mixing W and J in accounting; non-conservative transfers of
power; stale solutions after geometry updates; iteration divergence; a new
`PhysicsDomain` value reaching code that enumerates domains (materials
dialog, capabilities, project format tests).

## 13. Migration strategy (followed)

1. Engine core in a new module (`src/core/multiphysics`), domain-independent, layering-checked.
2. New domain code in the domains: `thermal/` (new), `electromagnetics/LayeredMedia`, `mechanics/ThermalLoads`.
3. Participants and coupling models in `src/core/coupling`; the one-way `StructuralRadiation` stays as it is and is listed as "external".
4. A reference system kind (`layered-slab`) whose every coupling has an analytical or independent reference.
5. Services, contract, Python and MCP by the existing procedure.
6. Standalone simulations untouched (their suites pass unchanged).
