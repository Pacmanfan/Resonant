# Multiscale Material Modeling

**Owner of:** `RES-MSC-001..017`
**Source:** REQ-CHAT-20261126-020 (Prompt 22, "Multiscale Material Modeling")
**Refines:** `shape_function_energy.md` RES-SFE-009 (multiscale integration), RES-SFE-005 (effective properties); `metamaterials.md` RES-META-011
**Decisions:** ADR-000224 (builds on ADR-000223, ADR-000220, ADR-000219)
**Documents:** `docs/materials/MULTISCALE_ARCHITECTURE.md`, `docs/materials/MATERIAL_LINEAGE.md`, `docs/physics/RVE_HOMOGENIZATION.md`, `docs/physics/ATOMISTIC_MODELS.md`, `docs/physics/MULTISCALE_VALIDATION.md`, `docs/physics/MULTISCALE_MODELING.md` (updated)
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-MSC-001 — Distinct representations per scale

Material representations at the electronic, atomistic, molecular,
microstructure, effective and continuum scales are distinct entities, each
with an identity, a scale, a kind, its structural data, constituents, the
physical state (temperature, pressure), assumptions, boundary conditions,
provenance, a content revision, a validation status, uncertainty notes and a
model card. Existing projects without them are unchanged.
*Verification:* `MultiscaleModel.*`.

### RES-MSC-002 — Navigable relationships

Representations and materials are related explicitly (derived from,
constituent of, homogenizes, computed from); a chain from a continuum
material down to the lowest representation it came from is navigable.
*Verification:* `MultiscaleModel.RelationshipsFormANavigableChain`, `MultiscaleServices.LineageNavigatesToTheSource`.

### RES-MSC-003 — Material lineage

A derived property records its derivation: source representation and its
revision, scale, method (model card), solver and version, inputs, boundary
conditions, temperature and pressure, convergence, resolution, sampling,
uncertainty, the calculation's identity and date. Property values carry the
derivation id and a standard uncertainty; provenance distinguishes measured,
manufacturer, literature, theoretical, illustrative, homogenized, computed,
database, fitted and surrogate values. Lineage survives save and reload.
*Verification:* `MultiscaleModel.LineageSurvivesSaveAndReload`, `Materials.*`.

### RES-MSC-004 — Model cards

Every representation kind and derivation method has a model card: the
physical system, scale, assumptions, equations or methods, required
parameters, excluded effects, valid conditions and how to interpret results.
*Verification:* `MultiscaleModel.EveryMethodHasAModelCard`.

### RES-MSC-005 — Microstructures and RVEs

Voxel microstructures with several phases, grains with crystallographic
orientations, inclusions and pores; procedural generators (homogeneous,
layered, random spheres, pores, Voronoi grains with random or textured
orientation) that are seeded, deterministic and labelled synthetic; imported
voxel data labelled imported.
*Verification:* `Microstructure.*`.

### RES-MSC-006 — Computational homogenization

Effective conductivity tensors (thermal conductivity, electrical
conductivity, static permittivity: the same steady potential problem) from
a voxel microstructure by finite elements with periodic, linear (kinematic
uniform) or mixed boundary conditions; periodic conditions only for
periodic microstructures; the energy (Hill–Mandel) consistency reported;
results bracketed by the bounds that apply.
*Verification:* `PotentialHomogenization.*`.

### RES-MSC-007 — RVE size, realizations and uncertainty

Effective properties are studied over RVE sizes and random realizations; the
mean, the standard deviation and whether the RVE is representative (relative
spread below a tolerance) are reported, and the spread becomes the derived
value's uncertainty.
*Verification:* `MultiscaleDerivations.RveStudyConvergesAndGivesAnUncertainty`.

### RES-MSC-008 — Atomistic representations and force fields

Crystals (lattice vectors, basis, species), supercells, builders (sc, bcc,
fcc, diamond) and import of XYZ (plain and extended) and POSCAR files with
validation and no fabricated defaults; force-field records with identity,
version, parameter source, applicability, validity and limitations; applying
a force field to species outside its applicability warns.
*Verification:* `Atomistic.*`.

### RES-MSC-009 — Built-in atomistic calculations

Pair-potential (Lennard-Jones) lattice statics with tail corrections:
cohesive energy, equilibrium lattice parameter, elastic constants (Voigt),
Born stability, cutoff sensitivity; NVE molecular dynamics with energy
conservation, time-step sensitivity, radial distribution and mean squared
displacement. A completed run and a reliable result are distinguished.
*Verification:* `Atomistic.*`.

