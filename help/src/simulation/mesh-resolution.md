---
id: sim.mesh-resolution
title: Mesh and grid resolution
category: Simulation
order: 30
status: available
summary: How finely each solver samples space: field-region samples, reflector surface elements, full-wave cells per wavelength and mechanical element sizes, with the warnings and the costs.
keywords: [resolution, cells per wavelength, sampling, element size, grid, staircasing, dispersion]
related: [sim.accuracy, sim.convergence, sim.resources, concepts.meshes, acoustics.fdtd]
---

Every solver samples space, and the sampling decides both the accuracy and
the cost. Resonant states the resolution of every run in wavelengths, warns
when it is coarse, refuses a run that would be too large, and **never lowers
the resolution by itself**.

There is no general mesh generator: the full-wave solvers use uniform
Cartesian grids and the mechanical solver structured meshes of boxes,
cylinders, plates and rods. Gmsh or another mesher is not integrated.

## Analytical field regions

The analytical models are exact at each sample; the sampling only decides
how well the picture shows the field. Set it on the region: **Samples U**,
**Samples V** (and **Samples W** for a volume) over **Size X/Y/Z**. The
Simulation tab of the Acoustics panel shows the spacing and the samples per
wavelength.

| Samples per wavelength | Message |
|---|---|
| below 2 | the interference pattern is aliased in the picture; the values at the samples are still correct |
| below 6 | the picture will look coarse |
| above 200 | the resolution adds cost without adding information |

Example: example C's plane, 120 × 160 mm with 241 × 321 samples, has 0.5 mm
spacing: 17.2 samples per 8.58 mm wavelength.

## Reflector surfaces

With **Free field + reflectors**, each reflector is cut into surface
elements no larger than the wavelength divided by the **Surface resolution**
(2 to 40 elements per wavelength, default 6), generated from the exact
equation, not from the display mesh. Near a parabolic reflector's focus, 8
elements per wavelength are within 1 % of 16, and 4 within 5 %.

## Full-wave acoustic grid

The cell size is

```math
Δx = λ_min / N,      λ_min = c_min / f_max
```

`N` is **Cells per wavelength** (4 to 200, default 12), `c_min` the slowest
speed of sound of any material in the grid and `f_max` the highest
significant frequency of the signal: `f` for a continuous signal, `f(1 + 2/cycles)`
for a tone burst, `f(1 + b)` for a Gaussian pulse, `2.5 f` for a Ricker
wavelet. A broadband signal costs more than its centre frequency suggests.
At 40 kHz in air, 12 cells per wavelength give 0.72 mm cells.

The grid wave travels slightly too slowly (numerical dispersion). Along a
grid axis, with `S = c Δt / Δx` (0.67 in 2D by default):

| Cells per wavelength | 6 | 8 | 10 | 12 | 16 | 20 | 30 |
|---|---|---|---|---|---|---|---|
| Phase velocity error | 2.8 % | 1.5 % | 0.9 % | 0.64 % | 0.36 % | 0.23 % | 0.10 % |

The error accumulates: at 12 cells per wavelength a wave is a quarter
wavelength late after about 40 wavelengths. Below 10 cells per wavelength the
preflight warns with the size of the error.

**Staircasing.** A cell belongs to an object when its centre lies inside the
body, so curved and inclined surfaces are stepped at the cell size. For
resonators and focusing structures this is often the larger error; it falls
with the first power of the cell size. Shells and plates thinner than two
cells are represented two cells thick, and the preflight names each one.

## Full-wave electromagnetic grid

The Yee solver uses the same idea: **Resolution** in cells per wavelength in
the slowest material at the highest frequency of the signal (default 20,
warning below 10). Thin wires are single edges with an effective radius of
about 0.23 cells, whatever their real radius. See
[Electromagnetic solvers](../electromagnetics/solvers.md).

## Mechanical meshes

An idealized structure gets a structured mesh. Its **Element size** (in the
**Mechanics** group of the property editor) is *Automatic* at 0, which
means:

| Idealization | Automatic size |
|---|---|
| Beam | length / 40 |
| Plate | largest side / 24 |
| Membrane | largest side / 32 |
| Solid | largest side / 20, at least 2 elements through each side |

The study warns when the highest requested frequency has fewer than 6
elements per structural wavelength, and when fewer than 4 solid elements span
the thinnest side (solids are far too stiff in bending otherwise). See
[Structural objects](../mechanics/structures.md).

## Choosing a resolution

1. Start from the default and read the preflight: cells per wavelength,
   phase error, memory and time.
2. Raise the resolution where geometry matters (curved surfaces, cavities)
   or where waves travel many wavelengths.
3. Check that the result does not change at a finer resolution
   ([Convergence](convergence.md)).

Cost grows quickly: halving the cell size multiplies the cells by 4 in 2D
and 8 in 3D, and the time steps by 2.
