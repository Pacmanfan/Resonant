---
id: ref.numerical-limits
title: Numerical limitations
category: Technical Reference
order: 70
status: available
summary: The precision of Resonant's arithmetic, the discretization errors of its solvers, and the ranges and limits every solver setting accepts.
keywords: [precision, dispersion, stability, limits, resolution, FFT, ranges]
related: [sim.accuracy, sim.convergence, ref.solver-assumptions, trouble.memory, sim.mesh-resolution]
---

A numerical result differs from the exact solution of its model by
rounding, by discretization and by the limits a calculation is given. This
page states each of them for Resonant. Assumptions of the models themselves
are on [Solver assumptions](solver-assumptions.md).

## Arithmetic

- All solvers calculate in **double precision** (IEEE 754, about 15 to 16
  significant digits), on the CPU and on the CUDA backend.
- Sampled fields kept for display and analysis (acoustic field maps,
  electromagnetic field regions, full-wave snapshots) are stored in **single
  precision**, about 7 significant digits. This is far below the
  discretization error of any solver.
- Threads do not change results: every thread count gives the same numbers
  bit for bit. The CUDA backend avoids fused multiply-add so that it matches
  the CPU to rounding (tested to 1e-12); it has not yet been validated on a
  GPU.

## Full-wave grids

The time step is limited by the stability (Courant-Friedrichs-Lewy)
condition, and the grid wave travels slightly slower than the real one:

```math
Δt ≤ Δx / (c_max · √D)
sin(ω·Δt / 2) = S · sin(k·Δx / 2),   S = c·Δt / Δx
v_p / c = (π / N) / asin( sin(πS / N) / S )
```

`Δx` is the cell size, `c_max` the largest wave speed in the grid, `D` the
number of dimensions (2 or 3), `N` the cells per wavelength and `v_p` the
numerical phase velocity along a grid axis (the worst direction).

| Cells per wavelength | 6 | 8 | 10 | 12 | 16 | 20 | 30 |
|---|---|---|---|---|---|---|---|
| phase-velocity error (S = 0.67, 2D) | 2.8 % | 1.5 % | 0.9 % | 0.64 % | 0.36 % | 0.23 % | 0.10 % |

The error accumulates with distance: at 12 cells per wavelength a wave is a
quarter wavelength late after about 40 wavelengths. A smaller time step
makes it slightly larger, not smaller. The propagation error falls with the
square of the cell size (measured: 2.03 %, 0.49 % and 0.12 % at 8, 16 and 32
cells per wavelength), but surfaces stepped at the cell size converge only
in proportion to it: check such problems at two resolutions.

## Sampled fields and spectra

- Field maps sample the exact model at their points. Fewer than 2 samples
  per wavelength alias the picture (the values at the points stay correct);
  fewer than 6 look coarse; more than 200 add cost without information.
- Closer to an emitter than its radius or the **Minimum distance**
  (whichever is larger, default 0.1 mm), the point-source field is held at
  its value at that distance; such samples are flagged and left out of the
  statistics.
- Spectra use a radix-2 FFT. The resolution is `1 / T` with the rectangular
  window and `2 / T` with the Hann window, `T` the record length; zero
  padding adds points but no resolution. Frequencies above half the
  sampling rate alias. Sidelobes start at −13 dB (rectangular) and −31 dB
  (Hann). A peak's frequency is uncertain by about half the resolution.
- Newmark time integration lengthens periods by about `(ω·Δt)² / 12`.
- A free structure has rigid-body modes, which appear as frequencies near
  0 Hz.

## Ranges of the settings

| Solver | Setting | Accepted | Default |
|---|---|---|---|
| acoustic field | **Surface resolution** (reflector model) | 2 to 40 elements / wavelength | 6 |
| acoustic field | samples of a region | at least 2 per sampled direction | |
| acoustic full-wave | **Cells per wavelength** | 4 to 200 (warning below 10) | 12 |
| acoustic full-wave | **Time step / stability limit** | above 0, at most 1 | 0.95 |
| acoustic full-wave | **Absorbing layer thickness** | 4 to 200 cells | 12 |
| acoustic full-wave | **Snapshots to keep** | 0 to 2000 | 40 |
| acoustic full-wave | **Steady-state window (periods)** | 1 to 1000 | 8 |
| electromagnetic full-wave | **Resolution** | 6 to 200 cells / wavelength (warning below 10) | 20 |
| electromagnetic full-wave | **Time step** | above 0, at most 1 × the stability limit | 0.95 |
| electromagnetic full-wave | **Absorbing layer** | 4 to 200 cells | 10 |
| electromagnetic full-wave | **Analysis window** | at least 1 period | 8 |
| electromagnetic full-wave | **Memory limit** | at least 1 MiB | 1024 MiB |
| mechanics | **Number of modes** | 1 to 500 | |
| mechanics | **Frequencies** of a sweep | 2 to 20 000 | |
| mechanics | **Modes for superposition** | 1 to 1000 | |
| mechanics | **Steps per period** | 4 to 10 000 | |
| mechanics | **Instants to keep** | 0 to 5000 | |
| mechanics | **Loss factor eta** / **Damping ratio zeta** | 0 to 2 / 0 to 1 | |
| mechanics | **Regularization** of targeting | 0 to 1 | |

## Size and work limits

| Solver | Refused above | Warning |
|---|---|---|
| acoustic field | the memory limit (1 GiB); for the reflector model 2e11 element evaluations | more than 2e9 source or 5e9 element evaluations |
| acoustic full-wave | the memory limit (1 GiB); 5e11 cell updates; 254 materials | an estimated run longer than 10 minutes |
| electromagnetic full-wave | **Memory limit** (1024 MiB); 2e11 cell updates | |
| mechanics | 300 000 free degrees of freedom; the memory limit (1 GiB) | fewer than 6 elements per wavelength |

The time estimates of the full-wave preflights assume 6e7 cell updates per
second (acoustic) and 1.5e7 (electromagnetic, one core); they err on the
long side and are rough. The array editor's element table shows the first
512 elements of an array.

## Related topics

- [Numerical accuracy](../simulation/accuracy.md)
- [Convergence](../simulation/convergence.md)
- [Solver assumptions](solver-assumptions.md)
- [Memory issues](../troubleshooting/memory.md)
