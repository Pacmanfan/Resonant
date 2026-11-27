# WORK-000009: Compute backends: deterministic CPU threads, optional CUDA backend for both full-wave solvers, VOLK kernels

**Status:** completed
**Type:** feature
**Request:** requests/processing/2026-11-26-add-a-ticket-to-audit-and-impl-req-chat-20261126-014.md
**Requirements:** RES-COMPUTE-001, RES-COMPUTE-002, RES-COMPUTE-003, RES-COMPUTE-004, RES-COMPUTE-005, RES-COMPUTE-006, RES-COMPUTE-007, RES-COMPUTE-008, RES-COMPUTE-009, RES-COMPUTE-010, RES-COMPUTE-011
**Decision:** ADR-000212
**Evidence:** VER-000009
**Test-Run:** 20261126T233158-479348
**Test-Suites:** core, app, cmake, layering

## Scope

Compute backends: deterministic CPU threads, optional CUDA backend for both full-wave solvers, VOLK kernels

## Implementation

- `src/core/compute` (new) — `ThreadPool` (chunked scheduling, deterministic), `Compute` (backend options, capability detection, backend checks), `CudaRuntime` (`.cu` probe, stub without CUDA), `VectorKernels` (VOLK with plain fallbacks).
- Electromagnetic solver — `Yee.h/.cpp`: compute options in the spec, threaded `updateH`/`updateE`/energy (ordered per-row reduction), `eAtNode`, device export, a stepper interface in `runYee` (CPU and CUDA steppers), backend in the output; fix of the periodic-axis last-node read past the arrays; `YeeDevice.h/.cu` (CUDA kernels), `YeeDeviceStub.cpp`.
- Acoustic solver — `FdtdGrid.h/.cpp`: compute options, threaded velocity/pressure passes, ordered energy and peak reductions, device export, source increments; `FdtdRun.h/.cpp`: stepper interface (CPU and CUDA), backend in the output; `FdtdDevice.h/.cu`, `FdtdDeviceStub.cpp`.
- Setups — `FdtdSetup` and `EmFullWave`: `compute` in the settings (JSON), "Runs on" in the plan, refusal when CUDA cannot run, backend and options in the result metadata; examples with solver settings regenerated (the `compute` block only).
- Threaded analytical work — `EmStudy` (`evaluateFields`, `patternOf`), `ArrayFactor` (`analyzePattern`); `EmFieldResult::componentArray` through VOLK, used by `app/EmController`.
- Builds — `acceleration.pri` (qmake detection and switches, included from `warnings.pri`; Eigen as a system directory), `src/core/core.pro` (nvcc rule, `--fmad=false`), `CMakeLists.txt` and `src/core/CMakeLists.txt` (options, CUDA language, static cudart, warnings for C++ only).
- Application — `FdtdPanel` (`forms/FdtdPanel.ui`) and `EmPanel` (`forms/EmPanel.ui`): Compute and CPU-threads controls; `AnalysisPanel` diagnostics group; `Application` startup log line.
- Tests and build hygiene — `tests/core/test_compute.cpp`, `tests/core/test_acceleration.cpp`, a case in `tests/app/test_em_ui.cpp`; warnings fixed in `test_mechanics.cpp` and `test_fdtd.cpp`; `scripts/test_manifest.json` (perf-lane benchmark group, `acceleration.pri` as an input).
- Documents — `docs/requirements/compute.md` (RES-COMPUTE-001..011), ADR-000212, `docs/compute/ACCELERATION_AUDIT.md`, `docs/compute/COMPUTE_BACKENDS.md`; updated `docs/electromagnetics/PERFORMANCE_AND_GPU.md`, `docs/advanced_acoustics/PERFORMANCE_AND_GPU.md`, RES-EM-024, `BUILDING.md`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `docs/README.md`, `docs/architecture/overview.md`, manifests.

## Tests

- `tests/core/test_compute.cpp` (4 cases: pool coverage and reuse, VOLK against plain loops, field-magnitude array, options/capabilities/backend checks); `tests/core/test_acceleration.cpp` (6 cases: threads bit-identical for both solvers in 2D and 3D, CUDA refused with the reason, CUDA against the CPU for both solvers, periodic last-node regression; plus the disabled perf-lane benchmark); `tests/app/test_em_ui.cpp` (compute choice reaches the plan, CUDA never silently replaced). The CUDA comparisons skip with the reason on a machine without a usable device; on this machine the GPU's memory was taken by another program, so CUDA was NOT validated here. A CMake build with VOLK and CUDA off was also built and its compute tests passed.

## Regression impact and documentation

- Both full-wave solvers now run threaded by default: the results are bit-identical, and every earlier full-wave test passes unchanged. The energy reductions are summed per row (a rounding-level change of the energy diagnostics, within every test's tolerance). The project format gains `compute` in the solver settings; older files load with the defaults.
- A defect found by the threading tests: in the electromagnetic run loop, the steady-state field at the last node of a periodic axis was read past the end of the arrays; fixed with a regression test.
- The first fresh qmake build failed to link (`undefined reference to cudaRuntimeGetVersion`): the CUDA and VOLK libraries were in `LIBS`, before the projects' static libraries; they are in `QMAKE_LIBS` now (after them). The qmake build then had no warnings at all (23 before: Eigen headers not treated as system headers, two test files).
- Documentation as listed under Implementation; the earlier "single thread" statements were updated.

## Traceability

REQ-CHAT-20261126-014 -> RES-COMPUTE-001, RES-COMPUTE-002, RES-COMPUTE-003, RES-COMPUTE-004, RES-COMPUTE-005, RES-COMPUTE-006, RES-COMPUTE-007, RES-COMPUTE-008, RES-COMPUTE-009, RES-COMPUTE-010, RES-COMPUTE-011 -> WORK-000009 -> evidence (pending)

## Transitions

- 2026-11-27T04:27:15.143110+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-27T04:40:02.825862+00:00 active -> completed by agent-dev-20261126151713-76f632
