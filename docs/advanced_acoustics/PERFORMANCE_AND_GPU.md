# Performance and Compute Backends

**Status:** current for REQ-CHAT-20261126-005, updated by REQ-CHAT-20261126-014.

## What exists

The CPU implementation in double precision is the reference and the
default. Since REQ-CHAT-20261126-014 (`docs/requirements/compute.md`,
ADR-000212) it runs on every hardware thread with results bit-identical to
one thread (1.9x on 16 threads here: the update is memory-bound), and an
optional CUDA backend repeats it on an NVIDIA GPU. The backend is chosen in
the Full-Wave Solver panel (Compute, CPU threads); CUDA is refused with the
reason when no usable device is present and is not yet validated on this
project's machine (`docs/compute/ACCELERATION_AUDIT.md`). The single-core
figures below remain the reference measurements.

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

1. Threads over slabs of the grid — done (REQ-CHAT-20261126-014): 1.9x on
   sixteen threads, limited by memory bandwidth as expected.
2. Single precision as an option: half the memory traffic. It must be
   compared with the double-precision reference; long runs accumulate
   rounding in the source integral and the energy sum.
3. Split fields in the layers only.
4. A GPU backend — a CUDA backend exists (optional, not yet validated on
   this machine); a portable one (OpenCL, SYCL or Vulkan compute) could be
   added behind the same stepper interface. The stencil is a textbook case and
   gains of one to two orders of magnitude are usual.

Each accelerated backend must reproduce the reference tests of
`ACOUSTIC_VALIDATION.md` within stated tolerances, including boundary
behaviour, a long-run energy check and cancellation with resource cleanup,
before it is offered. The CUDA backend has the comparison tests; it has not
been measured or validated here yet.
