# Optimization Architecture

## Modules

```text
src/core/optimization         domain independent (a layering rule: no physics domain included)
  Expression                  arithmetic expressions over variable ids and adapter constants
  Design                      design variables, derived variables, design space, design points
  Objectives                  objectives, constraints, constraint results
  Evaluator                   the adapter interface, capabilities, model records, evaluations,
                              linear systems, the adapter registry, templates
  Problem                     the problem definition (versioned JSON)
  Candidate                   candidates, Pareto ranking, priorities, comparison
  Random                      the reproducible generator, Latin hypercubes, grids
  Engine                      evaluation service, cache, runs, run records, checkpoints, jobs
  Algorithms                  the methods and the recommendation
  Sensitivity / Surrogate / MultiFidelity / LinearInverse / Persistence / SceneBindings
src/core/acoustic_optimization  the acoustic adapter (acoustic domain: acoustics, manipulators, fdtd)
src/core/mechanics/MechanicalOptimization, TopologyOptimization
src/core/electromagnetics/EmOptimization
src/core/geometry/MeshChecks  mesh integrity, printability, STL / OBJ
src/core/coupling/OptimizationRegistry   AdapterRegistry::instance(): registers every adapter
src/app/OptimizationController, OptimizationPanel (forms/OptimizationPanel.ui)
```

## The adapter boundary

A `DesignEvaluator` is an adapter bound to one design snapshot and its
configuration. It offers:

* `capabilities()` — variables (bindings with suggested bounds), quantities
  (key, unit, definition, models), models (id, version, fidelity,
  assumptions, availability, typical cost), constants for expressions,
  limitations, thread safety, whether it has exact gradients (none does);
* `evaluate(bindings, model)` — rebuild the design, apply the bindings, check
  it, run the model, return quantities, the model record, notes, warnings and
  compact details; statuses: ok, invalid design, refused, failed, cancelled;
* `apply(bindings, project)` — write a design into a project (promotion,
  export);
* `linearSystem(bindings, model)` — the source-to-target response matrix,
  when the configuration has one.

The core interprets nothing physically: an objective names a quantity key,
the adapter computes it. A problem is checked against the capabilities
before it runs (unknown bindings, quantities and models are refused).

## Snapshots and caching

`designSnapshot(project)` serializes the project without its optimization
data. Every evaluation reconstructs an independent project from it, so
evaluations run concurrently without touching the document and the
document never changes during a run. The snapshot key hashes the snapshot
without volatile fields (modification time) plus the adapter's configuration
and version; the cache key adds the model and every applied value (17
significant digits). An evaluation is reused only when all of that is equal.

## Runs

`runOptimization(problem, evaluator)` validates the problem, creates an
`EvaluationService` and runs the chosen `Algorithm`. The service:

* maps unit-cube points to designs (quantization, locks, derived values);
* evaluates batches on up to `parallel` worker threads (default half the
  hardware threads; one when the adapter is not thread safe);
* commits each candidate in submission order as soon as it and its
  predecessors are done (live inspection while large batches run);
* scores objectives and constraints and decides feasibility;
* counts every requested design against the budget (cached ones too, so a
  replay takes the same decisions), and stops at the budget, the time limit
  or a cancellation; a design whose evaluation was cancelled is not stored.

The run record keeps the problem as run, the method and settings, the
snapshot key, start and end time, the termination reason, every candidate,
physics evaluations and cache hits, notes, the Pareto front and - for one
objective only - the best feasible candidate.

## Jobs, checkpoints and restart

`OptimizationJob` runs a problem on a `JobManager` worker with progress, live
candidates and the latest checkpoint. A checkpoint is the run record so far
with the evaluation cache (`complete: false`), written every
`checkpointEvery` candidates (10 in the Studio) and at the end, to
`<project>.optimization/<run>.checkpoint.json` next to a saved project and
kept in memory otherwise. Restart: `resumeFromCheckpoint` reloads the cache,
the deterministic method runs again from its seed, the recorded candidates
come out of the cache (marked `replayed`, no physics) and the run continues;
the finished result equals that of an uninterrupted run. The Studio runs one
optimization job at a time.

## Storage and exchange

The project's extensions hold `"optimization": {version 1, problems, runs,
revisions}`. Formats (versioned JSON, readable by any tool; the C++ core
needs no Python):

* `resonant.optimization.problem` v1 — a problem;
* `resonant.optimization.run` v1 — a run record, also the checkpoint format;
* `resonant.optimization.candidate` v1 — an exported candidate with its meshes;
* CSV of the candidates (variables, objectives, constraint violations).

`examples/optimization/read_run.py` reads a run with the standard library;
Python bindings are the subject of REQ-CHAT-20261126-010.

## Promotion

`OptimizationController::promote` builds the candidate's design on a copy of
the present design and pushes one composite, undoable command (object states,
settings, the material library, and a revision record: candidate, run,
problem, model, fidelity, validated or not, applied values, replaced values,
lineage). A candidate evaluated on a different snapshot is refused unless the
user accepts applying its values to the present design.
