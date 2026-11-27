---
id: exp.sensors
title: Sensors
category: Experiments and Hardware
order: 20
status: available
summary: The physical transducers of an experiment, what each category measures, and how a sensor is bound to a device channel.
keywords: [sensor, microphone, accelerometer, load cell, channel binding, transducer]
related: [exp.calibration, exp.actuators, exp.measurements, concepts.sensors]
---

An experiment **sensor** is a physical transducer in an experiment
definition: a microphone, an accelerometer, a load cell, an antenna. It is not
the same thing as a probe or sensor object in the scene, which samples a
*simulated* field (see [Sensors](../core-concepts/sensors.md) in Core Concepts).

## What a sensor records

| Property | Meaning |
|---|---|
| category | the kind of transducer (table below) |
| measures | the physical quantity, in its SI unit |
| pose | position (m) and sensitive axis in a named frame (`scene` is the project's world frame), with position and orientation uncertainty |
| nominal sensitivity | the data-sheet value, reported as nominal, never used as a calibration |
| dynamic range, noise floor, frequency response | data-sheet information kept with the sensor |
| calibration | the calibration record this sensor uses (selected explicitly, never chosen automatically) |
| uncertainty | the sensor's own uncertainty components |

## Categories

| Category | Measures |
|---|---|
| microphone, ultrasonic-receiver, pressure-probe | pressure (Pa) |
| accelerometer | acceleration (m/s²) |
| velocity-sensor | velocity (m/s) |
| displacement-sensor | displacement (m) |
| strain-gauge | strain (m/m) |
| load-cell | force (N) |
| laser-vibrometer | velocity, displacement |
| rf-receiver-channel | full scale, voltage |
| antenna | voltage, power, full scale |
| near-field-probe | voltage, electric field (V/m), magnetic field (A/m) |
| field-strength-sensor | electric field |
| rf-power-sensor | power (W) |
| generic-voltage | voltage, full scale |

Validation refuses a sensor that claims a quantity its category does not
measure. No category measures **stress**: stress is inferred from measured
strain with a stated material model (a `scale` processing step, for example
σ = E ε, with the reason recorded in the result).

## Channel bindings

A sensor is not a device channel. A **channel binding** connects a sensor to
one channel of one device and states the conditioning gain between them (an
amplifier, in V/V, which must be positive), an optional channel calibration,
and whether the channel is the reference of a transfer-function measurement.
Every data set is recorded per binding and carries the sensor's pose.

```math
x_raw = g · S · x
```

Here `x` is the physical value at the sensor, `S` the sensitivity (raw unit per
physical unit, for example FS/Pa or V/(m/s²)), `g` the conditioning gain and
`x_raw` the device reading in volts or full scale.

## In the application

Open **Experiments > Experimental Hardware Panel** and select an experiment
on the **Experiments** tab. The **Definition** tree lists under *Sensors* each
sensor as *category measuring quantity at (x, y, z) m, frame; calibration id*,
or *calibration none (instrument units)*. Sensors whose pose is in the scene
frame are drawn in the viewport as small **green** markers.

Sensors and bindings come from the experiment templates (**Add From
Template**) and from example projects. The panel shows a definition but does
not edit it, except for the calibration a sensor uses (Calibration tab,
**Sensor**, **Calibration**, **Select For Sensor**). Other changes are made by
replacing the definition through the [Python API](../python/api.md).

## Worked example

Open `examples/experiments/acoustic_experiments.resonant` and select
*Speaker characterization*. Its sensors are:

- `mic-1`, a microphone 1 m in front of the loudspeaker, measuring pressure,
  with the calibration `cal-sim-mic-1` (0.0158 FS/Pa, taken from the simulated
  device's model and flagged as such);
- `drive-ref`, a generic-voltage sensor measuring full scale: the electrical
  loopback of the drive signal on input 7, the reference of the transfer
  function.

In `mechanical_experiments.resonant` the *Structural modal test* has a load
cell (force, simulated at 22.5 mV/N) and three accelerometers (10.2 mV/(m/s²)),
each with its simulated calibration.

## Validation messages

- *a microphone does not measure stress*: the category cannot measure the
  stated quantity (an error).
- *has no calibration selected: its data stays in instrument units*: a
  warning; the run records raw readings in V or FS.
- *has no calibration selected and the experiment requires calibrated
  sensors*: an error when the experiment requires calibration.
- *refers to the unknown sensor*: a binding names a sensor that does not exist.

## Limitations

- A sensor's frequency response is a note unless a frequency-dependent
  calibration record describes it.
- Only sensors whose pose is in the `scene` frame are drawn in the viewport.
- The data-sheet sensitivity is never applied: without a selected calibration
  the data stays in instrument units.

## Related topics

[Calibration](calibration.md) · [Measurements](measurements.md) ·
[Actuators and outputs](actuators.md) · [Synchronization](synchronization.md)
