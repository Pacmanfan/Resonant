# ADR-000224: Multiscale material modeling: representations and lineage in the project, built-in reference calculations, external engines as adapters, sequential and iterative coupling

**Status:** ACCEPTED
**Date:** 2026-11-27
**Type:** architecture
**Requirements:** `docs/requirements/multiscale.md` RES-MSC-001..014
**Request:** REQ-CHAT-20261126-020
**Builds on:** ADR-000223 (effective materials), ADR-000220 (coupled engine), ADR-000219 (multiscale direction, RES-SFE-009)

## Context

Resonant derived effective materials from organized structures (ADR-000223)
but had no representation of a material's microstructure, atoms or electrons,
no record of how a library value was obtained beyond its provenance and
source text, and no way to go from a macroscopic result back to a smaller
scale. No atomistic or electronic-structure engine is installed in the
reference environment (no LAMMPS, OpenMM, Quantum ESPRESSO, CP2K, GPAW or
ASE); installing them needs administrator rights.

## Decisions

1. **A multiscale model in the project** (extension `multiscale`,
   `src/core/multiscale`, no physics): representations (scale, kind, data,
   constituents, state, assumptions, boundary conditions, provenance, content
   revision, validation, uncertainty, model card), relationships and
   derivation records. Library materials stay the one material database;
   property values gain a derivation id and a standard uncertainty, and the
   provenances computed, database, fitted and surrogate (contract 7–10).
2. **Microstructures as voxel RVEs** (generators are seeded and stored as
   specifications, so an RVE is reproducible from its record; imported voxel
   data is stored as given) and one homogenization solver for the steady
   potential problem div(K grad u) = 0 (trilinear finite elements, per-voxel
   tensors, periodic / linear / mixed conditions) — math in `multiscale`,
   mapped to thermal conductivity, electrical conductivity and static
   permittivity in `coupling`.
3. **Atomistic models as a domain** (`src/core/atomistic`): crystals,
   imports (XYZ, POSCAR), force-field records, Lennard-Jones lattice statics
   and NVE molecular dynamics as *reference implementations* for pair
   potentials, validated against lattice sums and conservation laws. They do
   not replace a molecular-dynamics package.
4. **External engines are adapters** run as separate processes (the plugin
   or Python-worker path of `MULTISCALE_MODELING.md`): listed with detection
   and requirements, never linked, never shown as integrated without an
   adapter and a validation. The electronic scale has a representation and
   an interface only.
5. **A derivation pipeline** in `coupling/MultiscaleDerivations`: methods
   with source and target scales, applicability checks, jobs, convergence and
   uncertainty, assignment to a library material as one undoable step;
   staleness from the source representation's revision, the constituents'
   content and the stored value.
6. **Coupling strategies:** sequential (atomistic → elastic constants →
   continuum; RVE → effective → continuum) and iterative (a region's
   temperature from the coupled engine re-homogenizes its RVE until the
   states agree) are implemented, each with a coupling contract; FE² and
   concurrent coupling are recorded requirements.

## Consequences

- Elastic homogenization of RVEs, polycrystal stiffness, molecular topologies
  beyond pair potentials, imports of CIF and PDB, and any electronic-structure
  result remain planned.
- Projects without the extension, and property values without a derivation,
  are unchanged.
