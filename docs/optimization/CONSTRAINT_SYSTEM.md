# Constraint System

A constraint bounds an adapter quantity or an expression over design values:

```text
quantity                      "focus.sidelobeDb", "mass", "sources.clearance", ...
expr:<expression>             "expr:[object/b/geometry/aperture] - 4 * [object/b/geometry/focalLength]"
relation                      <=  >=  =  (= with a tolerance)
limit, tolerance, unit
hard | soft (penalty, scale)
category                      geometric, physical, manufacturing, computational
```

Expression constraints are evaluated on the design values before any model
runs; quantity constraints on the evaluation.

## Violation and feasibility

The **violation** is how far a value misses its constraint, in the
constraint's unit: max(0, v - limit) for <=, max(0, limit - v) for >=,
max(0, \|v - limit\| - tolerance) for =. A candidate is **feasible** when its
evaluation succeeded, every hard constraint has zero violation and its values
respect their bounds and locks. An infeasible candidate lists each failed
constraint with its value, limit and the amount it misses by (the Candidates
tab, `Candidate::failedConstraints`). A quantity the model could not provide
counts as a missed hard constraint. Hard constraints are never relaxed: a
method's penalty steers the search, the feasibility decision is separate.

Soft constraints add penalty x (violation / scale)^2 to the weighted score.

## How methods use constraints

* scalar methods (Nelder-Mead, L-BFGS-B, Bayesian) minimize the merit
  `weighted score + soft penalty + 1000 (h + h^2)`, h = sum of hard
  violations / scale (an exterior penalty that lets them approach the
  boundary); a design that could not be evaluated has an infinite merit;
* population methods (differential evolution, NSGA-II) use constraint
  domination: a feasible candidate beats an infeasible one, two infeasible
  ones compare by h, two feasible ones by score (or Pareto dominance);
* samplings evaluate everything and the results are filtered.

The best candidate and the Pareto front contain feasible candidates only.

## Categories

| Category | Examples |
|---|---|
| geometric | minimum spacing (`sources.clearance >= 0`), non-overlap, symmetry (as derived variables), relations between parameters (`expr:`) |
| physical | amplitude, power, field-strength, stress, frequency limits (`sources.maxAmplitude`, `static.maxVonMises`, `pattern.levelDb/null <= -40`) |
| manufacturing | wall thickness, feature size, maximum dimension (`manipulators.minThickness`, `manipulators.maxExtent`, `expr:` on a thickness); mesh checks at export (`GEOMETRY_OPTIMIZATION.md`) |
| computational | evaluation time limit, memory budget per evaluation, mesh limits (problem settings; a model that refuses a design records it as refused) |

Robust optimization: a hard constraint must be met in at least the required
fraction of the perturbed samples (`ROBUST_OPTIMIZATION.md`).
