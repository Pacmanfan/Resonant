# Shape–Function–Energy: Audit of the Current Architecture

How the Shape–Function–Energy direction (`docs/vision/SHAPE_FUNCTION_ENERGY.md`,
RES-SFE-001..012, ADR-000219) maps onto Resonant as it is at release 0.1.0:
what exists and can be reused, what is missing, and the abstractions that
are proposed. Facts are from the code (`src/core`, `src/render`, `src/app`)
and the domain documents; nothing here is implemented by this audit.

## Summary

| Area | Exists | Reuse | Gap |
|---|---|---|---|
| Scene | objects with transforms, geometry and material references; immutable domain components in a registry (ADR-000206) | components carry domain models; the registry is where new functional components go | no notion of *internal structure* (periodic arrangement, inclusions) of an object |
| Geometry | parametric primitives, tessellation for display, `SolidModel` (inside tests for full-wave setups), mesh checks, STL/OBJ export | `SolidModel` and the solvers' setups are the existing "engineering → simulation" conversions; their resolution reports are fidelity reports | no CAD import (the request assumes one; RES-PRODUCT-014 lists it as a V1 non-goal); no explicit boundary-geometry or material-region objects; no unit-cell / lattice description |
| Materials | per-domain property sets with provenance (measured, manufacturer, literature, theoretical, illustrative), validity ranges (frequency, temperature) and frequency tables | provenance and validity are the basis of material state and effective properties | properties are scalars of frequency only: no temperature, stress or field dependence, no anisotropic tensors (wood is stored along the grain), no spatial variation, no thermal property set, no "homogenized" or "computed by tool X" provenance |
| Acoustics | analytical propagation, focusing, inverse design, Kirchhoff reflectors, FDTD (with energy over time, spectra), cavity and waveguide estimates | field results with provenance; FDTD energy record | no Q or wall loss in cavity estimates; no intensity / energy-flux display; no acoustic radiation force |
| Mechanics | finite elements: static, modal (mass-normalized modes, effective mass), harmonic, transient (kinetic, strain, external work, dissipated energy), horns, periodic rods (Bloch), measured data | the transient energy balance is the model for accounting; modal results for the shared resonance vocabulary; Bloch analysis for periodic structures | no thermal expansion, no thermal stress, no 2D/3D unit cells, no nonlinear or plastic material, no geometry update from deformation |
| Electromagnetics | analytical antennas and arrays, lines, rectangular guides and cavities (ideal walls), Yee solver (PEC, periodic, absorbing boundaries; energy over time) | Yee periodic boundaries for unit cells; energy record; cavity modes | no losses converted to heat; no dispersive material models; no Q of cavities; RF only (no optics) |
| Coupling | `coupling/StructuralRadiation` (one-way plate → sound, Rayleigh), the optimization adapter registry, workflow actions | the only place two domains meet (layering rule); the first participant pair | no general coupling interface, no exchanged-quantity type, no transfer operators, no two-way or iterative coupling, no scheduler |
| Simulation framework | jobs (states, progress, cancellation), result metadata, solver registry with phenomena and limitations, compute backends (deterministic threads, CUDA, VOLK) | solver descriptors are where coupling capabilities are declared; jobs run coupled steps | no notion of a run made of several solvers; `Phenomenon` lists acoustic phenomena only |
| Optimization | domain-independent problems, variables, objectives, constraints, algorithms, multi-fidelity, robust, surrogates, inverse design, topology (SIMP) | the function-centric workflow as it is | objectives of coupled or cross-domain quantities need a coupled evaluation adapter |
| Python, automation API | `resonant` package over the core; services, protobuf contract, events, jobs, MCP | new capabilities reach every interface through the services (ADR-000216) | coupling runs, state fields and effective-property results are not in the contract yet |
| Visualization | complex fields on planes and volumes, deformed meshes with scalar colour, patterns, time signals and spectra | layers are the extension point (`render/RenderLayer`) | no vector or flux display, no synchronized multi-quantity probing at one point, no band-structure or unit-cell views |
| Project storage | JSON with schema versions, migration and domain extensions | extensions hold new workspaces (coupling setups) | state fields and coupled results need a storage design (size: HDF5 is available, ADR-000213) |
| Experiments | measured records, calibration, comparison of compatible quantities, uncertainty | validation of coupled and resonant predictions (Q, frequencies, temperatures) | no thermal sensors model; comparison of derived quantities (Q, effective parameters) |
| Help and documents | one Markdown source, status per page (available, experimental, planned, not implemented) | the status vocabulary matches RES-SFE-011 | help topics for the new direction come when capabilities exist |

## Conventions (RES-SFE-012)

Checked consistent across the domains: SI units internally with one display
boundary (`app/DisplayUnits`); time dependence `e^{+jωt}` with peak
amplitudes in acoustics (`acoustics/Conventions.h`), the structure-to-sound
coupling and electromagnetics (`MAXWELL_EQUATIONS.md`); outgoing waves
`e^{-jkr}`; right-handed world coordinates in meters; results carry solver,
version, assumptions and the project revision. No correction was needed for
the coupling work to rely on them.

