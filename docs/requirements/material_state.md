# Dynamic Material State and Nonlinear Matter

**Owner of:** `RES-DMS-001..014`
**Source:** REQ-CHAT-20261126-021 (Prompt 23, "Dynamic Material State, Nonlinear Matter and Evolving Physical Systems")
**Refines:** `coupled_engine.md` RES-CEME-003 (material state), `multiscale.md` RES-MSC-003 (lineage), `resonance.md` RES-RSN-001
**Decisions:** ADR-000225 (builds on ADR-000224, ADR-000220, ADR-000222)
**Documents:** `docs/materials/DYNAMIC_MATERIAL_STATE.md`, `docs/physics/CONSTITUTIVE_MODELS.md`, `docs/physics/STATE_EVOLUTION.md`, `docs/physics/MATERIAL_STATE_VALIDATION.md`, `docs/materials/MATERIAL_STATE_ARCHITECTURE.md` (updated)
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-DMS-001 — Definitions and states are separate

A material definition (the library) never holds an evolving state. A
material state belongs to a constitutive model and holds the variables that
model defines - each with a unit and a class (reversible, irreversible,
history) - plus its identity: material, region, model, time or load step,
previous state, revision and provenance.
*Verification:* `MaterialStateModel.*`.

### RES-DMS-002 — Spatially varying state

A state is a field over the cells or elements of its region (one material
point each) wherever the physics varies in space: temperature, plastic
strain, damage and phase fraction are not forced to be uniform.
*Verification:* `StateEvolution.*`, `DamagedBeam.*`, `PhaseChange.*`.

### RES-DMS-003 — History and persistence

A run's state history (per step: time, variables per material point or their
summaries, energies, convergence) is a record that can be stored in the
project (extension `material-state`) as one undoable step and survives save
and reload.
*Verification:* `MaterialStateServices.StudiesRunAsJobsAndRecordsSurviveSaveAndReload`, `MaterialStateModel.FieldsSummarizeAndRecordsRoundTrip`.

### RES-DMS-004 — State update contract

Each step computes a trial state from the committed one; it is committed only
when the step converges (Newton or fixed-point residual below its
tolerance), otherwise the step is rejected, the committed state kept and the
failure reported with its residual.
*Verification:* `Plasticity.*`, `StateEvolution.*`.

### RES-DMS-005 — Elastoplasticity

J2 plasticity with linear isotropic and kinematic hardening (radial return,
consistent tangent): elastic and plastic strain kept apart, loading and
unloading distinguished, residual strain after unloading, the Bauschinger
shift on reversal, quadratic Newton convergence, and the energy split into
elastic, hardening (stored) and dissipated parts.
*Verification:* `Plasticity.*`.

### RES-DMS-006 — Damage and resonance

Scalar isotropic damage driven by the largest equivalent strain reached
(history), exponential softening or a non-softening power law, secant
unloading and its dissipation; a
cantilever whose elements accumulate damage under load cycles, and its modes
computed from the damaged stiffness - no assumed damage-to-frequency law.
*Verification:* `Damage.*`, `DamagedBeam.*`.

### RES-DMS-007 — Phase transformation with latent heat

Transient conduction with melting and solidification by the enthalpy method:
phase fraction as a state, latent heat absorbed and released, properties of
each phase, the energy balance per step, and the melt front against the
Stefan (Neumann) solution.
*Verification:* `PhaseChange.*`.

### RES-DMS-008 — Hysteresis and memory

A Preisach model (relay hysterons, a reversible part) for magnetization:
loops arising from the model, return-point memory (wiping out), congruent
minor loops, the loss per cycle as the loop area; and a standard linear
solid for viscoelastic (rate-dependent) response.
*Verification:* `Hysteresis.*`, `Viscoelasticity.*`.

### RES-DMS-009 — Time-stepped coupled evolution

A physical system evolves in time steps, each a coupled run of the Coupled
Energy–Matter Engine continuing from the previous temperature field, with
temperature-dependent properties re-evaluated every step; the energy of each
step (input, stored, out) and of the run is balanced; refining the time step
converges.
*Verification:* `StateEvolution.*`.

### RES-DMS-010 — Thermo-mechanical evolution

Thermal strain, constrained stress and geometry updates evolve with the
temperature over the steps where the system has a mechanical domain.
*Verification:* `StateEvolution.ThermoMechanicalStressFollowsTheTemperature`.

### RES-DMS-011 — Cross-system integration

The damaged cantilever is a functional-structure analysis whose modes the
resonance architecture reports; an evolved state can be committed to a
library material as values with provenance computed and a derivation record,
so effective materials derived from that material become stale (lineage).
*Verification:* `MaterialStateServices.*`, `MaterialStateExamples.*`.

### RES-DMS-012 — Numerical stability and convergence

Load-increment and time-step refinement studies, Newton iteration counts and
convergence rates, and refusals of unstable settings (an explicit step above
its stability limit) are reported.
*Verification:* `Plasticity.*`, `PhaseChange.*`, `StateEvolution.*`.

### RES-DMS-013 — Implemented and unavailable models identified

Every model has a card (variables, parameters, assumptions, validity,
excluded effects); fracture propagation, fatigue, finite-strain plasticity,
hyperelasticity, nonlinear optics and nonlinear acoustics are listed as not
implemented and are never presented as available.
*Verification:* `MaterialStateModel.CatalogueSaysWhatIsNotImplemented`.

### RES-DMS-014 — Services, panel, clients and examples

`MaterialStateService` (models, material-point and structural runs,
evolutions, records, commits), Python and MCP; the Material State panel
(model, loading, response curve, history, spatial profile); examples for an
evolving conductor, thermo-mechanical expansion, plastic deformation, damage
and resonance, a phase transformation and a hysteretic core.
*Verification:* `MaterialStateServices.*`, `python/tests/test_material_state_api.py`, `MaterialStatePanelTest.*`, `MaterialStateExamples.*`.
