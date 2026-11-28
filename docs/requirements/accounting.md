# Unified Energy, Momentum, Force and Conservation Accounting

**Owner of:** `RES-ACC-001..016`
**Source:** REQ-CHAT-20261126-022 (Prompt 24, "Unified Energy, Momentum, Force and Conservation Framework")
**Refines:** `coupled_engine.md` RES-CEME-009 (the coupled ledger), `shape_function_energy.md` RES-SFE-007 (energy and momentum accounting), `resonance.md` RES-RSN-004/-005, `material_state.md` RES-DMS-005..008
**Decisions:** ADR-000226 (builds on ADR-000220, ADR-000222, ADR-000224, ADR-000225)
**Documents:** `docs/architecture/ACCOUNTING_AUDIT.md`, `docs/architecture/ACCOUNTING_ARCHITECTURE.md`, `docs/physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md` (updated), `docs/physics/FORCE_AND_MOMENTUM.md`, `docs/physics/CONSERVATION_MONITORING.md`, `docs/physics/ACCOUNTING_VALIDATION.md`, `docs/physics/PHYSICAL_QUANTITY_SYSTEM.md` (updated)
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-ACC-001 — Audit and reuse

The accounting framework coordinates and verifies the existing physical
calculations (the coupled ledger, the lumped-network integrator, the layered
electromagnetic solution, the material-state models); it contains no second
implementation of any solver's physics. The audit of what is computed where
is a document.
*Verification:* `docs/architecture/ACCOUNTING_AUDIT.md`; `Accounting*` tests consume solver outputs only.

### RES-ACC-002 — Dimensions and units

Units are parsed into SI dimension exponents (mass, length, time,
temperature, current, amount, luminous intensity) with a scale factor;
conversions between units of different dimensions are refused; affine
temperature scales are refused for conversion of differences.
*Verification:* `Dimensions.*`.

### RES-ACC-003 — Typed quantity families

Energy, power, force, torque, linear momentum, angular momentum, entropy,
heat capacity, pressure, momentum flux and the other accounted quantities are
families with a dimension and a conserved flag; quantities of different
families never combine, even with the same dimension (torque and energy,
entropy and heat capacity, pressure and momentum flux), and force is not
conserved.
*Verification:* `AccountingQuantities.*`.

### RES-ACC-004 — Systems and control volumes

An accounting system names its members (bodies, regions, fields,
participants, elements) and its kind (closed, open, control volume, object,
assembly, subsystem); the same exchange is internal when both its ends are
members and a boundary input or output otherwise, without changing the
underlying record.
*Verification:* `AccountingLedger.SameEventIsInternalOrBoundaryByTheSystem`, `AccountingNetworks.SubsystemsOfCoupledResonators`.

### RES-ACC-005 — Energy ledger

