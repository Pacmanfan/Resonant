# Numerical Accuracy: Stability, Dispersion, Resolution, Convergence

**Status:** current for REQ-CHAT-20261126-005.

## Stability (Courant–Friedrichs–Lewy)

The scheme is stable if and only if

```text
dt <= dx / (c_max sqrt(D))          D = 2 or 3 dimensions
```

`c_max` is the largest speed of sound of any fluid material in the grid.
`GridSpec::validate` refuses a larger step, with the limit in the message;
no run starts with one. The time step is set as a fraction (default 0.95)
of the limit. Tests run at 0.5 and 0.99 of the limit for 6000 steps with
the energy constant to 1e-7.

The locally reacting impedance boundary is integrated semi-implicitly and
does not lower the limit.

## Numerical dispersion

The grid wave is slower than the true one. For a plane wave along a grid
axis (the worst direction), with `S = c dt / dx` and `N` cells per true
wavelength,

```text
sin(w dt / 2) = S sin(k dx / 2)      =>      v_p / c = (pi / N) / asin( sin(pi S / N) / S )
```

| Cells per wavelength | Phase velocity error (S = 0.67, 2D default) |
|---|---|
| 6 | 2.8 % |
| 8 | 1.5 % |
| 10 | 0.9 % |
| 12 | 0.64 % |
| 16 | 0.36 % |
| 20 | 0.23 % |
| 30 | 0.10 % |

The error accumulates with distance: at 12 cells per wavelength a wave is
a quarter wavelength late after about 40 wavelengths. Along grid diagonals
the error is smaller, so the error is also anisotropic.

Counter-intuitively a **smaller** time step makes the error slightly
**larger** (the time and space errors cancel partly; in one dimension they
cancel exactly at `S = 1`). The tests measure this.

## Resolution

The cell size is the shortest wavelength divided by the chosen cells per
wavelength (4 to 200, default 12). The shortest wavelength is the slowest
speed of sound of any material in the scene divided by the highest
significant frequency of the signal (`BROADBAND_SIMULATION.md`).

Curved and inclined surfaces are stepped at the cell size
("staircasing"): a surface is where the grid puts it, within one cell. For
resonators and focusing structures this is often the larger error; it
falls with the first power of the cell size.

## Preflight

Before a run the plan states: dimensions, cells, cell size, time step, its
ratio to the stability limit, the Courant number, time steps, simulated
time and how it was chosen, cells per wavelength at the highest frequency
in the slowest medium, the phase-velocity error, memory for the state and
for the recordings, the number of cell updates, a rough time estimate,
sources simulated and left out, probes, cells of each kind, how each object
is represented, warnings and assumptions.

Warnings: fewer than 10 cells per wavelength (with the size of the error);
simulated time shorter than one crossing of the domain; a steady-state
window that starts before the wave has crossed the domain; reflecting
domain faces with a continuous signal; emitters at different frequencies;
thin shells that had to be thickened; objects that cannot be represented;
a long calculation.

Refusals: memory above the limit (default 1 GiB); more than 5e11 cell
updates. **The resolution is never lowered automatically**: the refusal
says what to change.

The time estimate assumes 6e7 cell updates per second and is rough (within
a factor of about three; see `PERFORMANCE_AND_GPU.md`).

## Automatic simulated time

Longest emitter delay + the signal (its ramp if continuous, its length
otherwise) + two crossings of the domain diagonal at the slowest speed +,
for continuous signals, the analysis window. A closed or resonant domain
needs more, by an amount only the user can judge; set it explicitly.

## Convergence (measured)

Spatial refinement at `S = 0.5`: phase-velocity error 2.03 % at 8 cells
per wavelength, 0.49 % at 16, 0.12 % at 32 — a factor of four per halving,
second order, and equal to the dispersion relation within 1e-4.

Temporal refinement at 16 cells per wavelength: the error at `S = 0.35` is
larger than at `S = 0.7`, both equal to the dispersion relation within
1e-4.

These refine the propagation scheme in a homogeneous medium. For problems
with stepped surfaces, convergence under refinement is first order and
should be checked for the case at hand by running two resolutions.
