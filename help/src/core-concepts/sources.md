---
id: concepts.sources
title: Emitters and sources
category: Core Concepts
order: 40
status: available
summary: What drives a field in each domain - acoustic emitters, mechanical actuators, antennas and ideal electromagnetic sources - and how their amplitude, phase, delay and signal are defined.
keywords: [emitter, source, amplitude, phase, delay, directivity, actuator, antenna]
related: [acoustics.sources, em.antennas, mechanics.actuators, concepts.arrays, acoustics.fdtd]
---

A **source** is the object that puts energy into a field. Every source is
a scene object with its own frequency, amplitude, phase and time delay, so
two sources never share settings by accident. Arrays are groups of sources
described in [Arrays](arrays.md).

| Domain | Source | Created with | Strength means |
|---|---|---|---|
| Acoustics | emitter | **Acoustics > Emitter** | peak pressure at 1 m on its axis, in a free field (Pa) |
| Mechanics | actuator | **Mechanics > Add Actuator** | peak force (N) along its local +Z |
| Electromagnetics | dipole, monopole, small loop, patch, horn, helix | **Electromagnetics > Add ... Antenna** | peak feed current (A) |
| Electromagnetics | electric dipole, magnetic dipole, plane wave | **Electromagnetics > Add ... Source / Add Plane Wave** | current moment I·l (A·m), I·A (A·m²), peak field (V/m) |

## Acoustic emitters

Fields of the **Acoustic emitter** editor in **Properties**:

| Field | Unit | Meaning |
|---|---|---|
| Frequency | Hz | new emitters take the project's analysis frequency |
| Amplitude at 1 m | Pa | peak pressure at 1 m on axis |
| Phase | deg | initial phase; positive leads |
| Time delay | us | a true delay of the whole signal |
| Waveform | | a label only: the analytical model is a single-frequency harmonic solution |
| Source model | | *Omnidirectional point source (ideal)*, *Cardioid family (directional point source)* with a *Cardioid order*, or *Baffled circular piston (approximation)* with a *Piston radius* |
| Physical radius | display length | the emitter's size; inside it the point-source model does not apply |
| Active | | an inactive emitter does not radiate |
| Stands for a physical transducer, Transducer profile | | stored and shown; no measured response is modelled yet |

The emitter radiates along its local **+Z** axis; rotate it to aim it. Its
field in the analytical model is

```math
P(x) = A · e^{j(φ − ωτ)} · (r_ref / r) · D(θ) · e^{−α r} · e^{−j k r}
```

with `A` the amplitude, `φ` the phase, `τ` the delay, `ω = 2πf`, `r` the
distance, `r_ref` = 1 m, `D(θ)` the directivity at the angle `θ` from the
axis, `α` the medium's attenuation (Np/m) and `k = ω / c`. Sources add
linearly; adding an emitter never rescales another one.

The analytical solver works at one frequency: emitters at another
frequency are left out of a run and counted in its notes.

## Sources in the acoustic full-wave solver

In **Simulation > Run Full-Wave Simulation** the emitters drive the grid
with the signal chosen in the **Full-Wave Solver** panel: *Continuous
sinusoid (steady state)*, *Tone burst*, *Gaussian pulse*, *Ricker wavelet
(broadband)* or *Frequency sweep*, one kind per run. Each emitter keeps its
amplitude, frequency and true time delay, and is a monopole: directivity
models are not applied, and the result says so. On a plane region (2D) an
emitter is a line source.

## Mechanical actuators

**Mechanics > Add Actuator** places a force on the selected structure
(*Acts on*). *Excitation* is a *Point force (nearest node)* or a
*Distributed force (within a radius)*; *Force amplitude* (N), *Phase*
(deg), *Time delay* (ms) and *Signal over time* (continuous sinusoid, tone
burst, Gaussian pulse, Ricker wavelet, frequency sweep) for transient
analyses. The force is applied exactly as set: a shaker's, piezo's or voice
coil's own response is not modelled ([Actuators](../mechanics/actuators.md)).

## Electromagnetic sources

Antennas are fed at their local origin with a *Feed current* (A, peak),
*Feed phase*, *Time delay* (ns) and a *Reference impedance* (ohm); new
dipoles and monopoles are sized for 1 GHz. Dipoles, monopoles and small
loops have analytical models; patches, horns and helices give design
estimates only. Ideal sources: an electric dipole (Hertzian current
element) and a magnetic dipole point along local +Z; a plane wave travels
along local +Z with its electric field along local +X
([Antennas](../electromagnetics/antennas.md)).

## Worked example

Open `examples/acoustics/a-two-source-interference.resonant`: two coherent
40 kHz emitters three wavelengths apart, a field plane and two probes. Press
F5. In **Analysis > Field Analysis**, the probe where both paths are equal
reads twice one emitter's pressure (2A/r), and the probe where the paths
differ by half a wavelength reads less than 5 % of that. Move one emitter
by a quarter wavelength (about 2.1 mm) and run again to see the pattern
shift.

## Limitations

- Acoustic waveforms other than a sinusoid, measured transducer responses and impedances are not modelled.
- The piston directivity is a far-field formula used at every distance.
- In the full-wave solvers, sources radiate without directivity (acoustic) and only dipoles, monopoles and current elements are represented (electromagnetic).
- Physical outputs of experiments (signal generators, speakers) are a separate system that needs an explicit authorization ([Actuators and outputs](../experiments/actuators.md)).