The ledger records holdings (stored energy by form and recoverability, at
the interval's ends and as histories) and exchanges (from, to, category,
mechanism, amount, interval, region, object, method, provenance, uncertainty,
the source-side amount and whether the source already includes the
destination's contribution); it keeps stored energy, transferred energy,
instantaneous power and cumulative energy distinct (rate and interval bases).
*Verification:* `AccountingLedger.*`.

### RES-ACC-006 — Cross-domain transfers

Inter-domain transfers are exchanges between the participants' entities;
an internal conversion appears once, as an output of its source and an input
of its destination; the difference between source and target integrals is
reported as a numerical transfer error, never as a source of energy.
*Verification:* `AccountingCoupled.*`.

### RES-ACC-007 — Momentum and impulse

Linear and angular momentum are held by bodies and exchanged through
elements, fields and boundaries; transfers across a boundary (fluxes) and
external impulses (time integrals of external forces) are distinguished; the
balance ΔP = P_in − P_out + I_external is evaluated per component.
*Verification:* `AccountingNetworks.*`, `AccountingRadiation.*`.

### RES-ACC-008 — Forces and torques

Every force record has its category (applied, contact, constraint, elastic,
viscous, pressure, gravity, electromagnetic, magnetic, acoustic radiation,
radiation pressure, internal), originating model, source, target, vector,
frame, application, time dependence, mechanism and method; the two forces of
an element are identified as an action-reaction pair; resultants are summed
per target without discarding the contributions.
*Verification:* `AccountingNetworks.ForcesKeepTheirOriginAndPairs`.

### RES-ACC-009 — Field momentum and radiation force

The force of a normally incident plane wave on a layered body between vacuum
half-spaces is computed from the solver's fields by the vacuum Maxwell stress
at the control surfaces and by the volume Lorentz force on the induced
currents, and agrees with (1 + R − T) I/c; other configurations, where the
momentum formulation inside matter matters, are refused with that reason.
*Verification:* `AccountingRadiation.*`.

### RES-ACC-010 — Conservation monitoring

A registry of conservation laws and consistency relations (quantity, domain,
sources, sinks, fluxes, equation, required data, method, assumptions);
each check reports initial and final values, inputs, outputs, internal
transfers, sources, dissipation, the absolute residual, a relative residual
only when its scale is meaningful, the tolerance, and one of the statuses
verified, warning, exceeded, not evaluated, insufficient data, not
applicable; a check with declared missing contributions is never verified.
*Verification:* `AccountingConservation.*`.

### RES-ACC-011 — Numerical error is not physical loss

Dissipation is only what a model reports as dissipated; a residual is
attributed to the numerical errors the sources state (integration,
transfer mapping, solver balance) when within them, and to an unaccounted
contribution otherwise; refinement studies show the residual's order.
*Verification:* `AccountingConservation.NumericalErrorIsNotDissipation`, `AccountingNetworks.ResidualConvergesWithTheStep`.

### RES-ACC-012 — Time-resolved and local accounting

Ledgers carry sampled histories, so every check can be evaluated over time;
local checks (a volume integral against boundary fluxes) are reported
separately from global ones, and no local conservation is claimed from a
global check.
*Verification:* `AccountingNetworks.*`, `AccountingRadiation.LocalAndGlobalAbsorptionAgree`.

### RES-ACC-013 — Thermodynamics

The first law is the energy check with work and heat distinguished; the
second law (non-negative entropy production) is evaluated where a model
provides entropy (the phase-change slab) or the dissipation increments of an
isothermal process; elsewhere it is reported as not evaluated.
*Verification:* `AccountingMaterialState.*`, `PhaseChange.EntropyProductionIsNonNegative`.

### RES-ACC-014 — Resonance and material state

Stored energy, supplied power and loss by mechanism of a driven resonator
give Q = ω E/P per mechanism against the modal Q (light damping only);
buildup and decay follow the modal decay rate; the energies of the
material-state models (elastic, hardening, dissipated, latent, loop loss)
enter the ledger, and the Preisach loop loss agrees with the sum over the
switching hysterons.
*Verification:* `AccountingNetworks.ResonatorQualityAndBuildupDecay`, `AccountingMaterialState.*`, `Hysteresis.LoopLossEqualsTheHysteronSum`.

### RES-ACC-015 — Scale contracts and effective materials

An entity using an effective material derived from a representation records
the scale contract (source and target representation, method, whether the
source energy remains represented); a system holding energy in both a
representation and the effective material that replaces it is not
evaluated (double counting).
*Verification:* `AccountingConservation.EffectiveAndExplicitRepresentationsAreNotBothCounted`, `AccountingCoupled.*`.

### RES-ACC-016 — Services, panel, clients, persistence and examples

`AccountingService` (quantity families, unit conversion, conservation
models, studies, runs as jobs, reports reclassified for any system,
subsystem comparison, export, records stored in the project extension
`accounting`), Python and MCP; the Accounting panel (energy flows with
calculated widths, breakdown, conservation checks, forces, time series,
subsystem selection); examples for each validated pathway.
*Verification:* `AccountingServices.*`, `python/tests/test_accounting_api.py`, `AccountingPanelTest.*`, `AccountingExamples.*`.
