# Coupled Simulation Execution

**Status:** implemented for steady schemes (RES-CEME-006, RES-CEME-007,
RES-CEME-014, ADR-000220): `multiphysics/Orchestrator`, `coupling/CoupledRun`,
`services/MultiphysicsServices`.

## The execution graph

The order of a pass comes from the field transfers of the coupling graph
(topological order; ties keep the system's domain order). State and geometry
updates are feedback: they act on the next participants to prepare. A cycle
of field transfers is refused (one direction of a loop must be a state or
geometry update).

```text
microwave-heated slab (examples/multiphysics):
   em ──absorbed power──► heat ──temperature──► solid
    ▲                       │                     │
    └── loss tangent (state)┘                     │
    └──────────── deformed thickness (geometry) ──┘
```

## A pass

For each participant in order: `prepare` (rebuild if the geometry or
material revision it used changed: Gauss–Seidel within the pass), `solve`,
then every coupling that starts at it. After the pass: the largest
temperature change of the state updates (K), the largest relative geometry
change, the energy transferred, and each participant's monitored values are
recorded.

## Schemes

| Scheme | Passes | Ends |
|---|---|---|
| one-way | 1 | completed (no feedback allowed) |
| sequential | `passes` | completed |
| iterative | up to `maxIterations` | converged when a pass's temperature change ≤ `temperatureTolerance` (K) and geometry change ≤ `geometryTolerance`; otherwise not-converged, with the history |

`relaxation` (0, 1] under-relaxes state and geometry updates (a slower but
more robust fixed-point iteration for strong feedback; the reference test
converges to the same answer with 0.5).

## Failures

| Event | Result |
|---|---|
| a participant's prepare or solve fails | status failed, `failedStage` ("solve heat (pass 3)"), the completed passes kept |
| a value that is not finite in an exchanged field | failed: "invalid values, not passed on" |
| a transfer or a state/geometry update is refused | failed, with the reason |
| cancellation (between stages, and inside a transient conduction solve) | cancelled |
| no convergence | not-converged (a result, never shown as converged) |

The job of a failed or cancelled run ends failed or cancelled; its partial
result is still stored and readable. Memory exhaustion surfaces as a failure
of the solver that ran out (the job records the exception).

## Jobs, results and provenance

`StartCoupledSimulation` validates the system, then runs it as one job of
the unified job system (progress per stage, cancellable through
`CancelJob`); clients poll or wait (`WaitForJob`) instead of holding a call
open. When it ends, the result document (`resonant.coupled-result`) is
registered and named in the job's results. Its provenance holds the system
document and its checksum, the scheme, the final material and geometry
revisions, each participant's solver and version (and whether its solution is
stale against the final revisions), every transfer's operator, the ledger and
audit, warnings (extrapolated laws, stale participants), and the application
version. The temperature history of the state updates is kept (at most 20
snapshots).

## Resources

The layered reference systems are small (tens of cells, milliseconds per
pass). Resource estimates per participant, checkpoint resume and parallel
execution of independent participants are planned.
