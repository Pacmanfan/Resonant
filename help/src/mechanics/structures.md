---
id: mechanics.structures
title: Structural objects
category: Mechanical Engineering
order: 20
status: available
summary: Make a box, cylinder, plane or rod a beam, plate, membrane or solid, see how it will be meshed, and calculate its static deformation.
keywords: [beam, plate, membrane, solid, idealization, finite elements, static]
related: [mechanics.boundary-conditions, mechanics.materials, mechanics.modal, mechanics.actuators, concepts.geometry]
---

A structure is a scene object whose geometry you have chosen to analyse as
an elastic body. Resonant never idealizes geometry automatically: you
decide whether an object is a **beam**, a **plate**, a **membrane** or a
**solid**, and the application explains what that means for the analysis.

## Four views of one object

| Layer | What it is |
|---|---|
| Engineering geometry | the object's parametric shape (box, cylinder, plane, horn, periodic rod) |
| Structural idealization | beam, plate, membrane or solid, chosen by you |
| Simulation mesh | a structured mesh built from the geometry for that idealization |
| Display | the deformed surface drawn from the mesh results |

## Making an object structural

1. Add the geometry (for example **Geometry > Box**) and set its size in the
   **Properties** panel.
2. With the object selected, choose **Mechanics > Make Beam**, **Make
   Plate**, **Make Membrane** or **Make Solid**.
3. The **Mechanics** panel opens with the object in its **Structure** list
   and a description of the study.

The starting supports are a cantilever (beam clamped at the start of its
axis), a simply supported plate (all four edges pinned) and a solid clamped
on one face. An object without a material gets *Aluminum*; a plane gets a
thickness of 2 mm.

| Idealization | Accepted geometry | Elements | Automatic element size |
|---|---|---|---|
| Beam (Euler-Bernoulli) | box (along its longest side or a chosen axis), cylinder (along Z), horn, periodic rod | 2-node 3D frame, 6 dofs per node | length / 40 |
| Plate (Kirchhoff) | box (normal to its thinnest side), plane with a thickness | 4-node ACM rectangle, out-of-plane bending | largest side / 24 |
| Membrane (pre-tensioned) | as plate | 4-node bilinear, transverse displacement | largest side / 32 |
| Solid (3D elasticity) | box | 8-node trilinear hexahedron | largest side / 20, at least 2 through each side |

Spheres, cones, reflectors and other shapes are refused with the reason.

## The Structure properties

Selecting a structure shows a **Structure** group in the Properties panel:

- **Idealization**: the four choices above.
- **Axis**: *Automatic* or Local X / Y / Z (the beam axis or plate normal).
- **Element size**: *Automatic* or a length.
- **Thickness**: *From the shape* or a value (plates and membranes made from a plane).
- **Membrane tension**: uniform pre-tension in N/m (default 100 N/m).
- **Supports**: see [Boundary conditions](boundary-conditions.md).
- **How this object is analysed**: the summary, section properties, notes and warnings.

The Mechanics panel's **Check** button repeats the study for the structure
chosen in its list: the attached actuators, sensors and targets, the
number of elements per structural wavelength at the top of the frequency
sweep and the transient time step.

## Element formulations

- **Beam**: axial stiffness EA/L, Saint-Venant torsion GJ/L and cubic
  Hermite bending in two planes; shear deformation and rotary inertia are
  neglected. Static tip deflections are exact for any mesh.
- **Plate**: bending stiffness D from [Mechanical materials](materials.md);
  only out-of-plane motion, so in-plane forces have nothing to act on.
- **Membrane**: stiffness from the tension T, mass ρh; every edge is held.
- **Solid**: full 2 × 2 × 2 integration. It passes the patch test but
  locks in bending: a cantilever with 2 elements through the thickness
  reaches 34 % of the beam deflection with 5 elements along it and 97 % with
  40. The study warns when fewer than 4 elements span the thinnest side.

## Static deformation

The **Static** tab's **Calculate Static Deformation** (also **Mechanics >
Calculate Static Deformation**) applies each active actuator's amplitude
times the cosine of its phase as a constant force and solves K u = f. The
result gives the largest displacement, the compliance f·u and the largest
stress where the formulation yields one:

| Idealization | Stress reported |
|---|---|
| Beam | largest normal stress at an element end's extreme fibre (axial plus bending; no torsional shear) |
| Plate | surface bending stress at the element centre, von Mises |
| Solid | stress at the element centre, von Mises |
| Membrane | none |

A structure that can move as a rigid body is refused ("not held").

**Worked example.** Open `examples/mechanics/c-forced-resonance.resonant`
(an aluminium cantilever 500 × 30 × 5 mm, 1 N at the tip, 40 beam
elements). The study describes "Beam along local X, length 500 mm,
rectangle 30 x 5 mm". The static result is the beam-theory deflection
P L³ / (3 E I) = 1.93 mm and a root stress P L c / I = 4.0 MPa.

## Warnings to heed

The study warns when a beam's length is less than 10 times its section, a
plate's side less than 20 times its thickness, and when there are fewer
than 6 elements per structural wavelength at the highest frequency an
analysis asks for.

## Limitations

- Linear elasticity, small displacements and strains, isotropic materials.
- No shells, curved members, frames of several members, trusses, assemblies,
  layered or composite structures, contact, plasticity or prestress other
  than membrane tension.
- Structured meshes of boxes, cylinders, planes and rods only; no mesh
  generation for arbitrary geometry.
- At most 300 000 free degrees of freedom and a memory estimate within the
  limit (default 1 GiB); an oversized mesh is refused, never coarsened.
