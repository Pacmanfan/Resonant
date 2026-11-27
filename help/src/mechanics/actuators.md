---
id: mechanics.actuators
title: Actuators
category: Mechanical Engineering
order: 30
status: available
summary: Drive a structure with ideal point or distributed forces, and place sensors and vibration targets on it.
keywords: [actuator, force, shaker, sensor, vibration target, excitation, signal]
related: [mechanics.forced-response, mechanics.optimization, mechanics.waves, concepts.sensors, exp.actuators]
---

A mechanical actuator is a scene object that applies a **force** to a
structure. Resonant models the force only: a named shaker, piezo stack or
voice coil is recorded, but its own response is not modelled. Sensors and
vibration targets are placed the same way and are described below.

## Adding an actuator

Select the structure and choose **Mechanics > Add Actuator**. The actuator
is placed on top of the centre of the selected structure (or of the one
chosen in the Mechanics panel). Move it with the transform in the
**Properties** panel. The force acts along the actuator's **local +Z
axis**: rotate the actuator to change the direction.

| Field | Meaning |
|---|---|
| Excitation | *Point force (nearest node)* or *Distributed force (within a radius)*, spread evenly over the nodes within **Radius** |
| Force amplitude | peak force in N (default 1 N) |
| Phase | phase of the force in degrees (harmonic analyses) |
| Time delay | a real time shift in ms |
| Signal over time | *Continuous sinusoid*, *Tone burst*, *Gaussian pulse*, *Ricker wavelet (impulse)* or *Frequency sweep* (transient analyses) |
| Signal frequency, Burst cycles | parameters of the signal |
| Active | inactive actuators do not drive |
| Acts on | *The nearest structure* or a named one |
| Stands in for a physical device, Device | a label for the real drive; not modelled |

## How the force enters each analysis

```math
F(ω) = A · e^{j(φ − ωτ)} · p          harmonic analyses (time convention e^{+jωt}, peak values)
f(t) = A · w(t − τ) · p               transient analyses
f = A · cos φ · p                     static analysis
```

A is the force amplitude, φ the phase, τ the time delay, w(t) the signal
over time and p the load pattern (one node, or the nodes within the
radius). All active actuators act together with their own amplitudes and
phases.

## Attachment

An actuator is attached when it lies within 2 × the element size plus half
the section (or thickness) of the structure. A named actuator that is
farther away is reported as not attached, with its distance. A force
component that the idealization cannot carry, such as the in-plane part of
a tilted force on a plate or membrane, is reported with its fraction, for
example "50 % of the force of 'Tilted actuator'".

## Sensors

**Mechanics > Add Sensor** places a measurement point. It records the
response along its local +Z axis.

- **Measures**: *Displacement*, *Velocity* or *Acceleration* (default).
- **On**: the nearest structure or a named one.
- **Sensitivity**, **Sensitivity unit**, **Sensor model**: calibration data
  for measured records. No solver uses them.

Sensors appear in the **Response at** list of the Frequency Response tab
and in the **Show** list of the Transient tab.

## Vibration targets

**Mechanics > Add Vibration Target** marks a point where a vibration is
wanted: **Wanted displacement** (µm, peak, along local +Z), **Phase**,
**Weight**, and **Quiet point (wanted displacement zero)**. Targets are
used by the Targeting tab and by the Optimization Studio
([Mechanical optimization](optimization.md)).

## Worked example

`examples/mechanics/c-forced-resonance.resonant` holds the aluminium
cantilever with *Shaker at the tip (ideal force)*: a 1 N continuous point
force, and two sensors, *Accelerometer at the tip* and *Accelerometer at
mid-length*. Run **Mechanics > Calculate Frequency Response** to see the
accelerance at both points ([Forced response](forced-response.md)).

`examples/mechanics/d-transient-vibration.resonant` strikes a clamped
steel plate with *Impulse (Ricker force pulse)*: 5 N, signal frequency
400 Hz, used by the transient analysis
([Mechanical waves](waves.md)).

## Common mistakes

- **Wanting a transfer function with several actuators active.** The
  frequency response is per newton of all active actuators together. Keep
  one actuator active at 1 N for a transfer function between it and a point.
- **Actuator floating above the structure.** Check the study text: an
  unattached actuator drives nothing.
- **Signal settings in a harmonic analysis.** Harmonic analyses use the
  amplitude, phase and delay at each frequency; the signal over time only
  matters for transient analyses.

## Limitations

- Forces only: prescribed displacement or velocity excitation is not
  implemented.
- Physical actuator response models (shaker dynamics, piezo coupling) are
  not implemented; the force is applied exactly as set.
- Real devices are driven from the Experiments panel, not from here
  ([Actuators and outputs](../experiments/actuators.md)).
