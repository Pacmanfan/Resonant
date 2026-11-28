# Functional Structure Architecture

**Status:** implemented (REQ-CHAT-20261126-017, RES-FUNC-001..016, ADR-000221).
The direction is [Shape–Function–Energy](../vision/SHAPE_FUNCTION_ENERGY.md);
the audit is [FUNCTIONAL_STRUCTURE_AUDIT.md](FUNCTIONAL_STRUCTURE_AUDIT.md).

A functional structure is an engineered object whose geometry, materials,
excitation and environment are configured to produce a measurable response.
It is a project resource (extension `functional`), not a shape category: it
names scene objects by role and the analysis that computes what they do.

```text
                         geometry + materials + excitation + environment + boundaries
                                                     │
 FunctionalStructure ── parameters (bound / free / derived) ──► scene objects (engineering geometry)
   │ bodies, regions, boundaries, domains, frequency range                       │
   │ analysis {kind, model, settings}                                            ▼
   │ objectives, constraints (manufacturing, physical)           analysis kind (registry) ── model
   │ physicalSystem (optional, CEME)                                             │  (domain code)
   ▼                                                                             ▼
 studies: analyze │ sweep │ sensitivity │ optimize (optimization framework, adapter "functional")
   ▼
 result document: metrics with units, modes, curves, behaviour, assumptions, warnings,
                  objectives, constraints, dependencies (content keys), provenance
```

## Modules

```text
src/core/functional     no physics, no domain, no coupling (layering rule)
  FunctionalStructure   the document, validation, storage in the project
  Parameters            resolve and apply bound, free and derived parameters
  Dependencies          content keys, staleness, the simulation-geometry cache
  Analysis              the registry interface: kinds, models, metrics, roles, outcomes
  FunctionalStudy       analyze, sweep, sensitivity, mode tracking, objectives, constraints, explanations
  EffectiveMaterial     the effective material of an effective-medium analysis and its library form

src/core/coupling
  FunctionalAnalyses    the analyses (cavity, optical resonator, horn, mechanical, photonic cell, periodic rod, coupled slab)
  ResonanceAnalyses     Helmholtz, cantilever and coupled resonators
  MetamaterialAnalyses  the effective-medium analyses (ADR-000223)
  FunctionalAdapter     the optimization adapter "functional"

domains                 CavityResonator, OpticalResonator, PeriodicLayers (electromagnetics), Horn + horn.acoustic (manipulators);
                        existing: mechanics studies and Bloch, layered media, the coupled engine
src/core/services       FunctionalStructureService (15 operations, with CreateEffectiveMaterial)
proto/resonant/v1/functional    the contract
src/app                 FunctionalController, FunctionalPanel (dock "Functional Structures")
```

## Geometry representations (RES-FUNC-002)

| Representation | Where | In a result |
|---|---|---|
| visual | the viewport's tessellation of the scene objects | — |
| engineering | scene objects and their geometry parameters | `representations.engineering`, the bound parameters |
| physical | the structure's regions (materials by role) and boundaries | `representations.regions`, `.boundaries` |
| simulation | what a model builds: closed form, a Yee grid (cells, rounded size), a finite-element mesh (grid, element size), Webster steps | `representations.simulation`, `discretization` |
| functional | the structure's parameters, objectives and constraints | `parameters`, `objectives`, `constraints` |

[Geometry parameterization](../geometry/GEOMETRY_PARAMETERIZATION.md) describes parameters and revisions.

## Analyses

