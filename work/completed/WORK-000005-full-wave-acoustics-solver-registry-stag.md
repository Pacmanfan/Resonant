# WORK-000005: Full-wave acoustics: solver registry, staggered-grid FDTD, boundaries, materials, transient signals, spectra

**Status:** completed
**Type:** feature
**Request:** requests/processing/2026-11-26-prompt-9-resonant-advanced-aco-req-chat-20261126-005.md
**Requirements:** RES-FULLWAVE-001, RES-FULLWAVE-002, RES-FULLWAVE-003, RES-FULLWAVE-004, RES-FULLWAVE-005, RES-FULLWAVE-006, RES-FULLWAVE-007, RES-FULLWAVE-008, RES-FULLWAVE-009, RES-FULLWAVE-010, RES-FULLWAVE-011, RES-FULLWAVE-012, RES-FULLWAVE-013, RES-FULLWAVE-014, RES-FULLWAVE-015, RES-FULLWAVE-016, RES-FULLWAVE-017, RES-FULLWAVE-018
**Decision:** ADR-000209
**Evidence:** VER-000005
**Test-Run:** 20261126T200829-3835036
**Test-Suites:** core, app, cmake, layering

## Scope

Full-wave acoustics: solver registry, staggered-grid FDTD, boundaries, materials, transient signals, spectra

## Implementation

- `src/core/fdtd` (new) — `FdtdGrid` (staggered-grid pressure-velocity solver, 2D/3D, per-cell materials, rigid / pressure-release / impedance / absorbing faces, split-field PML, conserved energy), `FdtdRun` (run, probes, snapshots, steady-state extraction, energy, stability monitor, cancellation), `Waveform` (continuous, tone burst, Gaussian, Ricker, sweep, sampled), `Spectrum` (radix-2 FFT, resolution, windows, peaks), `SolidModel` (inside test for every geometry kind), `FdtdSetup` (settings, preflight plan, project-to-grid setup, results, job).
- `src/core/solvers` (new) — `SolverRegistry`: declared capabilities of the three acoustic solvers and an assessment of a scene against them; nothing selects a solver.
- `src/core/acoustics/Propagation` — sources keep amplitude, phase and delay separately for time-domain use.
- `src/app` — `FdtdController`, `FdtdPanel` (`forms/FdtdPanel.ui`), `TimeSignalPanel` (`forms/TimeSignalPanel.ui`), `PlotWidget`; `forms/MainWindow.ui` gains the Full-Wave Solver dock and Run Full-Wave Simulation (F6); `AcousticController::addResult` / `displayTransient`; `src/main/main.cpp` gains `--run-full-wave`.
- Documents: `docs/requirements/full_wave_acoustics.md`, ADR-000209, eleven documents in `docs/advanced_acoustics/`, status, decisions, overview, roadmap pointer, example index.

## Tests

- `tests/core/test_fdtd.cpp` (26 cases: signals, spectra, stability, propagation and convergence, boundaries, PML, materials, resonance, recording), `tests/core/test_fdtd_scene.cpp` (14 cases: simulation geometry, planning, refusals, assessment, settings, scene sources, eight example projects generated and checked), `tests/app/test_fullwave_ui.cpp` (5 cases). Core suite 187 cases, application suite 63 cases, CMake build and CTest, layering: all PASS in the linked run.

## Regression impact and documentation

- The acoustic point source gained three fields (existing behaviour unchanged); the acoustic controller gained two entry points used only by the full-wave controller. All earlier acoustic, manipulator and foundation tests pass unchanged. Documentation updated: requirements manifest, decision manifest, `docs/README.md`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, architecture overview, `docs/acoustics/ACOUSTIC_VALIDATION.md` (pointer), `examples/README.md`.

## Traceability

REQ-CHAT-20261126-005 -> RES-FULLWAVE-001, RES-FULLWAVE-002, RES-FULLWAVE-003, RES-FULLWAVE-004, RES-FULLWAVE-005, RES-FULLWAVE-006, RES-FULLWAVE-007, RES-FULLWAVE-008, RES-FULLWAVE-009, RES-FULLWAVE-010, RES-FULLWAVE-011, RES-FULLWAVE-012, RES-FULLWAVE-013, RES-FULLWAVE-014, RES-FULLWAVE-015, RES-FULLWAVE-016, RES-FULLWAVE-017, RES-FULLWAVE-018 -> WORK-000005 -> evidence (pending)

## Transitions

- 2026-11-27T01:07:15.467704+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-27T01:16:50.631974+00:00 active -> completed by agent-dev-20261126151713-76f632
