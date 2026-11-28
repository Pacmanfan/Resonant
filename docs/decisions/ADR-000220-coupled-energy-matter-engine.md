# ADR-000220: Coupled Energy–Matter Engine — a domain-independent engine module, participants and coupling models in coupling, typed quantities on cell grids, steady coupled schemes with an energy ledger, a thermal domain and a layered-media reference system

**Status:** ACCEPTED
**Date:** 2026-11-27
**Type:** architecture
**Requirements:** `docs/requirements/coupled_engine.md` RES-CEME-001..016
**Request:** REQ-CHAT-20261126-016
**Builds on:** ADR-000219 (direction), ADR-000205 and ADR-000210 (layering, coupling), ADR-000216 (services, contract)

## Context

ADR-000219 decided that a Coupled Energy–Matter Engine coordinates the
existing solvers. The audit (`docs/architecture/COUPLED_ENGINE_AUDIT.md`)
found no thermal domain, no field type shared between domains, materials
whose properties depend on frequency only, no thermal strain in the
finite-element model, one one-way coupling written directly against two
domains, and the electromagnetic full-wave solver (Yee) being transient only:
a harmonic absorbed-power field would need long runs to steady state and a
phasor extraction for every iteration of a coupled loop.

## Decisions

1. **Two modules.** `src/core/multiphysics` is the engine: quantities, field
   data, transfers, the ledger, material and geometry state, the physical
   system document, the participant interface and the orchestrator; it
   includes no physics domain and no coupling (layering rule).
   `src/core/coupling` holds the participants (adapters of domain solvers)
   and the registry of coupling models.
2. **Field data on axis-aligned cell grids** in world meters, with a
   descriptor per quantity. A grid covers 1D (one cell across), 2D and 3D.
   Participants with other discretizations (finite-element meshes) convert
   at their boundary and say how. Unstructured mesh-to-mesh mapping is
   deferred.
3. **Steady coupled schemes first.** One-way, sequential and iterative
   coupling of steady (or harmonic, time-averaged) solutions, with
   under-relaxation of state updates and convergence on the maximum state
   change. Time-dependent multirate coupling is designed (time character in
   every field, the participant interface admits `advance`) but not
   implemented; the thermal domain alone has transient conduction.
4. **A thermal domain** (`src/core/thermal`): finite-volume conduction on a
   cell grid, steady (sparse Cholesky) and transient (implicit Euler).
5. **The first reference system is a layered stack** (`geometry.kind =
   "layered-slab"`): layers along z with a lateral area, a plane wave at
   normal incidence. Its electromagnetic model is an exact transfer-matrix
   solution (`electromagnetics/LayeredMedia`) rather than the Yee solver, so
   absorption, heating, temperature-dependent loss and expansion can be
   validated against analytical references; the mechanics participant uses
   the existing hexahedral finite elements with thermal loads; expansion
   changes the layers' thicknesses, which the electromagnetic participant
   picks up through the geometry revision. Systems built from scene objects
   (and the Yee solver as a participant) are the next step.
6. **Material laws on regions.** The physical system attaches laws
   (constant, linear in temperature, table) to a region's material
   property; the library keeps intrinsic values. Moving laws into the
   material library itself is deferred until the material editor supports
   them.
7. **Accounting by participants.** Each participant reports its terms; the
   engine records transfers with the operator's source and target integrals;
   the audit computes residuals per participant and for the system and
   attributes known errors (transfer, iteration).
8. **Services and contract.** `MultiphysicsService` in the services and the
   protobuf contract; physical systems travel as their JSON document (as
   workflow definitions do), results as typed summaries plus the full
   result document. Runs are jobs.
9. **Couplings not implemented are registered as planned** with their
   equations; validation refuses a system that uses them.

## Consequences

- `PhysicsDomain` gains `Thermal`; materials gain a thermal property set;
  the material dialog shows it.
- No GUI for physical systems yet (Python, the API and project files only);
  the roadmap names it.
- The Yee solver, the acoustic FDTD and scene-based systems join as
  participants in later stages without changing the engine.
