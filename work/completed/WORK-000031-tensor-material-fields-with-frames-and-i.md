# WORK-000031: Tensor material fields with frames and interpolation, atomistic engine adapters with the bridge record, and microstructure statistics with boundary resistance

**Status:** completed
**Type:** feature
**Request:** requests/processing/2026-11-28-3-3-functionally-graded-materi-req-chat-20261128-006.md
**Requirements:** RES-FAM-021, RES-MSC-015, RES-MSC-016, RES-MSC-017
**Decision:** ADR-000234
**Evidence:** VER-000033
**Test-Suites:** core, gui, cmake, audits
**Test-Run:** 20261128T150043-3323152
**Test-Run:** 20261128T145046-3242849

## Scope

Tensor material fields with frames and interpolation, atomistic engine adapters with the bridge record, and microstructure statistics with boundary resistance

## Implementation

- `src/core/materials/MaterialField.{h,cpp}` (new, domain-free):
  - properties: density, stiffness (6x6 mechanics Voigt), thermal and electrical conductivity, permittivity, piezoelectric `e` (3x6), permeability;
  - Cartesian, cylindrical and spherical frames with local bases; Bond-matrix rotation into the object's axes;
  - analytic blends (constant, linear, power law, exponential, table) and sampled forms (nearest, inverse-distance, piecewise linear), never extrapolated;
  - the source stated separately from the form; invalid tensors refused; JSON form.
- `src/core/mechanics/LayeredSolid.{h,cpp}`: `LayerMaterial::fields` (stiffness, density, a tensor conductivity) evaluated per element; a stiffness field together with an orthotropic or graded description is refused; conduction takes a 3x3 conductivity.
- `src/core/mechanics/PiezoelectricSolid.{h,cpp}`: `PiezoLayer3D::fields` (`e`, eps^S, c^E) replace the rotated constants per element.
- `src/core/coupling/EngineAdapters.{h,cpp}` (new; coupling, because it runs the atomistic domain):
  - the adapter interface: applicability, availability, prepare, run;
  - the in-house static lattice, which runs;
  - LAMMPS: strain-deformation decks and the log parser; it runs only when found and otherwise refuses;
  - GROMACS: applicability only;
  - `TransferredProperty`, the bridge record, and its computed material field.
- `src/core/multiscale/MicrostructureAnalysis.{h,cpp}` (new): microstructure statistics (phases, porosity, grains, boundary densities, inclusions, defect records) and a conduction problem with a grain-boundary resistance.
- `src/core/multiscale/ExternalSolvers.{h,cpp}`: `findExecutable` is public; the GROMACS entry and the adapters' statuses are added.
- Matter (`matter/MatterArchitecture`, `matter/SolverCapability`, `coupling/MatterAnalyses`):
  - architectures store `materialFields` and validate them;
  - only analyses that declare `tensor-field` read them (`layered-solid`, `layered-solid-thermal`); the others refuse the stack and name the field;
  - the layered analyses attach a region's fields in the body's frame.

## Tests

- `tests/core/test_material_fields.cpp` (6): frames; blends and refusals; interpolation within the support only; cylindrical-basis rotation; a constant stiffness field equal to the plain body (1e-12) and a graded field equal to the graded layer; anisotropic conduction through k_zz only.
- `tests/core/test_engine_adapters.cpp` (7):
  - every engine states its applicability and availability;
  - the in-house elastic constants cross to the continuum with their provenance, and a cube strains by their compliance;
  - the LAMMPS deck is complete, and a run without the engine is refused;
  - the parser recovers a stiffness from a labelled synthetic fixture;
  - GROMACS refuses tasks outside its applicability;
  - a bicrystal resists in series, L / (L / k + 2R) across and k along;
  - porosity, inclusions and defect records are reported.
- `tests/core/test_matter_analyses.cpp` (+1): matter fields reach the 3D solid (equal fields give the same modes; a fourfold density halves them), round-trip through JSON, and are refused by lamination theory.

## Regression impact and documentation

- Layered solids and the piezoelectric continuum: with no fields, their behaviour is unchanged; their existing suites pass.
- External solver statuses keep "not integrated".
- Docs:
  - new: `docs/materials/MATERIAL_FIELDS.md`, `docs/architecture/ATOMISTIC_INTEGRATION.md` (including the other engines' domains of applicability), ADR-000234;
  - requirements: RES-FAM-021 and RES-MSC-015..017;
  - updated: `LAYERED_SOLIDS.md`, `ARCHITECTED_MATTER.md`, `SOLVER_CAPABILITY_MATRIX.md`, the gap analysis, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `decisions.txt`, `requirements.txt` and `docs/README.md`.
- Stated limits:
  - fields are element-constant in the solids;
  - the LAMMPS run path is not exercised (no LAMMPS installed);
  - GROMACS has no input generation;
  - defects are recorded, not computed.

## Index bookkeeping

`docs/decisions/decisions.txt` also loses a duplicate ADR-000229 line in this request's commit. REQ-CHAT-20261128-181 (the MIT licence, da8d6b7) amended that line, but the shared-manifest commit added the new text and kept the old line. This request owns the file concurrently, so it removes the stale line. The text of ADR-000229's row is unchanged.

## Traceability

REQ-CHAT-20261128-006 -> RES-FAM-021, RES-MSC-015, RES-MSC-016, RES-MSC-017 -> WORK-000031 -> evidence (pending)

## Transitions

- 2026-11-28T19:04:36.855364+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T20:05:32.292098+00:00 active -> completed by agent-dev-20261126151713-76f632
