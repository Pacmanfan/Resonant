# Metamaterial Architecture

**Status:** current (REQ-CHAT-20261126-019). **Requirements:** `docs/requirements/metamaterials.md`
RES-META-001..012. **Decision:** ADR-000223.

A metamaterial in Resonant is an organized structure whose analysis computes
what the organization does. There is no "metamaterial" resource and no exotic
material value typed in: a **functional structure** (ADR-000221) names the
unit cell's objects and materials and its parameters, and an analysis of
category `effective-medium` derives the effective behavior with a named
model, a validity and a derivation (RES-META-001).

## Representations kept apart (RES-META-002)

Every effective-medium result reports, in `details.representations`:

| Representation | What it is | Where it comes from |
|---|---|---|
| constituents | the materials of the cell | the project's library (or the analysis settings) |
| unit cell | the cell's geometry: layer thicknesses, the resonator, the ring | scene objects bound to parameters |
| periodic structure | the period, the number of cells of the finite structure | settings |
| effective description | the homogenized or retrieved properties, or the reduced model's parameters | the analysis |
| equivalent model | a circuit, a transmission line, a lumped lattice | the analysis |

Where the effective description holds and the library can represent it, the
result also carries an **effective material** (`details.effectiveMaterial`,
format `resonant.effective-material`, `src/core/functional/EffectiveMaterial.h`).

## The analyses

| Kind | Domain module | Model | Effective material |
|---|---|---|---|
| `mechanics.mass-in-mass-lattice` | `mechanics/LocallyResonantLattice`, `oscillators/BlochLattice` | closed-form dispersion, cell Bloch eigenproblem, driven finite chain | none: the dynamic mass is dispersive and negative in the gap (reported) |
| `acoustic.resonator-duct` | `manipulators/ResonatorDuct` (with `Helmholtz`) | cell transfer matrices with shunt Helmholtz admittances | sound speed (frequency table) and density below half the resonance |
| `em.layered-effective-medium` | `electromagnetics/ObliqueLayers`, `Retrieval` | uniaxial mixing against exact TE/TM transfer matrices; retrieval | ε parallel and perpendicular (one component per saved material) |
| `em.split-ring-array` | `electromagnetics/SplitRing` | quasi-static LC circuit (an approximation) | permeability table below 0.8 f0, when the cell is small enough |
| `thermal.layered-conductivity` | `thermal/LayeredConductivity` | series and parallel conductances against finite volumes | conductivity perpendicular and parallel, density, specific heat |

The analyses are registered in `src/core/coupling/MetamaterialAnalyses.cpp`
with the other functional analyses; each domain's physics stays in its
module (`tools/check_layering.py`). The Bloch solver for lumped cells is
domain independent (`oscillators`).

## Effective materials in the library (RES-META-011)

`FunctionalStructureService.CreateEffectiveMaterial(resultId, options)`
turns a current analysis result's effective material into a library material
(one undoable step):

- every value has provenance **homogenized** (new, contract value 6) and a
  source naming the kind, method, component, structure and result;
- the validity range becomes each value's frequency validity; dispersive
  values keep their frequency table;
- the description carries the validity and the limitations;
- a stale result, a sweep, or a result without an effective material is
  refused; `components` chooses the principal component an isotropic library
  value carries (`{"thermal.thermalConductivity": "parallel"}`).

The saved material is an ordinary material: the Coupled Energy–Matter Engine
uses it through the same interfaces, and a homogenized region reproduces the
explicit laminate's heat flow to rounding
(`MetamaterialServices.AHomogenizedRegionReproducesTheExplicitLaminateInTheCoupledEngine`).

## Clients (RES-META-012)

The GUI's Functional Structures panel lists the effective material with the
analysis's model and offers **Save effective material** while the result is
current; Python and the API call `CreateEffectiveMaterial`; the MCP adapter
exposes `create_effective_material` at the `modify` level.

## Not covered

2D and 3D Bloch analysis of continua (plane waves or finite elements), a
full-wave split ring (no periodic plane-wave port in the Yee solver),
anisotropic library materials (tensors), architected lattices and graded
structures remain planned (`docs/architecture/ENERGY_MATTER_RESEARCH_ROADMAP.md`, stages 7 and 8).
