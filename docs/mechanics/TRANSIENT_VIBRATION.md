# Transient Vibration

**Status:** current for REQ-CHAT-20261126-006. Code:
`src/core/mechanics/Analyses.cpp` (`solveTransient`).

## Method

Implicit Newmark integration, average acceleration (`beta = 1/4`,
`gamma = 1/2`):

* unconditionally stable for linear systems — no Courant limit;
* second-order accurate;
* no numerical damping: the energy of an undamped free vibration is
  conserved exactly (to rounding) by the trapezoidal rule;
* period elongation `(T_num - T) / T = omega / omega_num - 1` with
  `omega_num dt = 2 atan(omega dt / 2)`, about `(omega dt)^2 / 12`.

The effective stiffness `K + (gamma / (beta dt)) C + M / (beta dt^2)` is
factorized once. The structure starts at rest (initial acceleration from
`M a0 = f(0)`). High mesh modes that the time step does not resolve are not
damped and can add noise; the result says so.

## Settings

Simulated time; time step either set or `1 / (steps per period x f_max)`
with `f_max` the highest significant frequency of the forces' signals
(default 20 steps: 0.8 % period error at `f_max`); Rayleigh damping or none
(a loss factor or modal ratio is refused); number of recorded instants.

## Excitation

Each active actuator's force `amplitude x w(t - delay)` with its own signal
(continuous, tone burst, Gaussian pulse, Ricker wavelet, sweep, sampled;
`src/core/dsp/Waveform.h`). The delay is a real time shift.

## Recording

Not every dof at every step:

* sensors: displacement, velocity and acceleration along their direction at
  every step;
* recorded instants: the full displacement field, evenly spaced (default 60);
* energy, 400 samples: kinetic `v^T M v / 2`, strain `u^T K u / 2`, work done
  by the forces (trapezoidal), dissipated = work - kinetic - strain.

Stress and strain histories are not recorded. Velocity and acceleration
fields of the instants are not kept.

## Validation

Energy balance to 1e-9 without damping and constant after the force ends;
a suddenly applied force doubles the static tip deflection of a cantilever
(within 3 %); free vibration from a mode shape has the Newmark-predicted
period error at 10, 20 and 40 steps per period (1e-4); Rayleigh decay of
`exp(-2 pi zeta / sqrt(1 - zeta^2))` per cycle (0.01); energy decreases
monotonically after an impulse with Rayleigh damping; the sensor spectrum
of an impulse peaks at the plate's natural frequencies within the spectral
resolution.
