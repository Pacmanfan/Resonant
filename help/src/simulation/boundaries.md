---
id: sim.boundaries
title: Boundary conditions in simulations
category: Simulation
order: 60
status: available
summary: The faces of a computational domain and the surfaces of objects: rigid, pressure-release, impedance and absorbing boundaries, perfectly matched layers, and how to tell a real reflection from an artefact.
keywords: [boundary, absorbing layer, pml, rigid, pressure release, impedance, reflection]
related: [acoustics.fdtd, acoustics.reflectors, mechanics.boundary-conditions, em.solvers, sim.accuracy]
---

Two different things are called boundaries in a simulation: the **faces of
the computational domain**, where the calculation stops, and the **surfaces
of objects** inside it. Only the full-wave solvers have domain faces; the
analytical models calculate in unbounded space.

## Analytical acoustic models

The **analytical free field** has no boundaries at all: the medium is
infinite and objects are ignored. The **reflector model** gives each
reflector a **Boundary** (manipulator editor):

| Boundary | Reflection coefficient |
|---|---|
| *Rigid (reflection +1)* | +1 |
| *Pressure release (reflection -1)* | −1 |
| *Impedance of the assigned material* | `(Z cos θ − ρc) / (Z cos θ + ρc)`, `Z` = ρc of the material |
| *Absorbing (no reflection)* | 0 |

## Faces of the acoustic full-wave domain

In the **Full-Wave Solver** panel, **Domain boundaries** sets one kind for
all faces (the project file can hold one kind per face):

| Kind | Condition | Use |
|---|---|---|
| *Absorbing layers (open space)* | perfectly matched layer outside the region | open space; the default |
| *Rigid walls* | normal velocity = 0 | a closed hard enclosure; a symmetry plane of an even field |
| *Pressure-release walls* | pressure = 0 | a free water surface seen from the water |
| *Impedance walls* | `p = Z v_n`, locally reacting, **Wall impedance** in Pa s/m | a lossy wall |

Measured at normal incidence: rigid `R` = +1 and pressure release −1 within
1 %; impedance `3ρc` gives 0.5, `ρc/3` gives −0.5, `ρc` gives 0. An
impedance wall follows

```math
R(θ) = (Z cos θ − ρc) / (Z cos θ + ρc)
```

so `Z = ρc` absorbs fully only at normal incidence: use absorbing layers for
open space. Periodic boundaries are not implemented.

## Absorbing layers (PML)

The layers are added **outside** the region (**Absorbing layer thickness**,
default 12 cells, 4 to 200), so the region is not shortened. In the layer the
pressure is split into one part per direction and each part is damped along
its own direction only:

```math
σ(d) = σ_max · (d / L)²,      σ_max = −3 c · ln(R₀) / (2 L),      R₀ = 10⁻⁶
```

`d` is the depth into the layer, `L` its thickness and `R₀` the design
reflection at normal incidence. Measured: a plane pulse at normal incidence
reflects below 0.2 % (−54 dB) with 12 cells and below 0.05 % with 24; 8
cells reflect more. A point pulse in a small domain with layers matches open
space within 1 % of the peak near walls and corners.

Keep sources and structures **a few cells away** from the layers: evanescent
fields and grazing waves are absorbed less. Long runs with layers (beyond
about 2 000 steps) have not been tested; the energy record would show a
slow growth.

## Telling a reflection from an artefact

1. Look at **Energy over time** in the **Time Signals** tab: in an open
   domain it falls once the sound has left.
2. Change the **Absorbing layer thickness** (for example 12 → 24 cells) and
   run again. A physical reflection does not move; a boundary artefact
   changes.

## Surfaces of objects in the full-wave solver

The cells of an object are **rigid**, **pressure release** or **material**
(a fluid with the object's density and speed of sound). A manipulator's
**Boundary** chooses among them; *Impedance of the assigned material* means
the wave enters the material, which is more than a locally reacting surface.
An *Absorbing* manipulator surface cannot be represented on the grid: it is
simulated as rigid and the preflight says so in capitals. A lens is always
its material; any other object is its material when it has acoustic data,
rigid otherwise.

## Electromagnetic full-wave domain

The Yee solver's faces are absorbing layers (split-field PML, design
reflection 1e-6, 10 cells by default: **Absorbing layer** in the Fields
tab), perfect electric conductors, or periodic pairs. The panel sets the
layer thickness; the other kinds are per-face entries of the project's
solver settings. Good conductors inside the domain are perfect conductors:
the electric field on their cell edges is held at zero. See
[Electromagnetic solvers](../electromagnetics/solvers.md).

## Mechanical supports

Structures are held by supports on their ends, edges or faces (*Free*,
*Pinned (simply supported)*, *Clamped*), set in the property editor. A
structure that can move as a rigid body is refused for static analysis. See
[Boundary conditions](../mechanics/boundary-conditions.md).

## Worked example

`examples/advanced_acoustics/b-reflection.resonant`: an emitter 50 mm above a
rigid plate, absorbing domain faces, continuous signal. Run it (**F6**): in
front of the plate a standing wave forms with minima half a wavelength apart,
the first a quarter wavelength from the plate; behind the plate is a shadow.
Set **Domain boundaries** to *Rigid walls* and run again: the preflight now
warns that a reflecting face with a continuous signal keeps building up the
field, and the pattern fills with reflections from the domain walls.
