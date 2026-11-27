---
id: opt.multi-fidelity
title: Multi-fidelity optimization
category: Optimization
order: 100
status: available
summary: Screen designs with cheap models, re-evaluate the promising ones with more complete physics, and never mix the two; surrogate models as labelled predictions.
keywords: [multi-fidelity, model, validation, re-evaluation, surrogate, fidelity]
related: [opt.studio, opt.algorithms, sim.solver-selection, opt.robust]
---

Cheap models screen designs; more complete models check the promising ones.
Every candidate records exactly which model judged it, re-evaluations are new
candidates linked to the screened ones, and values of different models are
shown side by side, **never combined, averaged or ranked together**.

## Fidelity levels

| Level | Meaning | Models |
|---|---|---|
| analytical | closed form, idealized | acoustic free field; array factor; Euler-Bernoulli and Kirchhoff plate formulas; induced-EMF antennas |
| reduced | approximate numerical or more complete analytical | Kirchhoff reflector integration; coarse finite elements (element size × 2); exact dipole fields at a finite distance |
| high-fidelity | full-wave or refined numerical | acoustic FDTD on the configured region; the finite-element study as configured |
| experimental | calibrated measurement | not an optimization model yet |

The levels describe how complete the physics is, not a guarantee of
accuracy; each model's assumptions are recorded with every candidate and
shown in its details (*Assumes: ...*).

## In the Studio

- **Physical model** (Problem tab) judges every candidate of a run. Its name
  and fidelity are shown on the Run tab and in the candidate table's
  **Model** column. A model that is unavailable for the project (for example
  the analytical mechanical model for a shape it does not cover) is marked
  *(unavailable)* and refused with its reason.
- **Validation model** is the higher-fidelity model for re-evaluation; one
  that is not of higher fidelity is noted in the run record.
- On the **Candidates** tab select candidates, choose a model next to
  **Re-evaluate with Model** and press it. A new run record holds the new
  candidates (origin *re-evaluation with model*, parent: the screened
  candidate); the original candidates are unchanged. The text reads
  *Re-evaluated with model (fidelity) - values side by side, never
  combined:* and lists `screened -> validated: value -> value (feasible)` or
  *(NOT feasible)* per candidate, then notes, including how well the cheap
  model ranked the designs (the Spearman correlation of the first
  objective's scores).

A design is **validated** only when it was evaluated with the problem's
validation model or a model of high fidelity or above. A promoted revision
records whether it was; otherwise its note says *NOT validated: evaluated
with 'model' only*.

## Workflow

1. Run the problem with a cheap model (templates default to analytical or
   reduced models).
2. Select the best feasible candidates (the Pareto front first for several
   objectives).
3. Re-evaluate them with the validation model.
4. Promote a validated candidate.

## Worked examples

- `examples/optimization/d-mechanical-resonator.resonant`: screened with the
  Euler-Bernoulli formulas (`mechanical.analytical`), validated with finite
  elements (`mechanical.fe`). The best candidate is within 0.25 Hz of 25 Hz
  on the analytical model and within 0.5 Hz (2 %) on the finite elements.
- Acoustic: a phase focus found with the free field, re-evaluated with the
  full-wave solver on a plane region (a 2D problem of line sources), still
  concentrates more pressure at the focus than the unfocused design. Its
  absolute values are not comparable with the 3D model, and the report says
  so.
- Electromagnetic: the array factor and the exact dipole fields 100
  wavelengths away agree within 0.3 dB in directivity; neither includes mutual
  coupling.

## Surrogate models

On the **Analysis** tab, *Surrogate model of an objective* fits a response
surface to the evaluated candidates of the selected run: choose the kind
(`quadratic`, `rbf` or `gaussian-process`) and the objective, then **Fit**.
The text gives the training points, the training RMSE and the leave-one-out
RMSE (the validation error). A surrogate prediction is **not a simulation
result**: it never becomes a candidate's evaluation. The Gaussian process
also gives an uncertainty, which the Bayesian method uses (see
[Algorithms](algorithms.md)).

## Limitations

- The experimental level (measurements) is not an optimization model yet.
- Surrogates use one isotropic length scale and suit hundreds, not tens of
  thousands, of points.
- No model here includes mutual coupling between array elements.

## Related topics

[The Optimization Studio](studio.md) · [Algorithms](algorithms.md) ·
[Choosing a solver](../simulation/solver-selection.md) · [Robust optimization](robust.md)
