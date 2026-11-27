# WORK-000011: Experimental hardware, acquisition, calibration and simulation correlation

**Status:** completed
**Type:** feature
**Request:** requests/processing/2026-11-26-prompt-12-resonant-experimenta-req-chat-20261126-008.md
**Requirements:** RES-EXP-001, RES-EXP-002, RES-EXP-003, RES-EXP-004, RES-EXP-005, RES-EXP-006, RES-EXP-007, RES-EXP-008, RES-EXP-009, RES-EXP-010, RES-EXP-011, RES-EXP-012, RES-EXP-013, RES-EXP-014, RES-EXP-015, RES-EXP-016, RES-EXP-017, RES-EXP-018, RES-EXP-019, RES-EXP-020, RES-EXP-021, RES-EXP-022, RES-EXP-023, RES-EXP-024, RES-EXP-025, RES-EXP-026, RES-EXP-027
**Decision:** ADR-000213
**Evidence:** VER-000011
**Test-Run:** 20261127T013109-998624
**Test-Suites:** core, app, cmake, layering

## Scope

Experimental hardware, acquisition, calibration and simulation correlation

## Implementation

- `src/core/experiments/` (new, domain independent): ExperimentTypes (quantities, categories incl. command, formats, poses, processing steps), Uncertainty, Safety (limits, single-run output authorization, latched emergency stop), Hardware (abstraction, registry with asynchronous discovery), SimulatedHardware (audio interface, vibration DAQ, RF receiver, signal generator, VNA, positioner; fault injection), Experiment (definition, validation, stimulus), Spatial (scans, reconstruction with coverage), MeasurementRecord, MeasurementStore (HDF5 / binary, exclusive and read-only), Acquisition (reader/recorder threads, bounded queue, integrity checks, live view), Calibration (append-only library, application, procedures), Processing (17 operations, deterministic ids), Comparison, ExperimentRunner (13 steps, states, job), ExperimentReport, ExperimentArchive, ExperimentProject.
- `src/core/dsp/`: Windows, SpectralEstimation (Welch, H1, coherence, impulse response, tone), CrossCorrelation (delay), Filters (FIR, decimation, resampling).
- Domain adapters: `acoustics/AcousticExperiments`, `mechanics/MechanicalExperiments`, `electromagnetics/RfExperiments`; `electromagnetics/Touchstone` N-port 1.1 / 2.0.
- `src/hardware/` (new library resonant_hardware): ALSA driver, SoapySDR discovery, driver registration; `hdf5.pri`, `hardware.pri`, CMake options RESONANT_WITH_HDF5/ALSA/SOAPYSDR; project.pro and all link lines.
- App: ExperimentsController, ExperimentsPanel (forms/ExperimentsPanel.ui), OutputAuthorizationDialog (forms/OutputAuthorizationDialog.ui), Experiments menu and dock in MainWindow(.ui), viewport extra items; `--run-experiment` / `--authorize-output` in main.cpp.
- `tools/check_layering.py`: core never includes src/hardware; hardware has no Qt; experiments includes no physics domain.
- Examples: `examples/experiments/` (acoustic, mechanical, RF projects; labelled synthetic .s2p).

## Tests

- New: `tests/core/test_experiments.cpp` (14), `test_experiment_dsp.cpp` (6), `test_experiment_domains.cpp` (10), `test_experiment_examples.cpp` (3), `test_hardware_drivers.cpp` (3: ALSA on the null device, SoapySDR discovery), `tests/app/test_experiments_ui.cpp` (4). `tests/core/test_examples.cpp`: an example holds scene objects or experiments.
- Bugs found by the new tests and fixed: a silent calibrator recording was accepted (zero sensitivity); dangling iteration over a temporary JSON in experiment loading; dangling pointers into a temporary workspace in the controller; scan angles shifted by one position in the runner.
- t2 run 20261127T013109-998624: PASS (core 295 cases, app 81 cached from the previous run, cmake build and ctest, scripts, audits, layering). A first t2 run failed on the example check (fixed) and on the load-sensitive framework test test_workers.test_worker_state_is_isolated, which passed alone and in the second run (EXPECTED / PRE-EXISTING, load sensitive). The experiment tests also passed from a Release CMake tree (37 core, 4 GUI). CUDA comparison tests skipped (GPU memory taken by another program; unchanged from REQ-014).

## Regression impact and documentation

- Offline work unchanged: projects without experiments load unchanged (no schema change; extensions.experiments only when present). Existing examples, app and core suites pass.
- Documentation: `docs/requirements/experiments.md` (RES-EXP-001..027), ADR-000213, 13 documents in `docs/experiments/` with an index, manifests, IMPLEMENTATION_STATUS.md (new section), DECISIONS.md, docs/README.md, BUILDING.md (HDF5, ALSA, SoapySDR), docs/architecture/overview.md, examples/README.md, README.md.
- Honest limits recorded: ALSA tested on the null device only; SoapySDR discovery only; level triggering, SCPI/VISA, UHD, libiio, PortAudio not implemented; uncertainties assumed uncorrelated.

## Traceability

REQ-CHAT-20261126-008 -> RES-EXP-001, RES-EXP-002, RES-EXP-003, RES-EXP-004, RES-EXP-005, RES-EXP-006, RES-EXP-007, RES-EXP-008, RES-EXP-009, RES-EXP-010, RES-EXP-011, RES-EXP-012, RES-EXP-013, RES-EXP-014, RES-EXP-015, RES-EXP-016, RES-EXP-017, RES-EXP-018, RES-EXP-019, RES-EXP-020, RES-EXP-021, RES-EXP-022, RES-EXP-023, RES-EXP-024, RES-EXP-025, RES-EXP-026, RES-EXP-027 -> WORK-000011 -> evidence (pending)

## Transitions

- 2026-11-27T06:16:37.148708+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-27T06:47:04.568297+00:00 active -> completed by agent-dev-20261126151713-76f632
