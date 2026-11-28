# ADR-000228: Functionally architected matter: a canonical architecture of regions, stacks, interfaces and fields, analyzed by the existing solvers through declared capabilities

**Status:** ACCEPTED
**Date:** 2026-11-28
**Type:** architecture
**Requirements:** `docs/requirements/architected_matter.md` RES-FAM-001..020
**Request:** REQ-CHAT-20261127-001
**Builds on:** ADR-000220 (the coupled engine), ADR-000224 (multiscale lineage), ADR-000225 (material state), ADR-000226 (accounting), ADR-000227 (laboratory), ADR-000214 (optimization)

## Context

Resonant knows matter in three places, each partial:

- **The material library:** one scalar property per key and domain, with
  provenance.
- **The multiscale lineage:** representations, RVEs and derivations.
- **Several solvers:** layers given inline (the layered electromagnetic solver,
  the coupled engine's layered slab, the metamaterial analyses with two box
  bodies).

There is no record of how the matter of an object is organized: its regions,
their composition, their orientation, the layers on it, and the interfaces
between them. Interfaces are perfect everywhere, implicitly. The audit is
`docs/materials/ARCHITECTED_MATTER_AUDIT.md`.

## Decisions

1. **A new infrastructure module.** `src/core/matter` includes no physics
   domain and no coupling, and holds the canonical record:
   - architectures with objects, functional regions (composition, phases,
     microstructure, orientation, anisotropic sets, operating conditions,
     process), stacks of layers with patterns, interfaces with their models,
     property fields, functions with pathways, the adaptive class and states,
     manufacturing processes, constraints and records;
   - validation, a content revision, and persistence in the project extension
     `matter` (format `resonant.matter`).

   The module also holds the arithmetic that needs no domain:
   - field evaluation and discretization;
   - the effective-property estimators and bounds;
   - the capability declaration type and its compatibility check;
   - the manufacturing checks.

2. **Regions reference library materials; the library is unchanged.**
   Anisotropic constants and piezoelectric coefficients live on the region,
   with provenance, because the library has scalar properties only. A
   composition's effective property is given only by a named method, with its
   bounds and validity (RES-FAM-008).

3. **New physics goes in the domains, tested against closed forms:**
   - `mechanics/Laminate`: classical lamination theory, with thermal
     resultants and plate and beam modes from the reduced bending stiffness;
   - `mechanics/PiezoelectricBeam`: a lumped unimorph or bimorph cantilever
     with a resistive load;
   - `thermal::ConductionProblem::contactResistance`: a series resistance on
     internal planes of the grid, used by the coupled engine's thermal
     participant for the stack's interfaces.

4. **Adapters in coupling translate architectures to the solvers:**
   - `coupling/MatterAnalyses`: the capability declarations, the compatibility
     check, and the laminate, thermal, electrical, electromagnetic, modal and
     harvester analyses;
   - `coupling/MatterSystems`: stacks as physical systems of the coupled
     engine;
   - `coupling/MatterAdapter`: the optimizer's adapter `matter`;
   - `coupling/MatterExploration`: exploration, the mechanisms and pathways,
     with the accounting ledger of evaluated stages.

   The laboratory gains the link kind `matter-architecture` and the model
   `matter.stack-metric`.

5. **Refuse rather than approximate.** An analysis refuses what its
   declaration does not cover: an anisotropic layer in the electromagnetic
   solver, a finite-stiffness interface in lamination theory, a graded field
   in a solver that cannot discretize it, a composition without a method. The
   refusal names the feature and the reason.

6. **Two ways of working, one service.**
   - *Explore* an architecture: what it can do, what is missing, what may
     fail.
   - *Mechanisms* for a desired transformation.

   Both are deterministic evidence (findings apart from hypotheses) that an
   assistant reads through MCP. Neither changes anything.

7. **The service, contract and panel.** `MatterService` (automation API,
   Python, MCP) and the Matter panel, whose stack view is labelled as an
   idealized model.

## Consequences

- An architecture is the single description that the solvers, the optimizer,
  the laboratory and the manufacturing checks read. Each solver reads only
  what its declaration covers.
- Interfaces are explicit everywhere they matter. A stack whose interface has
  no model is reported, never treated as perfect.
- The existing analyses keep their own inputs, and nothing in them changes for
  projects without an architecture.

## Deviations from the request

- **No shell or solid finite-element model of layered bodies.** A bell (or any
  curved shell) with deposited layers is not analyzed. Coated beams and plates
  are analyzed through lamination theory and closed-form modes. The energy
  converter example is a coated cantilever, not a bell.
- **Piezoelectric conversion** is a lumped single-mode model (Rayleigh, static
  deflection shape, first mode, resistive load). There is no coupled
  piezoelectric field solver, and the coupled engine's piezoelectric coupling
  stays planned.
- **Transformations not modeled:** thermoelectric, magnetostrictive,
  photovoltaic, triboelectric, electromagnetic induction and chemical
  transformations. They are listed as mechanisms with that status.
- **Interface models consumed by solvers:** thermal and electrical contact
  resistance, plus the cohesive strength used as a delamination criterion.
  Finite-stiffness, cohesive-zone, frictional, diffusive, reactive and
  charge-transfer models are recorded and refused by the analyses.
- **Fields and anisotropy:** property fields vary along one axis. Anisotropy
  is used by lamination theory and through-thickness conduction only. The
  coupled engine and the electromagnetic solver refuse it.
- **Not delivered:**
  - no microstructure generation beyond the multiscale RVE generators;
  - no CAD export of architectures (JSON and a layer table only);
  - no 3D viewport rendering of property fields;
  - no machine-learned discovery: exploration is rule-based evidence.
