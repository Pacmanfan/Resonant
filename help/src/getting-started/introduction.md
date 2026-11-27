---
id: gs.introduction
title: Introduction to Resonant
category: Getting Started
order: 10
status: available
summary: What Resonant is for, which wave domains it covers today, and how a typical study goes from a scene to a result you can trust.
keywords: [overview, introduction, wave field, domains, workflow, examples]
related: [gs.installation, gs.new-project, gs.interface, concepts.scene, concepts.solvers]
---

Resonant is a desktop **Wave Field Engineering Studio**: you place sources,
structures and measurement points in a 3D scene, run real physics on it,
look at the field, change one thing and run it again. Its motto is
*Design. Simulate. Focus. Measure.*

Every result says which model produced it and what that model leaves out.
Resonant never picks a solver for you and never switches to a different one
silently, so what you see is always the answer of a model you chose.

## What you can do today

| Domain | You build | You calculate | Start with |
|---|---|---|---|
| Acoustics | emitters, phased arrays, target points, probes, field planes and volumes | free-field pressure, phase, SPL and intensity; focusing; inverse design | [Acoustic arrays](../acoustics/arrays.md) |
| Wave manipulation | parabolic, elliptical, spherical, trough and flat reflectors; lenses; waveguides; cavities; a slit plate | reflector fields (Kirchhoff approximation), ray paths, analytical estimates | [Wave manipulators](../core-concepts/manipulators.md) |
| Full-wave acoustics | the same scene, with materials | time-domain FDTD in 2D and 3D: diffraction, reflection, refraction, cavities, pulses and spectra | [Full-wave simulation](../acoustics/fdtd.md) |
| Mechanics | beams, plates, membranes and solids made from geometry; actuators; sensors | natural frequencies and mode shapes, frequency responses, transients, static deformation, the sound a plate radiates | [Structural objects](../mechanics/structures.md) |
| Electromagnetics | dipoles, monopoles, loops, ideal sources, RF arrays, waveguides, lines | impedance, gain, patterns, near fields, steering and tapers, Yee FDTD | [Antennas](../electromagnetics/antennas.md) |
| Optimization | design variables, objectives, constraints | searches over candidate designs with the physics above | [The Optimization Studio](../optimization/studio.md) |
| Experiments | experiments with real or simulated instruments | acquisition, calibration, measurements set beside the simulation | [Acquisition](../experiments/acquisition.md) |

Python scripting and guided demonstrations are described in
[The Python console](../python/console.md) and
[Demo Studio and presenter mode](../ui/demo-studio.md).

## How a study goes

1. **Build a scene.** Add objects from the domain menus (Geometry, Acoustics, Wave Manipulators, Mechanics, Electromagnetics). Each object appears in the **Scene** panel and is edited in the **Properties** panel. See [Scene architecture](../core-concepts/scene.md).
2. **Set the physics.** Choose the propagation medium and frequency in the **Acoustics** panel, assign [materials](../core-concepts/materials.md), turn a shape into a beam or plate, set a signal.
3. **Say where to look.** Add a field plane or volume ([Simulation regions](../core-concepts/regions.md)), probes, sensors or target points.
4. **Choose and run a solver.** Each solver has its own command, for example **Acoustics > Run Field Simulation** (F5) or **Simulation > Run Full-Wave Simulation** (F6). A preflight estimate comes first; the run is a background job you can cancel. See [Solver selection](../core-concepts/solvers.md).
5. **Analyse.** Results appear in the viewport and in the **Analysis** panel with their model, settings and assumptions.
6. **Iterate.** Change the design by hand, or let the [Optimization Studio](../optimization/studio.md) search for you.

Results belong to the session: a project file stores the scene, the
settings and a record of each run, and you recompute fields after reopening
it ([Saving and loading projects](saving.md)).

## Example projects

The `examples` folder of the source tree holds ready-to-open projects for
every area. Each one is loaded and checked by the automated tests, so they
always match the current version. Good first projects:

| Project | What it shows |
|---|---|
| `examples/foundation/primitives.resonant` | the seven primitive shapes, a group, material assignments |
| `examples/acoustics/c-focused-planar-array.resonant` | an 8 x 8 array focused on a point 100 mm away (press F5) |
| `examples/manipulators/a-parabolic-reflector.resonant` | a rigid parabolic reflector concentrating an emitter's sound |
| `examples/mechanics/b-vibrating-plate.resonant` | a simply supported aluminium plate and its modes |
| `examples/electromagnetics/a-half-wave-dipole.resonant` | a 1 GHz dipole: 73.08 + j42.51 ohm, directivity 1.64 |

## What Resonant is not

- It is not a CAD package: geometry is built from parametric shapes, and importing CAD or mesh files is not implemented yet ([Geometry](../core-concepts/geometry.md)).
- It does not choose a solver or hide what a model ignores. The analytical acoustic solver, for example, ignores every object that is not a reflector, and each result says so.
- Example material values are illustrative, not authoritative ([Materials](../core-concepts/materials.md)).
- Only Linux builds have been built and run; Windows and macOS are untested, and there is no binary installer yet ([Installation and first launch](installation.md)).

> **Note** Every page of this help carries a status badge. *Available* means implemented and tested; *experimental* means it works with stated gaps; *planned* and *not implemented* pages describe what is missing and the nearest alternative.

## Where to go next

- [Installation and first launch](installation.md) to build and start Resonant.
- [Creating a project](new-project.md) for a first focused-array study in ten steps.
- [Interface overview](interface.md) and [Navigation and selection](navigation.md) to find your way around.
- [Units and coordinate systems](units.md) before you type your first number.
