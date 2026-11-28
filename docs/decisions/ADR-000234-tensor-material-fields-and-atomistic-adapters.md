# ADR-000234: Tensor material fields, atomistic engine adapters, and microstructure statistics

**Status:** ACCEPTED
**Date:** 2026-11-28
**Requirements:** `docs/requirements/architected_matter.md` RES-FAM-021; `docs/requirements/multiscale.md` RES-MSC-015..017
**Request:** REQ-CHAT-20261128-006 (Prompt 27, Part III.3 and Part IV)
**Builds on:**
- ADR-000228 (functionally architected matter: scalar property fields along one axis);
- the multiscale model of RES-MSC-001..014 (representations, lineage, voxel RVEs, an external-solver list that only detected executables);
- ADR-000232 (layered 3D solids).

## Context

Before this decision, material fields were scalar: one property of a region
along one axis of the stack. Nothing represented:
- anisotropic or tensor-valued fields, their coordinate systems, or their
  component bases;
- interpolation of measured or imported data.

No atomistic engine is installed (LAMMPS, GROMACS and the electronic-structure
codes are absent; installing them needs administrator rights). The in-house
atomistic module computes static-lattice properties of pair-potential
crystals. Voxel RVEs held phases and grains but no boundary or porosity
statistics, and grain boundaries could not affect a homogenized property.

## Decision

1. **Material fields** (`materials/MaterialField`, a domain-free module).
   - **Properties:** density; stiffness (6x6, mechanics Voigt order);
     thermal and electrical conductivity; permittivity; piezoelectric `e`
     (3x6); permeability.
   - **Representation:** each field states its rank and its frame
     (Cartesian, cylindrical, spherical), and whether its components are in
     the frame's local basis. Tensors rotate into the object's axes (Bond
     matrix for stiffness and coupling).
   - **Form and source:**
     - the form is a blend of two tensors along one coordinate (linear,
       power law, exponential, table), or sampled;
     - sampled data is interpolated by nearest, inverse-distance weighting
       or piecewise linear, all convex, so they keep positive definiteness;
     - the source (analytic, measured, imported, computed) is stated
       separately from the form;
     - there is no extrapolation beyond the samples or the blend's range.
   - **Refusals:** invalid tensors are refused, never repaired.
   - **Use:**
     - the layered 3D solid reads stiffness, density and a tensor
       conductivity;
     - the piezoelectric continuum reads `e`, the permittivity and `c^E`,
       element by element at the centre;
     - architectures store them (`materialFields`). Only analyses that
       declare `tensor-field` read them; the others refuse with the reason.
2. **Engine adapters** (`coupling/EngineAdapters`, in the coupling layer
   because they run the atomistic domain).
   - **Interface:** applicability (material classes, properties, what an
     engine is not for), availability (what is missing), input preparation,
     running.
   - **In-house static lattice:** runs elastic constants, density and
     cohesive energy at 0 K, with a finite-difference uncertainty.
   - **LAMMPS:** prepares static plus-or-minus-strain decks (restart-based)
     for orthogonal cells and parses their output into stiffness. It runs
     only when found, and otherwise refuses: "no result is invented".
   - **GROMACS:** states its applicability (soft matter, liquids,
     biomolecules) only.
   - The external-solver list names the adapters and keeps "not
     integrated".
3. **The bridge record.** A `TransferredProperty` carries:
   - its representation and order (Voigt orders are converted explicitly:
     the atomistic module's xx yy zz yz xz xy against the mechanics xx yy
     zz xy yz zx), units and frame;
   - temperature, pressure, composition and simulation conditions;
   - its uncertainty and its kind;
   - the engine and its version, the force field, the method and an input
     digest.

   It becomes a computed material field of a region.
4. **Microstructure statistics** (`multiscale/MicrostructureAnalysis`):
   - phase fractions, porosity, grains and mean grain size;
   - grain- and phase-boundary area densities, separate inclusions;
   - defect records as metadata;
   - a conduction problem with a grain-boundary (Kapitza) resistance: each
     boundary-adjacent voxel carries R / 2 per face in series along that
     axis. This is exact for planar boundaries normal to an axis and an
     approximation otherwise. It feeds the existing homogenization, so the
     microstructure changes the continuum conductivity.

## Consequences

- **Validation:**
  - frames orthonormal and right-handed; blends and refusals; interpolation
    without extrapolation; a cylindrical-basis tensor rotated (trace
    invariant);
  - a constant stiffness field gives the plain body to 1e-12, and a graded
    field the graded layer;
  - anisotropic conduction uses k_zz only;
  - the in-house elastic constants cross to the continuum, and a cube
    strains by their compliance;
  - the LAMMPS deck is complete, and its parser recovers a stiffness from a
    labelled synthetic fixture;
  - a bicrystal's conductivity is L / (L / k + 2R) across, and k along;
  - matter fields reach the 3D solid (a fourfold density halves the modes)
    and are refused by lamination theory.
- **Limits:**
  - fields are element-constant in the solids;
  - a region with a conductivity field still needs its scalar conductivity
    for the capability's required properties;
  - the LAMMPS run path is not exercised here (not installed);
  - GROMACS has no input generation;
  - microstructure defects are recorded, not computed;
  - voxel boundary areas are staircase estimates.
