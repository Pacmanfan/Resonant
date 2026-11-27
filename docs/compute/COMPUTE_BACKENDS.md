# Compute Backends

**Status:** current (RES-COMPUTE-001..011, ADR-000212).

## Choosing where a solver runs

The full-wave panels (Full-Wave Solver for acoustics, Electromagnetics >
Fields for the Yee solver) have:

* **Compute:** *CPU (reference, double precision)* or *CUDA GPU (double
  precision)*.
* **CPU threads:** *All cores* (default) or a number.

The choice is part of the project's solver settings (`compute` in the
settings: `backend` `cpu`/`cuda`, `threads`, `device`). The plan says where
the run will go ("Runs on: ..."). If CUDA is chosen and cannot run, the
plan is refused with the reason, for example "CUDA device 0 (NVIDIA GeForce
RTX 5070 Laptop GPU) cannot be used: a CUDA context could not be created:
out of memory (the GPU's memory is taken by other programs). Choose the
CPU, or free the device; the backend is never switched automatically."

Every result records where it ran (`backend` in its metadata, for example
"CPU, double precision, 16 threads" or "CUDA on NVIDIA GeForce RTX 5070
Laptop GPU, double precision") and the options.

## What changes with threads

Nothing in the numbers. Each cell is computed by one thread exactly as
alone, and sums (energy, peak pressure) are formed per row and added in row
order, so any thread count gives the same results bit for bit. Threads
take small chunks of each loop, which keeps fast cores busy next to slow
ones. Analytical electromagnetic fields and pattern sampling are threaded
the same way.

## CUDA

* Needs an NVIDIA GPU of compute capability 7.5 or newer (Turing onward), a
  driver supporting the CUDA runtime the build used (the runtime is linked
  into Resonant), and free GPU memory for the solver state (the plan states
  it) plus 64 MiB.
* Double precision; the reference arithmetic without fused multiply-add,
  so results match the CPU to rounding (the tests use 1e-12).
* The CPU arrays are still allocated alongside the device's (twice the
  memory of a CPU run, in host memory).
* Analysis > Diagnostics lists the CUDA runtime, the driver and each device
  with its memory and whether it can be used.

## VOLK

VOLK provides SIMD kernels for single-precision data. Resonant uses them
through `compute/VectorKernels.h` (with plain fallbacks) for field display
conversions and, with request 008, for measured sample streams. VOLK picks
the best kernel family for the CPU at run time; `volk_profile` (from
`libvolk-bin`) can tune that choice per machine and stores it in
`~/.volk/volk_config`. The solvers do not use VOLK: their arithmetic is
double precision.

## Building

Both builds find VOLK and CUDA by themselves (BUILDING.md):

| | CMake | qmake |
|---|---|---|
| Turn VOLK off | `-DRESONANT_WITH_VOLK=OFF` | `CONFIG+=no_volk` |
| Turn CUDA off | `-DRESONANT_WITH_CUDA=OFF` | `CONFIG+=no_cuda` |
| Toolkit location | `CMAKE_CUDA_COMPILER` / `CUDACXX` | `CUDA_DIR=...` (default: the `nvcc` on `PATH`, else `/usr/local/cuda`) |
| GPU architectures | `RESONANT_CUDA_ARCHITECTURES` (default `75-real;86-real;89-real;120`) | `RESONANT_CUDA_ARCHS` (default `75 86 89 120`) |

The configure step reports `Resonant acceleration: VOLK ON|OFF, CUDA
ON|OFF`. Without CUDA the stub sources (`*Stub.cpp`) answer that CUDA is not
part of the build.

## Measuring

`python3 scripts/run_tests.py --tier perf` runs the benchmark (each solver
single-threaded, threaded and on CUDA when usable; VOLK against a plain
loop). Results for this project's machine: `ACCELERATION_AUDIT.md`.
