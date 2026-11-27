---
id: mechanics.boundary-conditions
title: Boundary conditions
category: Mechanical Engineering
order: 40
status: available
summary: Hold the ends of a beam, the edges of a plate or the faces of a solid as free, pinned or clamped.
keywords: [support, clamped, pinned, free, cantilever, simply supported, rigid-body modes]
related: [mechanics.structures, mechanics.modal, mechanics.forced-response, sim.boundaries]
---

Supports decide how a structure is held. Each idealization has its own
support positions, and each position is **Free**, **Pinned (simply
supported)** or **Clamped**. Supports are edited in the **Structure**
group of the Properties panel; every change is undoable and the study text
below the supports is updated at once.

## Support positions

| Idealization | Positions shown | Default when made structural |
|---|---|---|
| Beam | *Start of the axis*, *End of the axis* | clamped start, free end (a cantilever) |
| Plate | *Edge at -axis 1*, *Edge at +axis 1*, *Edge at -axis 2*, *Edge at +axis 2* | all four pinned |
| Membrane | none: every edge is always held | |
| Solid | *Face -X*, *Face +X*, *Face -Y*, *Face +Y*, *Face -Z*, *Face +Z* | Face -X clamped |

Axis 1 and axis 2 are the plate's in-plane axes; the study text names them
for the object.

## What each choice holds

| Idealization | Pinned | Clamped |
|---|---|---|
| Beam | the three translations and the twist about the axis | all six degrees of freedom |
| Plate | the deflection w and the slope along the edge (simply supported) | also the slope across the edge |
| Solid | all translations of the face's nodes | the same: solid nodes have no rotations |

**Free** holds nothing.

## Effect on the natural frequencies

For an Euler-Bernoulli beam the bending frequencies follow

```math
f_n = (β_n L)² / (2π L²) · √(E I / (ρ A))
```

where L is the length, E I the bending stiffness, ρ A the mass per unit
length and β_n L a constant of the support case:

| Supports (start / end) | β₁ L | First mode of example A |
|---|---|---|
| Clamped / Free (cantilever) | 1.8751 | 16.33 Hz |
| Pinned / Pinned | π | 45.85 Hz |
| Clamped / Pinned | 3.9266 | 71.62 Hz |
| Clamped / Clamped | 4.7300 | 103.9 Hz |
| Free / Free | 4.7300 (after six rigid-body modes) | 103.9 Hz |

The last column is the bending mode through the 5 mm thickness of the
aluminium beam in `examples/mechanics/a-vibrating-beam.resonant`
(500 × 30 × 5 mm). Change *Start of the axis* and *End of the axis* and
run **Calculate Modes** again to see it. The beam solver is validated
against this formula in four support cases, first four modes, within
0.01 %.

## Free structures

A structure that is not held can move as a rigid body.

- **Modal analysis** handles it: the solver shifts slightly below zero so
  the factorization succeeds, and the rigid-body modes appear near 0 Hz (six
  for a free 3D beam). Ask for more modes so the elastic ones are included.
- **Static analysis** is refused with "not held": a free structure has no
  unique static deformation.
- **Forced response and transient analysis** work (examples E, G and H are
  free-free rods and horns).

## Common mistakes

- **Expecting supports on a membrane.** A membrane is held at every edge;
  its support settings do not apply and the study says so.
- **Pinned solids.** Pinned and Clamped are the same for a solid; a hinge
  cannot be represented by holding translations only.
- **Plate edges confused.** Check the study text for which edge is axis 1.

## Limitations

- Supports act on whole ends, edges or faces. Point supports, partial
  edges, springs or elastic mounts, isolation mounts, symmetry planes and
  contact are not implemented.
- Prescribed (non-zero) displacements are not implemented; the only drive
  is a force ([Actuators](actuators.md)).
- The boundary conditions of the acoustic and electromagnetic solvers are a
  different subject: see [Boundary conditions in simulations](../simulation/boundaries.md).
