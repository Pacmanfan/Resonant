# WORK-000002: V0.1 application foundation: core library, OpenGL renderer, Qt application, tests, dual build

**Status:** completed
**Type:** feature
**Request:** requests/processing/2026-11-26-prompt-6-resonant-v0-1-applica-req-chat-20261126-002.md
**Requirements:** RES-APP-001, RES-APP-002, RES-APP-003, RES-APP-004, RES-APP-005, RES-APP-006, RES-APP-007, RES-APP-008, RES-APP-009, RES-APP-010, RES-APP-011, RES-APP-012, RES-APP-013, RES-APP-014, RES-APP-015, RES-APP-016, RES-APP-017, RES-APP-018, RES-APP-019, RES-APP-020
**Decision:** ADR-000206
**Evidence:** VER-000002
**Test-Run:** 20261126T160935-3213326
**Test-Suites:** core, app, cmake, layering

## Scope

V0.1 application foundation: core library, OpenGL renderer, Qt application, tests, dual build

## Implementation

- `src/core` — foundation (ids, units, logging, results, transforms), geometry (parametric kinds, seven primitives, display meshes, ray intersection), materials (per-domain property sets, provenance, example library), scene (graph, components, events), commands (undo / redo), project (document, JSON format 1, validation, migration), simulation (configuration, jobs, demonstrator).
- `src/render` — camera, shader library, GPU meshes, render layers (grid and axes, lit meshes, lines), renderer lifecycle for OpenGL 4.5 Core.
- `src/app`, `src/main` — document controller, viewport, scene tree, properties, analysis panels, material editor, about dialog, main window; six Qt Designer forms in `src/app/forms`; `--screenshot` option.
- `CMakeLists.txt` (+ per-directory), `project.pro`, `warnings.pri` (+ per-directory .pro) — the two builds; `third_party/` — vendored GoogleTest 1.17.0 and nlohmann-json 3.11.3.
- `tests/core` (66 cases), `tests/app` (36 cases); `tools/verify_cmake_build.sh`, `tools/check_layering.py`; `scripts/test_manifest.json` registers suites core, app, cmake, layering.
- `examples/foundation/primitives.resonant`, `examples/README.md`.
- Documentation: `README.md`, `BUILDING.md`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `docs/requirements/application_foundation.md`, ADR-000206, amended ADR-000204, architecture overview and roadmap, manifests.
- `scripts/tests/test_workers.py` — the identity scan of the development system's own tests now skips top-level entries git ignores. It was reading the git-ignored framework checkout `auto_ci_cd/` kept inside this project, and failed for that reason alone. No rule of the scan changed for tracked or untracked-but-not-ignored files.

## Tests

- New: suites `core` (t1), `app` (t2, under xvfb), `cmake` (t2: CMake build + CTest), `layering` (t0).
- Run 20261126T160935-3213326: tier t2, PASS, 9 groups: core 66 cases, app 36 cases, cmake, layering, the development system's own suites.
- Also run by hand, not part of the gate: CMake Release build without warnings, 66 + 36 cases pass; the application started on a virtual display (xvfb, Mesa llvmpipe, OpenGL 4.5 Core) with `--screenshot`, loading `examples/foundation/primitives.resonant`; the image shows the menus, the three docks, the grid and axes and all nine objects.

## Regression impact and documentation

- First code in the repository: nothing to regress. The root `README.md` now serves the product and still carries the development system's orientation its own test requires.
- Conflicts with the request, resolved by recorded decision rather than silently: qmake in addition to CMake and vendored GoogleTest (ADR-000204); deviations listed in ADR-000206.
- Outside this milestone, each listed with its state in `IMPLEMENTATION_STATUS.md`: viewport manipulator handles, moving an object into a group from the user interface, editing frequency tables in the material dialog, a run on a machine without OpenGL 4.5, Windows / macOS.

## Traceability

REQ-CHAT-20261126-002 -> RES-APP-001, RES-APP-002, RES-APP-003, RES-APP-004, RES-APP-005, RES-APP-006, RES-APP-007, RES-APP-008, RES-APP-009, RES-APP-010, RES-APP-011, RES-APP-012, RES-APP-013, RES-APP-014, RES-APP-015, RES-APP-016, RES-APP-017, RES-APP-018, RES-APP-019, RES-APP-020 -> WORK-000002 -> evidence (pending)

## Transitions

- 2026-11-26T21:00:19.790571+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-26T21:14:08.406999+00:00 active -> completed by agent-dev-20261126151713-76f632
