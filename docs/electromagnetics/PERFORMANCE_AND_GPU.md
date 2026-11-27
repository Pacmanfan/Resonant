# Performance and Compute Backends

## The reference backend (RES-EM-024)

All electromagnetic calculations run on the CPU in double precision; the
Yee solver uses one thread. This is the reference against which any
accelerated backend would be validated. Every full-wave result records
"CPU, double precision, single thread" in its metadata.

## Costs

* **Analytical fields:** proportional to samples x radiators (x elements
  for arrays); an 81 x 81 plane around a dipole takes milliseconds.
* **Patterns:** (180 / step + 1) x (360 / step) directions; a 64-element
  array at 1 deg takes well under a second.
* **Array analysis:** samples the sphere at 0.25 - 0.5 deg.
* **Yee:** memory about 27 doubles per cell plus coefficients (the plan
  states it); work = cells x steps. A 2D run at 30 cells per wavelength on
  5 x 5 wavelengths takes about 2 s; a 3D dipole on 1.6^3 wavelengths at
  16 cells per wavelength about 2.5 s (Release build, one core). The plan
  estimates the time at 1.5e7 cell updates per second.

## Limits and refusal

The default memory limit is 1 GiB (adjustable in the Fields tab); plans
above it, or above 2e11 cell updates, are refused with the numbers. The
resolution is never lowered to fit (RES-EM-017).

## Not implemented

No OpenMP, SIMD-specific, CUDA or OpenCL backend exists yet. When one is
added it must stay vendor-neutral, keep the numerical core independent of
the execution backend, and be validated against this CPU reference with
the tolerances in `ELECTROMAGNETIC_VALIDATION.md`.
