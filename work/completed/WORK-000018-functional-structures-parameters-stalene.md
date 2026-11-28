# WORK-000018: Functional structures: parameters, staleness, analyses (cavity, optical resonator, horn, mechanical, unit cell, coupled slab), sweeps, objectives, services, panel

**Status:** completed
**Type:** FEATURE
**Request:** requests/processing/2026-11-26-prompt-19-geometry-driven-ener-req-chat-20261126-017.md
**Requirements:** RES-FUNC-001, RES-FUNC-002, RES-FUNC-003, RES-FUNC-004, RES-FUNC-005, RES-FUNC-006, RES-FUNC-007, RES-FUNC-008, RES-FUNC-009, RES-FUNC-010, RES-FUNC-011, RES-FUNC-012, RES-FUNC-013, RES-FUNC-014, RES-FUNC-015, RES-FUNC-016
**Decision:** ADR-000221
**Evidence:** VER-000018
**Test-Suites:** core, app, cmake, layering, audits
**Test-Run:** 20261127T204945-196363

## Scope

Functional structures: parameters, staleness, analyses (cavity, optical resonator, horn, mechanical, unit cell, coupled slab), sweeps, objectives, services, panel

## Implementation

- Functional module (`src/core/functional`, no physics, layering rule in `tools/check_layering.py`): `FunctionalStructure` (the `resonant.functional-structure` v1 document, validation, storage in the extension `functional`), `Parameters` (bound parameters through the optimization framework's scene bindings, free and derived parameters, application through the geometry kinds' rules), `Dependencies` (content keys of geometry, materials, definition and discretization; staleness with reasons; the simulation-geometry cache), `Analysis` (the registry interface: kinds, models, metrics, roles, outcomes), `FunctionalStudy` (analyze, sweep, sensitivity, mode tracking, objectives, constraints, explanations, result staleness).
- Domain physics: `electromagnetics/CavityResonator` (TE/TM fields, exact energy and wall-loss integrals, fill loss, Q, port reflection, sensitivities, a Yee model with spectral peaks and energy decay), `electromagnetics/OpticalResonator` (paraxial two-mirror resonator, ABCD round trip), `electromagnetics/PeriodicLayers` (exact Bloch dispersion, band gaps, finite stack, homogenization), `manipulators/Horn` (Webster equation by Runge-Kutta, baffled-piston radiation impedance with Struve H1, directivity) and the `horn.acoustic` geometry kind (`ManipulatorGeometry.cpp`) with its full-wave solid model (`geometry/SolidModel.cpp`).
- Coupling: `FunctionalAnalyses` (cavity, optical resonator, horn, mechanical modal with the mesh cache, photonic cell, periodic rod, coupled slab with thickness from the linked boxes; defines the registry instance), `FunctionalAdapter` (the optimization adapter `functional`, registered in `OptimizationRegistry.cpp`).
- Services and clients: `FunctionalStructureService` (`Services.h`, `FunctionalServices.cpp`, `ProjectServices.cpp`) with 14 operations, analyses as capabilities (`ApplicationServices.cpp`), the contract `proto/resonant/v1/functional/*.proto` with handlers in `src/automation_api/Handlers.cpp`, `docs/reference/PROTOBUF_API.md` regenerated, MCP tools in `python/resonant_api/mcp.py`.
- GUI: `FunctionalController`, `FunctionalPanel` and its form, the "Functional Structures" dock in `MainWindow` (`MainWindow.ui`, `MainWindow.cpp`, `MainWindow.h`); help page `help/src/optimization/functional-structures.md`, coverage row in `tools/help/coverage.py`.
- Examples: `examples/functional/a-microwave-cavity` ... `f-coupled-slab` and their README.

## Tests

- `tests/core/test_cavity_resonator.cpp` (9), `test_optical_resonator.cpp` (6), `test_acoustic_horn.cpp` (9), `test_periodic_layers.cpp` (4): each against closed forms or an independent computation (Pozar wall Q, Yee convergence and decay, the exact layered Fabry-Perot, the Struve power series, infinite and conical horn impedances, finite-stack attenuation).
- `tests/core/test_functional_structures.cpp` (9): serialization, validation, parameters, staleness, the cache, results, sweeps, sensitivity, mode tracking, objectives and constraints with a synthetic analysis.
- `tests/core/test_functional_examples.cpp` (11): every example's analysis against its references, sweeps, sensitivity, the mesh reuse, the coupled slab, staleness and the optimization adapter.
- `tests/core/test_functional_services.cpp` (6): storage and undo, parameters as one undoable step, conflicts, jobs, staleness on completion, optimization with explanations, capabilities and permissions.
- `python/tests/test_functional_api.py` (4) through the headless host; `tests/app/test_functional_panel.cpp` (2) in the real main window.

## Regression impact and documentation

- A new geometry kind (`horn.acoustic`) reaches code that enumerates kinds (the full-wave solid model was added and is checked against the physics profile); a new dock and a new service; standalone simulations are unchanged; the full core and app suites pass.
- Documents: `docs/requirements/functional_structures.md`, ADR-000221, `docs/architecture/FUNCTIONAL_STRUCTURE_ARCHITECTURE.md`, `FUNCTIONAL_STRUCTURE_AUDIT.md`, `docs/geometry/GEOMETRY_PARAMETERIZATION.md`, `docs/physics/ENERGY_TRANSFORMATION.md`, `RESONANT_STRUCTURE_ENGINEERING.md`, `FUNCTIONAL_STRUCTURE_VALIDATION.md`, `docs/optimization/FUNCTION_DRIVEN_DESIGN.md`, `docs/electromagnetics/CAVITY_RESONATORS.md`, `OPTICAL_RESONATORS.md`, `PERIODIC_LAYERS.md`, `docs/manipulators/ACOUSTIC_HORNS.md`, the help page and `docs/help/DOCUMENTATION_COVERAGE.md`; updated `docs/vision/SHAPE_FUNCTION_ENERGY.md`, `docs/architecture/COUPLED_ENGINE_ARCHITECTURE.md`, `ENERGY_MATTER_RESEARCH_ROADMAP.md`, `overview.md`, `README.md`, `roadmap.md` (M14), `docs/requirements/product.md`, `docs/README.md`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `examples/README.md`.
- Scope recorded in ADR-000221 and `IMPLEMENTATION_STATUS.md`: no CAD import or OpenCASCADE (no CAD kernel exists); rectangular cavities; paraxial optics; one-dimensional horns and cells; force metrics, coupled resonators and 2D/3D lattices are on the research roadmap; structures are created through the API, Python and examples (the panel edits parameters, analyzes and sweeps).

## Traceability

REQ-CHAT-20261126-017 -> RES-FUNC-001, RES-FUNC-002, RES-FUNC-003, RES-FUNC-004, RES-FUNC-005, RES-FUNC-006, RES-FUNC-007, RES-FUNC-008, RES-FUNC-009, RES-FUNC-010, RES-FUNC-011, RES-FUNC-012, RES-FUNC-013, RES-FUNC-014, RES-FUNC-015, RES-FUNC-016 -> WORK-000018 -> evidence (pending)

## Transitions

- 2026-11-28T00:55:37.350098+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T01:54:31.816822+00:00 active -> completed by agent-dev-20261126151713-76f632
