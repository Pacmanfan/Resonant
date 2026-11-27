---
id: sim.comparison
title: Comparing simulations
category: Simulation
order: 90
status: available
summary: Compare results of different designs, models and resolutions: the Result Comparison table, fair display settings, probe values, and comparison with measurements.
keywords: [comparison, results, metrics, focal gain, models, measurement]
related: [acoustics.field-analysis, sim.results, sim.accuracy, exp.correlation, opt.studio]
---

Every run is kept as its own result for the session, with its own
configuration, so comparing designs, models or resolutions means running
each one on the same region and comparing the results. Resonant does not
show two fields side by side or a difference field; it compares them by
numbers and by switching the displayed result.

## The Result Comparison table

**Analysis > Field Analysis > Result Comparison** lists every acoustic field
result of the session (analytical runs and steady-state fields of full-wave
runs):

| Column | Meaning |
|---|---|
| **Result**, **Model** | the result's name and solver id |
| **Peak X / Y / Z** | where the largest pressure lies |
| **Width U / V / W (-6 dB)** | full width at half the peak pressure along each sampled axis; "0 (not resolved)" when the half level is not reached inside the region |
| **Distance to intended** | from the peak to the first target point, or else a reflector's intended focus |
| **Sidelobe / peak** | the largest other maximum along U through the peak, over the peak |
| **Time (s)** | solver time |

The table also gives the peak pressure (Pa) and the pressure at the intended
point (Pa). The focal gain of a reflector is the ratio of the pressure with
and without it at the same point: run the region once with **Free field +
reflectors** and once with the analytical free field, and divide.

## Comparing fairly

- **Same region, same sampling.** Metrics are measured on the sampled grid;
  a coarser grid gives coarser widths.
- **Absolute colour scale.** In the **Display** tab choose **Colour scale:
  Absolute (pascal)** and one **Scale top** for all results; the normalized
  scale makes every result look equally strong.
- **Same frequency and medium.** The Field Analysis summary states both for
  the displayed result.
- **Read the assumptions.** An analytical result ignores the objects a
  full-wave result includes.
- **2D is not 3D.** A full-wave run on a plane region is a two-dimensional
  problem with line sources; its amplitudes are not comparable with an
  analytical field sampled on the same plane. Patterns of a geometry that is
  really uniform along the plane's normal are.

## Values at points

The **Probes and Targets** table calculates its values from the **current
scene** with the selected analytical model; it is not tied to a stored
result. To compare designs at a point, change the design and press
**Refresh**, or export both states with **Export Points...**. Full-wave
values at probes are in **Analysis > Time Signals**, one probe and one
curve at a time.

## Worked example: original and optimized reflector

`examples/manipulators/e-geometry-optimization.resonant` holds a reflector
with f = 40 mm and, hidden beside it, the design the optimizer found for a
target at 70 mm.

1. Press **F5**: the result of the original reflector.
2. Hide the original (**Scene > Hide / Show Selected**, H) and show the
   optimized reflector. A hidden reflector is not simulated.
3. Press **F5** again.
4. In **Result Comparison** the second run's pressure at the intended point
   (the target) is more than three times that of the first; compare the
   peak positions and widths of the two rows as well.

## Comparing models

The same scene can be run with each solver it suits. In
`examples/advanced_acoustics/c-diffraction.resonant` the analytical free
field (F5) ignores the plate with the slit, while the full-wave solver (F6)
diffracts through it; the assessment in the Full-Wave Solver panel lists the
difference before you run ([Choosing a solver](solver-selection.md)).

## Comparing resolutions

Run the same problem at two resolutions and compare the quantity you need;
see [Convergence](convergence.md).

## Comparing with measurements

- **Acoustics, antennas, modes and patterns**: the experiments workspace
  compares measurements with simulations
  ([Simulation correlation](../experiments/correlation.md)).
- **Mechanics**: **Import Measured Response...** in the Frequency Response
  tab plots a measured response from CSV against the calculated one.
- **Electromagnetics**: Touchstone one-port data, measured against
  calculated.

## Comparing optimization candidates

The Optimization Studio compares the candidates of a run, ranks them and
shows Pareto fronts for several objectives ([The Optimization
Studio](../optimization/studio.md)).

## Not available

A side-by-side or difference view of two fields, probe-to-probe comparison
plots, and overlaying the curves of two full-wave runs.
