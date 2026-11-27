# Performance and Compute Backends

## Backends (RES-EM-024, RES-COMPUTE-001..011)

All electromagnetic calculations run in double precision. The CPU path is
the reference and the default; the Yee solver, the analytical field
evaluation and pattern sampling use every hardware thread with results
bit-identical to one thread. The Yee solver also has an optional CUDA
backend (chosen in the Fields tab), which repeats the reference arithmetic
on an NVIDIA GPU; it is compared with the CPU by an automated test and is
not yet validated on this project's machine (`docs/compute/ACCELERATION_AUDIT.md`).
Every full-wave result records where it ran, for example "CPU, double
precision, 16 threads".

## Costs

* **Analytical fields:** proportional to samples x radiators (x elements
  for arrays); an 81 x 81 plane around a dipole takes milliseconds.
* **Patterns:** (180 / step + 1) x (360 / step) directions; a 64-element
  array at 1 deg takes well under a second.
* **Array analysis:** samples the sphere at 0.25 - 0.5 deg.
* **Yee:** memory about 27 doubles per cell plus coefficients (the plan
  states it); work = cells x steps. A 2D run at 30 cells per wavelength on
  5 x 5 wavelengths takes about 2 s; a 3D dipole on 1.6^3 wavelengths at
  16 cells per wavelength about 2.5 s (Release build, one core; threads divide these times by about four). The plan
  estimates the time at 1.5e7 cell updates per second (one core); threads run faster.

## Limits and refusal

The default memory limit is 1 GiB (adjustable in the Fields tab); plans
above it, or above 2e11 cell updates, are refused with the numbers. The
resolution is never lowered to fit (RES-EM-017).

## Measured

Yee 3D, 5.3e5 cells, 60 steps (Release, Intel Core Ultra 7 255H): 1.95 s on
one thread, 0.45 s on sixteen (4.4x). See `docs/compute/ACCELERATION_AUDIT.md`
for the method and the other paths.

## Not implemented

No OpenCL, SYCL or other vendor-neutral GPU backend, and no single
precision. Any new backend plugs in behind the Yee stepper interface
(ADR-000212), keeps the physics independent of the execution backend and
must reproduce the tolerances of `ELECTROMAGNETIC_VALIDATION.md`.
