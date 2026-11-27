# WORK-000012: Optimization, inverse design, geometry optimization and automated engineering

**Status:** completed
**Type:** feature
**Request:** requests/processing/2026-11-26-prompt-13-resonant-advanced-op-req-chat-20261126-009.md
**Requirements:** RES-OPT-001, RES-OPT-002, RES-OPT-003, RES-OPT-004, RES-OPT-005, RES-OPT-006, RES-OPT-007, RES-OPT-008, RES-OPT-009, RES-OPT-010, RES-OPT-011, RES-OPT-012, RES-OPT-013, RES-OPT-014, RES-OPT-015, RES-OPT-016, RES-OPT-017, RES-OPT-018, RES-OPT-019, RES-OPT-020, RES-OPT-021, RES-OPT-022, RES-OPT-023, RES-OPT-024, RES-OPT-025
**Decision:** ADR-000214
**Evidence:** VER-000012
**Test-Run:** 20261127T122051-2851810
**Test-Suites:** core, app, cmake, layering, audits

## Scope

Optimization, inverse design, geometry optimization and automated engineering (Prompt 13, milestone M8): a physics-independent
optimization framework with acoustic, mechanical and electromagnetic adapters, the Optimization Studio and seven example projects.

## Implementation

- `src/core/optimization/` (new, domain independent): Expression (derived variables, relational constraints), Design (variables,
  derived variables, design space, unit cube, bindings), Objectives (objectives, constraints, violations), Evaluator (adapter
  interface, capabilities, model records, evaluations, linear systems, registry, templates), Problem (versioned JSON), Candidate
  (candidates, Pareto ranking, priorities, comparison), Random (xoshiro256**, Latin hypercube, grids), Engine (evaluation service
  with exact caching, parallel evaluation with in-order commits, robust sampling, run records, checkpoints and replayed restart,
  OptimizationJob), Algorithms (sweep, random, Latin hypercube, Nelder-Mead, L-BFGS-B with checked finite differences, differential
  evolution, NSGA-II, Bayesian; recommendation), Sensitivity, Surrogate (quadratic, RBF, Gaussian process, expected improvement,
  active learning), MultiFidelity (re-evaluation, validation), Robust (robustness of one design), LinearInverse (Tikhonov, power
  penalty, minimum norm, bounded weights by FISTA, sparse by OMP, classification), Persistence (project extensions, snapshots,
  dependency documents, CSV), SceneBindings (generic bindings, snapshot adapter base).
- Adapters: `src/core/acoustic_optimization/AcousticOptimization` (free field, Kirchhoff reflectors, full-wave FDTD; focus, target,
  region, focal-line, source and manipulator quantities; non-point targets sampled; linear system; four templates),
  `mechanics/MechanicalOptimization` (Euler-Bernoulli / Kirchhoff analytical, coarse and standard finite elements; modal, harmonic,
  static quantities; hypothetical materials labelled; actuator linear system; two templates), `electromagnetics/EmOptimization`
  (array factor, exact dipole fields, induced-EMF antennas; pattern metrics with a descent-defined main lobe; directional linear
  system; four templates); registry in `coupling/OptimizationRegistry`.
- `mechanics/TopologyOptimization` (SIMP compliance proof of concept with filter, cleanup, extrusion); `geometry/MeshChecks`
  (integrity, overhang, printability, STL / OBJ).
- App: OptimizationController (problems, background runs, live candidates, checkpoints next to the project, resume, re-evaluation,
  sensitivity, inverse design, surrogates, promotion as one undoable composite command with a revision record, export, topology job),
  OptimizationPanel (`forms/OptimizationPanel.ui`, eight tabs), Optimization menu and dock in MainWindow(.ui), `--run-optimization`.
- `tools/check_layering.py`: the optimization framework includes no physics domain; `acoustic_optimization` is part of the acoustic domain.
- Examples: `examples/optimization/` A-G and `read_run.py` (standard-library reader of exported runs).

## Tests

