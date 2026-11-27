---
id: exp.safety
title: Safety
category: Experiments and Hardware
order: 100
status: available
summary: Physical outputs never start without an explicit, single-run authorization by a named operator within the experiment's safety limits; the latched emergency stop ends them.
keywords: [safety, authorization, emergency stop, output, limits, interlock, operator]
related: [exp.actuators, exp.acquisition, exp.discovery, trouble.devices]
---

Loudspeakers, ultrasonic transducers, shakers, piezo actuators, RF sources
and network-analyser test ports can hurt people and damage equipment.
Resonant therefore starts **no physical output without an output
authorization**, and the only way to get one is an explicit act of a named
operator, for one run of one experiment, within that experiment's safety
limits.

> **Warning** A software limit is not a hardware interlock, and a warning
> about hearing, ultrasound, mechanical loads or radio regulations does not
> establish compliance with them. Use amplifier limits, attenuators, dummy
> loads, shielded enclosures and hardware interlocks as the setup requires.

## What needs an authorization

An experiment drives a physical output when its stimulus is not *none* and
names output channels, or when it measures with a network-analyser sweep. The
**Run** tab says *This experiment drives no physical output.* for every other
experiment; those run without an authorization.

Nothing else authorizes an output: not starting the application, not opening
or creating a project, not adding an experiment from a template, not
importing an archive, not Python. Authorizations exist only in the running
application's memory and are **never saved** in the project.

## The safety limits

| Limit | Default | Meaning |
|---|---|---|
| maximum amplitude | 0 (no output) | the highest peak command amplitude, in the limit's unit (V by default, FS for the acoustic examples) |
| permitted band | not restricted | when an upper frequency is set, the stimulus must lie within the band |
| duty cycle | 1 | the highest fraction of time an output may be on |
| longest output | not restricted | the total output time: stimulus duration × repetitions × scan positions |
| permitted output channels | none | the output bindings that may be driven |
| interlock required | no | every output device must report a closed hardware interlock |
| maximum RF transmit power | none (no RF transmission) | dBm; required for any electromagnetic output |
| exposure warning level | 85 dB | the sound pressure level at 1 m above which a hearing warning is shown |
| regulatory note | empty | the licence or test-site conditions an RF run relies on |

## The check

Any one **violation** forbids the output:

- the maximum amplitude is 0, or the peak amplitude exceeds it (an
  electromagnetic source or network analyser with no command amplitude is
  limited by its transmit power instead);
- the stimulus band lies outside the permitted band;
- the duty cycle or the total output time exceeds its limit;
- an output channel is not permitted, or the request names no channel;
- an electromagnetic experiment has no transmit-power limit, or exceeds it.

**Warnings** are shown and do not forbid: RF transmission (always, with the
regulatory note), ultrasonic output above 20 kHz (*inaudible does not mean
harmless*), acoustic output (check amplifier gain and transducer rating),
mechanical excitation (a resonance can amplify the response far above the
drive level) and a required interlock. The hearing warning needs an
estimated sound pressure level, which the application does not compute in
this version, so it is not shown.

## Authorizing in the application

1. Select the experiment. The Run tab shows *The output lies within the
   safety limits.* or *The output is NOT within the safety limits:* with each
   violation (`-`) and warning (`!`).
2. **Authorize Output...** is enabled only when the experiment drives an
   output, the check passes, no run is active and the emergency stop is not
   latched. It opens **Authorize Physical Output**: the request (experiment,
   domain, output channels, peak command amplitude, transmit power,
   frequencies, duty cycle, total output time) and the warnings.
3. Type the **Operator** name and tick *I have checked the setup, the
   amplitude and the safety of everyone present, and I authorize this output
   for one run.* **Authorize** is enabled only with a name (spaces alone do
   not count), the box ticked and the limits met.

The status line says *Output authorized by name for one run.*, the Run tab
*Authorized for one run.* The authority checks again when it grants: it
refuses while the emergency stop is latched, without a name or an
acknowledgement, or when the limits are violated.

## One run, then spent

- Unused, an authorization **expires after 10 minutes**.
- A run that starts takes the authorization with it, whatever its outcome;
  the next run needs a new authorization. Run is disabled, and refuses, while an output
  experiment is not authorized (*authorize the output first (nothing was
  output)*).
- Changing the experiment (for example **Select For Sensor**), removing it,
  or opening or creating another project drops it.
- At step 6 the run checks the limits again, that an authorization was given
  (otherwise *Nothing was output.*), that it is for this experiment and still
  valid, that the experiment asks for no more than was authorized (the same
  channels; no higher amplitude, transmit power or total time; no wider
  band), and, when required, that every output device reports a closed
  interlock. A device without an interlock reports it open.
- Just before the outputs start the run consumes the authorization; it fails
  if the authorization expired, was revoked or used, or the stop is latched.
  Devices check it for **every output block** and stop at once when it is no
  longer valid. When the run ends it is released and spent.
- A device error or a lost connection ends the acquisition; outputs are
  stopped.

## The emergency stop

The red **EMERGENCY STOP** button on the Run tab, and **Experiments >
EMERGENCY STOP** (**Ctrl+Shift+Space** in the main window), latch
the stop at once: every output session stops at its next block, every
authorization (granted or in use) is revoked, a running experiment is
cancelled (outputs first, then acquisition; its data is kept, marked
incomplete), and no authorization can be granted. The Run tab shows
*EMERGENCY STOP is latched: no output is possible until it is reset.*

**Reset Stop** is enabled only while the stop is latched and no run is
active. It clears the latch and grants nothing: authorize again before the
next output.

## From the command line

```text
resonant project.resonant --run-experiment <id|first> --authorize-output "operator name"
```

`--authorize-output` is the same explicit act, with the same limits and the
same single-run rules. Without it an experiment with outputs does not run.

## Worked example

*Antenna array channel calibration* in `examples/experiments/rf_experiments.resonant`
transmits a 2.4 GHz carrier at −20 dBm on `ch-tx`. Its limits permit `ch-tx`,
2.16-2.64 GHz and at most −10 dBm, with the note *low-power test transmission
in a shielded enclosure or under the applicable licence*. The check passes
with the RF warning; the step list of the run records who authorized the
output and when.

## Related topics

[Actuators and outputs](actuators.md) · [Acquisition](acquisition.md) ·
[Hardware discovery](discovery.md) · [Device connection failures](../troubleshooting/devices.md)
