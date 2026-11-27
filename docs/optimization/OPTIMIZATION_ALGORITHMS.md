# Optimization Algorithms

Every method works in the unit cube of the free variables through the
evaluation service; none depends on a domain or on a third-party library
(ADR-000214). The same seed gives the same candidates on every platform.

| Id | Method | Family | Several objectives | Discrete variables | Stops |
|---|---|---|---|---|---|
| `sweep` | full-factorial grid; one variable = a one-variable sweep; every choice of a categorical variable | sampling | yes | yes | grid complete or budget |
| `random` | uniform samples | sampling | yes | yes | samples or budget |
| `latin-hypercube` | stratified samples (one per stratum and variable; best of ten by the smallest pairwise distance) | sampling | yes | yes | samples or budget |
| `nelder-mead` | simplex, adaptive coefficients (Gao and Han 2012), points projected onto the bounds | derivative-free, local | weighted sum | no | simplex below 1e-4 of every range, or vertex merits within tolerance x (1 + \|best\|), or budget |
| `lbfgs` | bound-constrained limited-memory BFGS (memory 6, projected direction, Armijo backtracking) with central finite-difference gradients | gradient, local | weighted sum | no | projected gradient step below tolerance x max(1, \|merit\|), no decrease in the line search, or budget |
| `differential-evolution` | DE/rand/1/bin, F 0.6, CR 0.9, Latin-hypercube start, constraint domination | population, global | weighted sum | quantized | population collapsed (merit spread within tolerance and extent below 1e-4), or budget |
| `nsga2` | NSGA-II: non-dominated sorting, crowding distance, SBX (eta 15, p 0.9), polynomial mutation (eta 20, p 1/n), constraint domination | population, Pareto | yes | quantized | budget |
| `bayesian` | Gaussian-process surrogate of the merit, expected improvement over 2000 random and 200 local pool points, one design per step | surrogate | weighted sum | no | expected improvement below tolerance x (1 + \|best\|), or budget |

Defaults: population max(10, min(8n, 60)) for DE and max(20, min(12n, 80))
for NSGA-II; Nelder-Mead initial simplex 15 % of each range; finite-difference
step 1e-4 of each range; the user's initial design is always the first
candidate of DE, NSGA-II and Bayesian runs.

## Gradients

L-BFGS-B uses central differences (2 evaluations per variable, one-sided at a
bound). These are approximations, never exact gradients. At every gradient
the one-sided differences are compared; where they disagree by more than
half the central value the run records that the gradient of that variable is
unreliable (a discontinuity, solver noise, a mesh change, a quantized
variable). Without curvature information the first step moves a tenth of a
range. `Capabilities::providesGradients` is reserved for adapters with exact
(adjoint or analytic) gradients; none provides them yet.

## Choosing a method

`recommendAlgorithms` (shown in the Studio) suggests, in order:

* several objectives: NSGA-II; a Latin hypercube to see the trade-off; DE
  with a weighted sum when priorities are known;
* discrete or categorical variables: DE; a sweep for up to three variables;
* expensive evaluations (>= 1 s, robust samples included) of up to ten
  continuous variables: Bayesian;
* one or two cheap variables: a sweep;
* smooth continuous problems: L-BFGS-B; Nelder-Mead for noisy or kinked
  ones; DE when several local optima are likely.

Methods for continuous variables (Nelder-Mead, L-BFGS-B, Bayesian) refuse a
design space with integer, stepped or categorical variables.

## Limitations

* Nelder-Mead and L-BFGS-B are local: from a poor start they stop in a local
  optimum (`OptimizationAlgorithms.DifferentialEvolutionFindsTheGlobalMinimumOfAMultimodalFunction`
  shows Nelder-Mead stuck where DE is not).
* Amplitude / phase parameterizations have a saddle at zero amplitude; a
  gradient method can stop there. The vibration template uses Nelder-Mead,
  and the linear inverse solution (`INVERSE_DESIGN.md`) is the better start.
* DE and NSGA-II give no optimality guarantee; NSGA-II approximates the front
  with as many points as its population.
* A time limit makes a run non-deterministic (and its checkpoint replay
  different).

## Not implemented

Sequential quadratic programming, CMA-ES, Powell's method, adjoint and
automatic-differentiation gradients. The `Algorithm` interface admits them,
and a library method could be wrapped behind it.
