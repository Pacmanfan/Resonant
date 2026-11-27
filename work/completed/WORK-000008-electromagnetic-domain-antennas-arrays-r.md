# WORK-000008: Electromagnetic domain: antennas, arrays, RF analysis, guides, Yee full-wave solver, UI and examples

**Status:** completed
**Type:** feature
**Request:** requests/processing/2026-11-26-prompt-11-resonant-electromagn-req-chat-20261126-007.md
**Requirements:** RES-EM-001, RES-EM-002, RES-EM-003, RES-EM-004, RES-EM-005, RES-EM-006, RES-EM-007, RES-EM-008, RES-EM-009, RES-EM-010, RES-EM-011, RES-EM-012, RES-EM-013, RES-EM-014, RES-EM-015, RES-EM-016, RES-EM-017, RES-EM-018, RES-EM-019, RES-EM-020, RES-EM-021, RES-EM-022, RES-EM-023, RES-EM-024
**Decision:** ADR-000211
**Evidence:** VER-000008
**Test-Run:** 20261126T222137-89790
**Test-Suites:** core, app, cmake, layering

## Scope

Electromagnetic domain: antennas, arrays, RF analysis, guides, Yee full-wave solver, UI and examples

## Implementation

- `src/core/electromagnetics` (new) — `Conventions` (constants, e^{+jωt}), `EmMaterial` (complex permittivity, classification, skin depth, Fresnel), `Antennas` (Si/Ci, Hertzian / small-loop / thin-dipole exact fields, induced-EMF impedance, monopole, loop, resonant length, directivity, matching), `ArrayFactor` (element patterns, array factor, phase / true-time-delay steering, Chebyshev, pattern analysis, grating-lobe prediction, least-squares synthesis), `Lines` (coax, microstrip, stripline, CPW, loaded lines, transformers, L-networks, losses, guide modes, propagation, cavities, TE10), `Yee` (2D TM / 3D FDTD, split-field PML, PEC, periodic, materials, thin-wire edges, probes, snapshots, harmonic DFT, energy), `EmComponents` (antenna, source, array, guide components; antenna and guide geometry kinds), `EmStudy` (settings, antenna report, sweeps, patterns, guide report, analytical fields on grids, TE10 cross-section), `EmFullWave` (setup from a project, plan and refusal, run, job), `Touchstone` (one-port import / export).
- `src/core/geometry/SolidModel` (moved from `src/core/fdtd`, plain move) — shared simulation bodies with antenna and guide kinds; `FdtdSetup` and `test_fdtd_scene.cpp` include the new path; registrations in `scene/Component.cpp` and `geometry/Geometry.cpp`.
- `src/render/FieldLayer` — a decibel quantity (40 dB range); `src/app/AcousticController.cpp` describes it.
- `src/app` — `EmController`, `EmPanel` (`forms/EmPanel.ui`), `EmEditor` (`forms/EmEditor.ui`), `EmMarkers`, `ObjectLabels`, `MainWindow` (`forms/MainWindow.ui`: Electromagnetics menu with 19 actions and dock), `src/main/main.cpp` (`--run-em field|pattern|full-wave`).
- `examples/electromagnetics/a-` .. `g-` (seven projects, generated and checked by the scene tests).
- Documents: `docs/requirements/electromagnetics.md` (RES-EM-001..024), ADR-000211, thirteen documents in `docs/electromagnetics/` with two screenshots in `docs/images/`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `docs/README.md`, `docs/architecture/overview.md`, `examples/README.md`, root `README.md`, manifests.

## Tests

- `tests/core/test_electromagnetics.cpp` (17 cases: foundations, antennas, matching, arrays, lines, guides, Yee stability, line source, PEC cylinder with refinement, Fresnel and absorbing layer, cavity with refinement, 3D current element), `tests/core/test_electromagnetics_scene.cpp` (9 cases: examples A - G including a mesh-convergence study for G, a 3D dipole from a scene with refusals, Touchstone and persistence), `tests/app/test_em_ui.cpp` (6 cases through the real main window). Results in the linked run.

## Regression impact and documentation

- The `SolidModel` move touches the acoustic full-wave setup (same code, new path): covered by the existing `Fdtd*` cases. The field layer's new quantity is appended to the enum, so acoustic displays are unchanged. A dangling range-for over a temporary JSON value in array override loading was found by the example round trip and fixed before commit.
- The CMake check (`tools/verify_cmake_build.sh`, suite `cmake`) timed out at 600 s once the electromagnetic solver tests ran unoptimized (run 20261126T221016-56211: core 247 and app 76 cases passed, cmake TIMEOUT). It now builds its own tree `build/cmake-verify` as Debug at -O1 (assertions kept) and its expected time in `scripts/test_manifest.json` is 420 s; `BUILDING.md` says so.
- Documentation as listed under Implementation.

## Traceability

REQ-CHAT-20261126-007 -> RES-EM-001, RES-EM-002, RES-EM-003, RES-EM-004, RES-EM-005, RES-EM-006, RES-EM-007, RES-EM-008, RES-EM-009, RES-EM-010, RES-EM-011, RES-EM-012, RES-EM-013, RES-EM-014, RES-EM-015, RES-EM-016, RES-EM-017, RES-EM-018, RES-EM-019, RES-EM-020, RES-EM-021, RES-EM-022, RES-EM-023, RES-EM-024 -> WORK-000008 -> evidence (pending)

## Transitions

- 2026-11-27T03:08:11.677149+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-27T03:28:27.766871+00:00 active -> completed by agent-dev-20261126151713-76f632
