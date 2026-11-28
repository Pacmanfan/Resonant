# ADR-000221: Functional structures — a domain-independent functional module over scene objects, analyses registered in coupling, dependency fingerprints for staleness, the optimization framework for candidates, and validated reference models

**Status:** ACCEPTED
**Date:** 2026-11-27
**Type:** architecture
**Requirements:** `docs/requirements/functional_structures.md` RES-FUNC-001..016
**Request:** REQ-CHAT-20261126-017
**Builds on:** ADR-000219 (direction), ADR-000220 (coupled engine), ADR-000214 (optimization), ADR-000216 (services, contract), ADR-000205 and ADR-000210 (layering)

## Context

The audit (`docs/architecture/FUNCTIONAL_STRUCTURE_AUDIT.md`) found:
parametric geometry kinds with validated parameters and undoable changes
through the services; scene bindings, an expression evaluator, sampling
algorithms, sensitivities and multi-objective optimization in the
optimization framework; closed-form cavity modes and a Yee solver that
reproduces them; finite-element modal studies and a periodic-rod Bloch
analysis; exact layered media. Missing: any notion of a structure's
function, named parameters spanning objects, revision tracking of results,
wall-loss Q, an optical resonator, acoustic horns, a radiation impedance of
a piston, electromagnetic periodic cells. There is no OpenCASCADE or CAD
import in Resonant (geometry import is an open V1 item, RES-PRODUCT-015).

## Decisions

1. **A functional module without physics.** `src/core/functional` holds the
   structure document, parameters, fingerprints and staleness, the analysis
   registry interface, studies (analyze, sweep, sensitivity, mode
   tracking), objectives, constraints and explanations. It includes no
   physics domain and no coupling (layering rule); it may use the
   optimization framework's expression evaluator, scene bindings and
   content keys, which are domain-independent.
2. **Engineering geometry is the scene.** Parameters bind to scene objects
   through the optimization framework's scene bindings
   (`object/<id>/geometry/<key>`, `object/<id>/position/<axis>`); the geometry
   kinds' own rules validate changes. No second geometry engine. CAD import
   is out of scope (no CAD kernel exists); a structure works on any scene
   object, imported or not, once import exists.
3. **Fingerprints, not counters.** A result's dependencies are content keys
   (FNV-1a of the canonical JSON) of the referenced objects' geometry and
   placement, the referenced materials, the structure definition and the
   discretization settings. Comparing keys detects staleness across save and
   load, undo and other clients without a revision counter in the scene.
4. **Analyses in the domains, registered in coupling.** New physics goes to
   its domain: `electromagnetics/CavityResonator`, `OpticalResonator`,
   `PeriodicLayers`, `manipulators/Horn` (with a `horn.acoustic` geometry
   kind); the mechanical resonator and the periodic rod reuse the mechanics
   studies; the coupled slab reuses the coupled engine. `coupling/FunctionalAnalyses`
   adapts them to the registry.
5. **Candidates through the optimization framework.** An adapter
   `functional` exposes a structure's parameters as variables and its
   analysis metrics and constraint values as quantities; the existing
   algorithms generate candidates. Sweeps are evaluated directly (each point
   on its own copy of the project). Explanations are computed from the
   evaluated candidates, never written by hand.
6. **Models honest about fidelity.** The cavity's analytical model (closed
   form, perturbational wall loss) and its full-wave model (Yee, PEC walls,
   lossy fill) are distinct models of one analysis; the optical resonator is
   paraxial and says so; the horn is a plane-wave (Webster) model with a
   baffled-piston mouth. Each result names its model.
7. **Services and contract.** `FunctionalStructureService` in the services
   and `proto/resonant/v1/functional`; structures travel as their JSON
   document, results as documents with provenance. Analyses, sweeps and
   sensitivities are jobs; optimization uses the existing optimization job.
8. **A panel, not a new application.** A dock panel and a controller in
   `src/app` over the same services.

## Consequences

- Geometry kinds gain `horn.acoustic`; projects without functional
  structures are unchanged.
- No CAD import, no topology optimization of functional structures, no
  general 3D acoustic or optical full-wave model: these are named in the
  roadmap of the architecture document.
- Force and momentum metrics are not computed (no momentum accounting yet,
  REQ-CHAT-20261126-022); analyses declare the force category only when a
  model computes it.
- The examples' material values are illustrative and labelled.
