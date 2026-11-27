# ADR-000212: Compute backends — deterministic CPU threads, an optional CUDA backend behind a stepper interface, VOLK for single-precision kernels

**Status:** ACCEPTED
**Date:** 2026-11-26
**Type:** architecture
**Requirements:** `docs/requirements/compute.md` RES-COMPUTE-001..011
**Request:** REQ-CHAT-20261126-014
**Builds on:** ADR-000205 (CPU reference first), ADR-000209, ADR-000211

## Context

The operator asked to "audit and implement VOLK and CUDA where and when
possible". Both full-wave solvers had a validated single-threaded CPU
reference; nothing else was accelerated except the acoustic analytical
field evaluation (threads). The machine has CUDA 13.5 with an RTX 5070
Laptop GPU (sm_120) and VOLK 3.1.2; its CPU mixes performance, efficiency
and low-power cores.

## Decisions

1. **A domain-independent `src/core/compute` module**: a thread pool,
   compute options (backend, threads, device), capability detection, the
   CUDA runtime queries, and the VOLK kernel layer. Domains use it like
   `dsp`; it knows no physics.
2. **Threads first, and deterministic.** Cell updates are split by rows;
   each cell is computed by one thread with unchanged arithmetic; every
   reduction keeps a fixed order (a partial per row, summed in row order).
   Results are bit-identical for any thread count, so threads need no
   separate validation tolerance and are the default (all hardware threads).
3. **Chunked scheduling** (a shared counter, about eight chunks per thread)
   instead of one fixed share per thread: on hybrid processors a fixed split
   waits for the slowest core (measured: 1.6x instead of 4.4x).
4. **A stepper interface per solver** (`runYee`, `fdtd::run`): the run loop
   and its recording are written once; a CPU stepper and a CUDA stepper
   implement step, probes, steady-state accumulation, snapshots, energy and
   peak. A further backend (for example a vendor-neutral one) is another
   stepper; the physics and the results do not change.
5. **CUDA as an optional backend, not a dependency.** It is compiled only
   when a toolkit is found, switched off on request, linked statically
   (cudart_static), for Turing to Blackwell plus PTX. Kernels repeat the CPU
   arithmetic in double precision and are compiled without fused
   multiply-add (`--fmad=false`); the host build has no FMA contraction
   either, so the fields are the reference's numbers. Device code sees only
   plain-data headers (`YeeDevice.h`, `FdtdDevice.h`), never the core's
   C++ headers. This resolves the earlier wish for a vendor-neutral GPU
   path (`docs/advanced_acoustics/PERFORMANCE_AND_GPU.md`, the
   electromagnetics request): CUDA is used because the operator asked for
   it, it is optional, the CPU stays the default and reference, and the
   stepper interface keeps a portable backend possible.
6. **Never a silent fall-back.** Choosing CUDA where it cannot run refuses
   the plan or the run with the reason (no device, no context because the
   memory is taken, not enough free memory). The backend actually used is
   recorded in every result.
7. **VOLK only where data is single precision.** VOLK's kernels are almost
   all float; the solvers' reference arithmetic is double. VOLK is used for
   field display conversions now and is the layer for measured sample
   streams (request 008); each kernel has a plain implementation and a test
   comparing the two.
8. **Not accelerated, by decision**: the mechanics sparse direct solvers
   and eigensolvers (Eigen's sparse factorizations are sequential; a GPU
   sparse solver would be a new dependency for problems that are small
   here), the acoustic analytical engine on the GPU (already threaded,
   memory-light), the own FFT (short signals).

## Consequences

* Full-wave runs use all cores by default with unchanged results.
* The memory bandwidth limits the acoustic solver to about 2x on this
  machine; the electromagnetic solver (more arithmetic per cell) gains
  about 4.4x.
* GPU validation needs a usable device: on a machine whose GPU memory is
  taken the CUDA comparisons are skipped and say so; they are not counted
  as validation.
* Example projects store the compute options (`compute` in the solver
  settings); files without them load with the defaults.

## Deviations from the request

* VOLK is not used inside the solvers (double precision); see decision 7.
* No vendor-neutral GPU backend was written; the interface allows one.
