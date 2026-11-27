---
id: opt.constraints
title: Constraints
category: Optimization
order: 40
status: available
summary: What must hold: hard and soft constraints on quantities and expressions, violations, feasibility, and how each method uses them.
keywords: [constraint, feasibility, violation, penalty, hard, soft, manufacturing]
related: [opt.objectives, opt.variables, opt.algorithms, opt.robust]
---

A constraint bounds a quantity of the physics adapter, or an expression over
design values, with a relation and a limit. **Hard** constraints decide
whether a candidate is feasible; **soft** constraints only add a penalty to
the score. Hard constraints are never relaxed: a method's penalty steers the
search, but the feasibility decision is separate and exact.

## A constraint

| Part | Meaning |
|---|---|
| quantity | an adapter quantity (`mass`, `focus.sidelobeDb`, `sources.clearance`, ...) or `expr:` followed by an expression over design values |
| relation | `<=`, `>=` or `=` (with a tolerance) |
| limit, unit | the bound, in the quantity's unit |
| hard or soft | soft ones carry a penalty and a scale |
| scale | normalizes the violation in penalties |
| category | geometric, physical, manufacturing or computational |

Expression constraints are evaluated on the design values **before any model
runs**; quantity constraints on the evaluation. Example:
`expr:[object/b/geometry/aperture] - 4 * [object/b/geometry/focalLength]`.

## Violation and feasibility

```math
<=:  violation = max(0, v − limit)
>=:  violation = max(0, limit − v)
=:   violation = max(0, |v − limit| − tolerance)
```

The violation is in the constraint's unit. A candidate is **feasible** when
its evaluation succeeded, every hard constraint has zero violation, and its
values respect their bounds and locks. A quantity the model could not provide
counts as a missed hard constraint; an invalid design is infeasible. The best
candidate and the Pareto front contain feasible candidates only.

A soft constraint adds `penalty · (violation / scale)²` to the weighted score.

## How methods use them

- Nelder-Mead, L-BFGS-B and the Bayesian method minimize the merit
  `weighted score + soft penalties + 1000 · (h + h²)`, where `h` is the sum of
  the hard violations, each divided by its scale: an exterior penalty that
  lets them approach the boundary. A design that could not be evaluated has
  an infinite merit.
- Differential evolution and NSGA-II use constraint domination: a feasible
  candidate beats an infeasible one, two infeasible ones compare by `h`, two
  feasible ones by score (or Pareto dominance).
- Sweeps and samplings evaluate everything; the results are filtered.

## The Objectives tab

The lower table, *Constraints (hard constraints decide feasibility; soft ones
add a penalty)*, has **Quantity or expr:...**, **Relation**, **Limit**,
**Hard**, **Category** and **Scale**. Choose a quantity in the list and press
**Add Constraint**; **Remove Constraint** deletes the selected one. On the
**Candidates** tab, the **Failed constraints** column of an infeasible
candidate lists each failed constraint with its value, the limit and the
amount it misses by.

The Studio adds constraints on adapter quantities. Expression constraints,
a tolerance for `=`, and a penalty other than 1 come from templates, example
projects or the problem file; the Studio shows them but does not edit them in
this version.

## Categories

| Category | Examples |
|---|---|
| geometric | spacing (`sources.clearance >= 0`), relations between parameters (`expr:`), symmetry (as derived variables) |
| physical | amplitude, power, stress, frequency limits (`sources.maxAmplitude`, `static.maxVonMises`, `pattern.levelDb/null <= -40`) |
| manufacturing | wall thickness, feature size, maximum dimension (`manipulators.minThickness`, `manipulators.maxExtent`) |
| computational | evaluation time and memory (the **Time limit** and **Memory per evaluation** settings; a model that refuses a design records it as refused) |

In a [robust optimization](robust.md) a hard constraint must hold in
at least the required share of the perturbed samples.

## Worked examples

`examples/optimization/d-mechanical-resonator.resonant` tunes a cantilever to
25 Hz with two hard constraints: *Mass* `mass <= 0.25` kg (scale 0.01 kg) and
*Thickness for the clamp*
`expr:[object/ex-opt-d-beam/geometry/height] >= 0.004` m (scale 0.5 mm).
Differential evolution (1200 evaluations, seed 4) finds a feasible design
within 0.25 Hz of the target with a thickness of at least 4 mm; candidates
below 4 mm are rejected before any model runs.

The template *Electromagnetic: keep the beam, place a null* maximizes the
directivity toward the target while the hard constraint
`pattern.levelDb/null <= -40` dB keeps the null direction 40 dB below the
main lobe.

## Common mistakes

- Using a soft constraint for a requirement: a soft constraint can be
  violated by the best candidate.
- A scale far larger than typical violations: the penalty then barely steers
  the search, and scalar methods wander in the infeasible region.
- Constraining a quantity the chosen model does not calculate: the problem
  is refused (for example stress with the analytical mechanical model).

## Related topics

[Objectives](objectives.md) · [Design variables](variables.md) ·
[Algorithms](algorithms.md) · [Robust optimization](robust.md)
