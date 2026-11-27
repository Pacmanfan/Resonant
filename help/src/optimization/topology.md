---
id: opt.topology
title: Topology optimization
category: Optimization
order: 80
status: experimental
summary: A proof of concept that distributes material in a 2D plane-stress region for the stiffest structure of a given volume (SIMP); the result is a numerical material distribution, not a finished design.
keywords: [topology optimization, SIMP, compliance, density, filter, structure]
related: [opt.geometry, opt.studio, mechanics.structures, opt.constraints]
---

Topology optimization decides **where** material should be, not only how
thick or long a part is. Resonant's implementation is a **proof of concept**:
two benchmark problems in a 2D plane-stress region, one static load case,
minimum compliance for a given volume. It does not use the project's scene,
materials or simulations, and its result must be detailed and re-analysed
before it becomes a design.

## The method

```math
minimize    c(x) = Uᵀ K(x) U
K(x) = Σ_e (E_min + x_e^p (E_0 − E_min)) k_e
subject to  Σ_e x_e = f · N,   0 ≤ x_e ≤ 1
```

`x_e` is the density of element `e` (0 void, 1 solid), `p` the penalty
(SIMP: intermediate densities give little stiffness for their weight), `E_0`
the solid's Young's modulus, `E_min = 10⁻⁹ E_0`, `k_e` the element stiffness,
`U` the displacements, `c` the compliance (J; lower is stiffer), `f` the
volume fraction and `N` the number of elements. The elements are square
bilinear plane-stress elements; a density filter of radius `r_min` smooths
the sensitivities; optimality-criteria updates (move limit 0.2) stop when the
largest density change is below 0.01 or after the iteration limit. The
formulation is that of the widely used educational "88-line" SIMP code,
re-implemented in Resonant.

## Running it

Open the **Topology** tab of the Optimization Studio.

| Field | Range (default) | Meaning |
|---|---|---|
| Benchmark | *Cantilever (load at mid-height)*, *Half MBB beam* | the supports and the load |
| Elements x, Elements y | 4-400 (60), 2-200 (30) | the mesh; elements are 1 mm squares |
| Volume fraction | 0.06-0.94 (0.5) | the share of the region filled with material |
| Penalty | 1-6 (3) | the SIMP exponent `p` |
| Filter radius | mm (0 = 1.5 elements) | `r_min`, held in metres so the result does not depend on the mesh |
| Iterations | 1-2000 (200) | the iteration limit |

The material and load are fixed: E = 70 GPa, ν = 0.33 (aluminium-like), 1 mm
thick, 100 N. The cantilever is clamped on its left edge and loaded downward
at the middle of the right edge; the half MBB beam has a symmetry condition on
its left edge, a roller at the bottom right and a downward load at the top
left.

Press **Run**: the job runs in the background and the density appears as an
image (dark is solid). The text reports the compliance of the uniform design
and the final one, the number of iterations (*not converged* when the limit
was reached), the cleaned design's compliance and volume, the islands
removed, and notes such as the share of intermediate densities.
**Export STL...** saves the cleaned design extruded by the thickness as a
closed voxel surface.

## What the result is, and is not

- **Mesh dependence**: without a filter the result changes with the mesh; a
  filter radius in metres keeps it (40 × 20 and 80 × 40 elements give
  compliances within 10 %).
- **Feature size**: the filter sets the smallest feature at about 2 `r_min`.
- **Intermediate densities** ("grayness", the share between 0.1 and 0.9) are
  reported: no real material has them.
- **Cleanup**: densities are thresholded at 0.5, islands not connected to a
  support are removed, and the binary design is re-analysed.

## Worked example

Run the default cantilever (60 × 30 elements, volume fraction 0.5, penalty 3).
In the automated tests the optimization lowers the compliance below 60 % of
the uniform design's, meets the volume within 0.2 %, gives a design
symmetric about the mid-plane, and the extruded mesh is closed with the
cleaned volume. The finite-element model itself matches beam theory: a
slender cantilever (length 20 times its height) deflects within 6 % of
P L³ / (3 E I).

## Limitations

- Two dimensions, linear, static, one load case, the two benchmarks only.
- No acoustic or electromagnetic topology optimization yet.
- Not connected to the scene: the result is not promoted into a project.

## Related topics

[Geometry optimization](geometry.md) · [The Optimization Studio](studio.md) ·
[Structural objects](../mechanics/structures.md) · [Constraints](constraints.md)
