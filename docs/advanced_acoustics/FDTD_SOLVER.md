# The FDTD Solver

**Status:** current for REQ-CHAT-20261126-005. Code: `src/core/fdtd`.
Solver id `acoustic.fdtd.staggered`, version 1.

## Equations

Linear acoustics in a fluid at rest, as a first-order system:

```text
rho dv/dt = -grad p
dp/dt     = -K div v - gamma p + K q          K = rho c^2
```

`p` pressure (Pa), `v` particle velocity (m/s), `rho` density, `K` bulk
modulus, `gamma` a frequency-independent loss rate (1/s), `q` a
volume-velocity source density (1/s). `rho`, `K` and `gamma` may differ
from cell to cell. Eliminating `v` in a homogeneous lossless medium gives
the wave equation `lap p = (1/c^2) d2p/dt2`.

## Discretization

Uniform Cartesian grid with cell size `dx` in every direction. Pressure at
cell centres and at times `n dt`; each velocity component at the centres of
the cell faces normal to it and at times `(n + 1/2) dt` (staggered
"leapfrog" scheme). Central differences in space and time; second-order
accurate.

```text
vx[i+1/2] += -(dt / (rho_f dx)) (p[i+1] - p[i])          rho_f = (rho[i] + rho[i+1]) / 2
p[i]      += -(K[i] dt / dx) (vx[i+1/2] - vx[i-1/2] + ...) + sources
```

The face density is the mean of the two cells, which keeps pressure and
normal velocity continuous across a material interface
(`MATERIAL_INTERACTIONS.md`).

Two dimensions: the same scheme without the third velocity component.
Fields do not vary along the third direction.

Stability, dispersion and resolution: `NUMERICAL_ACCURACY.md`.

## Sources

An emitter is a volume-velocity point source in the cell that contains it.
Its volume acceleration is `dQ/dt = S w(t - delay)`, `w` the signal
(`BROADBAND_SIMULATION.md`). The solver integrates `Q` with the
trapezoidal rule and adds `K Q dt / V_cell` to the pressure of the cell
each step (a "soft" source: the cell stays part of the medium).

* 3D: the radiated pressure is `rho S w(t - delay - r/c) / (4 pi r)`, so
  `S = 4 pi A / rho` gives the emitter's amplitude `A` at 1 m — for any
  signal.
* 2D: the source is a line source. Its steady field is
  `(rho S / 4) |H0(2)(k r)|`, so `S = 4 A / (rho |H0(2)(k * 1 m)|)` gives
  `A` at 1 m at the emitter's frequency. A 2D pulse is not a delayed copy
  of the signal (a line source has a wake).

A source inside a rigid or pressure-release body radiates nothing and is
reported.

## Domain

The selected region object: a plane region gives a 2D run in that plane, a
volume region a 3D run. The grid is aligned with the region, whatever its
orientation. The number of cells along each side is the side divided by
`dx`, rounded. Absorbing layers are added outside the region.

In 2D only emitters, probes and targets within one cell of the plane take
part; the others are counted and reported.

## Recording

Nothing is stored at every step except what is asked for:

* **Probes and targets** inside the region: pressure at every step.
* **Snapshots**: `N` instants of the pressure in the region, evenly spaced,
  the last at the end (default 40; single precision).
* **Steady-state field** (continuous signals): the complex amplitude at the
  emitters' frequency, accumulated over the last `M` periods (default 8) as
  `P = (2 / N) sum p(t_n) exp(-j w t_n)`, convention `e^{+jwt}`. It becomes
  an ordinary field result.
* **Energy** in the grid, 200 samples: `sum p^2 / (2K) + rho v(t-dt/2) .
  v(t+dt/2) / 2`, the quantity the scheme conserves exactly in a closed
  lossless domain.
* Peak pressure, elapsed time, cell updates per second.

Not recorded: particle velocity fields, full time histories of the field,
regions of interest smaller than the region, checkpoints. Results are kept
in memory for the session; the project file stores the settings and a
record of each run, not the samples. HDF5 output is not implemented; probe
signals export as CSV.

## Stability monitor

A time step above the limit is refused before the run. During the run the
field is checked at every energy sample; if it stops being finite the run
ends as failed with the step and time, and no result is produced.

## Result

`FdtdResult` (kind `acoustic.fdtd`): the plan, the settings, the medium,
the recordings, the material map, and the steady-state field if there is
one. Its metadata holds the solver id and version, every setting, the grid
(dimensions, cells, cell size, time step, steps, Courant number, stability
limit, cells per wavelength, phase-velocity error), how each object was
represented, the warnings, and the assumptions. A snapshot shown in the
viewport is labelled as an instantaneous pressure.