### RES-MSC-010 — External engines and the electronic scale

Candidate integrations (LAMMPS, OpenMM, Quantum ESPRESSO, CP2K, GPAW, phonopy)
are listed with their detection, requirements and status; none is presented
as integrated without an adapter and validation; the electronic scale has a
representation and interface but no calculation without a real solver.
*Verification:* `MultiscaleModel.ExternalSolversAreListedAndNeverClaimed`.

### RES-MSC-011 — Derivation pipeline and staleness

Derivation methods declare their source and target scales, required data,
applicable properties and model card; a request is checked for applicability
(unsupported transitions refused), executed as a job, assessed for
convergence and uncertainty, and assigned to a library material as one
undoable step with its derivation record. Changing the source representation
or a constituent, or overriding the value, marks the property stale.
*Verification:* `MultiscaleDerivations.*`, `MultiscaleServices.*`.

### RES-MSC-012 — Downscaling and coupling strategies

The local state of a region (temperature mean, range and gradient) is
extracted from a coupled result and configures a lower-scale calculation;
sequential and iterative coupling are implemented (an RVE re-homogenized at
the region's temperature until the macroscopic and microscopic states
agree); every coupling has a contract (scales, transferred quantities,
assumptions, convergence, failure handling, uncertainty, provenance); FE² and
concurrent coupling are documented requirements, not implemented.
*Verification:* `MultiscaleServices.IterativeCouplingReachesAFixedPoint`.

### RES-MSC-013 — Integration with the engineering workflows

Derived materials are ordinary library materials: the coupled engine,
functional structures and the metamaterial analyses use them; one test runs
source representation → calculation → derived property → material →
engineering simulation.
*Verification:* `MultiscaleExamples.*`, `MultiscaleServices.*`.

### RES-MSC-014 — Services, clients and examples

`MultiscaleService` (representations, generators, imports, methods, model
cards, couplings, external solvers, derivation jobs, results, assignment,
lineage, local state, iterative coupling), Python and MCP; the Material
Scales panel shows representations, relationships, model cards and a
material's lineage; example projects for a crystal, a polycrystal, a porous
composite, a multiscale metamaterial and a cross-scale investigation.
*Verification:* `MultiscaleServices.*`, `python/tests/test_multiscale_api.py`, `MaterialScalesPanel.*`, `MultiscaleExamples.*`.

### RES-MSC-015 — Atomistic engine adapters

Atomistic engines are reached through adapters. Each adapter states:
- its engine's applicability: material classes, properties, and what it is
  not for;
- whether the engine can run here, and if not, what is missing.

It prepares the engine's input for a task and runs it. Engines are optional
and never linked. A missing engine is reported, and no result is invented.
Adapters exist for:
- the in-house static lattice (elastic constants, density, cohesive energy
  at 0 K);
- LAMMPS (strain-deformation decks and their output parser, run only when
  found);
- GROMACS (applicability only).

*Verification:* `EngineAdapters.EachEngineStatesWhatItIsForAndWhetherItCanRun`, `-LammpsInputsArePreparedAndNothingIsInventedWithoutTheEngine`, `-TheLammpsLogParserRecoversTheStiffness`, `-GromacsIsDeclaredForSoftMatterOnly`.

### RES-MSC-016 — The atomistic-to-continuum bridge

A property computed at the atomistic scale crosses to the continuum as a
record. The record carries:
- its representation and component order, its units and its frame;
- temperature, pressure, composition and simulation conditions;
- its uncertainty and its kind;
- the engine, version, force field, method and an input digest.

Component orders are converted explicitly. The record becomes a computed
material field of a region.
*Verification:* `EngineAdapters.TheInHouseLatticeCrossesToTheContinuumWithItsProvenance`.

### RES-MSC-017 — Microstructure statistics and boundary resistance

The statistics of a voxel microstructure are:
- phase fractions and porosity;
- grains and their mean size;
- grain- and phase-boundary area densities;
- separate inclusions;
- recorded defects.

A conduction problem with a grain-boundary resistance feeds the
homogenization, so the microstructure changes the continuum conductivity.
*Verification:* `Microstructure.GrainBoundariesAreCountedAndResistInSeries`, `-PorosityInclusionsAndDefectsAreReported`.

