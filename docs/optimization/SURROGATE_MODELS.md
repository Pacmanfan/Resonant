# Surrogate Models and Response Surfaces

A surrogate approximates an objective from evaluated candidates so that fewer
expensive simulations are needed. **A surrogate prediction is not a
simulation result**: it is reported with the model that made it and never
becomes a candidate's evaluation.

| Kind | Model | Uncertainty |
|---|---|---|
| `quadratic` | least-squares polynomial of degree 2, with cross terms when there are enough points (1 + 2n + n(n-1)/2 + 1), otherwise without | no |
| `rbf` | cubic radial-basis interpolation with a linear tail (a 1e-10 ridge keeps duplicated points solvable) | no |
| `gaussian-process` | squared-exponential kernel on the unit cube, isotropic length scale and noise chosen by the largest marginal likelihood on a grid (11 x 6), outputs centred and scaled | mean and standard deviation |

`fitSurrogate(problem, candidates, objective, kind)` trains on the evaluated
candidates of one model (inputs: the free variables in the unit cube; output:
the objective value) and records the kind and version, the inputs, the
output and model, the number of training points, the training RMSE and the
leave-one-out RMSE (validation error; up to 400 points). In the Studio:
Analysis tab, "Surrogate model of an objective".

## Active learning and Bayesian optimization

`mostInformative` picks, from a pool of points, those the surrogate is least
certain about (largest predicted standard deviation; for surrogates without
uncertainty, the points farthest from the training data) - the next most
informative simulations.

The `bayesian` method fits a Gaussian process to the merit after a Latin-
hypercube initial design, then evaluates, one at a time, the pool point of
largest expected improvement

```text
EI = (best - mu) Phi(z) + sigma phi(z),   z = (best - mu) / sigma
```

with the physical model. Each such candidate's origin records the prediction
("surrogate predicted merit m +/- s (a prediction, not a simulation)"), so the
prediction and the simulated value can be compared; merits that could not be
computed are replaced in the training data by the worst finite merit plus a
margin. It stops when the largest expected improvement is below tolerance x
(1 + \|best\|).

## Validation

`OptimizationSurrogates.ModelsReproduceKnownFunctionsAndReportTheirErrors`: the
quadratic surface reproduces a quadratic to 1e-9 with a leave-one-out error
below 1e-8; the Gaussian process interpolates its data and is uncertain far
from it; the RBF interpolates; expected improvement equals its closed form.
`OptimizationAlgorithms.BayesianOptimizationNeedsFewEvaluations`: 30
evaluations reach the minimum of a smooth function.

## Limitations

Isotropic length scale (no per-variable relevance); a grid instead of a
continuous hyperparameter search; cubic cost in the number of points (fine
for hundreds); continuous variables only for the Bayesian method.
