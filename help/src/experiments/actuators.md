---
id: exp.actuators
title: Actuators and outputs
category: Experiments and Hardware
order: 30
status: available
summary: Loudspeakers, transducers, shakers and RF sources in an experiment, the stimulus they are driven with, and why a commanded output is not a physical output.
keywords: [actuator, stimulus, output, sweep, shaker, loudspeaker, command]
related: [exp.safety, exp.sensors, exp.acquisition, mechanics.actuators]
---

An experiment **actuator** is a physical source driven by a device output: a
loudspeaker, an ultrasonic transducer, a shaker, a piezo actuator, an RF
transmitter or an antenna element. It is not the actuator object of a
mechanical simulation (see [Actuators](../mechanics/actuators.md)).

> **Warning** Every physical output needs an explicit output authorization by
> a named operator, for one run, within the experiment's safety limits. Read
> [Safety](safety.md) before driving real hardware.

## What an actuator states

| Property | Meaning |
|---|---|
| category | speaker, ultrasonic-transducer, shaker, piezo-actuator, rf-transmitter, antenna-element, generic-source |
| command quantity | what the device channel delivers: V, FS or power |
| output quantity | the physical result: pressure, force, field |
| rated band, rated command amplitude | the actuator's ratings; validation compares the stimulus with them |
| nominal delay | the data-sheet delay from command to output |
| pose | position and axis in a frame; drawn as an **orange** marker when in the scene frame |
| calibration | a command-to-output transfer function, when measured |

## Commanded is not physical

Resonant knows the signal it sends to a channel; it does not know what the
actuator does with it. Every run therefore records the commanded signal of
each driven output as a data set of category **command** ("the commanded
output signal, not a measurement of the actuator's physical output"). The
actuator's real behaviour is measured as a transfer function from that
command to a calibrated sensor, which is what the *Mechanical: shaker
calibration* template does.

## The stimulus

| Kind | Signal |
|---|---|
| none | no output |
| sine | a tone at the start frequency |
| log-sweep | an exponential sweep from the start to the end frequency |
| linear-chirp | a linear sweep |
| white-noise | uniform noise within the peak amplitude, reproducible (the same seed gives the same samples on every platform); the safety check treats it as reaching half the sample rate |
| pulse | a single Hann pulse lasting one period of the start frequency |
| rf-carrier | an RF carrier at the start frequency with a power in dBm |

Its parameters are the peak amplitude (in the command unit, for example FS or
V), the band (start and end frequency, Hz), the duration (s), the duty cycle
(one burst), raised-cosine fades, the output bindings it drives, and per
binding a **delay** (a true time shift, s) and a **gain**, which is how array
steering and focusing are expressed. A per-binding delay τ shifts the signal
by a whole time, not only its phase:

```math
u_n(t) = g_n · s(t − τ_n)
```

with `s(t)` the stimulus, `g_n` and `τ_n` the gain and delay of binding `n`.

## Validation of the stimulus

- *the stimulus names no output channel*, or names a binding that is not an
  output: an error.
- The amplitude above an actuator's rated command amplitude: an error. A band
  reaching outside its rated band: a warning.
- A stimulus at or above the acquisition's Nyquist frequency (half the sample
  rate): an error. A stimulus longer than the acquisition: a warning (its end
  is not recorded).
- *the safety limits permit no output*: a warning; authorization is refused
  until the limits allow an amplitude.

## Worked examples

*Acoustic focusing validation* (`examples/experiments/acoustic_experiments.resonant`):
two loudspeakers play a 4 kHz sine of 0.2 FS for 1 s; the binding `ch-spk` is
delayed by 70.6 µs so that both arrivals line up at the target (0.2, 1.0, 0) m.
In the automated test the loudest microphone is the one nearest the target,
and the predicted focus lies at the same position.

*Shaker calibration* (`examples/experiments/mechanical_experiments.resonant`):
a 1 V log sweep from 10 Hz to 2 kHz for 8 s through the simulated DAQ; the
load-cell force is calibrated and divided by the commanded voltage, giving the
shaker's force per volt (the simulated shaker is 10 N/V with a 5 Hz
high-pass).

## Common mistakes

- Treating the command data set as the output level. It is what was sent, in
  V or FS.
- Forgetting the amplifier: an external amplifier gain is not part of the
  command; state it on the binding and in the safety limits.
- Expecting a changed stimulus to keep its authorization: any change to the
  definition needs a new one.

## Limitations

- The panel shows the stimulus (*Stimulus: kind, start ... end Hz, amplitude,
  s*) but does not edit it; definitions come from templates, example projects
  or the [Python API](../python/api.md).
- RF transmission is possible only with a transmit-power limit in the safety
  limits; none is set by default.

## Related topics

[Safety](safety.md) · [Acquisition](acquisition.md) ·
[Sensors](sensors.md) · [Calibration](calibration.md)
