---
id: opt.robust
title: Robust optimization
category: Optimization
order: 90
status: available
summary: Optimize for designs that keep working under stated variations of their parameters, with common random samples, mean, mean plus k sigma or worst-case measures.
keywords: [robust, tolerance, uncertainty, worst case, sampling, variation]
related: [opt.constraints, opt.objectives, opt.algorithms, exp.uncertainty]
---

A design that is best at its nominal values can fail under the variations of
reality: a frequency band instead of one frequency, a manufacturing tolerance,
a material spread. A **robust** problem names the uncertain parameters and
how they vary, evaluates every design under the same perturbations, and
scores it by a robust measure of the results.

## The settings

On the **Problem** tab, group **Robust optimization**:

| Field | Meaning |
|---|---|
| *Evaluate every design under the uncertain parameters below* | turns robust evaluation on |
| Samples per design | perturbed evaluations per design (16 by default) |
| Measure | **Mean**, **Mean + k sigma**, or **Worst case** of the objective's score |
| Parameter | any non-categorical binding the adapter offers, a design variable or not (choose it in the list, **Add Parameter**; **Remove**) |
| Distribution | *uniform (+/- width)*: a tolerance band; *normal (sigma = width)*: clipped at 3 σ |
| Width | the half-width or standard deviation |
| Relative | the width as a fraction of the design value (or of the project's value for a binding that is not a variable) |
| Source (required) | where the distribution comes from: a tolerance, a data sheet, a measurement, a stated requirement |

**Resonant never assumes a distribution**: a parameter without a source, a
width that is not positive, no parameter or fewer than two samples are
refused. The factor k (1 by default), the share of samples in which hard
constraints must hold (1: all of them) and the sampling seed are kept in the
problem; the Studio neither shows nor edits them in this version (templates,
example projects and the problem file set them).

## How designs are evaluated

- The samples form a Latin hypercube in probability, mapped through each
  distribution, generated once from the robust seed: **common random
  numbers**. Every design is judged under the same perturbations, so a
  difference between two designs is not sampling noise.
- Each design is evaluated nominally and at every sample; the perturbation is
  added to the design value of the binding.
- The candidate's objective values are the nominal ones; its scores are the
  robust measure of the samples' scores. Its robust statistics hold, per
  objective, the mean, standard deviation, minimum, maximum and worst score,
  and per constraint the share of samples meeting it. A hard constraint met in
  fewer than the required share makes the candidate infeasible, with the
  worst value as its violation.

```math
uniform ±w:   σ = w / √3
mean + kσ:    score = mean(score_s) + k · std(score_s)
cost:         (1 + samples) evaluations per design
```

On the Candidates tab a robustly evaluated candidate is marked *(robust)*;
its details show *Robust (N samples, measure)* with mean, standard deviation,
minimum and maximum. A comparison says when one of two candidates is robust
and the other not.

## Worked example

Open `examples/optimization/g-robust-design.resonant`. Phase steering set for
1 GHz squints across a 0.9-1.1 GHz band. The problem *Robust taper for a
0.9 - 1.1 GHz band* maximizes the **worst** directivity toward 30° over the
band by a symmetric amplitude taper: one uncertain parameter, the array's
operating frequency, uniform, relative width 0.1 (±10 %), source *the
example's operating band (a stated requirement, not a measured distribution)*,
8 samples, measure worst case, differential evolution with 400 evaluations.
Compared with the nominal optimum (directivity at 1 GHz only), the robust
design is at least as good in the worst case over the band, and the nominal
design at least as good at 1 GHz: each is best in its own measure.

In the automated tests a uniform ±0.3 tolerance on a parameter `p` of the
quantity `x + p` gives the nominal mean, a standard deviation of 0.3 / √3 and
a maximum of nominal + 0.3; a constraint placed at the nominal value is met in
half the samples, so the design is feasible nominally and infeasible
robustly.

## Limitations

- Independent parameters only (no correlations); uniform and normal
  distributions only.
- The worst case is the worst of the samples, not a guaranteed bound.
- Each design costs 1 + samples evaluations; prefer cheap models (see
  [Multi-fidelity optimization](multi-fidelity.md)).

## Related topics

[Constraints](constraints.md) · [Objectives](objectives.md) ·
[Algorithms](algorithms.md) · [Uncertainty](../experiments/uncertainty.md)
