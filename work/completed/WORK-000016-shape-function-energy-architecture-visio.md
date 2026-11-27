# WORK-000016: Shape-Function-Energy architecture: vision, coupled energy-matter architecture, accounting, material state, functional geometry, resonance, metamaterials, multiscale, roadmap

**Status:** completed
**Type:** FEATURE
**Request:** requests/processing/2026-11-26-prompt-17-resonant-shape-funct-req-chat-20261126-015.md
**Requirements:** RES-SFE-001, RES-SFE-002, RES-SFE-003, RES-SFE-004, RES-SFE-005, RES-SFE-006, RES-SFE-007, RES-SFE-008, RES-SFE-009, RES-SFE-010, RES-SFE-011, RES-SFE-012
**Decision:** ADR-000219
**Evidence:** VER-000016
**Test-Suites:** audits
**Test-Run:** 20261127T184506-4117096, 20261127T184507-4117284

## Scope

Prompt 17: establish the Shape–Function–Energy principle as Resonant's scientific direction and document its architecture (functional geometry, resonance across domains, metamaterials, the Coupled Energy–Matter Engine, energy and momentum accounting, material state, multiscale modeling, visualization), audit the existing architecture against it, and plan the work in stages. By the request's terms no engine is implemented in this milestone.

## Implementation

- Requirements `docs/requirements/shape_function_energy.md` (RES-SFE-001..012, refining RES-PRODUCT-006, -010, -013, -016) and ADR-000219.
- New documents (the request's paths; the research roadmap sits with the milestone roadmap in `docs/architecture/` instead of a new `docs/roadmap/`): `docs/vision/SHAPE_FUNCTION_ENERGY.md`, `docs/physics/COUPLED_ENERGY_MATTER.md`, `docs/physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md`, `docs/physics/RESONANT_STRUCTURES.md`, `docs/physics/MULTISCALE_MODELING.md`, `docs/materials/MATERIAL_STATE_ARCHITECTURE.md`, `docs/materials/METAMATERIALS_AND_ENGINEERED_STRUCTURES.md`, `docs/geometry/FUNCTIONAL_GEOMETRY.md`, `docs/architecture/COUPLED_MULTIPHYSICS_ARCHITECTURE.md`, `docs/architecture/ENERGY_MATTER_RESEARCH_ROADMAP.md`, and the audit `docs/architecture/SHAPE_FUNCTION_ENERGY_AUDIT.md` (mapping of every subsystem, gaps, eleven proposed abstractions, risks, external packages).
- Updated: `docs/requirements/product.md` (the direction; RES-PRODUCT-006 points to RES-SFE-001), `docs/architecture/overview.md` (direction section; the shared `e^{+jωt}` convention), `docs/architecture/README.md`, `docs/architecture/roadmap.md` (M12), `docs/README.md`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `README.md`.
- No code changed: the audit found the conventions coupling relies on (SI, `e^{+jωt}` peak phasors, provenance) consistent across the domains, so no correction was required.
- `README.md`: the Linux ZIP sentence of REQ-CHAT-20261126-013 named `./resonant` as a code span, which `scripts/tests/test_root_readme.py` reads as a repository path; reworded (that test is in the cached `scripts` suite and was not re-run for REQ-013).

## Tests

- `scripts/doc_integrity.py`, `scripts/drift_detect.py` and `tools/audit --no-tests` (no new finding; the two DRIFT findings about `agent_coordination.md` predate this request), `scripts/tests/test_root_readme.py`, the t2 tier linked below.
- Facts in the documents were checked against the code (energy records of FdtdGrid, Yee and the mechanical transient solution; mode types of every domain; Bloch analysis; Yee and FDTD boundary kinds; material model; optimization adapter quantities).

## Regression impact and documentation

- Documentation only, plus a README wording fix; nothing in the build or the tests changes behavior.
- The queued requests REQ-CHAT-20261126-016..023 and REQ-CHAT-20261127-001 are mapped to roadmap stages; each keeps its own scope.

## Traceability

REQ-CHAT-20261126-015 -> RES-SFE-001, RES-SFE-002, RES-SFE-003, RES-SFE-004, RES-SFE-005, RES-SFE-006, RES-SFE-007, RES-SFE-008, RES-SFE-009, RES-SFE-010, RES-SFE-011, RES-SFE-012 -> WORK-000016 -> evidence (pending)

## Transitions

- 2026-11-27T23:37:01.277599+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-27T23:48:50.470296+00:00 active -> completed by agent-dev-20261126151713-76f632
