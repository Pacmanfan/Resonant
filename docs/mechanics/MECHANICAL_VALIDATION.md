# Mechanical Validation

**Status:** results of the run recorded for REQ-CHAT-20261126-006. Tests:
`tests/core/test_mechanics.cpp` (element formulations and analyses, test
materials: steel `E = 200 GPa, nu = 0.3, rho = 7800`, aluminium `70 GPa,
0.33, 2700`), `tests/core/test_mechanics_scene.cpp` (projects, examples;
library materials), `tests/app/test_mechanics_ui.cpp`. Every row passes.

## Materials and sections

Derived `G`, `K`, `lambda`, wave speeds, `D` from `E, nu, rho` (exact);
`K = lambda + 2G/3`; limits of `nu`; missing properties named. Rectangle
torsion constant within 0.001 of the exact series (2:1); circle and tube
properties exact.

## Statics

| Check | Reference | Tolerance |
|---|---|---|
| Cantilever tip, 1 / 4 / 17 elements | `P L^3 / (3 E I)` | 1e-12 m (exact) |
| Root bending stress | `P L c / I` | 1e-6 relative |
| Axial bar | `P L / (E A)` | 1e-15 m |
| SS square plate, central load, 20 x 20 | `0.01160 P a^2 / D` (Timoshenko, nu = 0.3) | 1 % |
| Solid bar, uniform traction (patch test) | `u = P x / (E A)` at every node | 1e-13 m |
| Solid bar stress | `P / A` in every element | 1e-3 Pa |
| Unsupported structure | refused, "not held" | |

## Natural frequencies

| Check | Reference | Tolerance | Measured |
|---|---|---|---|
| Beam bending, 4 support cases, modes 1-4, 40 elements | `(beta L)^2 / (2 pi L^2) sqrt(EI / rho A)` | 1e-4 | |
| Free-free beam rigid modes | 6 below 1e-3 of the first elastic | | |
| Beam convergence, cantilever mode 2 | order 4 (± 0.4) | | 1.2e-3, 8.0e-5, 5.2e-6 at 4, 8, 16 elements; 1.9e-7 at 40 |
| Rod axial and torsional, fixed-free | `(2n-1) c / (4L)` | 1e-3 | |
| SS plate 400 x 300 x 2 mm, first 6 | Navier | 1 % | |
| SS plate convergence (1,1) | order 2 (± 0.5), below 0.3 % at 16 x 16 | | -3.1 %, -0.83 %, -0.21 %, -0.053 % at 4, 8, 16, 32 (from below) |
| Clamped square plate | `lambda^2 = 35.99` (Leissa) | 1 % | |
| Membrane 500 x 350 mm, first 5 | `(c/2) sqrt((m/a)^2 + (n/b)^2)` | 1 % | |
| Solid bar, first axial | `c / (4L)` | 1e-3 | |
| Solid cantilever (locking) | below the beam value, rising with refinement, above 0.7 at 20 elements | | 0.34, 0.67, 0.89, 0.98 at 5, 10, 20, 40 elements |
| Lanczos against dense, 10 modes | | 1e-7 relative | |
| Backward error of every pair | | below 1e-8 | |

## Forced response

Static limit 1e-6; direct equals modal superposition with every mode (1e-8;
Rayleigh and loss factor); modal ratio refused with the direct method;
resonance peak at `f1 sqrt(1 - 2 zeta^2)` (1e-4), half-power bandwidth `2
zeta f1` (5e-4), phase -90 degrees (0.05 rad); example C: peaks at the
natural frequencies within the sweep spacing (0.6 %), loss-factor bandwidth
`eta f` (0.001).

## Transient

Energy balance 1e-9 without damping; step load doubling 2.00 ± 0.06; period
error equal to the Newmark relation at 10, 20, 40 steps per period (1e-4),
ratio 4 per halving; Rayleigh decay over 5 cycles (0.01); loss factor
refused; example D energy decreasing after the impulse and spectral peaks
at plate modes within the resolution.

## Waves

Rod pulse speed 1 % (core), 1.5 % (example E); reflected-to-incident
velocity at a free end 1 ± 0.05 / 0.06; flexural group velocity = 2 x phase
velocity at 2 and 5 kHz (6 %).

## Design, periodic structures, coupling, data

Targeting: residual below 1e-4, target 1 %, quiet points below 1 %,
over-determined case reported. Bloch stop bands against the dispersion
relation (0.5 %; measured 0.1 %); Bragg frequency inside the first gap;
mid-gap transmission below 1e-3 of the pass band. Stepped horn frequency
0.5 %, gain 4 ± 0.08 with opposite end motion. Piston radiation 1 %;
antisymmetric null 1e-6. CSV import with metadata, comparison exactly 6.02
dB for a doubled response, bad files refused.

## Limits of this validation

* References are closed-form solutions; no comparison with an independent
  finite-element code or with measurements.
* Solids are validated in axial deformation and as a convergence trend in
  bending; their bending accuracy at practical meshes is poor and stated.
* Plate stresses and beam stresses away from the root are not compared with
  references.
* Membranes are validated for frequencies only.
* Transient waves on plates are not compared with a reference.
