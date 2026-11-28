# WORK-000017: Coupled Energy-Matter Engine: engine core, thermal domain, layered EM media, thermal loads, validated EM-thermal-mechanical couplings, services and contract

**Status:** completed
**Type:** FEATURE
**Request:** requests/processing/2026-11-26-prompt-18-resonant-coupled-ene-req-chat-20261126-016.md
**Requirements:** RES-CEME-001, RES-CEME-002, RES-CEME-003, RES-CEME-004, RES-CEME-005, RES-CEME-006, RES-CEME-007, RES-CEME-008, RES-CEME-009, RES-CEME-010, RES-CEME-011, RES-CEME-012, RES-CEME-013, RES-CEME-014, RES-CEME-015, RES-CEME-016
**Decision:** ADR-000220
**Evidence:** VER-000017
**Test-Suites:** core, app, cmake, layering, audits
**Test-Run:** 20261127T194500-59855

## Scope

Coupled Energy-Matter Engine: engine core, thermal domain, layered EM media, thermal loads, validated EM-thermal-mechanical couplings, services and contract

## Implementation

- Engine core, domain-independent (`src/core/multiphysics`, layering rule in `tools/check_layering.py`): `Quantity` (registry of typed descriptors, compatibility), `FieldData` (rectilinear cell grids, time character, provenance), `Transfer` (conservative overlap remapping and clamped linear interpolation with reports; conservative transfer of non-conserved quantities refused), `Ledger` (terms, transfer records, per-participant and system residuals, attribution, `powerScale` for transient ledgers), `MaterialState` (constant / linear / table laws of temperature with validity, extrapolation warnings, relaxed updates, revisions, history), `GeometryState` (reference and current thicknesses, revisions), `PhysicalSystem` (the `resonant.physical-system` v1 document and its validation), `Orchestrator` (participant interface, topological order of field transfers, prepare before every solve, one-way / sequential / iterative schemes with relaxation, state and geometry updates, failure, invalid-value and cancellation handling, `CoupledResult` with provenance).
- Thermal domain (`src/core/thermal`): `HeatConduction` (finite volumes on cell grids, adiabatic / temperature / convective faces, steady LDLT and implicit-Euler transient with energy balance), `ThermalMaterial`; `PhysicsDomain::Thermal` and the thermal property set (`foundation/Types`, `materials/Material.cpp`), shown by the materials dialog (`MaterialsDialog.ui`, `MaterialsDialog.cpp`).
- Electromagnetics: `LayeredMedia` (exact transfer-matrix plane-wave solution of lossy layers, absorbed power by flux differences and its density).
- Mechanics: `ThermalLoads` (thermal-strain nodal loads and stresses for solids and frames) using the newly public `solidStrainMatrix` / `solidElasticity` of `StructuralModel`.
- Coupling (`src/core/coupling`): `CouplingModels` (validated, experimental, external and planned models with equations; participant solvers), `MultiphysicsParticipants` (layered EM, thermal column, solid column), `CoupledRun` (validate, build, run, the job).
- Services and clients: `MultiphysicsService` (`Services.h`, `MultiphysicsServices.cpp`, `ProjectServices.cpp`, result documents in `Session.h`), capabilities for the engine, participants and every coupling model (`ApplicationServices.cpp`), the protobuf contract `proto/resonant/v1/multiphysics/*.proto` (8 methods) with handlers in `src/automation_api/Handlers.cpp`, `docs/reference/PROTOBUF_API.md` regenerated, MCP tools in `python/resonant_api/mcp.py`.
- Example: `examples/multiphysics/a-microwave-heated-slab.resonant` (EM -> heat -> expansion -> geometry -> EM, temperature-dependent loss, iterated) and its README.

## Tests

- `tests/core/test_multiphysics.cpp` (11): transfers, laws, geometry revisions, system document and validation, order and cycles, fixed-point iteration, non-convergence, failure, invalid values, cancellation, ledger attribution.
- `tests/core/test_heat_conduction.cpp` (7), `test_layered_media.cpp` (5), `test_thermal_loads.cpp` (5): each against analytical solutions.
- `tests/core/test_coupled_validation.cpp` (8): EM -> thermal against a semi-analytic reference, two-way loss against the lumped fixed point, thermal expansion and stress against closed forms, E(T), geometry feedback against a standalone solve, transient energy balance, refusal of planned couplings, and the example project converging with a balanced ledger.
- `tests/core/test_multiphysics_services.cpp` (4): storage, undo, save/load and renames; validation issues; a coupled job with provenance; permissions.
- `python/tests/test_multiphysics_api.py` (4): the contract end to end through the headless host.
- `tests/core/test_project.cpp`: the unknown-domain example changed from `thermal` (now a domain) to `optical`.

## Regression impact and documentation

- A new `PhysicsDomain` value reaches code that enumerates domains: the materials dialog, capabilities and the project format test were updated; the example library is unchanged (its examples regenerate identically). Standalone simulations are untouched; the full core and app suites pass.
- Documents: `docs/requirements/coupled_engine.md`, ADR-000220, `docs/architecture/COUPLED_ENGINE_ARCHITECTURE.md`, `COUPLED_ENGINE_AUDIT.md`, `PHYSICAL_SYSTEM_MODEL.md`, `COUPLED_ENGINE_ROADMAP.md`, `docs/physics/PHYSICAL_QUANTITY_SYSTEM.md`, `COUPLING_ARCHITECTURE.md`, `SPATIAL_AND_TEMPORAL_TRANSFER.md`, `COUPLED_SIMULATION_EXECUTION.md`, `COUPLED_SIMULATION_VALIDATION.md`, `docs/thermal/HEAT_CONDUCTION.md`, `docs/electromagnetics/LAYERED_MEDIA.md`, `docs/mechanics/THERMAL_LOADS.md`; updated `docs/materials/MATERIAL_STATE_ARCHITECTURE.md`, `docs/physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md`, `docs/architecture/COUPLED_MULTIPHYSICS_ARCHITECTURE.md`, `ENERGY_MATTER_RESEARCH_ROADMAP.md`, `overview.md`, `README.md`, `roadmap.md` (M13), `docs/vision/SHAPE_FUNCTION_ENERGY.md`, `docs/requirements/product.md`, `docs/README.md`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `examples/README.md`.
- Scope recorded in ADR-000220 and `IMPLEMENTATION_STATUS.md`: systems are layered slabs; the GUI panel, 3D participants (Yee, FDTD, plates), acoustic, force and fluid couplings and multirate time coupling are on `docs/architecture/COUPLED_ENGINE_ROADMAP.md`. The research roadmap location follows the repository convention (`docs/architecture/` instead of `docs/roadmap/`).

## Traceability

REQ-CHAT-20261126-016 -> RES-CEME-001, RES-CEME-002, RES-CEME-003, RES-CEME-004, RES-CEME-005, RES-CEME-006, RES-CEME-007, RES-CEME-008, RES-CEME-009, RES-CEME-010, RES-CEME-011, RES-CEME-012, RES-CEME-013, RES-CEME-014, RES-CEME-015, RES-CEME-016 -> WORK-000017 -> evidence (pending)

## Transitions

- 2026-11-27T23:53:51.700079+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T00:51:16.754491+00:00 active -> completed by agent-dev-20261126151713-76f632
