---
id: acoustics.fdtd
title: Acoustic full-wave (FDTD) simulation
category: Acoustic Engineering
order: 90
status: available
summary: The time-domain full-wave solver: reflection, diffraction, transmission, resonance and pulses in 2D and 3D, its settings, preflight, results and limits.
keywords: [fdtd, full-wave, time domain, pulse, diffraction, preflight, cells per wavelength]
related: [sim.solver-selection, sim.mesh-resolution, sim.boundaries, sim.resources, acoustics.field-analysis]
---

The full-wave solver integrates the linear acoustic equations in time on a
uniform staggered grid (finite differences in the time domain, second
order). Unlike the analytical models, geometry and materials take part:
reflection (any number of times), diffraction, shadows, transmission and
refraction through fluid-like materials, enclosure resonances, and pulses
with true time delays.

## When to use it

When objects, materials, several reflections or pulses matter. For emitters
in open space at one frequency the analytical free field is exact and far
cheaper ([Choosing a solver](../simulation/solver-selection.md)).

## The equations

```math
ρ · ∂v/∂t = −∇p,      ∂p/∂t = −K · ∇·v − γ · p + K · q,      K = ρ c²
```

Pressure `p` lives at cell centres, each velocity component `v` on the cell
faces half a time step later; density `ρ`, bulk modulus `K` and loss rate
`γ` come per cell from the medium and the objects' materials
([Acoustic physics](physics.md)).

## Setting up a run

1. Add a region with **Acoustics > Field Plane** or **Field Volume**. It is
   the computational domain: a **plane gives a 2D calculation** in that
   plane, a **volume a 3D one**. Size it to contain the emitters, the
   objects and the probes.
2. Open **View > Panels > Full-Wave Solver** and choose the **Region**.
3. Read **Which solver represents this problem**: what each acoustic solver
   would leave out for this scene.
4. Choose the signal and the numerics (tables below), then **Check** to
   update the **Preflight**.
5. **Run Full-Wave Simulation** (or **Simulation > Run Full-Wave
   Simulation**, F6). Progress appears in **Analysis > Simulation Status**;
   **Cancel** stops the run.

## Signal and duration

| Field | Meaning |
|---|---|
| **Signal** | *Continuous sinusoid (steady state)*, *Tone burst*, *Gaussian pulse*, *Ricker wavelet (broadband)*, *Frequency sweep* |
| **Burst cycles** | tone burst: whole cycles under a Hann envelope |
| **Bandwidth / centre frequency** | Gaussian pulse: −6 dB bandwidth as a fraction of the frequency |
| **Peak / start frequency** | Ricker peak or sweep start (Hz) |
| **End frequency**, **Sweep duration** | frequency sweep |
| **Simulated time** | ms; *Automatic* when 0 |

Continuous signals, tone bursts and Gaussian pulses use each emitter's own
frequency and phase; the Ricker wavelet and the sweep apply as defined to
every emitter. Amplitude and time delay are always the emitter's. One signal
kind applies to all emitters of a run. The automatic time is the longest
delay, plus the signal, plus two crossings of the domain diagonal, plus the
analysis window; a resonant enclosure needs more, set by hand.

## Resolution, stability, boundaries, recording

| Field | Default | Meaning |
|---|---|---|
| **Cells per wavelength** | 12 | at the highest significant frequency, in the slowest material (4 to 200) |
| **Time step / stability limit** | 0.95 | fraction of the stability limit (0.05 to 1) |
| **Domain boundaries** | absorbing | *Absorbing layers (open space)*, *Rigid walls*, *Pressure-release walls*, *Impedance walls* |
| **Wall impedance** | 415 Pa s/m | for impedance walls |
| **Absorbing layer thickness** | 12 cells | added outside the region (4 to 200) |
| **Snapshots to keep** | 40 | instants of the pressure in the region |
| **Steady-state window (periods)** | 8 | continuous signals: periods used to extract the steady state |
| **Compute** | CPU | *CPU (reference, double precision)* or *CUDA GPU (double precision)* |
| **CPU threads** | All cores | results are identical for any thread count |

The time step is stable only if `Δt ≤ Δx / (c_max · √D)`, with `Δx` the
cell size, `c_max` the fastest speed of sound in the grid and `D` = 2 or 3;
a larger step is refused. The grid wave travels slightly too slowly
(numerical dispersion): about 0.6 % at 12 cells per wavelength. See
[Mesh and grid resolution](../simulation/mesh-resolution.md) and
[Boundary conditions in simulations](../simulation/boundaries.md).

## The preflight

The **Preflight** box states the grid, cell size, time step and its ratio to
the limit, the Courant number, steps, simulated time, cells per wavelength,
the phase-velocity error, memory, cell updates, a rough time estimate, where
it runs, sources simulated and left out, probes, cells of each kind, how each
object is represented, WARNINGS and WHAT IS CALCULATED. A run is refused
above the memory limit (1 GiB) or 5 × 10¹¹ cell updates, with what to change;
**the resolution is never lowered automatically**
([Resource estimation](../simulation/resources.md)).

## Objects

Each object's cells are rigid, pressure release, or a fluid of its
material; a cell belongs to an object when its centre is inside it, so
surfaces are stepped at the cell size. Thin shells are thickened to two
cells; hidden objects, and emitters inside rigid bodies or outside the
domain, are left out and counted.

## Results

- **Analysis > Time Signals**: for each run and probe (probes and targets
  inside the region are recorded at every step), **Pressure over time**,
  **Spectrum magnitude**, **Spectrum phase** (Hann or Rectangular window) or
  **Energy over time**; the **Recorded instant** slider shows the snapshots
  in the viewport as instantaneous pressure; **Show Steady State**;
  **Export Signals...** (CSV).
- A continuous signal also yields a **steady-state field** at the emitters'
  frequency, added to the Acoustics panel's results and handled like any
  field result ([Field analysis](field-analysis.md)).

## Worked examples

| Example | Shows | Result in the validation |
|---|---|---|
| `a-free-field-pulse-3d` | Gaussian pulse in a 34 mm cube | amplitude falls as 1/r within 0.05; 1 Pa at 1 m within 8 % |
| `b-reflection` | emitter 50 mm above a rigid plate | standing-wave minima half a wavelength apart, the first a quarter wavelength from the plate |
| `c-diffraction` | rigid plate with a 5 mm slit | field in the geometric shadow above 30 % of the on-axis value |
| `e-resonant-cavity` | rigid cavity, Ricker pulse | four modes within 100 Hz of the gridded cavity |
| `f-passive-focusing` | parabolic trough, emitter 150 mm away | peak at the mirror-equation image (37.5 mm) within half a wavelength |

All are in `examples/advanced_acoustics`; their solver settings are stored
in the project. The 3D cube (10 cells per wavelength, 8-cell layers) has
176 000 cells and 470 steps: about 0.8 s on one core at the measured rate.

## Limits

On a plane region the analytical models sample the 3D field on the plane,
while this solver solves a **2D problem** (emitters as line sources, objects
as infinite cylinders): amplitudes are not comparable between the two.

Every emitter is a monopole (no directivity); solids are fluids (no shear
waves); attenuation is frequency-independent; no nonlinearity, flow or
thermoviscous losses; no periodic boundaries; particle velocity is not
recorded; results stay in memory for the session. 3D runs are validated for
a point source in open space; boundaries and materials are validated in 2D.
The CUDA backend is implemented but not yet validated on a GPU.
