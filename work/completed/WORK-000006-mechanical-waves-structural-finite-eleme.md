# WORK-000006: Mechanical waves: structural finite elements, modal, harmonic and transient analyses, actuators, targeting, periodic rods, coupling

**Status:** completed
**Type:** feature
**Request:** requests/processing/2026-11-26-prompt-10-resonant-mechanical--req-chat-20261126-006.md
**Requirements:** RES-MECH-001, RES-MECH-002, RES-MECH-003, RES-MECH-004, RES-MECH-005, RES-MECH-006, RES-MECH-007, RES-MECH-008, RES-MECH-009, RES-MECH-010, RES-MECH-011, RES-MECH-012, RES-MECH-013, RES-MECH-014, RES-MECH-015, RES-MECH-016, RES-MECH-017, RES-MECH-018
**Decision:** ADR-000210
**Evidence:** VER-000006
**Test-Run:** 20261126T210629-3989299
**Test-Suites:** core, app, cmake, layering

## Scope

Mechanical waves: structural finite elements, modal, harmonic and transient analyses, actuators, targeting, periodic rods, coupling

## Implementation

- `src/core/mechanics` (new) — `ElasticMaterial`, `StructuralModel` (frame, ACM plate, membrane, hexahedron; sparse assembly; stresses), `Meshing` (structured meshes, sections, supports), `Eigensolver` (dense / shift-invert Lanczos with backward errors), `Analyses` (static, modal, harmonic direct and modal, Newmark transient, damping models), `MechanicalComponents` (structure, actuator, sensor, vibration target), `MechanicalGeometry` (horn, periodic rod), `Study` (idealization from scene objects, attachments, preflight, results, jobs), `Bloch`, `Targeting`, `Measurement`, `DisplayMesh`.
- `src/core/dsp` (moved from `src/core/fdtd`) — signals and spectra shared by every domain; `src/core/coupling` (new) — one-way Rayleigh radiation from plates.
- `src/core/fdtd/SolidModel.cpp` — inside tests for the new rod kinds; `src/core/materials/ExampleMaterials.cpp` — eight materials; example projects regenerated.
- `src/render/ScalarMeshLayer` (new), `Renderer`; `src/app` — `MechanicsController`, `MechanicsPanel` (`forms/MechanicsPanel.ui`), `MechanicalEditor` (`forms/MechanicalEditor.ui`), `MechanicsMarkers`, `PlotWidget` (several curves, log axis), `SceneRenderBridge` (replaced objects), `ViewportWidget::setScalarMesh`, `ObjectLabels`, `forms/MainWindow.ui` (Mechanics menu and dock), `src/main/main.cpp` (`--run-mechanics`).
- `tools/check_layering.py` — domains may not include each other; no duplicate source names inside one target (a qmake object collision found on the way).
- Documents: `docs/requirements/mechanics.md`, ADR-000210, eleven documents in `docs/mechanics/`, status, decisions, overview, example index, two full-wave documents' code paths.

## Tests

- `tests/core/test_mechanics.cpp` (20 cases: materials, sections, frame, plate, membrane, solid, eigensolver, harmonic, transient, waves), `tests/core/test_mechanics_scene.cpp` (14 cases: idealization, attachments, settings, eight example projects, coupling, measurement, persistence), `tests/app/test_mechanics_ui.cpp` (7 cases). Core suite 221 cases, application suite 70 cases, CMake build and CTest, layering: all PASS in the linked run.

## Regression impact and documentation

- Waveform and spectrum moved to `src/core/dsp` (the full-wave solver uses them through using-declarations; its 40 tests pass unchanged). The material library grew from 10 to 18 entries, so every generated example file was regenerated and the two count tests updated. The full-wave solid model gained the rod kinds. All earlier acoustic, manipulator, full-wave and foundation tests pass. Documentation updated: requirements and decision manifests, `docs/README.md`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, architecture overview, `examples/README.md`, `docs/advanced_acoustics/FREQUENCY_ANALYSIS.md`, `docs/advanced_acoustics/BROADBAND_SIMULATION.md`.

## Traceability

REQ-CHAT-20261126-006 -> RES-MECH-001, RES-MECH-002, RES-MECH-003, RES-MECH-004, RES-MECH-005, RES-MECH-006, RES-MECH-007, RES-MECH-008, RES-MECH-009, RES-MECH-010, RES-MECH-011, RES-MECH-012, RES-MECH-013, RES-MECH-014, RES-MECH-015, RES-MECH-016, RES-MECH-017, RES-MECH-018 -> WORK-000006 -> evidence (pending)

## Transitions

- 2026-11-27T01:59:34.239771+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-27T02:16:17.903684+00:00 active -> completed by agent-dev-20261126151713-76f632