| Kind | Category | Models | Domain code |
|---|---|---|---|
| `em.cavity.rectangular` | confinement | closed form with perturbational losses; Yee full-wave | `electromagnetics/CavityResonator` ([page](../electromagnetics/CAVITY_RESONATORS.md)) |
| `optics.resonator.two-mirror` | confinement | paraxial Gaussian beams | `electromagnetics/OpticalResonator` ([page](../electromagnetics/OPTICAL_RESONATORS.md)) |
| `acoustic.horn` | propagation | Webster equation, baffled-piston mouth | `manipulators/Horn` ([page](../manipulators/ACOUSTIC_HORNS.md)) |
| `mechanics.modal` | confinement | finite elements; closed-form beams and plates | `mechanics/Study`, `MechanicalOptimization` |
| `em.periodic.layers` | reflection | exact Bloch dispersion and transfer matrices | `electromagnetics/PeriodicLayers` ([page](../electromagnetics/PERIODIC_LAYERS.md)) |
| `mechanics.periodic-rod` | reflection | exact longitudinal dispersion | `mechanics/Bloch` |
| `multiphysics.coupled-slab` | coupled | the Coupled Energy–Matter Engine (slabs from boxes, cavities from cavity objects) | `coupling/CoupledRun` |
| `acoustic.helmholtz` | confinement | lumped with end corrections; plane-wave stepped duct | `manipulators/Helmholtz` ([page](../manipulators/HELMHOLTZ_RESONATORS.md)) |
| `mechanics.cantilever-resonator` | confinement | Euler–Bernoulli modes, first mode as an oscillator (ring-down) | `mechanics/LumpedResonators` |
| `mechanics.coupled-cantilevers` | confinement | two tip-referred oscillators and a spring | `mechanics/LumpedResonators`, `oscillators` ([page](../physics/COUPLED_RESONATORS.md)) |
| `acoustic.coupled-helmholtz` | confinement | lumped acoustic network | `manipulators/Helmholtz`, `oscillators` |
| `mechanics.mass-in-mass-lattice` | effective-medium | closed form, cell Bloch eigenproblem, finite chain | `mechanics/LocallyResonantLattice`, `oscillators/BlochLattice` ([metamaterials](../materials/METAMATERIAL_ARCHITECTURE.md)) |
| `acoustic.resonator-duct` | effective-medium | cell transfer matrices with shunt Helmholtz resonators | `manipulators/ResonatorDuct` |
| `em.layered-effective-medium` | effective-medium | uniaxial mixing vs exact TE/TM stacks; retrieval | `electromagnetics/ObliqueLayers`, `Retrieval` |
| `em.split-ring-array` | effective-medium | quasi-static LC circuit (an approximation) | `electromagnetics/SplitRing` |
| `thermal.layered-conductivity` | effective-medium | series/parallel conductances vs finite volumes | `thermal/LayeredConductivity` |

Each kind declares its mechanism (used in explanations), its models with
fidelity, assumptions and validity, its metrics with units and definitions,
and the roles it needs. Settings may name a structure parameter instead of a
number (`"reflectivity1": "R"`), so free parameters drive models.
[Energy transformation](../physics/ENERGY_TRANSFORMATION.md) maps the request's
categories to these analyses; [resonant structures](../physics/RESONANT_STRUCTURE_ENGINEERING.md)
covers resonance across domains.

## Studies, design and optimization

Sweeps (grids up to 400 points, each on its own copy of the design),
central-difference sensitivities, mode tracking, objectives, constraints
and explanations: [function-driven design](../optimization/FUNCTION_DRIVEN_DESIGN.md).

## Coupled behavior (RES-FUNC-013)

A structure that names a physical system (`physicalSystem`) is analyzed by the
coupled engine; regions of the system linked to box objects take their
thickness from the box, so a structure parameter bound to the box drives the
coupled run (`examples/functional/f-coupled-slab.resonant`). Structures
without feedback use their single-domain analysis; the cavity's operating
temperature, for example, scales the dimensions with the wall's expansion
coefficient (one-way) without a coupled run.

## Services, contract and clients (RES-FUNC-014)

`FunctionalStructureService`: `ListAnalysisKinds`, `ListFunctionalStructures`,
`GetFunctionalStructure`, `SetFunctionalStructure` and `DeleteFunctionalStructure`
(undoable), `GetStructureParameters`, `SetStructureParameters` (undoable, one
step for the scene and the structure, optimistic concurrency by
`expected_revision`), `ValidateFunctionalStructure`, `AnalyzeStructure`,
`SweepStructure`, `ComputeStructureSensitivity` (jobs), `GetFunctionalResult`
(with the current staleness; the full document only on request),
`OptimizeStructure` (a job of the optimization framework) and
`ExplainCandidates`. Python uses them through `resonant_api`; MCP offers them by
level (read, modify, execute); capabilities list each analysis. The GUI panel
calls the same services through the GUI's session.

## The workflow in the application (RES-FUNC-015)

```text
 open or create a project ─► objects in the scene ─► a structure (examples, Python, the API)
        │                                                   │
        ▼                                                   ▼
 Functional Structures panel: choose the structure ─► edit parameters (undoable scene changes)
        ─► choose the model ─► Analyze (job) ─► metrics, modes, model and assumptions, staleness
        ─► curves of the result (view only) ─► Sweep a parameter (job) ─► metric against the parameter
        ─► optimize and explain (API / Python) ─► promote a candidate by setting its parameters
```

Creating and editing structure documents is done through the API and Python
(or by example projects); the panel inspects, edits parameters, analyzes and
sweeps. Viewport overlays of functional results are not part of this work.

## Limits

No CAD import; no topology optimization of structures; the cavity analyses
cover rectangular cavities; the optical model is paraxial; the horn model is
one-dimensional; force and momentum metrics are not computed. See
`IMPLEMENTATION_STATUS.md`.
