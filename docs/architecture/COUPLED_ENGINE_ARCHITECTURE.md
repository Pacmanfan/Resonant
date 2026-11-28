# Coupled Energy–Matter Engine: Architecture

**Status:** implemented for layered-slab systems (REQ-CHAT-20261126-016,
RES-CEME-001..016, ADR-000220). The direction it implements is
[the coupled multiphysics architecture](COUPLED_MULTIPHYSICS_ARCHITECTURE.md)
(ADR-000219); what is planned next is [the engine roadmap](COUPLED_ENGINE_ROADMAP.md).

## Modules

```text
src/core/multiphysics   the engine (no physics domain, no coupling: layering rule)
  Quantity        typed quantity descriptors (registry)
  FieldData       values of a quantity on a rectilinear cell grid, with time character and provenance
  Transfer        conservative remapping and linear interpolation between grids, with reports
  Ledger          energy terms, transfer records, the conservation audit
  MaterialState   region material laws, the temperature state, revisions, history
  GeometryState   reference and current dimensions, revisions
  PhysicalSystem  the system document (resonant.physical-system), validation
  Orchestrator    the participant interface, execution order, schemes, failures, the result

src/core/coupling       domains meet only here
  CouplingModels           the registry of coupling models (validated, experimental, external, planned)
  MultiphysicsParticipants electromagnetic, thermal and mechanical participants
  CoupledRun               validate, build, run; the job
  StructuralRadiation      (existing) one-way plate radiation, outside the engine

src/core/thermal        the thermal domain: HeatConduction (finite volumes), ThermalMaterial
src/core/electromagnetics/LayeredMedia   exact layered-media plane-wave solution
src/core/mechanics/ThermalLoads          thermal strain loads and stresses
src/core/services/MultiphysicsServices   the application service
proto/resonant/v1/multiphysics           the contract (MultiphysicsService, 8 methods)
```

## Engine components (the request's conceptual list, mapped)

| Responsibility | Where |
|---|---|
| Physical system model | `multiphysics/PhysicalSystem` ([the model](PHYSICAL_SYSTEM_MODEL.md)) |
| Simulation domain registry | `coupling/CouplingModels::participantSolvers`, `coupling::makeParticipant` |
| Material state manager | `multiphysics/MaterialState` ([material state](../materials/MATERIAL_STATE_ARCHITECTURE.md)) |
| Geometry state manager | `multiphysics/GeometryState` |
| Physical quantity registry | `multiphysics/Quantity` ([quantities](../physics/PHYSICAL_QUANTITY_SYSTEM.md)) |
| Coupling manager | `coupling/CouplingModels` + the orchestrator's coupling application ([couplings](../physics/COUPLING_ARCHITECTURE.md)) |
| Energy transfer, momentum and force | `multiphysics/Ledger` ([accounting](../physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md)); force couplings planned |
| Conservation auditor | `EnergyLedger::audit` |
| Time coordination | steady schemes and one transient thermal participant ([execution](../physics/COUPLED_SIMULATION_EXECUTION.md)); multirate planned |
| Spatial mapping and projection | `multiphysics/Transfer` ([transfer](../physics/SPATIAL_AND_TEMPORAL_TRANSFER.md)) |
| Solver orchestrator | `multiphysics/Orchestrator`, `coupling/CoupledRun` (a job) |
| Simulation state history | `MaterialState::history`, the pass history in the result |
| Provenance and validation | the result's provenance; `CoupledValidation.*` ([validation](../physics/COUPLED_SIMULATION_VALIDATION.md)) |

## Data flow of a run

```text
 PhysicalSystem ──validate──► participants (makeParticipant) ──► Orchestrator
                                                                   │  order = field transfers, topologically
   for each pass:                                                  ▼
     for each participant p in order:  prepare(state) → solve → couplings from p:
        field-transfer:   provide(q) → validate (finite) → [declared conversion] → transfer → target.accept
                          → TransferRecord + TransferOut/TransferIn terms (energy densities)
        state-update:     provide(temperature) → MaterialState.updateTemperature (relaxed, revision++)
        geometry-update:  geometryChanges() → GeometryState.apply (relaxed, revision++)
     record the pass (state changes, transferred energy, monitored scalars)
     iterative: stop when the pass's changes are below the tolerances
   account (external terms of the last solves) → audit → CoupledResult (fields, ledger, provenance, warnings)
```

## Participants (layered-slab systems)

| Participant | Solver | Configuration | Provides | Accepts |
|---|---|---|---|---|
| electromagnetic | `em.layered.transfer-matrix` | frequency, incident intensity; the regions' sublayers with properties at their temperature; current geometry | `absorbed_power_density` (per reference volume) | — |
| thermal | `thermal.conduction.fv` | -z/+z faces (adiabatic, temperature, convective), optional transient | `temperature`, `heat_source_density` | `heat_source_density` |
| mechanical | `mechanics.fe.solid-column` | lateral free (symmetry) or constrained | `displacement`, `stress`, the layers' new thicknesses | `temperature` |

Configurations: the electromagnetic solution works in the deformed geometry;
conduction and mechanics in the reference configuration (small strain); the
absorbed power is reported per material (reference) volume so the transfer
to the reference grids is exact.

## Services and clients

`MultiphysicsService` (services and contract): `ListCouplingModels`,
`ListPhysicalSystems`, `GetPhysicalSystem`, `SetPhysicalSystem` (undoable),
`DeletePhysicalSystem` (undoable), `ValidatePhysicalSystem`,
`StartCoupledSimulation` (a job), `GetCoupledResult`. Python uses them through
`resonant_api` (`api.MultiphysicsService.*`); MCP offers them by level
(read, modify, execute). Capabilities list the engine, its participants and
every coupling model with its real maturity. There is no GUI panel yet.

## Not implemented (and refused)

Scene-object systems and the Yee/FDTD/finite-element-plate solvers as
participants; acoustic, Lorentz, radiation-pressure, magnetic-loss,
piezoelectric and fluid couplings (registered as planned: validation refuses
them); multirate time-dependent coupling; checkpoint resume; unstructured
mesh mapping. See [the roadmap](COUPLED_ENGINE_ROADMAP.md).
