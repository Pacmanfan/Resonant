---
id: trouble.mesh
title: Mesh problems
category: Troubleshooting
order: 50
status: available
summary: What to do when a structure cannot be meshed, a mesh is too coarse or too large, a thin wall is thickened on the grid, or an exported mesh is not closed.
keywords: [mesh, element size, grid, staircase, refinement, STL]
related: [concepts.meshes, sim.mesh-resolution, sim.convergence, trouble.solver-failures, ref.import-export]
---

Resonant builds every mesh and grid itself, from the parametric definition
of each geometry object. There is no general mesh generator (no Gmsh), no
adaptive refinement and no import of meshes or CAD files in this version, so
most mesh problems come down to the **resolution setting** of the solver and
to the **kind of geometry** a solver accepts.

## Which mesh a calculation uses

| Calculation | Mesh | Resolution setting |
|---|---|---|
| Mechanics: beam | Euler-Bernoulli frame elements along the axis | **Element size** (Properties, Mechanics group) |
| Mechanics: plate, membrane | rectangular grid in the plate's plane | **Element size** |
| Mechanics: solid | trilinear hexahedra filling a box | **Element size** |
| Acoustic full-wave (FDTD) | uniform grid over the field region | **Cells per wavelength** (Full-Wave Solver panel) |
| Electromagnetic full-wave (Yee) | uniform cubic grid over the field region | **Resolution**, cells / wavelength (Fields tab) |
| Reflector model | surface elements on each reflector | **Surface resolution**, elements / wavelength (Acoustics panel) |
| Viewport, STL and OBJ export | triangle display meshes | fixed tessellation of each primitive |

## A structure cannot be meshed

The Mechanics panel's **Check** reports `CANNOT BE ANALYSED:` with one of:

- `a <kind> cannot be idealized as a beam; beams are made from boxes, cylinders and rods`
- `a <kind> cannot be idealized as a plate: the rectangular elements need a box or a plane`
  (the same for a membrane)
- `solid elements need a box; a <kind> cannot be meshed with hexahedra here`
- `a plane has no thickness: set the structure's thickness`
- `the mesh would have about N degrees of freedom, more than the 300 000 this build accepts; use a larger element size`

Any object with geometry can be made structural from the **Mechanics**
menu; the check then refuses a shape its idealization does not accept
(spheres, cones, ellipsoids, reflectors, ...). Model the part as one of the
accepted shapes.

## The mechanical mesh is too coarse

An **Element size** of 0 chooses one automatically: the length / 40 for a
beam (at least 4 elements), the largest side / 24 for a plate and / 32 for a
membrane, the largest dimension / 20 for a solid. **Check** states the mesh
(`N elements, N nodes, N degrees of freedom (N free); element size ... mm`)
and the elements per bending or shear wavelength at the top of the sweep:

```text
At 2000 Hz (top of the sweep): 4.2 elements per bending wavelength of the plate - too coarse, refine the mesh or lower the range.
```

Below 6 elements per wavelength results are flagged, and a modal result
says `At the highest mode (... Hz) the mesh has only ... elements per ...:
the higher modes are inaccurate. Refine the mesh.` Halve the element size
and compare: a converged frequency changes little.

Two warnings concern the element type, not the size:

- `Fewer than 4 elements span the thinnest dimension: trilinear hexahedra are far too stiff in bending (shear locking).`
  Refine through the thickness, or use a plate or beam idealization.
- `The plate is thick (smaller side / thickness = ..., below 20): Kirchhoff theory neglects transverse shear and overestimates its frequencies.`

## Surfaces on a full-wave grid

Curved and inclined surfaces are stepped at the cell size ("staircasing"):
a surface lies where the grid puts it, within one cell. For resonators and
focusing structures this is often the largest error, and it falls only in
proportion to the cell size. Walls thinner than the grid resolves are
thickened, and the preflight says so:

```text
'Reflector': its thickness of 0.5 mm is below what the grid resolves and is represented as 1.43 mm.
```

The minimum is two cells (1.43 mm at 40 kHz in air and 12 cells per
wavelength). Raise **Cells per wavelength** until the wall is
represented as designed, or accept the stated thickness. In the Yee solver
an object not aligned with an axis of the grid is left out with a warning,
and a wire only a few cells long is flagged as barely resolved.

## Reflector surface resolution

The reflector model divides each reflector into surface elements, by
default 6 per wavelength (2 to 40). The preflight states the count
(`Reflector model: N surface element(s) at 6 per wavelength ...`). Check a
focus by running again at twice the resolution: if the peak and the focal
widths do not change, the surface is resolved.

## An exported mesh is not closed

**Export...** on the **Candidates** tab of the Optimization Studio writes an
STL and an OBJ file per changed object and records, for each, whether the
mesh is closed, its volume, its boundary edges and its overhang fraction.
A mesh with boundary edges has holes and is not a solid for fabrication.
These checks are not a manufacturing approval.

> **Note** STL and OBJ files from Resonant are in **metres**. Many CAD and
> slicing programs assume millimetres: scale by 1000 on import.

## Related topics

- [Meshes](../core-concepts/meshes.md)
- [Mesh and grid resolution](../simulation/mesh-resolution.md)
- [Convergence](../simulation/convergence.md)
- [Solver failures](solver-failures.md)
