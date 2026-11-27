# Topology Optimization (Proof of Concept)

`mechanics/TopologyOptimization` distributes material in a 2D plane-stress
design region for the stiffest structure of a given volume:

```text
minimize   c(x) = U^T K(x) U,   K(x) = sum_e (E_min + x_e^p (E_0 - E_min)) k_e
subject to sum_e x_e = f N,  0 <= x_e <= 1
```

* SIMP (solid isotropic material with penalization), penalty p = 3 by
  default, E_min / E_0 = 1e-9;
* square bilinear (Q4) plane-stress elements with the closed-form element
  stiffness, thickness t, Eigen sparse Cholesky;
* a density filter of radius r_min (in metres; default 1.5 element sizes)
  with the filtered sensitivities;
* optimality-criteria updates (move limit 0.2, bisection on the volume
  multiplier); stop when the largest density change is below 0.01 or after
  the iteration limit.

The formulation is that of the 88-line code of Andreassen, Clausen,
Schevenels, Lazarov and Sigmund (2011), re-implemented on Eigen.

## Benchmarks

| Benchmark | Supports | Load |
|---|---|---|
| cantilever | left edge clamped | downward at the middle of the right edge |
| half MBB beam | x held on the left edge (symmetry), roller at the bottom right | downward at the top left |

## What the result is - and is not

The result is a numerical material distribution, not a finished design:

* **mesh dependence** — without a filter the result changes with the mesh; a
  filter radius held in metres keeps it (40 x 20 and 80 x 40 elements give
  compliances within 10 %, `OptimizationTopology.AFilterRadiusInMetresKeepsTheResultMeshIndependent`);
* **feature size** — the filter sets the smallest feature at about 2 r_min;
* **intermediate densities** — the share of elements between 0.1 and 0.9
  ("grayness") is reported; no real material has them;
* **cleanup** — the densities are thresholded at 0.5, islands not connected
  to a support are removed, the binary design is re-analysed (its compliance
  is reported), and the load path is checked;
* **export** — the cleaned design extruded by the thickness as a closed
  voxel surface (STL), with the note that it must be detailed and
  re-analysed before manufacture;
* two-dimensional, linear, static, one load case; no acoustic or
  electromagnetic topology optimization yet.

## Validation

* the finite-element model against beam theory: a slender cantilever
  (L / h = 20) deflects within 6 % of P L^3 / (3 E I) (Q4 elements are stiff
  in bending; shear deformation adds a little), `OptimizationTopology.TheFiniteElementModelMatchesBeamTheory`;
* the optimization lowers the compliance below 60 % of the uniform design's,
  meets the volume within 0.2 %, gives a design symmetric about the
  mid-plane for the symmetric cantilever load, and the extruded mesh is closed
  with the cleaned volume (`OptimizationTopology.SimpReducesComplianceMeetsTheVolumeAndIsCleanedForExport`).

In the Studio: the Topology tab (benchmark, elements, volume fraction,
penalty, filter radius, iterations; 1 mm elements, aluminium-like E = 70 GPa,
nu = 0.33, 1 mm thick, 100 N) runs as a background job and shows the density.