- New: `tests/core/test_optimization.cpp` (32: design space, expressions, problems, algorithms against known optima, constraints,
  caching, parallel determinism, cancellation, checkpoint/restart through a file, provenance, linear inverse against closed forms,
  surrogates, sensitivity, robust statistics against known distributions, multi-fidelity, persistence, mesh checks, topology),
  `test_optimization_domains.cpp` (15: phase focus against the coherent sum, inverse design against the acoustic and mechanical
  solvers, reflector against the mirror equation, invalid geometry, full-wave re-evaluation, beam against Euler-Bernoulli, steering
  against phase steering, Dolph-Chebyshev sidelobes, nulls, dipole resonance, exact fields against the array factor, capabilities),
  `test_optimization_examples.cpp` (7 examples run with the physics), `tests/app/test_optimization_ui.cpp` (6 GUI tests),
  shared fixtures `tests/core/OptimizationFixtures.h`.
- t2 run 20261127T122051-2851810: PASS (core 349 cases, app 87 cases, CMake build and ctest, layering, drift, doc integrity,
  invariants) on the final code. Earlier t2 runs 20261127T121027-2794331 and 20261127T121503-2819155 also passed; after them the
  panel rebinds the adapter on every editor refresh, the checkpoint directory is created only when a run writes into it (a
  query had created it), and the GUI tests save examples into a temporary directory so checkpoints never land in the source tree. `scripts.tests.test_root_readme` passed (README at 80 lines). The Studio was also driven
  headless: `resonant examples/optimization/f-sidelobe-reduction.resonant --run-optimization first --screenshot` ran 1600
  candidates and showed them in the candidate browser.
- Bugs found by the new tests and fixed: names with '/' swallowed divisions in expressions (now bracketed ids); candidates were
  committed only after whole batches (no live inspection or prompt cancellation of large batches); three dangling iterations over
  temporary JSON (`value(...).items()`) and dangling pointers into temporary workspaces; the L-BFGS first step jumped amplitudes to
  a saddle at zero; the EM sidelobe metric failed for fan beams (main lobe redefined by descent); robust perturbations of bindings
  outside the design were added to zero instead of the project's value; the analytical beam had one bending plane only; the
  `replayed` flag was not serialized; a multi-objective run named a single best candidate.

## Regression impact and documentation

- Projects without optimization data load and save unchanged (extensions.optimization only when present); no schema change.
  Existing domain solvers (acoustic inverse design, vibration targeting, beam synthesis, the Manipulators panel's coordinate
  search) are unchanged and reproduced by the framework in tests.
- Documentation: `docs/requirements/optimization.md` (RES-OPT-001..025), ADR-000214, 14 documents in `docs/optimization/`
  (index, architecture, design variables, objective functions with every quantity, constraints, algorithms, inverse design,
  geometry, topology, multi-fidelity, surrogates, robust, adapters, validation), manifests, IMPLEMENTATION_STATUS.md (new section),
  DECISIONS.md, docs/README.md, docs/architecture/overview.md, examples/README.md, README.md.
- Honest limits recorded: SQP, CMA-ES, Powell and exact gradients not implemented; topology is a 2D compliance proof of concept;
  lenses and waveguides only in the full-wave model; no mutual coupling in array models; STEP not implemented; Python bindings in
  REQ-CHAT-20261126-010.

## Traceability

REQ-CHAT-20261126-009 -> RES-OPT-001, RES-OPT-002, RES-OPT-003, RES-OPT-004, RES-OPT-005, RES-OPT-006, RES-OPT-007, RES-OPT-008, RES-OPT-009, RES-OPT-010, RES-OPT-011, RES-OPT-012, RES-OPT-013, RES-OPT-014, RES-OPT-015, RES-OPT-016, RES-OPT-017, RES-OPT-018, RES-OPT-019, RES-OPT-020, RES-OPT-021, RES-OPT-022, RES-OPT-023, RES-OPT-024, RES-OPT-025 -> WORK-000012 -> evidence (pending)

## Transitions

- 2026-11-27T17:06:57.693664+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-27T17:19:02.166959+00:00 active -> completed by agent-dev-20261126151713-76f632
