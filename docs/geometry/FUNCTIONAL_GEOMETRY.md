# Functional Geometry

**Status:** the representations that exist are described as current; the
rest is planned (RES-SFE-003, ADR-000219).

A **functional geometry** is an object whose shape and internal structure
are designed to produce a physical response: a cavity, reflector, lens,
waveguide, horn, channel, impedance transformer, resonator, phononic or
photonic crystal, layered composite, porous structure, metamaterial cell,
resonator network or engineered interface. Its function is computed, not
declared (RES-SFE-001): the geometry only has to be represented faithfully
enough for the model that computes it.

## Six representations

| Representation | What it is | In Resonant today |
|---|---|---|
| 1. Visual geometry | display meshes for the viewport | tessellation of each geometry kind (`geometry/Geometry`), `render/GpuMesh` |
| 2. Engineering geometry | the design: parametric definition (or, later, imported CAD) with dimensions and tolerances | parametric primitives (box, sphere, cylinder, cone, plane) and the domain components' parametric shapes (reflectors, lenses, cavities, horns, antennas, plates); no CAD import (RES-PRODUCT-014 non-goal so far) |
| 3. Physical boundary geometry | the surfaces and boundary conditions a solver sees | implicit in each solver's setup (rigid, pressure-release, impedance and absorbing faces and material cells in FDTD; PEC, periodic and absorbing faces in Yee; supports in finite elements) |
| 4. Material regions | which material fills which volume | an object's material reference; `geometry/SolidModel` inside tests decide membership for grid solvers |
| 5. Simulation discretization | grids and meshes | FDTD and Yee grids (`FdtdSetup`, the Yee setup), finite-element meshes (mechanics model builders), Kirchhoff surface sampling |
| 6. Internal structure | periodic or functional arrangement inside an object | periodic rods (mechanics); otherwise none |

The rule: **a visual mesh is never used as simulation geometry**.
`SolidModel` is derived from the shape's mathematical definition, never from
its display mesh, and states when the simulation geometry differs from the
design (for example shells thickened to what a grid can resolve). This is the
pattern every conversion follows.

## Conversion with fidelity

```text
engineering geometry ──► conversion for solver S ──► discretization + fidelity report
                         (boundaries, regions,        resolution per wavelength, features lost or thickened,
                          mesh or grid)               staircase error, volume / area deviation, warnings
```

Existing fidelity reports: the FDTD and Yee preflight (cells per wavelength,
memory, time steps), `SolidModel` notes, mesh checks before export
(`geometry/MeshChecks`). Planned: one fidelity record per conversion,
attached to the result that used it, with a pass / warn / fail judgment
against the solver's needs.

## Planned additions

- **Boundary and region objects**: explicit surfaces with conditions
  (impedance, conductivity, temperature, fixed, free, periodic, Bloch) and
  named material regions inside an object, shared by the domains instead of
  being re-derived in each setup.
- **Internal structure**: `UnitCell` (cell geometry with its regions) and
  `Lattice` (Bravais vectors, extent, grading) to place structure inside an
  object; instantiation into a finite structure, or analysis as one cell
  with Bloch conditions ([metamaterials](../materials/METAMATERIALS_AND_ENGINEERED_STRUCTURES.md)).
- **Geometry updates from physics**: a displacement field producing a
  deformed engineering geometry for the next coupled step, with its own
  fidelity check ([coupling](../architecture/COUPLED_MULTIPHYSICS_ARCHITECTURE.md)).
- **Tolerances**: dimensional tolerances as distributions for robust
  optimization (already supported for design variables).
- **Imported geometry** (CAD formats, meshes) arrives as engineering
  geometry with a declared fidelity, never as simulation-ready by default;
  Open CASCADE is the candidate kernel (see the audit).

## Function-centric geometry design

The optimization framework already varies parametric dimensions (reflector
and lens geometry, plate and horn dimensions, antenna sizes) and does
topology optimization (SIMP). Functional geometry adds unit-cell and lattice
parameters and coupled objectives to the same machinery (RES-SFE-002).
