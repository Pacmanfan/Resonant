---
id: concepts.solvers
title: Solver selection
category: Core Concepts
order: 110
status: available
summary: Which solvers Resonant has in each domain, how each one is started, what it represents and leaves out, and how to choose - Resonant never chooses or substitutes a solver for you.
keywords: [solver, model, analytical, FDTD, finite element, Kirchhoff, Yee, choice]
related: [sim.solver-selection, sim.architecture, ref.solver-assumptions, acoustics.fdtd, em.solvers]
---

Each solver in Resonant is a separate model with its own command. The
application **never chooses a solver and never replaces one with
another**: a solver that cannot run is refused with the reason, and one
that runs on a scene it cannot fully represent says, in its result, what
it left out. Every result records the solver's id and version, its
settings and its assumptions.

## The solvers

| Domain | Solver | How to run it | Represents |
|---|---|---|---|
| Acoustics | Analytical free field | *Field model* *Analytical free field (objects ignored)*, then **Acoustics > Run Field Simulation** (F5) | direct radiation, interference, directivity, attenuation; one frequency |
| Acoustics | Free field + reflectors (Kirchhoff approximation) | *Field model* *Free field + reflectors (Kirchhoff approximation)*, then F5 | as above plus one reflection from each reflector, aperture diffraction, the shadow behind it |
| Acoustics | Full-wave time domain (FDTD) | **Simulation > Run Full-Wave Simulation** (F6), **Full-Wave Solver** panel | everything the grid holds: multiple reflections, diffraction, shadows, transmission and refraction (fluid-like), resonance, pulses and spectra, 2D or 3D |
| Mechanics | Finite elements of the chosen idealization (Euler-Bernoulli beam, Kirchhoff plate, membrane, trilinear solid) | **Mechanics > Calculate Modes**, **Calculate Frequency Response**, **Run Transient Analysis**, **Calculate Static Deformation** | modes, harmonic response (direct or modal superposition), Newmark transients, statics with stresses |
| Mechanics to acoustics | Rayleigh radiation of a baffled plate (one-way) | **Mechanics** panel, *Periodic Rod and Sound* tab | the sound from a calculated plate vibration |
| Electromagnetics | Analytical antenna, array, line and guide models | **Electromagnetics > Analyse Selected**, **Evaluate Analytical Field** | impedance, gain, patterns, exact near fields of dipoles, monopoles and loops; array factor; line and guide modes |
| Electromagnetics | Yee FDTD | **Electromagnetics > Run Electromagnetic Full-Wave (Yee)** | Maxwell's equations on a grid, 2D TM or 3D, with materials and conductors |

The *Field model* is chosen in the **Acoustics** panel's *Medium* tab.

## What the acoustic solvers leave out

| | Free field | Reflectors (Kirchhoff) | FDTD |
|---|---|---|---|
| Full-wave | no | no | yes |
| Source directivity | yes | yes | no (monopoles) |
| Single reflection | no | yes | yes |
| Multiple reflections, resonance | no | no | yes |
| Diffraction | no | aperture of a reflector | yes |
| Shadowing by objects | no | no | yes |
| Transmission, refraction | no | no | yes (fluid-like) |
| Transient and broadband signals | no | no | yes |
| Particle velocity, intensity | yes | no | calculated, not recorded |
| Shear waves, nonlinearity, flow | no | no | no |

The **Full-Wave Solver** panel shows, under *Which solver represents this
problem*, an assessment of the current scene for each acoustic solver, for
example *Analytical free field: would LEAVE OUT 1 reflector ... it would
still run, and its result would not include the items above*.

## How the cost grows

```math
analytical:  work ∝ N_samples · N_sources
reflectors:  work ∝ N_samples · N_surface elements
FDTD:        work ∝ N_cells · N_steps ∝ (L / Δx)^D · (T / Δt)
```

`L` is the size of the region, `Δx` the cell size, `D` the number of
dimensions, `T` the simulated time and `Δt` the time step. The analytical
model is exact within its assumptions and thousands of times cheaper than
FDTD; a 3D FDTD run is practical for regions of a few tens of wavelengths
at most.

## Choosing

1. **Open space, emitters and arrays only, one frequency:** the analytical free field.
2. **Reflectors several wavelengths large, one reflection, 3D, one frequency:** the reflector model.
3. **Obstacles, slits, lenses, cavities, waveguides, materials, several reflections, pulses:** FDTD, in 2D when the geometry allows it (a plane region), in 3D for small regions.
4. **Vibration of structures:** the mechanical analyses; the [Modal analysis](../mechanics/modal.md) first, to know the frequencies.
5. **Antennas and arrays:** the analytical models; the Yee solver for scattering by bodies and for checking a model.

On a plane region the analytical solvers sample a 3D field on the plane,
while FDTD solves a 2D problem in it (line sources, objects infinitely long
across it). Compare patterns, not amplitudes, between the two.

## Where the full-wave solvers run

Both full-wave solvers have *Compute*: *CPU (reference, double
precision)* or *CUDA GPU (double precision)*, and *CPU threads* (*All
cores* or a number). Any number of threads gives the same result bit for
bit. A CUDA choice that cannot run is refused with the reason; the backend
is never switched automatically, and each result records where it ran.

## Worked example

Open `examples/advanced_acoustics/c-diffraction.resonant`: an emitter in
front of a rigid plate with a 5 mm slit, probes on the axis and in the
geometric shadow. The assessment in the Full-Wave Solver panel says the
analytical free field would ignore the plate. Run F6: the shadow probe
records sound that reached it by diffraction. Run F5 on the same region
and the plate has no effect at all - a correct answer of a model that
leaves the plate out.

## Limitations

- FEM, BEM, angular-spectrum, pseudospectral and acoustic eigenmode solvers are not implemented.
- Moment-method wire antennas and mutual coupling in arrays are not implemented.
- Mechanics has no shells, frames of several members or composites; the mechanical solvers run on the CPU only (single-threaded sparse solvers).

See [Choosing a solver](../simulation/solver-selection.md) and
[Solver assumptions](../reference/solver-assumptions.md).
