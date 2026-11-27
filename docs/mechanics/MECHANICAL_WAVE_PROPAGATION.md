# Mechanical Wave Propagation

**Status:** current for REQ-CHAT-20261126-006.

A wave in a solid is not a sound wave in air. The idealizations carry
different waves:

| Wave | Structure | Speed | Dispersion |
|---|---|---|---|
| Longitudinal (rod) | beam, axial | `c = sqrt(E / rho)` | none in rod theory |
| Torsional | circular beam | `sqrt(G / rho)` | none |
| Flexural (bending) | beam | `c_phase = sqrt(omega) (EI / rho A)^(1/4)`, `c_group = 2 c_phase` | strong: higher frequencies travel faster |
| Flexural | plate | `c_phase = sqrt(omega) (D / rho h)^(1/4)` | as above |
| Transverse | membrane | `sqrt(T / rho h)` | none |
| Bulk P and S | solid | `sqrt((lambda + 2G)/rho)`, `sqrt(G/rho)` | none in the continuum |

Euler-Bernoulli and Kirchhoff theories are valid while the bending
wavelength is long against the thickness (about 6 times or more); above
that their phase speed grows without bound, which real structures do not
do (Timoshenko / Mindlin theories, not implemented).

## Numerical dispersion

Finite elements add their own dispersion: consistent-mass elements make
waves slightly fast, and Newmark integration slightly slow. At 20 elements
per wavelength and 20 steps per period both are below 1 %. The study warns
below 6 elements per structural wavelength.

## Demonstrations (tests)

* A longitudinal pulse on a 2 m steel rod (Gaussian, 20 kHz) travels at the
  rod speed (1 % in the core test, 1.5 % in example E) and returns from the
  free end with the same sign of velocity (a free end doubles the velocity
  and inverts the stress).
* Bending tone bursts at 2 kHz and 5 kHz on a 4 mm steel strip travel at
  twice the phase speed of their frequency (6 %): dispersion that no scalar
  acoustic model reproduces.
* Standing waves: a stepped horn's half-wave mode; the membrane and plate
  modes (`MODAL_ANALYSIS.md`).

## Transitions

Members of varying section (horns, periodic rods) are one beam with a
section per element: the impedance `rho c A` changes along it and waves
reflect at the steps. Joints between different structures (a beam on a
plate), material changes along a member and multi-member frames are not
implemented.
