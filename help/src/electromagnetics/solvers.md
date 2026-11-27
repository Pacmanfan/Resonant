---
id: em.solvers
title: Electromagnetic solvers
category: Electromagnetic Engineering
order: 120
status: available
summary: The analytical models and the Yee FDTD full-wave solver: what each represents, how to plan and run a full-wave calculation, and its accuracy.
keywords: [FDTD, Yee, full-wave, PML, solver, grid, CUDA]
related: [em.maxwell, em.near-far-field, sim.solver-selection, sim.mesh-resolution, acoustics.fdtd]
---

Resonant has two kinds of electromagnetic solver. The **analytical**
models give exact fields, impedances and modes for idealized antennas,
sources, lines and guides in free space. The **Yee FDTD** solver
discretizes Maxwell's equations on a grid and handles conducting and
dielectric bodies, at the price of numerical error that you control with
the resolution.

| Solver | Result label | Use it for |
|---|---|---|
| Analytical fields (em.analytical.fields) | analytical | antennas, sources and dipole arrays in free space ([Near field and far field](near-far-field.md)) |
| Guide modes (em.analytical.waveguide) | analytical | ideal guides and cavities ([Waveguides](waveguides.md)) |
| Array factor | array factor | patterns and beamforming of arrays ([Antenna arrays](arrays.md)) |
| Yee FDTD (em.fdtd.yee) | full-wave numerical (Yee FDTD) | scattering by bodies, materials, fields near conductors |

## The Yee method

E is stored on the cell edges and H on the face centres of a uniform cubic
grid of cell size d, and the two are advanced alternately (leapfrog) in
time; the scheme is second order in space and time. It is stable only
when

```math
Δt ≤ d / (c √D)
```

with D the number of dimensions. The time step is a chosen fraction of
this limit (**Time step**, default 0.95); a larger step is refused. A
field **plane** gives a two-dimensional TM calculation (E_z, H_x, H_y:
objects are infinitely long with the cross-section the plane cuts); a
field **volume** gives a 3D calculation. Each face of the domain has an
absorbing perfectly matched layer (quadratic grading, design reflection
1e-6) outside the region.

## Setting up a run

In the **Fields** tab choose the **Region**, then in the **Full-wave
numerical solution (Yee FDTD)** group:

| Field | Meaning |
|---|---|
| Resolution | cells per wavelength in the slowest material at the highest frequency of the signal (default 20; under 10 is warned) |
| Time step | fraction of the stability limit |
| Absorbing layer | thickness of the matched layer in cells (default 10) |
| Signal | *Continuous sinusoid (steady state)*, *Tone burst*, *Gaussian pulse*; **Burst cycles** for the burst |
| Analysis window | periods of a continuous signal over which a running DFT gives the steady-state field |
| Simulated time | *Automatic* (delays + signal + two domain crossings + the window) or a value in ns |
| Memory limit | MiB (default 1024) |
| Compute | *CPU (reference, double precision)* or *CUDA GPU (double precision)*; **CPU threads** (*All cores*) |

Press **Plan** to see the grid, cell size, time step, number of steps,
memory, cell updates, estimated time, what is represented and the
warnings. **Run Full-Wave** (or **Electromagnetics > Run Electromagnetic
Full-Wave (Yee)**) starts a background job, shown in the **Simulation
Status** tab of the Analysis panel; **Cancel** stops it. A plan above the
memory limit or 2 × 10¹¹ cell updates is **refused**; the resolution is
never lowered automatically.

## What the solver represents

- Dipole and monopole antennas: the feed current on one edge and the arms
  (and a monopole's ground disc) as thin conducting edges; in 2D an
  infinite line current along the plane's normal.
- Electric-dipole sources: a current element one cell long.
- Objects with geometry: good conductors as perfect conductors, other
  materials with εr, μr and σ, staircased onto the grid; guide objects as
  conducting walls.

Left out, with a warning: loops, patches, horns, helices, magnetic dipoles,
plane waves, arrays and radiators not aligned with the grid. With a
continuous signal the result is the steady-state E at the region's nodes
(H is not extracted), with the grid, settings, represented objects,
warnings and backend in its metadata and a run record in the project.

## Worked example

Open `examples/electromagnetics/g-full-wave-scattering.resonant`: a copper
cylinder of radius λ/2 at 1 GHz, lit by a line source 1.5 λ from its axis,
on a 5 × 5 wavelength plane at 30 cells per wavelength. Press **Plan**, then
**Run Full-Wave**. The run takes about 2 s on one core of a release build
(threads divide this by about four). The steady-state field differs from
the exact series solution by 4.7 %; at 15 cells per wavelength the error is
12.6 %, so refinement converges.

## Accuracy

| Check | Result |
|---|---|
| 2D line current at 20 cells per λ | magnitude within 3 %, phase within 0.1 rad |
| Plane wave on εr = 4 | Fresnel reflection −1/3 within 0.01 |
| Absorbing layer alone (12 cells) | reflection below 2e-3 |
| PEC cavity at 1.5 mm cells | first resonance within 0.5 %, below the exact value |
| 3D current element at 16 cells per λ | within 6 % of the Hertzian dipole |

The Yee grid has numerical dispersion (waves slightly slow) and staircase
error on curved surfaces; both fall with refinement.

## Limitations

- Thin wires use an effective radius of about 0.23 cells, independent of
  the real wire radius, so a full-wave dipole's impedance is not compared
  with the analytical one.
- The Fields tab sets only the absorbing layer; the solver's perfect-conductor
  and periodic faces are not offered in the panel.
- No moment-method or finite-element electromagnetic solver.
- The CUDA backend is implemented but not yet validated on a GPU; the CPU
  is the reference. Results record where they ran.
- Mutual coupling of arrays is not represented by any solver.
