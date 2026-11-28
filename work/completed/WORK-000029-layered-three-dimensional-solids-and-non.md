# WORK-000029: Layered three-dimensional solids and non-ideal interfaces: 20-node hexahedra on curved surfaces, explicit interface models (springs, sliding, Coulomb contact, debonding, initiation), 3D conduction, the layered-solid matter analyses

**Status:** completed
**Type:** feature
**Request:** requests/processing/2026-11-28-part-iii-three-dimensional-str-req-chat-20261128-004.md
**Requirements:** RES-MECH-019, RES-MECH-020, RES-MECH-021, RES-MECH-022
**Decision:** ADR-000232
**Evidence:** VER-000029
**Test-Suites:** core, gui, cmake, audits
**Test-Run:** 20261128T130617-2676235

## Scope

Layered three-dimensional solids and non-ideal interfaces: 20-node hexahedra on curved surfaces, explicit interface models (springs, sliding, Coulomb contact, debonding, initiation), 3D conduction, the layered-solid matter analyses

## Implementation

### The finite-element model (`src/core/mechanics/StructuralModel.{h,cpp}`)

- the quadratic 20-node hexahedron `Solid20` (shape functions, 3 x 3 x 3 Gauss points);
- per-element `Constitutive` elasticity and density, refused when not symmetric or not positive definite;
- zero-thickness `InterfaceElement`s (8 + 8 nodes, per-point factors, a general traction-jump tangent, internal forces);
- stress recovery at natural points;
- the 20-node cases in `DisplayMesh` (curved faces, feature-edge outline) and `Study`.

### The layered-body builder (`src/core/mechanics/LayeredSolid.{h,cpp}`)

- reference surfaces: plane, cylinder, sphere, revolved Catmull-Rom profile;
- layers: isotropic, orthotropic (nine constants, the axes following the surface) or graded (power law, Voigt); thickness constant or linear;
- the mesh merges seams and poles and keeps elements right-handed;
- interface models: perfect bonding as shared nodes, springs, sliding, frictional contact, debonded regions, the strengths' initiation index;
- supports: edges, symmetry planes, held axes, the inner face;
- loads: pressures, outer traction, edge forces;
- static solution: LDLT with refinement and mechanism detection by pivots; frictional contact by Newton's method with Coulomb return mapping, a symmetric quasi-Newton tangent and a line search;
- modal solution through the existing eigensolver (frictional contact refused);
- steady conduction with contact conductances.

### Architected matter

- `coupling/MatterAnalyses`: the capabilities `layered-solid` and `layered-solid-thermal`, the body built from the stack (graded regions as sublayers, each declared interface mapped, options for the surface, mesh, supports, loads and debonded regions), and their metrics;
- `matter/MatterArchitecture`: the interface kind `sliding`;
- the Matter panel lists both analyses and plots the 3D modes;
- the matter proto comment names them (no wire change). `docs/reference/PROTOBUF_API.md` is regenerated from it by `tools/automation/generate_protobuf_docs.py`, never edited by hand.

### Index repair

The shared-manifest commit of REQ-CHAT-20261128-003 left two `application-foundation` rows in `docs/requirements/requirements.txt`: it commits only lines carrying the request's own tokens, so the old row's deletion stayed uncommitted. This request registered the file as concurrent content from HEAD and removes the stale row.

### Process note

`StructuralModel.h`, `proto/resonant/v1/matter/matter_service.proto` and `src/app/MatterPanel.cpp` were edited before they were registered. Each was registered with `--adopt-existing-changes` as soon as this was noticed; nothing in them belongs to another request.

## Tests

- **`tests/core/test_layered_solid.cpp`:** 21 cases, each against an analytical reference:
  - Timoshenko cantilever and beam modes; lamination theory as the thin limit;
  - Lamé cylinders, ring and sphere, a revolved profile, a compound cylinder with springs;
  - series compliances; sliding; Coulomb stick, slip and open; debonding bounds; the initiation index;
  - refusals; a tapered beam; a graded profile; orthotropic axes; an invalid elasticity matrix;
  - series and logarithmic conduction; the display mesh.
- **`tests/core/test_matter_analyses.cpp`:** 3 cases: 3D conduction equal to the series model; 3D modes between the narrow-beam and wide-plate predictions, graded sublayers; static interface results, refusals, a cylinder from the options.
- **`tests/app/test_matter_panel.cpp`:** 2 cases: the 3D modes plotted; a cohesive zone refused in the panel.
- **`python/tests/test_matter_api.py`:** the capabilities listed, the refusal with its reason, and `AnalyzeStack` raising.
- **Regression:** the examples test now runs both analyses on every compatible example stack.

The suites (core, gui, cmake/ctest, audits) run through the linked t2 run.

## Regression impact and documentation

- **Existing elements:** `Element::nodes` grows from 8 to 20 entries. The 8-node and other kinds are unchanged, and the 8-node solid's matrix cache is skipped only for elements that carry their own elasticity.
- **Documents:**
  - RES-MECH-019..022 (`docs/requirements/mechanics.md`) and ADR-000232;
  - `docs/mechanics/LAYERED_SOLIDS.md` (new), `STRUCTURAL_MECHANICS.md`, `MECHANICAL_PHYSICS.md`;
  - `docs/materials/INTERFACE_ENGINEERING.md`, `SOLVER_CAPABILITY_MATRIX.md`, `ARCHITECTED_MATTER.md`;
  - `help/src/core-concepts/architected-matter.md`;
  - the indexes: `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `decisions.txt`, `requirements.txt`, `docs/README.md`;
  - the Milestone 27 gap row and the bell's demonstration-readiness row.

## Traceability

REQ-CHAT-20261128-004 -> RES-MECH-019, RES-MECH-020, RES-MECH-021, RES-MECH-022 -> WORK-000029 -> evidence (pending)

## Transitions

- 2026-11-28T17:55:23.293247+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T18:12:09.003624+00:00 active -> completed by agent-dev-20261126151713-76f632
