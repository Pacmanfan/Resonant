# Performance and Compute Backends

**Status:** current for REQ-CHAT-20261126-005.

## What exists

One backend: a single-threaded CPU implementation in double precision. It
is the reference. No OpenMP, SIMD intrinsics, GPU, CUDA or OpenCL code
exists, and no backend selection (there is nothing to select).

## Measured

Release build (GCC 13, `-O2`), one core of an Intel Core Ultra 7 255H,
2026-11-26:

| Grid | Cell updates per second | State memory |
|---|---|---|
| 2D, 1024 × 1024 | 1.5e8 | 41 MiB |
| 3D, 144 × 144 × 144 | 1.1e8 | 162 MiB |

A Debug build is about ten times slower. The preflight's time estimate
assumes 6e7 updates per second so that it errs on the long side.

Memory per cell: pressure, its two or three split parts and two or three
velocity components in double precision plus one material byte — 41 bytes
in 2D, 57 bytes in 3D. The split parts are stored everywhere, not only in
the absorbing layers, which costs two or three doubles per cell; storing
them in the layers only would save about a third.

Typical sizes at 40 kHz in air, 12 cells per wavelength (0.72 mm cells):

| Region | Cells with layers | Steps | Time (estimated from the measured rate) |
|---|---|---|---|
| 2D, 120 × 180 mm | 53 000 | 1 100 | 0.3 s |
| 3D, 34 mm cube, 10 cells per wavelength, 8-cell layers | 176 000 | 470 | 0.8 s |
| 3D, 100 mm cube | 4.4 million | 1 150 | about 45 s, 240 MiB |
| 3D, 300 mm cube | 88 million | 2 900 | refused at the default 1 GiB limit (4.6 GiB); about 40 minutes if the limit is raised |

## Resource control

Preflight estimates of memory and work; a memory limit (default 1 GiB) and
a work limit (5e11 cell updates) that refuse a run with what to change;
recording that is opt-in beyond probes; cancellation checked 200 times per
run; the user interface stays responsive because the run is a background
job with progress.

Not implemented: checkpointing and resumption, region-of-interest storage,
out-of-core or HDF5 storage of fields.

## What would help, in order

1. Threads over slabs of the grid: the update of each field reads only the
   other field, so slabs are independent. Expected: near-linear on a few
   cores until memory bandwidth limits it.
2. Single precision as an option: half the memory traffic. It must be
   compared with the double-precision reference; long runs accumulate
   rounding in the source integral and the energy sum.
3. Split fields in the layers only.
4. A GPU backend through a portable interface (OpenCL, SYCL or Vulkan
   compute; not a single vendor's). The stencil is a textbook case and
   gains of one to two orders of magnitude are usual.

Each accelerated backend must reproduce the reference tests of
`ACOUSTIC_VALIDATION.md` within stated tolerances, including boundary
behaviour, a long-run energy check and cancellation with resource cleanup,
before it is offered. None has been written, so none of this has been
measured here.
