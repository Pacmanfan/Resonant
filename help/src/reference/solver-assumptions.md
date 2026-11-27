---
id: ref.solver-assumptions
title: Solver assumptions
category: Technical Reference
order: 60
status: available
summary: What each solver in Resonant represents and what it leaves out, as the solvers themselves state it in their results.
keywords: [assumptions, solver, model, limitations, approximation, validity]
related: [sim.solver-selection, concepts.solvers, ref.numerical-limits, em.solvers, acoustics.fdtd]
---

Every result in Resonant names the solver that produced it, its version and
the assumptions of that model. This page collects them in one place. A
result is only as valid as these assumptions are for your problem.

## Where the assumptions are shown

- **Analysis > Field Analysis**: *Solver* and *Assumption 1, 2, ...* rows for
  the displayed acoustic result (or for the selected model before a run).
- **Full-Wave Solver** panel: *Which solver represents this problem* lists,
  for each acoustic solver, what it would leave out for the current scene;
  the **Preflight** ends with *WHAT IS CALCULATED*.
- **Electromagnetics** and **Mechanics** panels: the assumptions and notes
  under each result and under **Check**.
- **Optimization Studio**: the model and its assumptions for each candidate.

## Acoustics

| Solver (id) | Represents | Leaves out |
|---|---|---|
| Analytical free field (`acoustic.freefield.analytical`) | point sources with spherical spreading `A·(1 m / r)·D(θ)·e^(−jkr)`, superposed, in a homogeneous medium at rest, at one frequency; attenuation as one factor `e^(−αr)`; particle velocity and intensity | every object in the scene (no reflection, transmission, refraction, diffraction or scattering); transient signals |
| Free field + reflectors (`acoustic.reflector.kirchhoff`) | one reflection from each reflector by the Kirchhoff (physical-optics) approximation, with aperture diffraction and the interference of all paths | multiple reflections and resonance between surfaces, shadowing of one surface by another, all objects that are not reflectors, particle velocity and intensity |
| Full-wave time domain (`acoustic.fdtd.staggered`) | linear acoustics in a fluid at rest: reflection, diffraction, scattering, transmission, refraction, resonance, attenuation, pulses and broadband signals | emitter directivity (every emitter is a monopole), shear waves in solids, frequency-dependent attenuation, flow, nonlinearity, thermoviscous boundary layers |

Further assumptions:

- **Analytical model.** Within an emitter's radius the point-source model
  is not valid; the field there is held at its value on that sphere and
  flagged. The baffled-piston directivity is a far-field formula applied at
  every distance. Intensity is exact for omnidirectional sources and
  approximate with directivity.
- **Reflector model.** Thin reflector surfaces reflect on both sides.
  Accuracy falls where a reflector or its radius of curvature is not large
  against the wavelength: it is a high-frequency approximation.
- **FDTD.** A uniform staggered grid, second order; curved surfaces are
  stepped at the cell size; emitters sit in the nearest cell (up to 0.87 of
  a cell from their position); solids are fluids with their density and
  speed of sound; the grid wave is slightly slow by an amount the preflight
  states; absorbing layers reflect a small, tested amount. On a plane region
  the problem is two-dimensional: emitters are line sources and objects
  infinitely long cylinders, so amplitudes decay with the square root of the
  distance.
- **Estimates** for lenses (paraxial, fluid-like lens), waveguide cut-offs
  and cavity resonances (rigid, lossless walls) are formulas, not fields.

## Mechanics

| Idealization | Theory | Leaves out |
|---|---|---|
| Beam (Euler-Bernoulli) | 3D frame element: bending, axial, torsion | shear deformation and rotary inertia |
| Plate (Kirchhoff) | thin plate, out-of-plane bending | transverse shear, in-plane stretching; overestimates frequencies of thick plates |
| Membrane (pre-tensioned) | uniform tension set by the user, every edge held | bending stiffness; the tension is not derived from geometry or material |
| Solid (3D elasticity) | trilinear hexahedra, full integration | accuracy in bending unless several elements span the thickness |

All structures use linear, isotropic, elastic materials and small
deformations. Modal analysis solves `K·φ = ω²·M·φ`; a mode shape's amplitude
is arbitrary. Harmonic analysis is a steady state with `e^(+jωt)` and peak
amplitudes, with Rayleigh, loss-factor or modal damping. Transient analysis
uses Newmark average-acceleration integration (unconditionally stable, no
numerical damping, periods lengthened by about `(ω·Δt)² / 12`) from rest.
Static analysis applies each actuator's amplitude times the cosine of its
phase as a constant force. Actuator forces act exactly as set: a shaker's or
piezo's own response is not modelled. Sound radiated by a plate is one-way:
the vibration is taken as calculated in vacuo, the plate is baffled, and the
field behind the baffle is not calculated.

## Electromagnetics

| Model (id) | Represents | Leaves out |
|---|---|---|
| Analytical fields (`em.analytical.fields`) | exact fields of dipoles, monopoles over an infinite ground and small loops in free space, with assumed thin-wire currents | mutual coupling, scattering by objects in the scene |
| Feed impedance, gain | induced-EMF model for dipoles and monopoles, small-loop model for loops, both with an assumed current; conductor loss in the efficiency | the real current distribution; dielectric and ground losses |
| Array factor | identical, isolated elements times the array factor | mutual coupling, element impedance changes, the feed network, scattering |
| TE10 guide field (`em.analytical.waveguide`) | the TE10 mode of an ideal, empty guide with perfectly conducting walls | all other modes |
| Yee FDTD (`em.fdtd.yee`) | Maxwell's equations on a uniform cubic grid, second order; per-cell εr, μr, σ; perfect conductors; thin wires; absorbing layers | loops, patches, horns, helices, magnetic dipoles, plane waves and arrays; radiators not aligned with the grid |

Patch, horn and helical antennas are design estimates only: no field,
pattern or impedance is calculated for them. In the Yee solver a thin wire
has an effective radius of about 0.23 cells whatever its real radius, a
material without electromagnetic data is treated as a perfect conductor, and
a relative permittivity below 1 is raised to 1. Transmission lines use exact
formulas for coax, stripline (zero-thickness strip) and coplanar waveguide
(infinite substrate) and the Hammerstad-Jensen formulas for microstrip.

## Optimization and experiments

The Optimization Studio evaluates designs with the models above and records
which model and fidelity judged each candidate. Gradients are finite
differences, robust optimization treats uncertain parameters as
independent, and the topology optimization is a 2D compliance proof of
concept. Comparisons of measurements with simulations inherit the
assumptions of the simulation they use.

## Related topics

- [Choosing a solver](../simulation/solver-selection.md)
- [Solver selection](../core-concepts/solvers.md)
- [Numerical limitations](numerical-limits.md)
- [Electromagnetic solvers](../electromagnetics/solvers.md)
