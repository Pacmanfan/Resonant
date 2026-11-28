# Coupled Energy–Matter Engine

**Owner of:** `RES-CEME-001..016`
**Source:** REQ-CHAT-20261126-016 (Prompt 18, "Coupled Energy–Matter Engine")
**Refines:** `shape_function_energy.md` RES-SFE-006 (coupling coordinates solvers), RES-SFE-007 (accounting), RES-SFE-008 (material state), RES-SFE-012 (conventions)
**Decisions:** ADR-000220 (builds on ADR-000219)
**Documents:** `docs/architecture/COUPLED_ENGINE_AUDIT.md`, `docs/architecture/COUPLED_ENGINE_ARCHITECTURE.md`, `docs/architecture/PHYSICAL_SYSTEM_MODEL.md`, `docs/physics/PHYSICAL_QUANTITY_SYSTEM.md`, `docs/materials/MATERIAL_STATE_ARCHITECTURE.md`, `docs/physics/COUPLING_ARCHITECTURE.md`, `docs/physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md`, `docs/physics/SPATIAL_AND_TEMPORAL_TRANSFER.md`, `docs/physics/COUPLED_SIMULATION_EXECUTION.md`, `docs/physics/COUPLED_SIMULATION_VALIDATION.md`, `docs/architecture/COUPLED_ENGINE_ROADMAP.md`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-CEME-001 — The engine orchestrates, the domains compute

`src/core/multiphysics` (the engine) holds no physical equations and
includes no physics domain; it orchestrates participants, transfers,
material and geometry state, accounting and provenance. Physical equations
stay in their domains (`thermal`, `electromagnetics`, `mechanics`,
`acoustics`); the participants that adapt a domain to the engine, and the
coupling models, live in `src/core/coupling`.
*Verification:* `tools/check_layering.py`.

### RES-CEME-002 — Typed physical quantities

Every exchanged quantity has a registered descriptor: identifier, SI unit,
rank (scalar, vector, tensor), real or complex representation, meaning
(instantaneous, peak, RMS, time-averaged, integrated), and the domain that
defines it. Field data carry the descriptor, their support (an axis-aligned
cell grid in world meters), their time character (steady, harmonic at a
frequency, a time), the producing solver and the material and geometry
revisions they were computed from. A transfer between quantities whose
descriptors differ is refused.
*Verification:* `Multiphysics.Quantities*`, `Multiphysics.FieldData*`.

### RES-CEME-003 — Material state

Region material laws (constant, linear in temperature, tabulated) with
validity and provenance turn a material's intrinsic definition and the
current state (a temperature field per region) into properties at solver
locations; evaluation outside the validity is reported, never silent; every
state change increments a revision that participants check.
*Verification:* `Multiphysics.MaterialState*`.

### RES-CEME-004 — Geometry state

Reference and current geometry (region dimensions) are kept apart; a
geometry update increments a revision; data computed from an older revision
is identified as stale; a participant rebuilds its setup only when the
geometry revision it used changed.
*Verification:* `Multiphysics.GeometryState*`, `CoupledValidation.ExpansionFeedsBackIntoTheElectromagneticSolution`.

### RES-CEME-005 — The physical system

A physical system (regions with materials and dimensions, domains with
their solver and settings, couplings, excitation, environment, scheme and
convergence settings, observations) is a JSON document (`resonant.physical-system`,
version 1) with stable ids, stored in the project (extension
`multiphysics`), validated before a run (references resolve, coupling models
exist, quantities match), and preserved by save, load and rename.
*Verification:* `Multiphysics.PhysicalSystem*`, `MultiphysicsServices.*`.

### RES-CEME-006 — Coupling strategies

One-way, sequential (a fixed number of passes) and iterative (until every
monitored state change is below its tolerance, with relaxation) coupling are
run by one orchestrator from the coupling graph; the order follows the
graph; cycles require a sequential or iterative scheme. Non-convergence is a
result, reported with its history, never presented as converged.
*Verification:* `Multiphysics.Orchestrator*`.

### RES-CEME-007 — Failure, invalid values and cancellation

