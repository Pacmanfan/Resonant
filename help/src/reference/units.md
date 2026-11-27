---
id: ref.units
title: Units
category: Technical Reference
order: 10
status: available
summary: The SI units Resonant stores and calculates in, the units it displays, and how levels, amplitudes and decibels are defined.
keywords: [units, SI, display unit, decibel, SPL, peak, RMS]
related: [gs.units, ref.coordinates, ref.phase, ref.project-format]
---

Everything inside Resonant is **SI**: metres, seconds, hertz, kilograms,
pascals and radians. Units are converted only where a number is shown or
typed; no calculation converts units implicitly. **Analysis > Diagnostics >
Conventions** shows *Internal units: SI: m, s, Hz, kg, rad*, and every
project file states the same in its `units` block.

## Display units

**Tools > Display Length Unit** chooses how lengths are shown and typed:
**Metres (m)**, **Centimetres (cm)**, **Millimetres (mm)** or **Inches (in)**.
It applies to positions and sizes in the property editors, the field region
editor, the element size and thickness of structures and the coordinates in
the analysis panels, and it is remembered between sessions. Angles are
always shown in degrees and stored in radians. Fields with their own unit
suffix keep it whatever the display unit: **Minimum distance** in mm,
transmission-line dimensions in mm, frequency sweeps of antennas in MHz,
simulated times in ms or ns, wanted displacements in µm.

## Quantities and their units

| Quantity | Stored as | Shown as (where different) |
|---|---|---|
| length, position | m | display length unit |
| time, delay | s | ms, µs or ns in some fields |
| frequency | Hz | MHz for RF sweeps and lines |
| angle, phase | rad | degrees |
| temperature | K | °C for the acoustic medium |
| density | kg/m³ | |
| speed of sound, velocity | m/s | |
| pressure, stress, Young's modulus | Pa | |
| acoustic attenuation | Np/m (amplitude) | |
| specific acoustic impedance | Pa·s/m | |
| sound intensity, power density | W/m² | |
| force | N | |
| acceleration | m/s² | |
| electric field, magnetic field | V/m, A/m | |
| current, power, impedance | A, W, Ω | |
| conductivity | S/m | |
| relative permittivity and permeability, Poisson's ratio, loss factor, S-parameters | 1 (dimensionless) | |
| RF power | W | dBm in instrument settings |
| uncalibrated instrument reading | V or FS (digital full scale) | |

## Amplitudes are peak values

A complex amplitude is a **peak** value: a pressure amplitude of 1 Pa means
a sinusoid between −1 Pa and +1 Pa. RMS values, levels and time-averaged
powers are derived from it:

```math
p_rms = |P| / √2
SPL = 20 · log₁₀(p_rms / p_ref)
⟨I⟩ = ½ · Re{P · V*}
1 Np = 20 / ln 10 dB ≈ 8.686 dB
```

- `P` is the complex peak pressure amplitude (Pa) and `V` the complex peak
  particle velocity (m/s); `*` is the complex conjugate.
- `p_ref` is 20 µPa in air and 1 µPa in water. The reference used is always
  stated next to an SPL value and in exported point values
  (`spl_reference_Pa`).
- An attenuation `α` in Np/m reduces an amplitude by `e^(−α·r)` over a
  distance `r`.

An emitter's **Amplitude** is its free-field peak pressure at 1 m on its axis.

## Decibels

| Where | Reference |
|---|---|
| SPL in the analysis panels and exports | p_rms re 20 µPa (air) or 1 µPa (water) |
| mechanical frequency responses | *Magnitude (dB re 1 unit/N)*: displacement, velocity or acceleration per newton |
| radiation patterns | relative to the maximum of the pattern |
| EM field display *Level in dB (40 dB range)* | the colours span the 40 dB below the top of the colour scale |
| return loss, S-parameters | 20 · log₁₀ of the magnitude |

## Worked example

Air at 20 °C as an ideal gas gives `c = 343.23 m/s` and
`ρ = 1.204 kg/m³`. At 40 kHz the wavelength is `λ = c / f = 8.58 mm`,
the wavenumber `k = 2π / λ = 732 rad/m` and the characteristic impedance
`ρc = 413 Pa·s/m`. A peak amplitude of 1 Pa has `p_rms = 0.707 Pa` and an
SPL of 91.0 dB re 20 µPa.

## Measurements and expressions

Measured quantities have one SI unit each (volts, FS, Pa, m/s², N, V/m, ...);
a stated unit that is not the SI unit of its quantity is refused, never
converted. Raw readings stay in V or FS until a
[calibration](../experiments/calibration.md) is applied. Expressions for derived
design variables and constraints in the Optimization Studio have **no
units**: their inputs are the SI values of the quantities.

## Related topics

- [Units and coordinate systems](../getting-started/units.md)
- [Coordinate conventions](coordinates.md)
- [Phase conventions](phase.md)
- [Project file format](project-format.md)
