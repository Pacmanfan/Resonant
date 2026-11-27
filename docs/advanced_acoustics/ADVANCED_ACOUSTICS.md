# Advanced Acoustic Simulation

**Status:** current for REQ-CHAT-20261126-005. Requirements:
`docs/requirements/full_wave_acoustics.md`. Decision: ADR-000209.

Resonant has three acoustic solvers. Two are fast models of limited scope;
the third solves the wave equation numerically and lets geometry and
materials take part in the calculation.

| Solver | Id | Kind |
|---|---|---|
| Analytical free field | `acoustic.freefield.analytical` | closed form, one frequency |
| Free field + reflectors | `acoustic.reflector.kirchhoff` | high-frequency approximation, one frequency |
| Full-wave time domain (FDTD) | `acoustic.fdtd.staggered` | numerical solution of the linear acoustic equations |

This directory documents the third and the framework around all three.

| Document | Contents |
|---|---|
| `SOLVER_SELECTION.md` | the solver registry, what each solver represents, the assessment |
| `FDTD_SOLVER.md` | equations, discretization, sources, recording, results |
| `NUMERICAL_ACCURACY.md` | stability, dispersion, resolution, convergence, the preflight |
| `BOUNDARY_CONDITIONS.md` | rigid, pressure-release, impedance, absorbing layers |
| `MATERIAL_INTERACTIONS.md` | simulation geometry, material cells, interfaces, attenuation |
| `BROADBAND_SIMULATION.md` | signals, true time delays |
| `FREQUENCY_ANALYSIS.md` | spectra and what a record can resolve |
| `MODAL_ANALYSIS.md` | resonance peaks versus eigenmodes |
| `PERFORMANCE_AND_GPU.md` | measured speed, memory, what was not done |
| `ACOUSTIC_VALIDATION.md` | every numerical check with its tolerance |

## What the full-wave solver calculates

Propagation, interference, reflection (any number of times), diffraction
around edges and through apertures, shadows, transmission into and through
fluid-like materials, refraction, frequency-independent absorption,
resonance of enclosures, pulses and broadband signals with true time
delays, in two or three dimensions.

## What it does not calculate

* Shear waves and structural vibration: a solid is a fluid with its density
  and speed of sound.
* Emitter directivity: every emitter is a monopole.
* Frequency-dependent attenuation, dispersion, anisotropy.
* Nonlinear propagation, harmonic generation, shocks, streaming,
  cavitation, thermoviscous boundary layers, thermoacoustics.
* Moving media, flow, temperature gradients as such (a region of another
  sound speed can be modelled as a material object).
* Periodic and symmetry boundaries (a rigid wall is the symmetry plane of an
  even field).
* Eigenmodes: resonances are found as peaks of a response
  (`MODAL_ANALYSIS.md`).

No other numerical method (finite elements, boundary elements, angular
spectrum, pseudospectral) is implemented. The registry is where one would
be declared.

## Future effects — what each would need

| Effect | Equations | Material data | Solver implication | Validation |
|---|---|---|---|---|
| Nonlinear propagation, harmonics, shocks | Westervelt or the full continuity/momentum/state equations with `B/A` | nonlinearity parameter `B/A` | shock-capturing scheme or strong artificial absorption; finer grid for harmonics | Fubini and Fay solutions of a plane wave |
| Thermoviscous losses | linearized Navier–Stokes, or a boundary-layer impedance | shear and bulk viscosity, thermal conductivity, heat capacities | sub-micrometre boundary layers cannot be gridded: an impedance model | Kirchhoff tube attenuation |
| Frequency-dependent absorption | relaxation or fractional-derivative loss terms | power-law coefficient and exponent, or relaxation times | memory variables per cell | attenuation versus frequency of a plane wave |
| Acoustic streaming | time-averaged second-order equations driven by the first-order field | viscosities | a second, steady flow solver | Rayleigh streaming in a standing wave |
| Cavitation | bubble dynamics (Rayleigh–Plesset) coupled to the field | gas content, nuclei, surface tension | a multi-scale model | single-bubble thresholds |
| Shear waves in solids | elastodynamic equations (velocity–stress) | shear modulus or both wave speeds | a different staggered scheme with six stress components | reflection and mode conversion at a fluid–solid interface |
| Moving media | linearized Euler equations with mean flow | flow field | convective terms, upwinding | Doppler shift, refraction by shear flow |

None of these exists in the code, in any simplified form.

## In the application

**Full-Wave Solver** panel (tab beside Properties and Acoustics): region,
the solver assessment, signal, resolution, time step, boundaries,
recording, the preflight, and the run controls; **Simulation > Run
Full-Wave Simulation** (F6). **Time Signals** tab in the Analysis panel:
pressure over time at each probe, spectra, energy over time, and the
recorded instants of the field. A continuous-signal run adds its
steady-state field to the ordinary results, so it is displayed, analysed
and compared exactly like an analytical field.

The Probes and Targets table of the Field Analysis tab always lists values
of the analytical model selected in the Acoustics panel. Values of a
full-wave run at a probe are in the Time Signals tab.
