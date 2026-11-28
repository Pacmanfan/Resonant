# Atomistic Integration

**Status:** current (REQ-CHAT-20261128-006, RES-MSC-015..017, ADR-000234).
Code:
- `src/core/coupling/EngineAdapters.{h,cpp}` (the coupling layer: the adapters
  run the atomistic domain);
- `src/core/multiscale/MicrostructureAnalysis.{h,cpp}`;
- `src/core/multiscale/ExternalSolvers` (detection).

Tests: `tests/core/test_engine_adapters.cpp`.

## No engine is required

No external atomistic engine is installed on the development machine.
Installing one needs administrator rights, which is the owner's decision.
Engines are optional, never linked, and run as separate processes. A missing
engine is reported with what is missing, and no result is invented.

## Adapters

| Engine | Applies to | Properties | Not for | In this build |
|---|---|---|---|---|
| `resonant.lattice-statics` (in-house) | crystals of the built-in pair potentials (noble-gas solids) | elastic constants, density, cohesive energy (0 K) | finite temperature; metals, covalent and ionic solids; liquids and transport | runs: relaxes the lattice, differentiates the energy at two strains (the difference is the stated numerical uncertainty) |
| `lammps` | crystalline and amorphous solids, metals (EAM, MEAM), ceramics (Tersoff, ReaxFF), polymers, liquids, interfaces and defects | elastic constants, density, thermal conductivity, diffusion, interface and defect energies | electronic structure and optics; anything outside the potential's fitted range | prepares static plus-or-minus-strain decks for orthogonal cells with the built-in pair potentials, and parses their `RESONANT-STRESS` output into a stiffness; runs only when `lmp`, `lmp_serial` or `lmp_mpi` is found (the run path is not exercised here) |
| `gromacs` | biomolecules, polymers, liquids, soft matter | density, diffusion, viscosity | elastic constants of crystalline solids; defect and interface energies; electronic structure | applicability only: no input generation |

The engines are not interchangeable. The potential decides validity more than
the engine does, and every adapter refuses tasks outside its applicability.

### The other engines

The external-solver list (`multiscale/ExternalSolvers`, the automation call
that lists engines) detects these, but none has an adapter yet. Each covers
a different scale, and none substitutes for another:

| Engine | Scale | Domain of applicability | Not for |
|---|---|---|---|
| OpenMM | molecular | molecular dynamics of biomolecular and organic systems, GPU acceleration | inorganic crystals with many-body potentials; electronic properties |
| Quantum ESPRESSO | electronic | plane-wave DFT: ground-state energies and stresses (elastic constants from small cells), band structures, phonons, dielectric response | large cells, long times, finite-temperature transport |
| CP2K | electronic / atomistic | Gaussian-and-plane-wave DFT, ab initio molecular dynamics of a few hundred atoms | the length and time scales of classical molecular dynamics |
| GPAW | electronic | real-space and plane-wave DFT through ASE | the same limits as other DFT codes |
| phonopy | atomistic (post-processing) | phonon dispersion and density of states, heat capacity, quasi-harmonic thermal expansion, from forces another engine computes | anything without a force calculator |

An adapter for any of them implements the same interface: applicability,
availability, prepare and run. Its results cross to the continuum through the
same bridge record.

## The bridge record

A `TransferredProperty` carries everything a continuum value needs:
- the quantity, its representation and component order, its unit and the
  axes of a tensor;
- temperature, pressure and composition;
- the simulation conditions: ensemble, relaxation, strain, cell, cutoff;
- the uncertainty, with its kind (numerical or statistical);
- the engine and its version, the force field, the method, an input digest,
  and the provenance (`computed`).

Voigt orders differ between modules. The atomistic module uses xx yy zz yz xz
xy and the mechanics xx yy zz xy yz zx, so the bridge converts explicitly.
`toMaterialField` turns a record into a computed, constant material field of
a region. Its provenance states the engine, the force field, the temperature,
and that the tensor's axes are taken as the region's.

## Microstructure

Of a voxel RVE (`multiscale/Microstructure`), the statistics are:
- phase fractions and porosity;
- grains and their volume-equivalent mean diameter;
- grain- and phase-boundary area densities, from voxel faces: a staircase
  estimate;
- separate inclusions (face-connected);
- recorded defects, as metadata with provenance, not computed.

The conduction problem with a grain-boundary resistance R uses a diffuse
interface. Each voxel next to another grain carries R / 2 per such face in
series along that face's axis. This is exact for planar boundaries normal to
an axis, an approximation otherwise. Its homogenization
(`multiscale/PotentialHomogenization`) gives a bicrystal L / (L / k + 2R)
across its boundaries and k along them.
