# Acoustic Validation

**Status:** results of the run recorded for REQ-CHAT-20261126-003
(evidence: `evidence/`, work record of that request). Every check below is
an automated test in `tests/core/test_acoustics.cpp`,
`tests/core/test_acoustic_examples.cpp` or `tests/app/test_acoustics_ui.cpp`.

The full-wave (FDTD) solver's validation is recorded separately in
`docs/advanced_acoustics/ACOUSTIC_VALIDATION.md`.

## Principle

A calculation is compared with a closed-form expectation written out in
the test, or with an independent property the correct answer must have
(orthogonality of a least-squares residual, the triangle inequality). No
test validates a calculation only by calling the same code twice.

The test medium is a gas with `c = 340 m/s`, `ρ = 1.2 kg/m³` at
`f = 34 kHz`, so the wavelength is exactly 0.01 m and distances in
wavelengths are exact.

## Propagation

| Check | Reference | Tolerance | Result |
|---|---|---|---|
| Spherical spreading in four directions, four distances | `|P| = A/r` | 1e-12 relative | pass |
| Doubling the distance halves the amplitude | ratio 2 | 1e-12 | pass |
| Amplitude at the 1 m reference distance | `A` | 1e-12 | pass |
| Phase at distance `r` | `−2πr/λ` | 1e-9 rad | pass |
| Phase a quarter wavelength farther | lag of `π/2` | 1e-9 rad | pass |
| Phase a whole wavelength farther | unchanged | 1e-9 rad | pass |
| Source phase and time delay | `arg = φ − ωτ`; quarter-period delay = `−π/2`; same delay at `2f` = `π` | 1e-9 rad | pass |
| Attenuation | `e^{−αr}/r`; none when switched off | 1e-12 | pass |
| At and inside a source | finite, flagged, `|P| = A/radius` | 1e-6 | pass |

## Interference

| Check | Reference | Tolerance | Result |
|---|---|---|---|
| Two in-phase sources on their bisector | `2A/r` | 1e-10 | pass |
| Path difference 2.5 λ on the line through both | `|A/r₁ − A/r₂|` | 1e-9 | pass |
| Path difference 3 λ | `A/r₁ + A/r₂` | 1e-9 | pass |
| Antiphase pair on the bisector | 0 | 1e-12 | pass |
| A distant second source does not rescale the first | `A/r` | 1e-5 | pass |
| A source at another frequency | left out and counted | exact | pass |

## Velocity and intensity

| Check | Reference | Tolerance | Result |
|---|---|---|---|
| Active intensity of one spherical wave at 0.02 m, 0.3 m, 2 m | `|P|²/(2ρc)`, radial | 1e-9 relative | pass |
| Active intensity midway between two equal sources | 0 | 1e-12 | pass |
| RMS, SPL, instantaneous pressure | their definitions; 5 Pa peak = 104.95 dB re 20 µPa | 1e-9 | pass |

## Directivity

| Check | Reference | Tolerance | Result |
|---|---|---|---|
| Cardioid on axis, broadside, behind | 1, 0.5, 0; order 2 broadside 0.25 | exact | pass |
| Piston on axis | 1 | 1e-12 | pass |
| Piston first null | `k a sin θ = 3.8317` | 1e-9 | pass |
| Piston behind the baffle | 0 | exact | pass |
| An emitter pitched 90° radiates along world +X | axis and pattern | 1e-12 | pass |

## Arrays

| Check | Reference | Result |
|---|---|---|
| Linear, planar, circular, cylindrical, spherical, custom layouts | counts, spacings, radii, centroid at the origin, facing directions | pass |
| Spherical layout is evenly spread | centroid within 2 % of the radius for 200 elements | pass |
| Overrides survive spacing, default and count changes | values by element index | pass |
| Inactive elements are not sources; the array transform applies | positions after a yaw of 90° | pass |
| A symmetric planar array | field equal under `x → −x`, `y → −y`, `z → −z` to 1e-9 | pass |

## Focusing

| Check | Reference | Tolerance | Result |
|---|---|---|---|
| Time delays for a 3-4-5 geometry | `0`, `0.01 m / c`, `0` | 1e-15 s | pass |
| Phases | `k r mod 2π` in `[0, 2π)` | 1e-9 rad | pass |
| Field at the target after applying phases | `Σ A/r_i`, the triangle-inequality maximum | 1e-9 relative | pass |
| Time delays stay aligned at 0.6 f and 1.7 f | coherence 1 | 1e-9 | pass |
| Phases at 0.6 f | coherence below 0.8 | — | pass |
| Hann taper | 1 at the centre, symmetric, decreasing, positive at the edge | 1e-12 | pass |

## Inverse design

| Check | Reference | Tolerance | Result |
|---|---|---|---|
| Propagation matrix entries | `e^{−jkr}/r` | 1e-10 | pass |
| 16 sources, 3 targets (one a null), `λ = 0` | targets met; rank 3 | 1e-8 Pa | pass |
| Same, through the field evaluator | 30 Pa and 0 Pa | 1e-7 Pa | pass |
| Minimum-norm property | `w` lies in the row space of `H` | 1e-8 relative | pass |
| 2 sources, 6 targets | residual orthogonal to the columns of `H`; flagged "not achieved accurately" | 1e-7 relative | pass |
| Regularization `0 … 1` | `‖w‖` never increases, residual never decreases | 1e-9 | pass |
| Amplitude limit | strengths clipped, residual grows, note present | — | pass |
| Target weights | the heavier target is met more closely | — | pass |
| Applied to a 6 × 6 array | field equals the requested values | 1e-3 Pa of 25–40 Pa | pass |

## Grids, jobs, persistence

Grid dimensions and world positions for the three planes, the volume and a
rotated region; preflight numbers, the aliasing warning and the refusal of
an oversized grid; grid values equal point evaluation (single-precision
storage, 1e-4 relative); threaded and single-threaded results identical;
progress reaches 1; cancellation leaves no result; a result does not change
when the scene is edited afterwards; components and settings survive a
project round trip; invalid component data is rejected on load.

## Example projects

| Example | Check | Result |
|---|---|---|
| A two sources | constructive probe `2A/r`; destructive probe at a path difference of exactly `λ/2`, below 5 % of the constructive value | pass |
| B linear array | on-axis more than 8 times the value 30° off axis | pass |
| C focused planar array | `Σ A/r_i` at the target (607.2 Pa for 64 emitters of 1 Pa at 1 m); the field maximum is within 2 mm laterally and 20 mm axially of the target, on the array side | pass |
| D three targets | 60 Pa, 40 Pa at +90°, and a null, each within 0.05 Pa | pass |

Each example file is compared with what the engine builds today, so the
files cannot go stale.

## Performance check

Example C's plane, 241 × 321 samples and 64 sources (4.95 million source
evaluations), computed on one thread: 0.23 s in a Release build, 6.6 s in a
Debug build (GCC 13.3, x86-64). The field job uses every processor thread;
the user interface stays responsive and the job can be cancelled.

## Known limitations of the validation

* Everything is validated against the free-field analytical model itself;
  there is no comparison with measurements or with another solver.
* The baffled-piston pattern is validated as a formula, not as a model of
  a particular transducer.
* Intensity with directional sources is approximate and is only checked for
  the omnidirectional case.
