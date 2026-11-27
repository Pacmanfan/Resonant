# Compute Backends: Threads, CUDA and VOLK

**Owner of:** `RES-COMPUTE-001..011`
**Source:** REQ-CHAT-20261126-014 ("audit and implement volk and cuda where and when possible")
**Refines:** `product.md` RES-PRODUCT-014; `full_wave_acoustics.md` RES-FULLWAVE-006; `electromagnetics.md` RES-EM-017, RES-EM-024
**Decisions:** ADR-000205 (CPU reference first), ADR-000212
**Documents:** `docs/compute/`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-COMPUTE-001 — The CPU reference stays the reference

Every solver keeps its CPU implementation in double precision as the
reference and the default. An accelerated path is offered only after it is
compared with the reference by an automated test, and its result says
where it ran.
*Verification:* `AcceleratedYee.*`, `AcceleratedAcoustics.*`.

### RES-COMPUTE-002 — Threaded solvers give the reference's numbers

The acoustic FDTD and the electromagnetic Yee solvers split their cell
updates over CPU threads (all hardware threads by default, a chosen number
on request). Every cell is computed by one thread with the reference's
arithmetic, and reductions (energy, peak pressure) use one partial per row
added in row order, so results are bit-identical for any thread count.
*Verification:* `AcceleratedYee.ThreadsGiveBitIdenticalResultsIn3DAnd2D`, `AcceleratedAcoustics.ThreadsGiveBitIdenticalResultsIn2DAnd3D`.

### RES-COMPUTE-003 — Work is balanced over unequal cores

Threads take small chunks of a loop from a shared counter, so processors
with performance, efficiency and low-power cores are kept busy. The
schedule never changes a result (RES-COMPUTE-002).
*Verification:* `ComputeThreads.PoolCoversEveryIndexOnceWithAFixedPartition`; perf lane benchmark.

### RES-COMPUTE-004 — An optional CUDA backend for both full-wave solvers

Both full-wave solvers can run on an NVIDIA GPU through CUDA in double
precision, with the reference's arithmetic (no fused multiply-add) and the
same recording (probes, snapshots, steady-state field, energy, peak).
CUDA is used only when chosen; a plan or run that chose it on a machine
without a usable device (no CUDA in the build, no device, no context,
too little free memory) is refused with the reason, never moved to the CPU
silently. The physics does not depend on CUDA: builds without it are
complete.
*Verification:* `AcceleratedYee.CudaMatchesTheCpuReference`, `AcceleratedAcoustics.CudaMatchesTheCpuReference` (skipped, with the reason, without a usable device), `AcceleratedBackends.CudaIsRefusedWithTheReasonWhenItCannotRun`.

### RES-COMPUTE-005 — Acceleration is optional in both builds

CMake and qmake find VOLK (pkg-config) and CUDA (nvcc) by themselves and
build without them when absent; each can be switched off
(`RESONANT_WITH_VOLK`, `RESONANT_WITH_CUDA`; `CONFIG+=no_volk`,
`CONFIG+=no_cuda`). The CUDA runtime is linked statically; the GPU
architectures compiled in are Turing, Ampere, Ada and Blackwell, plus PTX
of the newest.
*Verification:* suite `cmake` (CMake build with both), the qmake build (`devsys.py build`), stub sources for builds without CUDA.

### RES-COMPUTE-006 — Vector kernels through VOLK, with plain fallbacks

Single-precision vector kernels (products, sums, scaling, square roots,
complex magnitudes, conjugate products, dot products, integer-to-float
conversion, precision conversion) run through VOLK when present and as
plain loops otherwise; both are compared by a test. They are used where
data is single precision: field display conversions now, measured sample
streams when acquisition exists. The solvers' double-precision arithmetic
does not use them.
*Verification:* `ComputeKernels.*`.

### RES-COMPUTE-007 — Analytical evaluations use the threads too

Analytical electromagnetic field evaluation on regions and pattern
sampling for antennas and arrays run over the threads with each sample
computed by one thread and sums kept in order, giving the single-threaded
result.
*Verification:* `EmExamples.*`, `EmArrays.*` (unchanged expectations).

### RES-COMPUTE-008 — What the machine offers is visible

The application logs the compute capabilities at start and shows them in
Analysis > Diagnostics (hardware threads, VOLK version and kernel family,
CUDA runtime and driver, every device with its compute capability, memory
and whether it can be used, with the reason when not). Each full-wave plan
states where it will run.
*Verification:* `ComputeBackends.OptionsCapabilitiesAndChecks`; plans in `EmUiTest.ExampleG_*`.

### RES-COMPUTE-009 — Where a result ran is recorded

Full-wave results record the backend that produced them (CPU and thread
count, or CUDA and device name, with the precision) and the compute
options, in their metadata and the project's run records. The options are
part of the solver settings saved in the project.
*Verification:* `AcceleratedYee.*` (backend strings), example files with `compute` settings.

### RES-COMPUTE-010 — Measured, in a lane of its own

A benchmark in the `perf` lane measures each solver single-threaded,
threaded and (when usable) on CUDA, and the VOLK kernels against plain
loops, and asserts a threaded speed-up on machines with four or more
threads. The figures are documented with the machine they were measured on.
*Verification:* `AcceleratedBenchmark.DISABLED_BenchmarkSolversAndKernels` (`run_tests.py --tier perf`); `docs/compute/ACCELERATION_AUDIT.md`.

### RES-COMPUTE-011 — Every compute path is assessed

`docs/compute/ACCELERATION_AUDIT.md` lists every compute path of the
core, how it is accelerated or why it is not, and what remains.
*Verification:* the document; `docs/compute/COMPUTE_BACKENDS.md` for use.
