# Multiscale Material Architecture

**Status:** current (REQ-CHAT-20261126-020). **Requirements:** `docs/requirements/multiscale.md`
RES-MSC-001..014. **Decision:** ADR-000224.

## Audit (before the work)

| Area | Found | Used for |
|---|---|---|
| Materials (`src/core/materials`) | one isotropic property set per domain, provenance, validity, frequency tables | the one material database; extended with lineage, not duplicated |
| Effective materials (ADR-000223) | `functional/EffectiveMaterial`, provenance *homogenized* | the effective scale; RVE results join the same provenance |
| Coupled engine (ADR-000220) | layered-slab systems, material laws, a temperature field per run | local states and the iterative coupling |
| Thermal and EM domains | constituent property readers (`thermal::thermalMaterial`, `em::emMaterialOf`) | RVE phase data |
| Finite elements | mechanics solid elements (no RVE support) | not reused: a dedicated scalar homogenization solver |
| External engines | none installed (no LAMMPS, OpenMM, Quantum ESPRESSO, CP2K, GPAW, ASE, phonopy) | listed as candidates, none integrated |
| Python worker, plugins | separate processes for external code | the planned adapter path for external engines |

## Map

```text
                 library material (continuum) <-- derivation record (lineage) --+
                        ^  values: provenance homogenized / computed,          |
                        |  derivation id, uncertainty                          |
  MultiscaleService --- assign (one undoable step) ------------------------------+
        |   derive (job)                                  coupling/MultiscaleDerivations
        v                                                    |          |
  multiscale model (project extension "multiscale")          v          v
    representations: electronic | atomistic | molecular |  multiscale/    atomistic/
                     microstructure | effective | continuum PotentialHomogenization, LatticeStatics,
    relationships, derivations, model cards, couplings       Microstructure  MolecularDynamics
                                                              (math)          (domain)
  coupled engine result -- local state --> coupling/MultiscaleCoupling --> iterative region-RVE coupling
```

| Module | Holds | Layering |
|---|---|---|
| `src/core/multiscale` | `Representation` (model, relationships, derivations, storage, staleness, lineage), `ModelCards`, `Couplings` (contracts), `ExternalSolvers`, `Microstructure` (voxel RVEs), `PotentialHomogenization` | no physics domain, no coupling |
| `src/core/atomistic` | `Crystal`, `StructureImport` (XYZ, POSCAR), `ForceField`, `LatticeStatics`, `MolecularDynamics` | a domain: includes no other domain |
| `src/core/coupling` | `MultiscaleDerivations` (methods, applicability, execution, assignment plans), `MultiscaleCoupling` (local states, iterative coupling) | the only place the domains meet |
| `src/core/services` | `MultiscaleService` (17 operations) | |
| `proto/resonant/v1/multiscale` | the contract | |
| `src/app` | `MultiscaleController`, `MaterialScalesPanel` (dock "Material Scales") | |

## Representations (RES-MSC-001)

| Scale | Kinds | Data |
|---|---|---|
| electronic | `electronic-structure` | interface only: no calculation |
| atomistic | `crystal`, `atomic-structure` | `structure` (lattice, atoms), `forceField` |
| molecular | `molecule`, `molecular-system` | data model only |
| microstructure | `rve.voxel`, `microstructure` | `rve` (generator specification or voxels), `phaseMaterials`, `temperatureLaws` |
| effective | `effective-material` | as ADR-000223 |
| continuum | `continuum-material` | the library material |

Each has an id, a name, a scale, a kind, the material it describes,
constituents (library materials), a state (temperature, pressure),
assumptions, boundary conditions, provenance (synthetic, imported, computed,
user), a validation status, an uncertainty note, a model card and a content
**revision** that changes when its physical content changes.

## Derivation pipeline (RES-MSC-011)

| Method | From → to | Derives |
|---|---|---|
| `rve.thermal-conductivity` | microstructure → effective | the conductivity tensor (mean and diagonal components), density, specific heat |
| `rve.electrical-conductivity` | microstructure → effective | electrical conductivity |
| `rve.static-permittivity` | microstructure → effective | quasi-static relative permittivity |
| `atomistic.elastic-constants` | atomistic → continuum | mechanical density, Young's modulus, Poisson's ratio; acoustic density and sound speed |
| `atomistic.md-nve` | atomistic → atomistic | observables only (energy drift, temperature, g(r), MSD) |

Select representation → method → applicability (scale, kind, data, force
field) → settings (boundary, sizes, realizations, temperature) → job →
convergence, sampling and uncertainty → *reliable* or the reasons not →
assignment (provenance, derivation record, relationships) → the library.
Unsupported transitions are refused with the reason; the electronic and
molecular scales have no method.

## Coupling strategies (RES-MSC-012)

Contracts in `multiscale/Couplings`: `sequential.atomistic-continuum`,
`sequential.rve-effective`, `iterative.region-rve-thermal` and
`hierarchical.chain` are implemented; `fe2` and
`concurrent.atomistic-continuum` are planned with their requirements.

## Integration (RES-MSC-013)

Derived values are ordinary library values: the cantilever analysis uses
the argon material, the layered effective-medium analysis uses the porous
dielectric (a chain pores → layer → uniaxial metamaterial), and the coupled
engine runs the pad with its iterated conductivity. Existing projects are
unchanged; property values without lineage serialize as before.
