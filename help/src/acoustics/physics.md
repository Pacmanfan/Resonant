---
id: acoustics.physics
title: Acoustic physics
category: Acoustic Engineering
order: 10
status: available
summary: The linear acoustics behind every acoustic result in Resonant: harmonic fields of point sources, the wave equation of the full-wave solver, and the conventions of each number.
keywords: [wave equation, pressure, wavelength, spl, intensity, superposition, conventions]
related: [acoustics.sources, acoustics.materials, acoustics.fdtd, ref.phase, ref.units]
---

Resonant treats sound as a small pressure disturbance in a fluid at rest
(linear acoustics). Three solvers use this physics in different ways: the
**analytical free field** and **free field + reflectors** models give a
harmonic field at one frequency, and the **full-wave (FDTD)** solver
integrates the wave equation in time. This page gives the equations and
conventions they share; how to set each one up is on its own page.

## Frequency, wavelength and wavenumber

```math
λ = c / f
ω = 2π f
k = ω / c = 2π / λ
```

`f` is the frequency (Hz), `c` the speed of sound of the medium (m/s), `λ`
the wavelength (m), `ω` the angular frequency (rad/s) and `k` the
wavenumber (rad/m). At 40 kHz in air at 20 °C (`c` = 343.2 m/s) the
wavelength is 8.58 mm; the **Medium** tab of the **Acoustics** panel shows
the **Wavelength** for the current **Frequency**.

## Harmonic fields and the time convention

```math
p(x, t) = Re{ P(x) · e^(+jωt) }
```

`P(x)` is the complex pressure amplitude at the point `x`, in pascal. All
complex amplitudes are **peak** values. A positive phase leads; a time
delay `τ` multiplies a source by `e^(−jωτ)`. The conventions are listed in
[Phase conventions](../reference/phase.md).

## The field of a source

An emitter's **Amplitude at 1 m** `A` is its free-field peak pressure at
1 m on its axis. Its field, and the total field, are

```math
P_i(x) = A_i · e^(j(φ_i − ω τ_i)) · (r_ref / r_i) · D_i(θ_i) · e^(−α r_i) · e^(−j k r_i)
P(x) = Σ_i P_i(x)
```

- `r_i`: distance from source `i` to `x`; `r_ref` = 1 m.
- `φ_i`, `τ_i`: the source's phase (rad) and time delay (s).
- `D_i(θ_i)`: its directivity at the angle `θ_i` from its local +Z axis
  ([Sources and transducers](sources.md)).
- `α`: the attenuation coefficient of the medium (Np/m).

Sources add linearly. Nothing is divided by the number of sources, so adding
an emitter never changes what another emitter's amplitude means.

## Derived quantities

```math
p_rms = |P| / √2
SPL = 20 · log₁₀(p_rms / p_ref)
V = −∇P / (jωρ)
I = ½ · Re{ P · V* }
```

- `p_rms`: RMS pressure of the harmonic signal (Pa).
- `SPL`: sound pressure level in dB re `p_ref`, which is 20 µPa in air and
  1 µPa in water and is always printed with the value. A 5 Pa peak
  amplitude is 104.95 dB re 20 µPa.
- `V`: particle velocity (m/s, peak), `ρ` the density of the medium.
- `I`: active intensity (W/m²). It is exact for omnidirectional sources; with
  directional sources the gradient of the directivity is neglected and the
  result says so.

The plane-wave formula `|P|² / (2ρc)` is **not** used to report intensity:
it holds only for one progressive wave far from sources. Midway between two
equal sources the active intensity is zero, while that formula gives four
times the intensity of one source.

## The wave equation of the full-wave solver

```math
ρ · ∂v/∂t = −∇p
∂p/∂t = −K · ∇·v − γ · p + K · q,      K = ρ c²
∇²p = (1 / c²) · ∂²p/∂t²
```

`v` is the particle velocity, `K` the bulk modulus (Pa), `γ` a
frequency-independent loss rate (1/s, `γ = 2αc`) and `q` the volume-velocity
source density (1/s). `ρ`, `K` and `γ` may change from cell to cell, which is
how objects and materials enter the calculation. The last line is the
lossless, homogeneous case. See [Acoustic full-wave (FDTD)
simulation](fdtd.md).

## Impedance, reflection and transmission

```math
Z = ρ c
R = (Z₂ − Z₁) / (Z₂ + Z₁)
T = 2 Z₂ / (Z₂ + Z₁)
```

`Z` is the characteristic impedance (Pa·s/m), `R` and `T` the pressure
reflection and transmission coefficients of a plane wave at normal incidence
from medium 1 into medium 2. Air at 20 °C has `Z` = 413 Pa·s/m, nominal
water 1.48 × 10⁶ Pa·s/m: from air, water reflects 99.9 % of the pressure.

## Near a source

The `1/r` of a point source diverges at the source. Within the larger of the
emitter's radius and the **Minimum distance** (Medium tab), the source's
contribution is held at its value on that sphere. Such points are marked
"inside a source: not valid" and are left out of the field statistics.

## Worked example: two-source interference

1. Open `examples/acoustics/a-two-source-interference.resonant`: two 1 Pa
   emitters at x = ±12.87 mm, three wavelengths apart at 40 kHz in air.
2. Press **F5** (**Acoustics > Run Field Simulation**).
3. In **Analysis > Field Analysis > Probes and Targets**, the probe on the
   bisector at z = 100 mm reads `2A/r` = 19.84 Pa; the second probe, where
   the two paths differ by half a wavelength, reads less than 5 % of that.

## What the physics leaves out

Nonlinear propagation, shear waves in solids, viscous and thermal boundary
layers, frequency-dependent absorption, flow and temperature gradients are
not modelled by any solver. Which solver includes which effect is listed in
[Choosing a solver](../simulation/solver-selection.md).
