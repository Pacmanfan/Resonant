# WORK-000004: Passive wave manipulation: manipulators, reflector field model, hybrid focusing, geometry optimization

**Status:** completed
**Type:** feature
**Request:** requests/processing/2026-11-26-prompt-8-resonant-passive-wave-req-chat-20261126-004.md
**Requirements:** RES-MANIP-001, RES-MANIP-002, RES-MANIP-003, RES-MANIP-004, RES-MANIP-005, RES-MANIP-006, RES-MANIP-007, RES-MANIP-008, RES-MANIP-009, RES-MANIP-010, RES-MANIP-011, RES-MANIP-012, RES-MANIP-013, RES-MANIP-014, RES-MANIP-015, RES-MANIP-016
**Decision:** ADR-000208
**Evidence:** VER-000004
**Test-Run:** 20261126T171626-3413267
**Test-Suites:** core, app, cmake, layering

## Scope

Passive wave manipulation: manipulators, reflector field model, hybrid focusing, geometry optimization

## Implementation

- `src/core/manipulators` (new module) — `Manipulator` (design component, support levels, constraints check), `ManipulatorGeometry` (reflector, lens, waveguide, cavity and slit-plate kinds from their equations; surface sampling; exact ray reflection; characteristic points), `Analytics` (mirror equation, interface coefficients, thick lens, duct cut-offs, cavity resonances, Helmholtz and quarter-wave resonators), `ReflectorModel` (Kirchhoff propagation model, combined evaluator, ray tracing, focus metrics, focal gain, model-based focus), `GeometryOptimization` (variables, objective, constraints, golden-section coordinate search).
- `src/core/acoustics` — `FieldEvaluator` made polymorphic (a model may provide no particle velocity); `AcousticSettings::modelId` and `reflectorElementsPerWavelength`; point reports mark intensity as not calculated.
- `src/app` — `ManipulatorController`, `ManipulatorEditor` with `forms/ManipulatorEditor.ui`; Wave Manipulators menu and ray toggle in `forms/MainWindow.ui`; model selection in `forms/AcousticsPanel.ui`; result comparison tab in `forms/FieldAnalysisPanel.ui`; `AcousticController` uses the selected model for evaluation, focusing, inverse design, preflight and run records.
- Documents: `docs/requirements/wave_manipulators.md`, ADR-000208, `docs/manipulators/*.md` (nine), status, decisions, overview, roadmap, example index.

## Tests

- `tests/core/test_manipulators.cpp` (27 cases), `tests/core/test_manipulator_examples.cpp` (5 cases, also generate `examples/manipulators/*.resonant`), `tests/app/test_manipulators_ui.cpp` (9 cases). Core suite 147 cases, application suite 58 cases, CMake build and CTest, layering: all PASS in the linked run.

## Regression impact and documentation

- The acoustic field evaluator, settings and panels changed: all acoustic core and application tests pass unchanged; the acoustic example files were regenerated because the settings gained two fields (old files still load with defaults). A dangling model pointer found by running the application on example A was fixed (`AcousticController`). Documentation updated: requirements manifest, decision manifest, `docs/README.md`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, architecture overview and roadmap, `examples/README.md`.

## Traceability

REQ-CHAT-20261126-004 -> RES-MANIP-001, RES-MANIP-002, RES-MANIP-003, RES-MANIP-004, RES-MANIP-005, RES-MANIP-006, RES-MANIP-007, RES-MANIP-008, RES-MANIP-009, RES-MANIP-010, RES-MANIP-011, RES-MANIP-012, RES-MANIP-013, RES-MANIP-014, RES-MANIP-015, RES-MANIP-016 -> WORK-000004 -> evidence (pending)

## Transitions

- 2026-11-26T22:20:55.617426+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-26T22:21:13.431757+00:00 active -> completed by agent-dev-20261126151713-76f632
