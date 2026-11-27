# Full-Wave Solver Validation

**Status:** results of the run recorded for REQ-CHAT-20261126-005. (The
analytical engine's validation is `docs/acoustics/ACOUSTIC_VALIDATION.md`;
the reflector model's is `docs/manipulators/PASSIVE_WAVE_VALIDATION.md`.)

Tests: `tests/core/test_fdtd.cpp` (numerical problems),
`tests/core/test_fdtd_scene.cpp` (projects and examples),
`tests/app/test_fullwave_ui.cpp` (the application). Test medium for the
numerical problems: `c = 340 m/s`, `rho = 1.2 kg/m^3`. Every row passes.

## Propagation

| Check | Reference | Tolerance |
|---|---|---|
| Phase velocity of a plane wave, 8 / 16 / 32 cells per wavelength, S = 0.5 | numerical dispersion relation | 2e-4, 1e-4, 1e-4 |
| — order of accuracy | error ratio 4 per halving of the cell | ± 0.4 |
| — 16 cells per wavelength | within 0.5 % of the true speed | |
| Phase velocity at S = 0.7 and S = 0.35 | dispersion relation; error larger at the smaller step | 1e-4 |
| Plane pulse over 0.5 m | speed of sound | 0.4 % |
| — amplitude | unchanged in a lossless duct | 2 % |
| 3D point source, continuous, `S = 4 pi A / rho` | `A / r` at 8, 12, 18 cells on an axis | 3 % |
| — on the grid diagonal | `A / r` | 5 % |
| — phase between two radii | `k dr` | 0.05 rad |
| 2D line source, continuous | `(rho S / 4) |H0(2)(k r)|` at 15, 30, 60 cells, two directions | 3 % |
| — phase between two radii | Hankel function | 0.12 rad |
| Two sources 3 wavelengths apart | `|H0(k r1) + H0(k r2)|` along a line | 5 % of the largest value |
| — | a null where the paths differ by half a wavelength: below 8 % of the on-axis value | |
| Time delay of 120 steps | the same signal 120 steps later | 1e-9 relative |

## Boundaries

| Check | Reference | Tolerance |
|---|---|---|
| Rigid end of a duct | reflected / incident = +1 | 0.01 |
| Pressure-release end | −1 | 0.01 |
| Impedance end, `Z = 3 rho c`, `rho c / 3`, `rho c`, `1e9 rho c` | 0.5, −0.5, 0, 1 | 0.01 |
| Absorbing layer, 12 cells, normal incidence | below 0.2 % | |
| — 24 cells | below 0.05 %; 24 < 12 < 8 cells | |
| Absorbing layers, point pulse, oblique incidence and corner | a domain too large for returns | 1 % of the peak |
| — energy left | below 1e-6 of the maximum | |
| Rigid wall and a line source | direct + image Hankel functions, 15 × 5 points | 5 % of the largest value |
| Pressure-release wall | direct − image | 5 % |

## Materials

| Check | Reference | Tolerance |
|---|---|---|
| Interface, three second media | `R = (Z2−Z1)/(Z2+Z1)`, `T = 2 Z2/(Z2+Z1)` | 0.012 |
| — energy flux | `R^2 + (Z1/Z2) T^2 = 1` | 0.02 |
| Speed in the second medium | 680 m/s | 0.5 % |
| Refraction at an oblique interface | least-time (Snell) path arrival | a quarter period |
| Attenuation 4 Np/m over 0.5 m | `exp(−alpha L)` | 0.01 |
| — energy | never increases after the source has ended | |
| Rigid wall across the domain | field behind below 1e-6 | |
| Slit one wavelength wide | field on the far side above 10 % of the open field; above 10 % of the on-axis value at 63 degrees; 0.35 to 1 of it at 45 degrees | |
| In front of the wall | standing-wave maxima above 1.6 × the free field | |
| Pressure-release body in a duct | reflects −1 (0.02); nothing behind (1e-9) | |

## Stability and energy

| Check | Result |
|---|---|
| Time step above the limit, 2D and 3D, and with a faster material in the grid | refused, with the limit |
| Closed rigid box, 6000 steps, at 0.5 and 0.99 of the limit | energy constant to 1e-7 |
| Run with twice the allowed step | refused before it starts |

## Resonance

Rigid 2D box 60 × 45 mm: six modes within half the spectral resolution
(20 Hz) of their analytical frequencies. See `MODAL_ANALYSIS.md`.

## Signals and spectra

Shapes, starts, lengths, repetition and delay of every signal kind; the
stated −6 dB bandwidth of the Gaussian pulse (0.02); zero mean of the
Ricker wavelet; amplitude, phase and mean of a sinusoid's spectrum for
both windows; resolution independent of padding; two components closer
than the resolution not separated, and separated by a longer record.

## From projects

Planning (cell size, counts, time step, layers, cost, frame), refusals
(memory, no emitter, emitter off the plane, bad settings), representation
of objects (rigid, pressure release, material, hidden, the substituted
"ideal absorber" named), an inside test for every geometry kind, the 2D
amplitude calibration at 1 m (4 %), a 100 µs emitter delay (1 µs).

Examples (each file compared with what the engine builds):

| Example | Check |
|---|---|
| A, 2D pulse | speed within 1 %; amplitude ∝ 1/sqrt(r) within 0.06; energy gone at the end |
| A, 3D pulse | amplitude ∝ 1/r within 0.05; 1 Pa at 1 m within 8 %; speed within 3 % |
| B, reflection | minima half a wavelength apart (4 %), first a quarter wavelength from the plate (one cell); wall maximum above 3 × the first minimum; shadow behind |
| C, diffraction | field in the geometric shadow above 30 % of the on-axis value; nothing through a closed plate |
| D, interface | speed in acrylic within 3 %; reflected pulse in the water with the normal-incidence coefficient times cylindrical spreading, within 0.06 |
| E, cavity | four modes within 100 Hz of the gridded cavity's frequencies, within 4 % of the designed ones; energy kept |
| F, passive focus | reflected-field maximum at the mirror-equation image (37.5 mm) within half a wavelength; more than 3 × the direct field; spot 0.3 to 2 wavelengths wide |
| G, hybrid focus | field at the target equals the sum of the single-element magnitudes within 3 %; more than 1.5 × the unphased array |

## Limits of this validation

* References are closed-form solutions. There is no comparison with
  another full-wave code or with measurements.
* Scattering from a curved obstacle is not compared with its series
  solution (cylinder, sphere); curved surfaces are validated only through
  the focusing examples and the cavity.
* Mesh convergence is measured for propagation in a homogeneous medium.
  For stepped surfaces it is shown only by the cavity example's two
  references (gridded and designed size).
* Three dimensions: validated for a point source in open space; boundaries
  and materials are tested in 2D only (the code paths are shared apart
  from the third direction's loops).
* Long-run stability of the absorbing layers: observed to about 2 000 steps;
  the closed-domain runs of 9 000 and 25 000 steps have no layers.
