# Robust Optimization

A design must work under realistic variation. A robust problem names the
uncertain parameters - any binding the adapter offers, a design variable or
not (element phases, material properties via hypothetical scalings, the
operating frequency, the air temperature, positions, thicknesses) - and how
they vary:

| Setting | Meaning |
|---|---|
| distribution | `uniform`: +/- width (a tolerance band); `normal`: standard deviation = width, clipped at `truncate` sigma |
| relative | width as a fraction of the design value (or of the project's value for a binding outside the design) |
| source | **required**: where the distribution comes from (a tolerance, a data sheet, a measurement, a stated requirement). Resonant never assumes a distribution |
| samples | perturbed evaluations per design (16 by default) |
| measure | `mean`, `mean-plus-k-sigma` (k), `worst-case` of the objective's score |
| required probability | the share of samples in which every hard constraint must hold (1 by default) |

## How designs are evaluated

* The samples are a Latin hypercube in probability, mapped through each
  distribution (inverse normal CDF), generated once from the robust seed:
  **common random numbers** - every design is judged under the same
  perturbations, so differences between designs are not sampling noise.
* Each design is evaluated nominally (no perturbation) and at every sample;
  the perturbation is added to the design value of the binding.
* The candidate's objective values are the nominal ones; its scores are the
  robust measure of the samples' scores; its robust statistics hold, per
  objective, the mean, standard deviation, minimum, maximum and worst score
  of the samples and, per constraint, the share of samples meeting it. A hard
  constraint met in fewer than the required share of samples makes the
  candidate infeasible, with the worst value as its violation.

## Nominal versus robust

The two are distinguished everywhere: a candidate carries robust statistics
only when it was evaluated robustly, comparisons say when one of two
candidates is robust and the other not, and `assessRobustness` scores one
design both ways. Example G shows why it matters: phase steering set for 1
GHz squints across a 0.9 - 1.1 GHz band; maximizing the **worst** directivity
toward 30 degrees over the band gives a different taper than maximizing it
at 1 GHz, and the robust design is at least as good in the worst case while
the nominal one is at least as good at 1 GHz
(`OptimizationExamples.RobustDesignTradesNominalGainForTheWorstCaseOverTheBand`).

## Validation

`OptimizationRobust.StatisticsMatchKnownDistributions`: with a quantity
x + p, a uniform +/- 0.3 tolerance on p gives a mean equal to the nominal
value, a standard deviation 0.3 / sqrt(3) and a maximum nominal + 0.3; a
normal distribution with sigma 0.1 gives a standard deviation of 0.1 (within
sampling error); a constraint at the nominal value is met in half the
samples, so the design is feasible nominally and infeasible robustly.

## Limitations

Independent parameters only (no correlations); uniform and normal
distributions only; the cost is (1 + samples) evaluations per design; worst
case is the worst of the samples, not a guaranteed bound.
