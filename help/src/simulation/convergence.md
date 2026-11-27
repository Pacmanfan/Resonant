---
id: sim.convergence
title: Convergence
category: Simulation
order: 50
status: available
summary: How to check that a result no longer depends on the resolution, the measured convergence of each solver, and what Resonant does not automate.
keywords: [convergence, refinement, order of accuracy, resolution study, mesh]
related: [sim.mesh-resolution, sim.accuracy, sim.comparison, acoustics.fdtd, mechanics.modal]
---

A numerical result has converged when refining the resolution no longer
changes it by more than you care about. Convergence shows that the numerical
error is small; it says nothing about the model error (what the solver
leaves out).

> **Note** Resonant has no automatic refinement study: you run the same
> problem at two or three resolutions and compare. The full-wave settings can
> also be changed and the solver run from a script ([The Python API](../python/api.md)).
> The word "convergence" in the Optimization Studio means something else: the
> progress of an optimization run.

## How to check convergence

1. Run the problem at its current resolution.
2. Refine: for the full-wave acoustic solver raise **Cells per wavelength**
   (for example 12 → 16 → 24); for a reflector raise the **Surface
   resolution**; for a structure lower the **Element size**.
3. Run again on the same region. Each run is kept as its own result.
4. Compare what you need: the value at a probe, the peak and its position,
   a width or a resonance frequency (**Analysis > Field Analysis > Result
   Comparison**, or the **Time Signals** tab for full-wave probes).
5. When the change between the last two runs is small against your
   tolerance, the coarser of the two is good enough.

The order of accuracy tells you what to expect. With error `e` proportional
to `hᵖ` (cell size `h`, order `p`):

```math
e(h/2) / e(h) = 2^(−p)
p ≈ log₂( |R(h) − R(h/2)| / |R(h/2) − R(h/4)| )
```

`R(h)` is the result at cell size `h`. Three resolutions estimate `p`; a
measured `p` near the expected one says the runs are in the convergent range.

## Measured convergence

| Solver | Quantity | Measured | Order |
|---|---|---|---|
| Full-wave acoustic, homogeneous medium | phase-velocity error at 8 / 16 / 32 cells per wavelength | 2.03 %, 0.49 %, 0.12 % | 2 |
| Full-wave acoustic, stepped surfaces | positions of walls and curved surfaces | within one cell | 1 |
| Reflector model | value near a focus at 2 / 4 / 8 / 16 elements per wavelength | decreasing at every step; 8 within 1 % of 16 | |
| Beam (frame elements) | cantilever mode 2 at 4 / 8 / 16 elements | relative error 1.2e-3, 8.0e-5, 5.2e-6 | 4 |
| Plate (Kirchhoff) | first mode at 4 / 8 / 16 / 32 | −3.1 %, −0.83 %, −0.21 %, −0.053 % | 2 |
| Solid (hexahedra), bending | cantilever at 5 / 10 / 20 / 40 elements, fraction of the beam value | 0.34, 0.67, 0.89, 0.98 | slow (locking) |

The full-wave figures refine propagation in a homogeneous medium. With
stepped surfaces (reflectors, lenses, cavities) convergence is first order
and should be checked for the case at hand. The reflector-model convergence
is that of the surface integral, not of the Kirchhoff approximation.

## Worked example: a cavity at two resolutions

`examples/advanced_acoustics/e-resonant-cavity.resonant` is set to 30 cells
per wavelength and 10 ms.

1. Run it (**F6**) and read the response peaks in **Time Signals** with
   **Show: Spectrum magnitude**: the first four modes lie within 100 Hz of
   the frequencies of the cavity as the grid holds it and within 4 % of the
   designed cavity (4.29, 5.72, 7.15, 8.58 kHz).
2. Set **Cells per wavelength** to 15 and run again. Compare the peak
   frequencies with the first run: the walls are now stepped at a cell twice
   as large, so the difference shows how much the staircasing contributes.
3. The spectral resolution (here 200 Hz, `2/T` with the Hann window) limits
   how precisely a peak can be read at any grid: to resolve finer,
   **simulate longer**; zero padding does not help.

## Time step and duration

Do not refine the time step on its own: in the full-wave solver a smaller
step makes the dispersion error slightly larger. Keep **Time step /
stability limit** near 0.95. For continuous signals, check that the
steady state is reached: a longer **Simulated time** must not change the
steady-state field. In the mechanical transient analysis, **Steps per
period** sets the time step (Newmark lengthens periods by about
`(ω Δt)² / 12`).

## Common mistakes

- Comparing two resolutions whose amplitudes differ because the geometry
  changed with the grid (a thin shell thickened to two cells).
- Judging convergence from the normalized colour picture: compare numbers.
- Refining only one of several error sources (cells but not the absorbing
  layers, or the surface but not the region sampling).