## Proposed abstractions

All in `src/core` (Qt-free), coupling-specific ones in `src/core/coupling`:

1. **`PhysicalQuantity` descriptor** — a name from a registry
   (`temperature`, `heat_source_density`, `surface_velocity`,
   `acoustic_pressure`, `body_force_density`, `displacement` ...), unit,
   tensor rank, support (volume, surface, points) and time character
   (static, harmonic with its frequency, transient). Lets solvers exchange
   data without knowing each other.
2. **`FieldData` on a `Support`** — sampled values on a grid, mesh nodes,
   mesh cells or points, with the support's geometry; the payload of every
   exchange.
3. **`TransferOperator`** — interpolation, projection, conservative
   remapping between supports, each declaring what it conserves.
4. **`CouplingParticipant`** — the interface an existing solver exposes:
   the quantities it reads and writes, how to advance or solve, its state
   for iteration, its energy terms. The existing solvers are wrapped, not
   rewritten.
5. **`CouplingGraph` and `CouplingScheduler`** — which participant gives
   which quantity to which, and in what order (one-way, sequential,
   iterative with convergence criteria, time-dependent with sub-cycling).
6. **`EnergyLedger`** — terms per participant and exchange, with the
   residual (RES-SFE-007).
7. **`MaterialState` and state-dependent properties** — declared state
   variables and property models of them (RES-SFE-008).
8. **`ResonanceDescriptor`** — the shared description of a resonance that
   each domain's mode type can produce (RES-SFE-004).
9. **`EffectiveProperties`** — results of a homogenization or retrieval
   model with validity and provenance (RES-SFE-005).
10. **`UnitCell` / `Lattice`** — internal periodic structure as engineering
    geometry (RES-SFE-003).
11. **Provenance extensions** — `Computed` (tool, version, input) and
    `Homogenized` (model, cell, frequency range) in addition to the existing
    categories (RES-SFE-009).

## Missing domains and capabilities

A thermal domain (heat conduction with sources, conductivity, capacity,
boundary conditions) is the first missing domain: it connects absorption to
expansion. Others named by the direction (fluid dynamics beyond linear
acoustics, optics, plasticity, damage, phase change, electronic structure)
are later and mostly through external tools.

## Risks for the scientific models

- Coupling errors that look like physics: interpolation that does not
  conserve, time lag in staggered schemes, under-relaxed iterations that
  converge to the wrong answer. Mitigation: conservative operators where a
  quantity must be conserved, ledgers with residuals, convergence studies.
- Homogenization outside its validity (cell not small against the
  wavelength, near band edges, near local resonances, at boundaries).
  Mitigation: validity checks against the full structure.
- Negative or extreme effective parameters retrieved from noisy or
  non-unique data. Mitigation: retrieval with branch selection and passivity
  checks, never assigned by hand.
- State-dependent properties extrapolated beyond their data. Mitigation:
  validity ranges enforced (RES-SFE-008).
- Multiscale chains whose uncertainty is unknown. Mitigation: provenance by
  scale, uncertainty propagation where feasible, experimental comparison.
- Cost: two-way coupled 3D problems multiply solver costs; the compute
  backends and coarse-to-fine strategies matter.

## External packages worth evaluating

Optional, behind adapters; licenses as published by each project, to be
verified in a license audit before use (GPL tools only as separate
processes, so Resonant's own license stays open):

| Purpose | Packages |
|---|---|
| Coupling back end | preCICE (LGPL-3.0) |
| Finite elements for thermal and multiphysics | MFEM (BSD-3-Clause), deal.II (LGPL-2.1 for older releases, Apache-2.0 with the LLVM exception for newer ones: verify), FEniCSx (LGPL-3.0), MOOSE (LGPL-2.1), scikit-fem (BSD-3-Clause, Python) |
| Geometry and CAD import, meshing | Open CASCADE Technology (LGPL-2.1 with an exception), Gmsh (GPL-2.0 or later with exceptions: separate process) |
| Photonics and periodic electromagnetics | Meep and MPB (GPL: separate process) |
| Homogenization | SfePy (BSD-3-Clause) |
| Phonons and lattice dynamics | phonopy (BSD-3-Clause) |
| Atomistic simulation | LAMMPS (GPL-2.0 for the releases known here: verify; separate process), ASE (LGPL-2.1, Python), GROMACS (LGPL-2.1) |
| Electronic structure | Quantum ESPRESSO, CP2K (GPL: separate processes) |
| Materials data | pymatgen (MIT), the Materials Project data (its own terms of use) |

## Recommended sequence

The twelve stages of [the research roadmap](ENERGY_MATTER_RESEARCH_ROADMAP.md),
beginning with quantity conventions and material state, then accounting,
coupling interfaces and the first two-way coupling (electromagnetic loss →
heat → expansion → detuning) once a thermal domain exists.
