# Multi-Fidelity Optimization

Cheap models screen designs; more complete models check the promising ones.

| Level | Meaning | Models |
|---|---|---|
| 1 analytical | closed form, idealized | acoustic free field; array factor; Euler-Bernoulli / Kirchhoff formulas; induced-EMF antennas |
| 2 reduced | approximate numerical or more complete analytical | Kirchhoff reflector integration; coarse finite elements (element size x 2); exact dipole fields at a finite distance (no coupling) |
| 3 high fidelity | full-wave or refined numerical | acoustic FDTD on the configured region; the finite-element study as configured |
| 4 experimental | calibrated measurement | (the experiment framework; not an optimization model yet) |

The levels describe how complete the physics is, not a guarantee of
accuracy; each model's assumptions are recorded with every candidate.

## Rules

* Every candidate records the model (id, name, version, fidelity,
  assumptions, the solver settings that matter, adapter and snapshot).
* Re-evaluation creates new candidates with the screened candidate as their
  parent and the origin "re-evaluation with <model>"; the original candidate
  is unchanged.
* Values of different models are shown side by side (the comparison and the
  re-evaluation report) and never combined, averaged or ranked together:
  Pareto fronts and the best candidate are per model.
* A design is **validated** only when evaluated with the problem's
  validation model or a model of high fidelity or above; a promoted revision
  records whether it was.
* The report states, per objective, both values and their difference, which
  screened-feasible candidates became infeasible, and how well the cheap
  model ranked the designs (Spearman correlation of the first objective's
  scores).

## Workflow

1. Run the problem with a cheap model (template defaults: analytical or
   reduced).
2. `selectForValidation` picks the best feasible candidates (the Pareto front
   first for several objectives).
3. Re-evaluate them with the validation model (Candidates tab, "Re-evaluate
   with Model"; a new run record holds the results).
4. Promote a validated candidate.

## Examples

* Acoustic: the phase focus found with the free field, re-evaluated with the
  2D full-wave solver, still concentrates more pressure at the focus than the
  unfocused design. The 2D run is a different problem (line sources), so its
  values are not comparable in absolute terms - the report says so
  (`AcousticOptimization.FullWaveReevaluationValidatesTheFocusedDesign`).
* Mechanical: the resonator tuned with the Euler-Bernoulli formulas is
  within 2 % of its target with the finite elements (example D).
* Electromagnetic: the array factor and the exact dipole fields 100
  wavelengths away agree within 0.3 dB in directivity; neither includes
  mutual coupling.
