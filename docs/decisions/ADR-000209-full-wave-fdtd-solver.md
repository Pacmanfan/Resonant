# ADR-000209: Full-wave acoustics — staggered-grid FDTD with a split-field PML, a solver registry that assesses and never selects, simulation geometry from definitions

**Status:** ACCEPTED
**Date:** 2026-11-26
**Type:** architecture
**Requirements:** `docs/requirements/full_wave_acoustics.md` RES-FULLWAVE-001..018
**Request:** REQ-CHAT-20261126-005
**Builds on:** ADR-000205, ADR-000207, ADR-000208

## Decisions

1. **FDTD first, pressure–velocity form, staggered grid.** It handles
   heterogeneous media and interfaces without special cases, gives
   transient and broadband results, has an exact stability condition and a
   known dispersion relation to validate against, and needs no mesh
   generator or linear solver. Uniform cubic cells.
2. **A numerical core that knows nothing about scenes** (`GridSpec`,
   `FdtdSolver`, `run`), and a separate setup step from a project
   (`prepare`). The core is validated on its own.
3. **Double precision, single thread** as the reference backend.
4. **Split-field PML outside the region,** quadratic profile, design
   reflection 1e-6, backed by a rigid wall; its reflection is measured.
5. **Soft volume-velocity sources** calibrated so that an emitter's
   amplitude keeps the meaning it has in the analytical engine (ADR-000207).
6. **Two dimensions are a different problem,** selected by a plane region,
   and stated in every 2D result.
7. **Simulation geometry is an inside test per geometry kind,** from its
   definition; cells are assigned by their centres; shells thinner than two
   cells are thickened and reported.
8. **Solids are fluids** with their density and speed of sound, stated in
   every result. A manipulator's ideal absorbing surface is simulated as
   rigid, with a warning, because refusing the whole run for it would hide
   everything else the run shows.
9. **The steady state is extracted during the run** by a running Fourier
   sum at the emitters' frequency and becomes an ordinary `FieldResult`, so
   display, metrics and comparison need no second code path.
10. **A registry of solver declarations and an assessment** of what each
    solver would leave out. Nothing selects a solver.
11. **Plan before run; refuse, never degrade.**
12. **An own radix-2 FFT** for probe spectra; no library.
13. **Results stay in memory;** the project stores settings and run
    records.

## Deviations from the request

* One numerical method. No FEM, BEM, angular spectrum, pseudospectral or
  eigenmode solver.
* No mesh generation, refinement or mesh-quality tooling; no Gmsh.
* No GPU, OpenMP or SIMD backend; no backend selection.
* No HDF5; no checkpointing; no region-of-interest or velocity recording.
* Periodic and symmetry boundaries are not implemented.
* Attenuation is frequency-independent; no dispersive or anisotropic
  materials; no moving or continuously varying media.
* One signal kind per run; no editor for sampled signals.
* No isosurfaces, volume rendering or arbitrary cross-sections beyond what
  the existing field display offers.
* Boundaries and materials are validated in 2D; 3D is validated for open
  space.
* No comparison with an independent full-wave implementation.

## Rejected

* The second-order scalar wave equation scheme — simpler, but density
  contrasts and interfaces need the first-order form.
* A damped "sponge" layer presented as absorbing boundary — reflects much
  more and cannot be called a PML.
* Rasterizing the display mesh — its resolution is unrelated to the
  wavelength and it is not watertight for shells.
* Lowering the resolution to fit the memory limit.
