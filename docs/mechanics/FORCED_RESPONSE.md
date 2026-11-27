# Forced Response and Frequency-Response Functions

**Status:** current for REQ-CHAT-20261126-006. Code:
`src/core/mechanics/Analyses.cpp` (`solveHarmonic`), `Measurement.*`.

## Problem

Time convention `e^{+j omega t}`, peak amplitudes:

```text
(K (1 + j eta) - omega^2 M + j omega C) U = F(omega)
```

`F` is the sum of the active actuators' forces, each
`amplitude e^{j (phase - omega delay)}` times its load pattern. Velocity
`j omega U`, acceleration `-omega^2 U`.

## Methods

* **Direct** (default): the dynamic stiffness is factorized (sparse LU) at
  every frequency. Exact for the discretized model and every damping model
  except a modal ratio.
* **Modal superposition**: `U = sum_r phi_r (phi_r^T F) / (omega_r^2 -
  omega^2 + 2 j zeta_r omega_r omega)` (or `omega_r^2 (1 + j eta) - omega^2`
  for a loss factor) over the lowest `n` modes. Truncation is stated: the
  higher modes' contribution (residual flexibility) is left out. With every
  mode it equals the direct result (1e-8 in the tests).

## Damping (the result always states which)

| Model | Form | Where |
|---|---|---|
| None | | the response at a natural frequency is unbounded |
| Rayleigh (proportional) | `C = alpha M + beta K`; modal ratio `alpha / (2 omega) + beta omega / 2` | harmonic and transient |
| Structural loss factor | `K (1 + j eta)`; each material's own `eta` where it states one, otherwise the set `eta` | harmonic only (no causal time-domain form) |
| Modal damping ratio | `zeta` in every mode | modal superposition only |

Damping is never added silently; refused combinations (a modal ratio with
the direct method, a loss factor in transient analysis) are refused with
the reason.

## Sweep

From / to (Hz), number of frequencies, linear or logarithmic spacing. Full
complex fields are kept for every frequency when they fit half the memory
limit (at most 512 MiB); otherwise only sensors and targets, and the result
says so. A warning when the top frequency has fewer than 6 elements per
structural wavelength.

## Frequency-response functions

The Frequency Response tab plots the response at a sensor or target per
newton of drive — displacement, velocity or acceleration, magnitude in dB
or phase — with the natural frequencies of the latest modal result as
markers. All active actuators drive together with their own amplitudes and
phases; for a transfer function between one actuator and one point, keep
one actuator active at 1 N. Comparing actuator positions: one run per
configuration.

Validated: the response at 0.01 Hz is the static compliance (1e-6); peaks
at the natural frequencies within the sweep spacing; half-power bandwidth
`2 zeta f` for a modal ratio and `eta f` for a loss factor; phase lag 90
degrees at resonance.

## Measured data

`parseMeasuredResponse` reads CSV: a header with `frequency_Hz` and
`magnitude, phase_deg` or `real, imag`; `#` comment lines with `sensor`,
`quantity`, `unit`, `position`, `direction`, `sensitivity`, `source`.
Frequencies must increase; values must be finite. Imported responses are
stored in the project (undoable) and drawn on the plot of the same
quantity; `compareResponses` interpolates the simulation onto the measured
frequencies and reports the RMS level difference in dB and the largest
peaks of both. No acquisition from hardware.

Sensors carry calibration data (sensitivity, unit, model) for measured
records; solvers do not use them.
