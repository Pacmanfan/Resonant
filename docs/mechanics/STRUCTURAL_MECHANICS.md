# Structural Mechanics: Idealizations, Elements, Supports, Statics

**Status:** current for REQ-CHAT-20261126-006. Code:
`src/core/mechanics/StructuralModel.*`, `Meshing.*`, `Study.*`.

## Four kinds of geometry

| | Where |
|---|---|
| Engineering geometry | the object's parametric `Geometry` |
| Structural idealization | the `StructureComponent`: beam, plate, membrane or solid, chosen by the user |
| Simulation mesh | a structured mesh built from the geometry for that idealization |
| Display | the deformed surface built from the mesh (`DisplayMesh.h`) |

Nothing is idealized automatically: the user makes an object a beam, plate,
membrane or solid (Mechanics menu). The study explains the representation
("Beam along local X, length 500 mm, rectangle 30 x 5 mm") and warns when
the idealization's assumptions are stretched.

| Idealization | From | Mesh |
|---|---|---|
| Beam | box (along its longest side or a chosen axis), cylinder (along Z), horn and periodic rod (along Z, varying section) | frame elements along the axis; automatic size L / 40 |
| Plate | box (normal to its thinnest side or a chosen axis), plane (with a thickness setting) | n1 x n2 rectangles; automatic size (largest side) / 24 |
| Membrane | as plate | as plate, size / 32 |
| Solid | box | n1 x n2 x n3 hexahedra; automatic size (largest side) / 20, at least 2 through each side |

Other geometry (spheres, cones, reflectors, ...) is refused with the reason.

## Element formulations

**Frame (3D Euler-Bernoulli beam).** 2 nodes, 6 dofs each (`ux, uy, uz,
rx, ry, rz`). Axial `EA/L`, Saint-Venant torsion `GJ/L`, cubic Hermite
bending in two planes (`EIz`, `EIy`), consistent mass for translation and
torsional inertia `rho Ip`, rotary inertia of bending neglected. Local axes
from the element direction and a reference vector. Exact static tip
deflection for any mesh; bending frequencies converge with `h^4` from above.
Sections: rectangle (torsion constant `b t^3 (1/3 - 0.21 r (1 - r^4/12))`,
within 0.5 % of the series), circle, tube; varying sections take the value
at each element's midpoint.

**Plate (Kirchhoff, ACM).** Rectangular 4-node element of Adini, Clough and
Melosh: 12-term polynomial, 3 dofs per node (`w`, rotation about axis 1 =
`dw/dy`, rotation about axis 2 = `-dw/dx`). Stiffness from the curvatures
with `D = E h^3 / (12 (1 - nu^2))`, consistent translational mass, both by
Gauss quadrature (3 x 3 exact for the stiffness integrand, 4 x 4 for mass).
Non-conforming; converges for rectangular meshes, frequencies from below,
second order. Only out-of-plane motion: in-plane forces have nothing to act on
and are reported as lost.

**Membrane.** Bilinear 4-node element, 1 dof (`w`), stiffness `T grad N .
grad N`, mass `rho h N N^T`. The tension `T` (N/m) is a setting, uniform and
constant. All edges held.

**Solid (trilinear hexahedron).** 8 nodes, 3 dofs each, full 2 x 2 x 2 Gauss
integration, isotropic `C`. Passes the patch test. Shear locking: far too
stiff in bending unless several elements span the thickness (a cantilever
with 2 elements through the thickness reaches 34 % of the beam deflection
with 5 elements along it, 97 % with 40). The study warns when fewer than 4
elements span the thinnest side.

**Solid20 (quadratic 20-node hexahedron).** 20 nodes (corners and edge
midpoints), 3 dofs each, 3 x 3 x 3 Gauss integration. Curved geometry to
second order; bends thin layers without the trilinear element's locking
(a cantilever strip with 2 elements through the thickness is within 1 % of
Timoshenko theory). An element may carry its own elasticity matrix and density
(anisotropic, rotated, graded); an invalid matrix is refused. Zero-thickness
interface elements (8 + 8 nodes, 3 x 3 points) join two coincident faces with
normal and shear springs. Built by the layered-body builder:
`LAYERED_SOLIDS.md` (REQ-CHAT-20261128-004, ADR-000232).

## Supports

| Idealization | Positions | Free / Pinned / Clamped |
|---|---|---|
| Beam | start and end of the axis | Pinned holds the translations and the twist about the axis |
| Plate | the four edges | Pinned = simply supported: `w` and the slope along the edge held; Clamped also the slope across |
| Membrane | every edge | always held |
| Solid | the six faces | Pinned and Clamped both hold all translations (no rotations at solid nodes) |

A structure that can move as a rigid body is refused for statics ("not
held"); modal analysis handles rigid-body modes.

## Assembly and solution

Sparse matrices (`Eigen::SparseMatrix`) of the free dofs; identical
elements of a structured mesh share one element calculation. Statics: sparse
LDL^T. Limits: 300 000 free dofs and a memory estimate against the limit
(default 1 GiB); oversized meshes are refused before they are built, never
coarsened.

## Stresses

Only where the formulation yields them:

| | |
|---|---|
| Beam | largest normal stress at the extreme fibre of either element end, axial plus bending (vector sum for circular sections); torsional shear not included |
| Plate | bending stress at the surface at the element centre, plane stress, von Mises |
| Solid | stress at the element centre, von Mises |
| Membrane | none (only its pre-tension) |

## Stated assumptions (in every result)

Linear elasticity, small displacements and strains, no contact, plasticity,
fracture or prestress other than membrane tension; isotropic materials; plus
the idealization's own theory (Euler-Bernoulli, Kirchhoff, membrane,
trilinear solid). Warnings: beam length / section size below 10, plate side
/ thickness below 20, shells thickened, fewer than 6 elements per structural
wavelength at the highest frequency an analysis asks for.
