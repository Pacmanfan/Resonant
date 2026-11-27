---
id: concepts.meshes
title: Meshes
category: Core Concepts
order: 100
status: available
summary: Resonant builds its own structured meshes, grids and surface samplings from the parametric geometry; there is no general mesh generator, mesh import or mesh editing.
keywords: [mesh, grid, cells, elements, discretization, resolution, staircase]
related: [sim.mesh-resolution, mechanics.structures, acoustics.fdtd, trouble.mesh, concepts.geometry]
---

A numerical solver needs the geometry cut into small pieces. Resonant does
this itself, from the parametric definition of each shape, and differently
for each solver. You never draw or import a mesh; you choose a
**resolution**, and the preflight of each solver tells you what it built.

> **Note** There is no general mesh generator (no tetrahedral meshing, no Gmsh), no mesh refinement, no mesh-quality tool and no import of mesh files. Each solver below uses the structured discretization described for it.

## The discretizations Resonant uses

| Used by | Discretization | Resolution setting | Built from |
|---|---|---|---|
| Viewport | triangle display mesh | fixed (48 segments around a full turn) | each shape's tessellation; never used for a simulation |
| Reflector model | flat surface elements on the reflector | *Surface resolution* in elements per wavelength (2 to 40, default 6), **Acoustics** panel, *Medium* tab | the reflector's equation |
| Acoustic full-wave (FDTD) | uniform Cartesian grid of square or cubic cells | *Cells per wavelength* (4 to 200, default 12), **Full-Wave Solver** panel | an inside test per shape |
| Electromagnetic full-wave (Yee) | uniform cubic cells | *Resolution* in cells per wavelength (default 20), **Electromagnetics** panel, *Fields* tab | an inside test per shape; wires as single cell edges |
| Mechanics | structured finite-element mesh: frame elements, rectangles or hexahedra | *Element size* (*Automatic* or a length), **Properties > Mechanics** | the idealized box, cylinder, plane, horn or rod |

## Grids of the full-wave solvers

The cell size follows from the shortest wavelength in the problem:

```math
Δx = λ_min / N_λ,    λ_min = c_min / f_max
Δt = s · Δx / (c_max · √D)
```

`N_λ` is the cells-per-wavelength setting, `c_min` the slowest speed of
sound (or light) of any material in the grid, `f_max` the highest
significant frequency of the signal, `c_max` the fastest speed, `D` the
number of dimensions (2 or 3) and `s` the time-step fraction of the
stability limit (default 0.95; a larger step is refused).

A cell belongs to an object when its **centre** lies inside the object, so
curved surfaces are stepped ("staircased") at the cell size. Shells and
plates thinner than two cells would leak and are thickened to two cells;
the preflight names each one with both thicknesses. Where objects overlap,
the later object in the scene wins.

The "quality" of such a grid is one number, the cells per wavelength.
Below 10 the preflight warns and states the expected phase-velocity error;
the resolution is never lowered for you, and a plan over the memory limit
(1 GiB by default) is refused.

## Structural meshes

Once you have made an object a beam, plate, membrane or solid (Mechanics
menu), the mechanical solver meshes it with an automatic element size
unless you set one:

| Idealization | Elements | Automatic size |
|---|---|---|
| Beam | 2-node frame elements along the axis | length / 40 |
| Plate | 4-node rectangles | largest side / 24 |
| Membrane | 4-node rectangles | largest side / 32 |
| Solid | 8-node hexahedra | largest side / 20, at least 2 through each side |

The study warns when fewer than 6 elements fall on one structural
wavelength at the highest frequency an analysis asks for, and when fewer
than 4 solid elements span the thinnest side (trilinear solids are too
stiff in bending when coarse). Meshes over 300 000 free degrees of freedom
or the memory limit are refused before they are built, never coarsened.

## Worked example

Open `examples/advanced_acoustics/c-diffraction.resonant` (a rigid plate
with a 5 mm slit, 2D) and press **Check** in the **Full-Wave Solver**
panel. The preflight reports the grid, the cell size, the time step, the
cells per wavelength, the cells of each material and the memory. Raise
*Cells per wavelength* and check again: in 2D the number of cells grows with
its square and the number of time steps in proportion, so the work grows
with its cube; in 3D with its fourth power.

## Exporting meshes

Geometry of optimization candidates can be exported as STL or OBJ, with
mesh integrity and printability checks ([Geometry optimization](../optimization/geometry.md)).
STEP export is not available.

## Limitations

- No unstructured meshes, no local refinement, no partial-cell (conformal) treatment of curved boundaries.
- The finite-element meshes exist only for the idealizations above; other shapes are refused by the mechanical solver with the reason.
- FEM and BEM acoustic solvers, which would need a mesh generator, are not implemented.

See [Mesh and grid resolution](../simulation/mesh-resolution.md) for choosing the
resolution and [Mesh problems](../troubleshooting/mesh.md) when a preflight refuses.
