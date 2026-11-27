---
id: acoustics.sources
title: Sources and transducers
category: Acoustic Engineering
order: 30
status: available
summary: Acoustic emitters: frequency, amplitude, phase, delay and directivity, how each solver uses them, and what a physical transducer flag does and does not do.
keywords: [emitter, source, amplitude, directivity, piston, cardioid, transducer]
related: [concepts.sources, acoustics.arrays, acoustics.physics, acoustics.fdtd]
---

An **emitter** is a single acoustic source. Add one with **Acoustics >
Emitter** (or the acoustics toolbar). It is placed at the origin with the
project frequency; move and orient it in the **Properties** panel. An emitter
radiates along its local **+Z** axis. Arrays of emitters are described in
[Acoustic arrays](arrays.md).

## The emitter editor

Select the emitter; the **Acoustic emitter** group of the **Properties**
panel shows:

| Field | Unit | Meaning |
|---|---|---|
| **Frequency** | Hz | the frequency it radiates at |
| **Amplitude at 1 m** | Pa | peak free-field pressure at 1 m on its axis |
| **Phase** | deg | initial phase; positive leads |
| **Time delay** | µs | true time delay of the whole signal |
| **Waveform** | text | a label only (see below) |
| **Source model** | | omnidirectional, cardioid family or baffled piston |
| **Cardioid order** | | the exponent `n` of the cardioid family |
| **Piston radius** | length | `a` of the baffled piston |
| **Physical radius** | length | the size of the source; also its near-source limit |
| **Active** | | inactive emitters are not sources |

Phase and delay are kept apart: a delay shifts the signal in time and is
correct at every frequency; a phase is correct at one frequency only.

## Source models (directivity)

```math
omnidirectional:        D(θ) = 1
cardioid family:        D(θ) = ((1 + cos θ) / 2)ⁿ,   n ≥ 0
baffled piston:         D(θ) = 2 J₁(k a sin θ) / (k a sin θ)   for θ < 90°,   0 behind
```

`θ` is the angle from the emitter's +Z axis, `J₁` the Bessel function of the
first kind, `a` the piston radius and `k` the wavenumber. A cardioid of order
1 is 1 on axis, 0.5 broadside and 0 behind; order 2 is 0.25 broadside.

The piston pattern is a **far-field** formula applied at every distance; it
is an approximation closer than about `a²/λ`. Its first null lies where
`k a sin θ` = 3.8317. Example: at 40 kHz in air (`k` = 732 rad/m) a 5 mm
piston has `k a` = 3.66 and no null at all; an 8 mm piston has its first
null at about 41°.

The analytical models apply the directivity only when **Apply source
directivity** is checked (**Acoustics** panel, **Medium** tab, Field model
group). The full-wave solver never applies it: every emitter there is a
monopole, and the preflight says so.

## How each solver uses an emitter

| Solver | What the emitter is |
|---|---|
| Analytical free field | a point source with spherical spreading and its directivity |
| Free field + reflectors | the same, plus its reflection from every reflector |
| Full-wave (FDTD), volume region | a volume-velocity point source in one cell, scaled so that its amplitude at 1 m is `A` |
| Full-wave (FDTD), plane region | a line source through the plane, scaled to `A` at 1 m at its frequency |

Sources at another frequency than the project's **Frequency** (Medium tab)
are left out of an analytical field and counted in the results. In the
full-wave solver the signal shape comes from the **Full-Wave Solver** panel;
continuous signals, tone bursts and Gaussian pulses use each emitter's own
frequency and phase, the Ricker wavelet and the sweep do not.

## Physical transducers

**Stands for a physical transducer** and **Transducer profile** record that
an emitter represents a real device and name its data sheet or measurement.
They are stored and shown, but **nothing is modelled from them**: no
measured frequency response, phase response, impedance or calibration. An
ideal point source is not a model of a real transducer. Real transducers
are characterized by measurement in the experiments workspace
([Measurements](../experiments/measurements.md)).

## The Waveform field

The **Waveform** field of the emitter is a designation (for example
"sine"). The analytical models are single-frequency harmonic solutions and
ignore it; the full-wave solver takes its signal from its own panel.

## Worked example

1. **File > New Project**, then **Acoustics > Emitter**.
2. **Acoustics > Probe**: it appears at (0, 0, 100) mm, on the emitter's
   axis.
3. **Acoustics > Field Plane**, then **F5**.
4. **Analysis > Field Analysis > Probes and Targets** shows 10 Pa at the
   probe: `A / r` = 1 Pa / 0.1 m.
5. Set **Source model** to the cardioid family, order 1, rotate the emitter
   by 90° about Y and run again: the probe now lies broadside and reads half
   of that.

## Common mistakes

- Reading **Amplitude at 1 m** as a drive voltage or a source power: it is a
  pressure at a distance.
- Using a phase to align broadband pulses: use **Time delay**.
- Expecting directivity in a full-wave run.
- Placing probes inside the **Physical radius**, where values are flagged as
  not valid.
