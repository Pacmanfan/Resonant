---
id: ref.phase
title: Phase conventions
category: Technical Reference
order: 30
status: available
summary: Resonant's time convention exp(+jωt), what a positive phase and a time delay mean, and how phases of sources, fields, arrays and measurements are defined.
keywords: [phase, time convention, phasor, delay, lead, lag, complex amplitude]
related: [ref.units, ref.coordinates, acoustics.focusing, em.beamforming, exp.measurements]
---

Every harmonic quantity in Resonant, in every domain, uses the **engineering
time convention** `e^(+jωt)` with **peak** complex amplitudes. The acoustic
models, the reflector model, the acoustic and electromagnetic full-wave
solvers (their steady-state fields), the mechanical harmonic analyses, the
plate radiation model, the analytical electromagnetic models and the
spectra of measured signals all follow it, and results state it in their
assumptions (exported point values carry `"timeConvention": "exp(+j omega t); outgoing waves exp(-j k r)"`).

## The convention

```math
p(x, t) = Re{ P(x) · e^(jωt) },   ω = 2πf
p(x, t) = |P(x)| · cos(ωt + arg P(x))
P(r) ∝ e^(−jkr) / r,   k = ω / c
P_delayed = P · e^(−jωτ)
```

- `P(x)` is the complex peak amplitude at the point `x`, `p(x, t)` the
  physical signal, `f` the frequency in Hz.
- An outgoing spherical wave varies as `e^(−jkr)`: the phase falls with the
  distance `r` from the source.
- A **positive phase leads**: a signal with phase +90° reaches its peak a
  quarter period earlier than one with phase 0.
- A time delay `τ` multiplies the amplitude by `e^(−jωτ)`: at one
  frequency a delay is a phase lag of `ωτ`.

## Sources

An acoustic emitter with **Amplitude** `A`, **Phase** `φ` and **Time
delay** `τ` produces

```math
P(x) = A · e^(j(φ − ωτ)) · (r_ref / r) · D(θ) · e^(−αr) · e^(−jkr)
```

with `r_ref = 1 m`, `D(θ)` the directivity and `α` the attenuation in Np/m.
Electromagnetic radiators use their feed current's phase the same way; a
mechanical actuator's **Phase** and **Time delay** enter a harmonic analysis
as `e^(jφ)` and `e^(−jωτ)`, and a static analysis uses its amplitude times
`cos φ`.

## Focusing and steering

| Control | Element setting | Valid |
|---|---|---|
| acoustic focus, *Narrowband phase* | `φᵢ = k·rᵢ` (mod 2π), cancelling the propagation phase `−k·rᵢ` to the target | at one frequency |
| acoustic focus, *True time delay* | `τᵢ = (r_max − rᵢ) / c` ≥ 0; the farthest element has no delay | at every frequency |
| RF array, phase steering | `−k · r̂₀ · rₙ`, with `r̂₀` the beam direction and `rₙ` the element position | at one frequency (beam squint elsewhere) |
| RF array, true time delay | `(r̂₀ · rₙ − min) / c` | at every frequency |

The array factor of an RF array is `AF(r̂) = Σ wₙ · e^(jk·r̂·rₙ)`, the
far-field sum of the element weights `wₙ`.

**Worked example.** At 40 kHz in air (`c = 343.23 m/s`, `k = 732.2 rad/m`)
an element 50 mm from the target has a propagation phase of
`−k·r = −36.61 rad`. Phase focusing sets its phase to `36.61 rad` mod 2π,
that is 5.20 rad (297.7°). At 1 kHz a delay of 0.25 ms is a quarter period,
a phase of −90°.

## Lossy materials

With `e^(+jωt)` a lossy medium has a complex permittivity with a
**negative** imaginary part:

```math
ε_c = ε − jσ / ω = ε′ · (1 − j·tan δ)
```

`σ` is the conductivity (S/m) and `tan δ` the loss tangent. Acoustic and
mechanical losses are entered as real coefficients (attenuation in Np/m,
loss factor, damping ratio, Rayleigh coefficients).

## How phases are shown

| Where | Range and unit |
|---|---|
| acoustic field display *Pressure phase* | −180 to +180 degrees, cyclic colours |
| *Instantaneous pressure* and its animation | `Re{P · e^(jωt)}` as `ωt` advances |
| mechanical frequency response *Phase (deg)* | degrees |
| Experimental Hardware measurements *Phase (deg)* | degrees |
| exported point values | `phase_rad`, radians in (−π, π] |

## Measured data

A spectrum of a recorded signal `A·cos(2πft + φ)` reads `A·e^(jφ)` at
its frequency: the same convention as the models, so measured and simulated
phases compare directly. A transfer function is the response relative to
the reference channel: a positive phase means the response leads.
Calibrations with a phase are interpolated in unwrapped phase. Touchstone
files are read with the impedances, number format (RI, MA or DB, angles in
degrees) and frequency unit they state.

## Comparing with other conventions

Many physics texts use `e^(−iωt)`. To convert, take the **complex
conjugate** of every amplitude: phases change sign, outgoing waves become
`e^(+ikr)` and a lossy permittivity has a positive imaginary part.
Magnitudes, levels and powers are the same in both conventions.

## Related topics

- [Units](units.md)
- [Coordinate conventions](coordinates.md)
- [Focusing](../acoustics/focusing.md)
- [Beamforming](../electromagnetics/beamforming.md)
