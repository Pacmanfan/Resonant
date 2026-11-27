# ADR-000219: Shape–Function–Energy direction — function from physical models, a coordinating Coupled Energy–Matter Engine over independent solvers, explicit energy accounting, evolving material state, distinct geometry representations, multiscale through adapters

**Status:** ACCEPTED (direction; no engine is implemented by this decision)
**Date:** 2026-11-27
**Type:** architecture
**Requirements:** `docs/requirements/shape_function_energy.md` RES-SFE-001..012
**Request:** REQ-CHAT-20261126-015
**Builds on:** ADR-000205 (layers, domains, coupling only in `src/core/coupling`), ADR-000210 (the first coupling), ADR-000212 (compute backends), ADR-000214 (optimization framework), ADR-000216 (application services)

## Context

Resonant has three wave domains with their own solvers (acoustics:
analytical, Kirchhoff and FDTD; mechanics: finite elements; electromagnetics:
analytical models and a Yee solver), one one-way coupling (plate vibration
to sound, `coupling/StructuralRadiation`), a domain-independent optimization
framework with adapters, materials with per-domain property sets and
provenance, and an application service layer. The request asks to make the
principle "an object's geometry and internal structure can be the mechanism
of its physical function" central, and to define how energy, momentum,
material state, resonance, engineered structures and several scales fit the
architecture, without a rewrite and without implementing the coupled engine
yet. The audit of the current code is
`docs/architecture/SHAPE_FUNCTION_ENERGY_AUDIT.md`.

## Decisions

1. **Function is computed, not declared** (RES-SFE-001). Object types and
   categories select which models and analyses are offered; behavior comes
   only from a physical model. This generalizes RES-PRODUCT-006.
2. **A coordinating engine, not a unified solver** (RES-SFE-006). The Coupled
   Energy–Matter Engine will be a layer in `src/core/coupling`: a coupling
   graph of participants (existing solvers behind a participant interface),
   exchanged quantities (named, with units, support and time), transfer
   operators and a scheduler for one-way, sequential, iterative and
   time-dependent coupling. Solvers keep their meshes, grids, time steps and
   methods. Alternatives rejected: one monolithic multiphysics solver (would
   replace validated solvers and force one formulation); coupling inside each
   domain (breaks the layering rule that only `coupling` uses two domains);
   an external coupling library as a mandatory dependency (preCICE and
   others are evaluated as optional back ends, see the roadmap).
3. **Accounting is part of every coupled result** (RES-SFE-007): a ledger of
   energy (and momentum where defined) terms per participant and per
   exchange, with the residual. The existing energy records of the FDTD, Yee
   and mechanical transient solvers are its first entries.
4. **Material state extends the property model** (RES-SFE-008): properties
   become functions of declared state variables with validity and provenance;
   the per-domain property sets stay, and constitutive models stay in their
   domains. A state field (temperature, stress, damage) lives with the
   coupling run, not in the material library.
5. **Geometry has several representations** (RES-SFE-003): engineering
   geometry, boundary geometry, material regions, simulation discretizations
   and internal (periodic) structure are distinct, connected by conversions
   that report fidelity. `geometry/SolidModel` and the solvers' setups are
   the existing conversions.
6. **Resonance gets a shared vocabulary, not a shared solver** (RES-SFE-004):
   a common description of a resonance (frequency, shape, Q, damping, modal
   energy, coupling) that each domain's mode type can produce.
7. **Effective properties are results** (RES-SFE-005): produced by
   homogenization or retrieval models with their validity, stored apart from
   constituent materials.
8. **Multiscale through optional adapters** (RES-SFE-009) with provenance by
   scale and tool; GPL-licensed tools only as separate processes.
9. **Staged roadmap** (`docs/architecture/ENERGY_MATTER_RESEARCH_ROADMAP.md`):
   twelve stages from conventions to experimental validation, each with goals,
   dependencies, risks, validation and acceptance criteria.

## Consequences

- No code changes in this milestone: the conventions the engine needs (SI,
  `e^{+jωt}`, peak amplitudes, provenance) are already consistent across the
  domains (checked by the audit), so no correction was required.
- New capabilities in later requests must follow RES-SFE-001..012; reviews and
  the layering check enforce the structural parts.
- The thermal domain, material state, the coupling engine, a shared resonance
  result, effective-property models and multiscale adapters are planned work,
  listed as such in `IMPLEMENTATION_STATUS.md`.
