# Acceleration Audit: Threads, CUDA and VOLK

**Status:** current for REQ-CHAT-20261126-014 (RES-COMPUTE-011, ADR-000212).

Every compute path of `src/core`, what it costs, and how it is
accelerated or why it is not. "Bit-identical" means the accelerated path
returns exactly the reference's numbers (tested).

## Machine of the measurements

Intel Core Ultra 7 255H (16 cores: 6 performance at 5.1 GHz, 8 efficiency
at 4.4 GHz, 2 low-power at 2.5 GHz), NVIDIA GeForce RTX 5070 Laptop GPU
(sm_120, 8 GB), CUDA 13.5 (driver 13.2), VOLK 3.1.2 (`avx2_64_mmx_orc`),
GCC 13.3, Release build (`-O3`, x86-64 baseline: SSE2, no AVX, no FMA).

## Summary

| Path | Character | CPU threads | CUDA | VOLK |
|---|---|---|---|---|
| Acoustic FDTD (`fdtd/FdtdGrid`, `FdtdRun`) | stencil, memory-bound, double | **done**, bit-identical; 1.9x | **implemented** (`FdtdDevice.cu`); validation pending a free GPU | no: double precision |
| Electromagnetic Yee (`electromagnetics/Yee`) | stencil, split fields, double | **done**, bit-identical; 4.4x | **implemented** (`YeeDevice.cu`); validation pending a free GPU | no: double precision |
| Steady-state accumulation, snapshots, energy, peak | per cell, every step in the window | **done** (ordered reductions) | **done** on the device | no |
| Acoustic analytical fields (`acoustics/FieldGrid`) | sum over sources per sample | already threaded (before this work) | not done: memory-light, already fast; candidate | no: double |
| EM analytical fields (`EmStudy::evaluateFields`) | exact fields per sample | **done**, bit-identical | not done: small grids | no |
| EM patterns, array analysis (`patternOf`, `analyzePattern`) | sum over elements per direction | **done**, bit-identical | not done | no |
| Field display conversions (`EmFieldResult::componentArray`) | single precision, per sample | — | — | **done** (magnitudes); 1.2x |
| Measured sample streams (request 008) | single precision | — | — | **layer ready** (int16 / int32 to float, products, conjugate products, dot products) |
| Spectra (`dsp/Spectrum`, own FFT) | short records, double | no: short | no | no: double |
| Mechanics: sparse LDLT / LU, Lanczos, dense eigen (`mechanics/*`) | sparse direct factorization | no: Eigen's sparse solvers are sequential | no: would need cuSPARSE/cuSOLVER, a new dependency for small models | no |
| Mechanics: Newmark transients | sparse matrix-vector per step | no (small models) | no | no |
| Inverse design, synthesis (`acoustics/InverseDesign`, `ArrayFactor` synthesis) | small dense SVD | no | no | no |
| Rendering | OpenGL already on the GPU | — | — | — |

## Measured (perf lane, `run_tests.py --tier perf`)

Release build, the step loop only, 2026-11-26:

| Benchmark | 1 thread | 16 threads | Speed-up |
|---|---|---|---|
| Yee 3D, 80³ nodes with layers (5.3e5 cells), 60 steps | 1.95 s (1.6e7 cell updates/s) | 0.45 s (7.1e7/s) | 4.4x |
| Acoustic 3D, 116³ cells with layers (1.6e6), 100 steps | 1.01 s (1.5e8/s) | 0.53 s (2.9e8/s) | 1.9x |
| \|z\| of 16.8 M complex floats, 20 times | plain loop 0.198 s | VOLK 0.167 s | 1.2x |

The acoustic update moves about 120 bytes per cell; one thread already
streams about 19 GB/s and sixteen about 35 GB/s, near what the memory
delivers, so more threads cannot help. The Yee update does more arithmetic
per byte and scales further. VOLK gains little over a loop GCC already
vectorizes, because the default build targets SSE2 and VOLK brings AVX2;
the gain is larger for kernels GCC does not vectorize (conjugate products,
integer conversion with scaling).

Before chunked scheduling the threaded speed-ups were 1.6x and 1.2x: a
fixed equal share per thread made every step wait for the two low-power
cores (RES-COMPUTE-003).

**CUDA:** not measured and not validated on this machine yet. The GPU's
memory was taken by another program (7.6 of 8 GB), so no CUDA context
could be created; the comparison tests skip with that reason. The kernels
compile for sm_75 to sm_120 and run the reference arithmetic in double
precision without fused multiply-add. Expected: memory-bound like the CPU,
with the GPU's higher bandwidth (about 384 GB/s); consumer GPUs compute
double precision at 1/64 of single precision, which a bandwidth-bound
stencil does not reach.

## Correctness of the accelerated paths

* Threads: `AcceleratedYee.ThreadsGiveBitIdenticalResultsIn3DAnd2D` and
  `AcceleratedAcoustics.ThreadsGiveBitIdenticalResultsIn2DAnd3D` compare 1,
  2, 3, 5 and 7 threads on problems with every boundary kind, several
  materials, conductors, thin wires, two sources (one shared with a probe),
  snapshots and a steady-state window: probes, snapshots, steady-state
  fields, energies and peaks are identical.
* Writing those tests found a defect in the electromagnetic run loop:
  along a periodic axis the steady-state field at the last node was read
  past the end of the arrays (garbage values on that face). Fixed; the
  regression test is `AcceleratedYee.PeriodicAxisLastNodeIsTheFirstNode`.
* CUDA: `AcceleratedYee.CudaMatchesTheCpuReference` and
  `AcceleratedAcoustics.CudaMatchesTheCpuReference` (same problems,
  relative tolerance 1e-12) and `AcceleratedBackends.CudaIsRefusedWithTheReasonWhenItCannotRun`.
* VOLK: `ComputeKernels.VectorKernelsMatchThePlainLoops`,
  `ComputeKernels.FieldMagnitudeArrayMatchesThePerSampleValue`.

## What remains

1. Validate the CUDA backends and measure them on a machine with a free GPU.
2. Keep the solver state on the device between the steady-state window and
   the result (done) and avoid the host copy of the fields (the CPU arrays
   are still allocated alongside the device arrays: twice the memory).
3. A vendor-neutral GPU backend behind the same stepper interface, if needed.
4. Single precision as an option on the GPU (half the memory traffic),
   validated against the double-precision reference.
5. VOLK in the acquisition pipeline of request 008 (the kernels exist).
6. The acoustic analytical engine on the GPU for very large field regions.
