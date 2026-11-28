# Architected Matter Audit

**Status:** the Phase 0 audit behind REQ-CHAT-20261127-001 (RES-FAM-001, ADR-000228),
2026-11-28. It records what existed and what the architecture reuses rather than
duplicates.

| Area | What existed | What was missing |
|---|---|---|
| Materials (`materials/Material`) | one scalar `PropertyValue` per domain and key: provenance, source, validity, frequency table, derivation, uncertainty | tensors, orientation, composition, phases; revisions (copies under new ids) |
| Scene (`scene/SceneObject`) | one `materialId` per object; box, cylinder and other kinds; groups | several regions per object; layered kinds |
| Electromagnetics | `em::LayeredSolution` from layers given inline or from regions of the coupled engine | a stack shared with the other solvers |
| Thermal | `thermal::ConductionProblem` on rectilinear grids; layered conductivity; harmonic-mean cell conductances | any contact resistance (perfect contact everywhere) |
| Mechanics | isotropic elastic materials; Kirchhoff plate elements; periodic rods | laminates, plies, orientation, coatings; any piezoelectric model |
| Effective properties | `functional::EffectiveMaterial`; two-phase layered analyses of metamaterials; `multiscale::homogenize` on voxel RVEs; `twoPhaseBounds` | estimators for compositions with their bounds |
| Coupled engine | `PhysicalSystem` (layered slab, cavity) with regions, domains, couplings and constant or temperature laws | a way in from an architecture; interfaces |
| Optimization | design variables (continuous, categorical), Pareto (NSGA-II), manufacturing constraint categories, adapters | architecture variables (thickness, angle, coverage, interface parameters) |
| Laboratory | model links to systems, structures, materials, studies, revisions, representations | architectures as links; predictions of stack metrics |
| Manufacturing | geometry and printability checks; free-text processes on functional constraints | process kinds, layer constraints, residual stress, records |

## The reuse map

| Concept | Reused | New |
|---|---|---|
| composition, phases | constituents are library materials; phases may name material-state studies | `matter::Composition`, `Phase` |
| microstructure | multiscale representations hold the RVEs | `Microstructure` points to one |
| region | scene objects, the engine's `RegionDef` (built from regions) | `FunctionalRegion` |
| layer stack | the engine's layered slab, `em::MediumLayer` (built from stacks) | `Stack`, `Layer`, `Pattern` |
| graded field | the engine's constant laws per sublayer | `PropertyField` |
| orientation, anisotropy | `VariableRole::Orientation` | `Orientation`, `AnisotropicSet` |
| interface | none existed | `Interface` and `thermal::ConductionProblem::contactResistance` |
| provenance | the library's provenance words | the request's words added |
| effective property | `multiscale::twoPhaseBounds` semantics | `matter/Mixing` |
| solver adapter | the solvers themselves | `coupling/MatterAnalyses` |
| optimization | `SnapshotAdapter`, NSGA-II, constraints | the `matter` adapter |
| experiment link | `laboratory::ModelLink`, estimation | the `matter-architecture` kind, `matter.stack-metric` |
| energy balance | the accounting ledger and checks | the ledgers of the stages |
| manufacturing | the constraint category | `matter/Manufacturing` |

## Previous prompts

- The Shape–Function–Energy direction, the coupled engine, functional
  structures, resonance, metamaterials, multiscale, material state,
  accounting and the laboratory are all current.
- Prompt 25 (function-driven inverse design) was never received. The
  optimizer and the functional structures' adapter cover its ground.
- `docs/architecture/ENERGY_MATTER_RESEARCH_ROADMAP.md` lists this request at
  stage 11.