A participant failure, a non-finite value in an exchanged field or a
cancellation ends the run with its state (failed, cancelled), the stage
that ended it, and the iterations completed (kept as partial results);
invalid values are never passed on.
*Verification:* `Multiphysics.OrchestratorStopsOn*`.

### RES-CEME-008 — Spatial transfer

Transfers between grids are either conservative (overlap-weighted cell
remapping: the integral over the overlap is preserved) or interpolating
(linear in cell centres: not conservative), chosen per coupling and
recorded with the source and target integrals and their relative
difference. Power and heat sources are transferred conservatively.
*Verification:* `Multiphysics.ConservativeTransfer*`, `Multiphysics.InterpolatingTransfer*`.

### RES-CEME-009 — Energy accounting

A coupled run keeps a ledger: per participant its input, boundary losses,
dissipation, storage and transfers (in watts for steady states, joules over
an interval for transient ones), explicit transfer records (source,
destination, mechanism, quantity, region, interval, amount, operator,
approximation), and a residual per participant and for the system,
attributed to transfer (interpolation) error, coupling (iteration) error
and solver error where these are known. Energy that moves between domains
is counted once.
*Verification:* `Multiphysics.Ledger*`, `CoupledValidation.*`.

### RES-CEME-010 — The thermal domain

`src/core/thermal`: steady and transient heat conduction on a cell grid
(finite volumes) with per-cell conductivity, heat capacity and sources;
faces adiabatic, at a fixed temperature or convective; reports boundary heat
flows and stored energy. Materials gain a thermal property set
(conductivity, specific heat, density, expansion coefficient).
*Verification:* `HeatConduction.*`.

### RES-CEME-011 — Layered electromagnetic media

`electromagnetics/LayeredMedia`: a plane wave at normal incidence on a stack
of homogeneous layers (exact transfer-matrix solution, `e^{+jωt}`, peak
amplitudes), with reflection, transmission, the field and Poynting flux
inside, and the absorbed power density ½ ω ε₀ ε″_eff |E|² per sample.
*Verification:* `LayeredMedia.*`.

### RES-CEME-012 — Thermal loads in mechanics

The finite-element model accepts element temperatures: thermal strain
α ΔT gives the load vector for solid and frame elements, and stresses are
recovered as D (B u − ε_th).
*Verification:* `ThermalLoads.*`.

### RES-CEME-013 — Validated reference couplings

Implemented and validated against references: electromagnetic absorption →
heat (one-way), temperature-dependent loss ↔ heat (two-way, iterative),
temperature → thermal expansion and stress, displacement → geometry →
electromagnetic solution (feedback). The structure-to-sound radiation
(REQ-CHAT-20261126-006) is listed as validated one-way coupling outside the
orchestrator. Other couplings (acoustic pressure on structures, Lorentz and
Maxwell forces, acoustic heating, fluid–structure) are registered as
planned, with their governing equations documented, and refused by
validation.
*Verification:* `CoupledValidation.*`; `docs/physics/COUPLED_SIMULATION_VALIDATION.md`.

### RES-CEME-014 — Runs are jobs with provenance

A coupled simulation runs as a job of the unified job system (progress per
stage, cancellation) and its result records the system document and its
checksum, the scheme, convergence settings and history, each participant's
solver and version, each transfer's operator, the ledger and audit, the
material and geometry revisions, and the build version.
*Verification:* `MultiphysicsServices.*`.

### RES-CEME-015 — Through the services, for every client

Listing coupling models, storing, reading and validating physical systems,
starting coupled simulations and reading their results are application
services (undoable where they change the project), exposed through the
protobuf contract (`resonant.v1.MultiphysicsService`), the Python client and
MCP; no client has its own logic.
*Verification:* `AutomationApi.TheDispatcherImplementsExactlyTheContractsMethods`; `python/tests/test_multiphysics_api.py`.

### RES-CEME-016 — Standalone simulations are unchanged

Existing acoustic, mechanical and electromagnetic simulations and their
results are unchanged by the engine.
*Verification:* the existing core and application suites.
