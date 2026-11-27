# ADR-000214: Optimization framework — a domain-independent core with adapters, own algorithms on the unit cube, snapshot evaluation, replayable checkpoints, never-merged fidelities, promotion as an undoable revision

**Status:** ACCEPTED
**Date:** 2026-11-27
**Type:** architecture
**Requirements:** `docs/requirements/optimization.md` RES-OPT-001..025
**Request:** REQ-CHAT-20261126-009
**Builds on:** ADR-000205 (layering), ADR-000208 (minimal geometry optimization), ADR-000209 .. ADR-000213

## Context

Resonant had to gain optimization and inverse design across its acoustic,
mechanical and electromagnetic domains: problems users describe by what they
want, what may change and what must hold; algorithms for expensive
simulations; candidates kept with their provenance; several model fidelities;
robust design; and never a silent change to the user's design. Earlier
milestones had local pieces: SVD inverse design for acoustic point targets,
a coordinate search over one reflector parameter, vibration targeting, and
beam synthesis for arrays.

## Decisions

1. **`src/core/optimization`, domain independent.** Problems, variables,
   objectives, constraints, algorithms, runs, candidates, comparison,
   sensitivity, surrogates, robust and multi-fidelity evaluation, the generic
   linear inverse solver and persistence know quantities only by key and
   unit. A layering rule forbids domain includes. Domains implement
   `DesignEvaluator` adapters (`acoustic_optimization/`,
   `mechanics/MechanicalOptimization`, `electromagnetics/EmOptimization`); the
   registry is defined in `src/core/coupling/OptimizationRegistry.cpp`, the
   one place that may name several domains. The existing domain solvers
   (acoustic inverse design, vibration targeting, beam synthesis, the
   Manipulators panel's coordinate search) stay; the framework reproduces their
   answers in tests.
2. **Own algorithms, no optimization library.** NLopt (LGPL), pagmo2
   (LGPL/GPL), Ceres (heavy, gradient based) and dlib were considered. The
   methods Resonant needs for expensive, constrained, partly discrete
   problems are a few hundred lines each, and owning them gives one
   deterministic random generator, one constraint treatment and no new
   dependency or licence condition. The `Algorithm` interface admits a library
   method later. SQP, CMA-ES, Powell and adjoint gradients are not
   implemented.
3. **The unit cube.** Algorithms see the free variables mapped into
   [0, 1]^n; integers, steps and categories are quantized when the design is
   built. Scalar methods minimize a merit (weighted score + soft penalty +
   an exterior penalty for hard violations; infinity for a design that could
   not be evaluated); population methods use constraint domination.
   Feasibility is decided by hard constraints only, after the fact, so a
   method's penalty never makes an infeasible design feasible.
4. **Snapshot evaluation.** An adapter is bound to a serialized project (the
   design snapshot, without optimization data) and its configuration; every
   evaluation reconstructs an independent project, applies the bindings and
   runs the model. The dependency key is a hash of the snapshot without
   volatile fields plus the configuration; the cache key adds the model and
   every applied value. Concurrency is therefore safe without locking the
   document, and caching is exact.
5. **Determinism and checkpoints by replay.** xoshiro256** with written-out
   uniform and normal conversions; candidates are committed in submission
   order as soon as they and their predecessors are done. A checkpoint is a
   run record with the evaluation cache; restarting reruns the deterministic
   method from its seed, the cache replays the recorded candidates without
   physics, and the result equals the uninterrupted run (except with a time
   limit, which is not deterministic).
6. **Fidelity levels, never merged.** Analytical, reduced, high-fidelity,
   experimental; every candidate records its model. Re-evaluation creates new
   candidates linked to their parents; Pareto fronts and comparisons are per
   model. "Validated" means evaluated with the problem's validation model or a
   high-fidelity one.
7. **Robust optimization with common random numbers.** Latin-hypercube
   stratified samples of the user's distributions, the same for every
   design; a source statement is required for every distribution.
8. **One best candidate only for one objective.** Several objectives give a
   Pareto front and priorities chosen afterwards; a weighted sum only when
   the user picks a scalar method.
9. **Promotion as one undoable revision.** Applying a candidate builds the
   changed design on a copy, then pushes a composite command (object states,
   settings, materials, and a revision record with lineage, validation state
   and replaced values). A candidate evaluated on another snapshot needs the
   user's consent.
10. **Topology optimization as a proof of concept in mechanics.** SIMP with
    a density filter and optimality criteria (the formulation of Andreassen et
    al. 2011) on its own Q4 plane-stress model, labelled as a numerical
    material distribution.
11. **Pattern metrics by descent.** The electromagnetic main lobe is the set
    of samples reachable from the maximum without climbing; a fan beam is one
    lobe, a grating lobe is not part of it.
12. **Python later, interfaces now.** Versioned JSON problems and run records
    and CSV candidates are the exchange format; the bindings belong to
    REQ-CHAT-20261126-010.
13. **Storage.** Problems, run records and revisions in the project's
    extensions ("optimization", version 1); checkpoints next to the project
    file (`<name>.optimization/<run>.checkpoint.json`), in memory for an
    unsaved project.

## Consequences

* Every new domain capability becomes optimizable by adding bindings and
  quantities to its adapter; the framework does not change.
* Re-running a whole design per evaluation costs a project reconstruction
  (milliseconds for the examples); a model whose setup is expensive pays it
  per candidate.
* The analytical and reduced models make optimization fast; claims about
  real performance need the validation model, which the user interface shows.

## Deviations from the request

* SQP, CMA-ES, Powell, adjoint and automatic-differentiation gradients are not
  implemented (the request asked not to implement every algorithm); the
  interfaces admit them.
* Topology optimization is a 2D plane-stress compliance proof of concept,
  not acoustic or electromagnetic topology optimization.
* Lenses and waveguides have no analytical field model; only the full-wave
  model evaluates them.
* STEP export is not implemented (no CAD kernel); STL and OBJ are.
* The electromagnetic higher-fidelity array model includes the exact element
  fields at a finite distance but not mutual coupling; the Yee solver does
  not represent arrays.
* Python bindings are deferred to REQ-CHAT-20261126-010; the exchange format
  and a reader script exist.
