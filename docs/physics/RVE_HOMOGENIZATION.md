# RVE Homogenization

**Status:** current (REQ-CHAT-20261126-020). **Requirements:** RES-MSC-005..007. **Code:** `src/core/multiscale/Microstructure`, `PotentialHomogenization`.

## Microstructures

Voxel boxes of phases (`VoxelRve`), grains with unit c axes. Generators
(seeded with a 64-bit Mersenne twister read bit-exactly, so identical on
every platform), all periodic:

| Generator | Structure |
|---|---|
| `homogeneous` | one phase |
| `layered` | layers along an axis with given fractions and periods |
| `spheres` | non-overlapping spheres (random sequential addition with periodic images) up to a volume fraction of 0.5 |
| `voronoi` | periodic Voronoi grains; c axes uniform on the sphere (`random`) or Gaussian about an axis (`aligned`, `spreadDeg`) |
| `voxels` | imported phase (and grain) indices; must state whether it is periodic |

Generated RVEs are labelled synthetic ("not a measured microstructure").

## The steady potential problem

div(K grad u) = 0 with K a tensor per voxel: heat conduction, electric
conduction and electrostatics are the same problem. Trilinear hexahedra on
the voxel grid, 2x2x2 Gauss points, conjugate gradients with an incomplete
Cholesky preconditioner (relative residual 1e-10).

| Boundary | Condition | Note |
|---|---|---|
| periodic | u = G.x + phi, phi periodic | needs a periodic structure; refused otherwise |
| linear (kinematic uniform) | u = G.x on the whole boundary | upper estimate |
| flux (static uniform) | K grad u . n = Q.n on the whole boundary | lower estimate; K_eff = <grad u>^-1 over Q = e1, e2, e3 |
| mixed | u = G.x on the faces normal to G, no flux elsewhere | between flux and linear |

For one RVE, flux ≤ periodic ≤ linear and flux ≤ mixed ≤ linear. The
energy (Hill–Mandel) residual is reported for every load case.

## Oriented grains

A phase may be uniaxial: `{"axial": material, "transverse": material}`; in a
grain with axis c, K = k_t (I − c cᵀ) + k_a c cᵀ.

## Size studies, realizations and uncertainty

`sizes` scales the RVE at constant voxel size (grain counts scale with
volume); `realizations` repeats a random generator with consecutive seeds.
The mean of the largest size is the derived value; its standard error is
the uncertainty. *Representative* means the relative spread and the change
from the previous size are below `tolerance` (default 0.02). One realization
of a random RVE is marked not reliable: its spread is unknown.

## State dependence

`temperatureLaws` of an RVE make a constituent's property linear in
temperature; `temperatureK` in the settings evaluates them (the iterative
coupling sets it from the macroscopic region).

## Limits

Interfaces thinner than a voxel, interface resistance (unless modeled as a
phase), convection or radiation in pores, nonlinear constituents within one
solve, elastic homogenization (planned) and FE2 (planned).
