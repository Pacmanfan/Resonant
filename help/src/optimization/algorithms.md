---
id: opt.algorithms
title: Algorithms
category: Optimization
order: 50
status: available
summary: The optimization methods, their settings, assumptions and stopping rules, the Studio's suggestion, and the methods that are not implemented.
keywords: [algorithm, method, Nelder-Mead, L-BFGS-B, differential evolution, NSGA-II, Bayesian]
related: [opt.studio, opt.objectives, opt.constraints, opt.multi-fidelity]
---

Every method works on the free variables mapped to the unit cube and asks
Resonant's evaluation service for candidates; none depends on a physics
domain or on a third-party library. **The same seed gives the same
candidates on every platform.** Choose the method in **Method** on the
Problem tab; the text below the settings describes it, says when it stops,
and shows Resonant's suggestion for the problem (*Suggested: method -
reason*).

## The methods

| Method (in the list) | Id | Family | Several objectives | Discrete variables | Stops |
|---|---|---|---|---|---|
| Parameter sweep (grid) | sweep | sampling | yes | yes | grid complete or budget |
| Random sampling | random | sampling | yes | yes | samples or budget |
| Latin hypercube (design of experiments) | latin-hypercube | sampling | yes | yes | samples or budget |
| Nelder-Mead simplex | nelder-mead | derivative-free, local | weighted sum | no | simplex below 10⁻⁴ of every range, merits within tolerance, or budget |
| L-BFGS-B (finite-difference gradients) | lbfgs | gradient, local | weighted sum | no | projected step below tolerance, no decrease in the line search, or budget |
| Differential evolution | differential-evolution | population, global | weighted sum | quantized | population collapsed, or budget |
| NSGA-II (Pareto) | nsga2 | population, Pareto | yes | quantized | budget |
| Bayesian optimization (Gaussian process) | bayesian | surrogate | weighted sum | no | expected improvement below tolerance, or budget |

Nelder-Mead, L-BFGS-B and the Bayesian method refuse integer, stepped and
categorical variables.

## Settings

| Setting | Meaning |
|---|---|
| Evaluations | the budget: candidate designs, cached ones included (200 by default) |
| Seed | every random choice of the method derives from it |
| Population | differential evolution and NSGA-II; 0 = automatic: max(10, min(8n, 60)) for DE, max(20, min(12n, 80)) for NSGA-II, n free variables |
| Grid points / variable | the sweep's points per variable (5 by default) |
| Samples | random sampling, Latin hypercube, and the Bayesian method's initial design (32) |
| Parallel evaluations | worker threads; 0 = half the hardware threads (one when the model is not thread safe) |
| Time limit | seconds, 0 = none; a time limit makes a run non-deterministic |
| Memory per evaluation | MiB passed to the model, 0 = its own limit |

Other details: a Nelder-Mead initial simplex of 15 % of each range;
differential evolution DE/rand/1/bin with F = 0.6, CR = 0.9 from a Latin
hypercube; NSGA-II with simulated binary crossover and polynomial mutation;
the Latin hypercube is the best of ten by the smallest pairwise distance.
The initial design is always the first candidate of differential evolution,
NSGA-II and Bayesian runs.

## Gradients

L-BFGS-B uses central finite differences with a step of 10⁻⁴ of each range
(two evaluations per variable, one-sided at a bound). These are
approximations, never exact gradients. Where the two one-sided differences
disagree by more than half the central value, the run records that the
gradient of that variable is **unreliable** (a discontinuity, solver noise, a
mesh change). No adapter provides exact (adjoint) gradients yet.

## Bayesian optimization

After a Latin-hypercube start, a Gaussian process models the merit and the
next design is the pool point of largest expected improvement:

```math
EI = (best − μ) · Φ(z) + σ · φ(z),   z = (best − μ) / σ
```

`μ` and `σ` are the predicted mean and standard deviation, `Φ` and `φ` the
normal distribution and density. Each such candidate records the prediction
it was chosen by, labelled as *a prediction, not a simulation*.

## Choosing a method

The suggestion follows these rules: several objectives, NSGA-II (or a Latin
hypercube to see the trade-off); discrete or categorical variables,
differential evolution (a sweep for up to three variables); expensive
evaluations (1 s or more) of up to ten continuous variables, Bayesian; one or
two cheap variables, a sweep; smooth continuous problems, L-BFGS-B;
Nelder-Mead for noisy or kinked ones; differential evolution when several
local optima are likely.

## Worked example

On the Rastrigin function, which has many local minima, differential
evolution finds the global minimum (0, 0) within 0.02 while Nelder-Mead stays
in a local one. Nelder-Mead solves Rosenbrock's valley to (1, 1) within
0.004, NSGA-II approximates the known ZDT1 front within a mean distance of
0.05, and the Bayesian method reaches the minimum of a smooth function within
30 evaluations. In `examples/optimization/e-electromagnetic-beam-steering.resonant`
L-BFGS-B steers a 4 × 4 array to (30°, 45°) by its 16 phases and reaches at
least the directivity of analytical phase steering (to within 0.05 dB), more
than 3 dB above the unsteered start.

## Limitations

- Nelder-Mead and L-BFGS-B are local: from a poor start they stop in a local
  optimum. Amplitude and phase parameterizations have a saddle at zero
  amplitude; the [linear inverse solution](inverse-design.md) is a
  better start.
- Differential evolution and NSGA-II give no optimality guarantee; NSGA-II
  approximates the front with as many points as its population.
- Not implemented: sequential quadratic programming, CMA-ES, Powell's method,
  adjoint and automatic-differentiation gradients.

## Related topics

[The Optimization Studio](studio.md) · [Constraints](constraints.md) ·
[Objectives](objectives.md) · [Multi-fidelity optimization](multi-fidelity.md)
