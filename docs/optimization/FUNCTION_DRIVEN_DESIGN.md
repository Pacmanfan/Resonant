# Function-Driven Design

**Status:** implemented (RES-FUNC-011, RES-FUNC-012, ADR-000221):
`functional/FunctionalStudy`, `coupling/FunctionalAdapter`, the optimization framework.

A goal becomes a measurable objective of a structure's analysis:

| Field | Meaning |
|---|---|
| `metric` | a metric key of the analysis (`cavity.f1`, `horn.meanTransmission`, `gap1.center`), with its unit |
| `sense` | `minimize`, `maximize`, `target` (met within 1 %), `within` (`low` … `high`) |
| `target`, `low`, `high`, `weight` | in the metric's unit; weights ≥ 0 |

The frequency range, excitation and environment of the structure are the
operating conditions; the analysis's model and its validity are the model
dependencies, reported with every candidate.

## Constraints and manufacturability

Constraints are expressions of parameter ids and metric keys
(`wall >= 0.002`, `[coupled.peakTemperature] <= 400`) with a relation and a
limit, a category (`manufacturing`, `physical`, `design`), an optional process
(`cnc`, `sheet`, `additive`, `injection-molding`, `lamination`, `composite`) and
hard (rejects) or soft (penalizes). The process is a label tying the limit
to how the part is made; no process is simulated, and no default limits are
assumed for a process (the user states them).

## Candidates

| Source | How | Ranking |
|---|---|---|
| sweeps | grids over parameters (`SweepStructure`), every point on its own copy of the design | feasible points by the weighted objectives: target and range deviations; minimize and maximize scaled over the swept points |
| the optimization framework | `OptimizeStructure`: an optimization problem built from the structure (variables = settable parameters; objectives; constraints as quantities `constraint.<id>`) run by the existing algorithms (Nelder–Mead for one objective, NSGA-II for several by default) | the framework's scores, feasibility and Pareto front; the run is recorded in the optimization workspace |
| sensitivities | `ComputeStructureSensitivity`: central differences, normalized (d ln m / d ln p) | — |

No new optimizer was written (RES-OPT, ADR-000214).

## Explanations

`ExplainCandidates` (and every sweep's best point) explains a candidate
against the present design: the parameters changed (from, to, relative
change), the materials by region, the operating conditions, the model, the
objectives improved and worsened, other metrics that changed by more than
5 % (side effects: heating that grows with concentration), the active or
violated constraints with their margin, the physical mechanism (from the
analysis), and the validity limits and assumptions. Every value was computed
by the named model for that candidate's own geometry; none is extrapolated.

```text
 objectives + constraints ──► OptimizeStructure ──► optimization job (adapter "functional")
                                                       │ each candidate: copy of the design ─► parameters ─► analysis
                                                       ▼
                                  run record (optimization workspace) ──► ExplainCandidates ──► explanations
                                                       │
                     promote: SetStructureParameters with the candidate's values (one undoable step)
```

## Limits

Gradients come from finite differences (no adjoints); topology optimization
of structures is not offered; candidates are not re-validated by a
higher-fidelity model automatically (run the full-wave model of the cavity
on a candidate to check it).
