# WORK-000025: Functionally architected matter: canonical architectures of regions, stacks, interfaces and fields, analyzed through declared solver capabilities, with coupled-engine, inverse-design, laboratory and manufacturing integration

**Status:** completed
**Type:** FEATURE
**Request:** requests/processing/2026-11-27-foundational-architecture-prom-req-chat-20261127-001.md
**Requirements:** RES-FAM-001, RES-FAM-002, RES-FAM-003, RES-FAM-004, RES-FAM-005, RES-FAM-006, RES-FAM-007, RES-FAM-008, RES-FAM-009, RES-FAM-010, RES-FAM-011, RES-FAM-012, RES-FAM-013, RES-FAM-014, RES-FAM-015, RES-FAM-016, RES-FAM-017, RES-FAM-018, RES-FAM-019, RES-FAM-020
**Decision:** ADR-000228
**Evidence:** VER-000025
**Test-Suites:** core, app, cmake, layering, audits
**Test-Run:** 20261128T034320-1408368

## Scope

Functionally architected matter: canonical architectures of regions, stacks, interfaces and fields, analyzed through declared solver capabilities, with coupled-engine, inverse-design, laboratory and manufacturing integration

## Implementation

- `src/core/matter/` (new infrastructure module, no domains, no coupling):
  - `MatterArchitecture`: the canonical architecture, its validation, revision and the extension `matter`.
  - `PropertyField`: one-axis fields with exact series and parallel combinations and discretization.
  - `Mixing`: effective-property estimators with their bounds and validity.
  - `PropertyResolution`: field, then region value, composition and library; missing otherwise.
  - `SolverCapability`: declarations and the compatibility check.
  - `Manufacturing`: process kinds, constraint checks and residual stresses.
- Domain physics:
  - `src/core/mechanics/Laminate`: lamination theory, thermal response, beam and plate modes.
  - `src/core/mechanics/PiezoelectricBeam`: the lumped unimorph or bimorph with a resistive load.
  - `src/core/thermal/HeatConduction`: contact resistances on internal planes.
  - `src/core/coupling/MultiphysicsParticipants.cpp`: the thermal participant reads interfaces.
- Coupling:
  - `MatterAnalyses`: capabilities and the laminate, modes, harvester, thermal, electrical, electromagnetic and interfaces analyses.
  - `MatterSystems`: stacks as physical systems.
  - `MatterExploration`: mechanisms, exploration, pathways with accounting ledgers.
  - `MatterAdapter`: the optimizer adapter `matter`, registered in `OptimizationRegistry.cpp`.
  - `LaboratoryModels.cpp` (the model `matter.stack-metric`), `LaboratoryStudies.cpp` and `laboratory/Laboratory.cpp` (the link kind `matter-architecture`).
- Services: `src/core/services/MatterServices.cpp`, `Services.h` and `ProjectServices.cpp` (MatterService); `ApplicationServices.cpp` (capabilities).
- Contract and clients:
  - `proto/resonant/v1/matter/matter_service.proto` (19 RPCs, each commented);
  - `src/automation_api/Handlers.cpp`;
  - `python/resonant_api/mcp.py` (19 tools at their levels).
- GUI: `src/app/MatterController`, `MatterPanel` (with `forms/MatterPanel.ui`), `StackView`, and the MainWindow dock.
- `tools/check_layering.py`: matter is infrastructure.
- `examples/matter/` a–h and their README.

## Tests

- Core tests: `tests/core/test_matter_architecture.cpp`, `test_matter_fields.cpp`, `test_matter_mixing.cpp`, `test_matter_mechanics.cpp`, `test_thermal_contact.cpp`, `test_matter_manufacturing.cpp`, `test_matter_analyses.cpp`, `test_matter_coupling.cpp`, `test_matter_exploration.cpp`, `test_matter_optimization.cpp`, `test_matter_services.cpp`, `test_matter_examples.cpp`, with `MatterFixture.h`.
- `tests/app/test_matter_panel.cpp`: the workflow in the real main window.
- `python/tests/test_matter_api.py`: the API against a real host, and the MCP levels.
- Updated: `tests/core/test_laboratory_services.cpp` (eight laboratory models); `tests/app/test_accounting_panel.cpp` (braces for a dangling-else warning).
- The benchmarks and their tolerances are in `docs/materials/ARCHITECTED_MATTER_VALIDATION.md`.

## Regression impact and documentation

- **Thermal conduction:** contact resistances are optional, so an empty list keeps the old behaviour. The full core suite and the coupled-engine tests run.
- **Laboratory and optimizer:** one more model and one more adapter.
- **Documentation:**
  - `docs/requirements/architected_matter.md` and ADR-000228;
  - `docs/materials/ARCHITECTED_MATTER.md`, `_AUDIT`, `INTERFACE_ENGINEERING`, `_INTEGRATION`, `MANUFACTURING_AWARENESS`, `SOLVER_CAPABILITY_MATRIX`, `_VALIDATION`, `_ROADMAP`;
  - `help/src/core-concepts/architected-matter.md`;
  - `docs/architecture/ENERGY_MATTER_RESEARCH_ROADMAP.md`: stages 11 and 12 updated;
  - the indexes (DECISIONS.md, decisions.txt, requirements.txt, docs/README.md, the architecture README, overview and roadmap M21, product.md, examples/README.md, IMPLEMENTATION_STATUS.md, README.md);
  - the regenerated `docs/reference/PROTOBUF_API.md` and `docs/help/DOCUMENTATION_COVERAGE.md`.
- **Deviations from the request:** listed in ADR-000228.

## Traceability

REQ-CHAT-20261127-001 -> RES-FAM-001, RES-FAM-002, RES-FAM-003, RES-FAM-004, RES-FAM-005, RES-FAM-006, RES-FAM-007, RES-FAM-008, RES-FAM-009, RES-FAM-010, RES-FAM-011, RES-FAM-012, RES-FAM-013, RES-FAM-014, RES-FAM-015, RES-FAM-016, RES-FAM-017, RES-FAM-018, RES-FAM-019, RES-FAM-020 -> WORK-000025 -> evidence (pending)

## Transitions

- 2026-11-28T07:32:22.398371+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T08:49:14.665195+00:00 active -> completed by agent-dev-20261126151713-76f632
