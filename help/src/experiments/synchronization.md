---
id: exp.synchronization
title: Synchronization
category: Experiments and Hardware
order: 50
status: available
summary: Clocks, timestamps and timing calibrations: when phases and delays between channels mean something, and when they do not.
keywords: [synchronization, clock, phase, timing, timestamps, delay, coherence]
related: [exp.acquisition, exp.calibration, exp.measurements, ref.phase]
---

A phase or a delay between two channels is meaningful only when both were
sampled on a known common time base. Resonant never assumes that devices
share a clock, a sample rate or a trigger: **equal nominal sample rates are
not coherence.** Two clocks that differ by a few parts per million drift
apart during a recording, and an unknown offset between two devices turns
directly into a phase error:

```math
Δφ = 2π · f · Δt
```

`Δφ` is the phase error (rad), `f` the frequency (Hz) and `Δt` the timing
error (s). A timing error of 1 µs is 0.36° at 1 kHz but 864° at 2.4 MHz.

## What every record states

Each data set records its **clock domain** (`<device>:<clock source>`),
whether its timestamps come from the hardware or from the computer, and any
timing correction applied to it. A comparison of phases between records from
different clock domains is therefore always visible as such.

## What Resonant assumes

| Situation | Assumption |
|---|---|
| channels of one device | one sample clock, only when the device says so (its *shared sample clock* capability) |
| several devices, strategy `shared-clock` | each input device must be configured on an external reference clock |
| several devices, strategy `timing-calibration` | delays from the named timing calibrations are applied per channel and recorded |
| several devices, anything else | independent clocks: phase and delay between them are not meaningful |

## Settings in the experiment

| Setting | Meaning |
|---|---|
| phase-sensitive | the experiment compares phases or delays between channels |
| strategy | `none`, `single-device`, `shared-clock` or `timing-calibration` (`shared-trigger` is accepted as a name but does not make a phase measurement across devices valid) |
| clock domains | per device: source (internal, external 10 MHz, PPS, ...), nominal and reference frequency, offset, drift (ppm), uncertainty |
| timing calibrations | the calibration records that hold channel delays |
| channel delay corrections | a delay per binding (s), subtracted |

The **Definition** tree on the Experiments tab shows them as *Synchronization:
phase-sensitive, strategy*.

## How a run checks it

**Validation** (before any hardware):

- phase-sensitive with input channels on more than one device and a strategy
  other than `shared-clock` or `timing-calibration`: an error;
- `timing-calibration` naming no calibration: an error;
- phase-sensitive on one device with strategy `none`: a warning (it relies on
  the device's channels sharing one clock, checked when it is connected).

**Step 5** (*verify synchronization*), with the devices open:

- one input device: it must report a shared sample clock, or the run fails;
- `shared-clock`: every input device's configured clock source must be an
  external reference, or the run fails;
- `timing-calibration`: every named calibration must be in the library; its
  delay is applied to the matching channel and recorded;
- not phase-sensitive with several devices: the run continues and records
  *device clocks are independent and were not aligned*.

## Measuring a timing calibration

The channel-to-channel procedure estimates the gain ratio and phase at a
frequency and the delay between two channels: the integer lag of the
cross-correlation peak plus a fraction from the slope of the cross-spectrum
phase. In the automated tests a gain of 0.5 and a delay of 3 samples are
recovered within 0.001 and 0.05 samples. A delay measurement in band-limited
noise of 37.3 samples is recovered within 0.02 samples.

## Worked examples

- *Acoustic field mapping* (`examples/experiments/acoustic_experiments.resonant`)
  is phase-sensitive with strategy `single-device`: all seven microphones are
  inputs of `sim:audio-8x2`, whose channels share one sample clock, so the
  phases of the 2 kHz tone across the line are comparable.
- *Antenna array channel calibration* (`rf_experiments.resonant`) is
  phase-sensitive on the four channels of `sim:rf-rx-4`. The simulated
  receiver has channel phases of 0°, 37°, −52° and 101° by default; the
  measured tone phases relative to channel 1 recover them within 0.5°.
- The simulated vibration DAQ has a `clockDriftPpm` option and the simulated
  RF receiver a `coherent` option (one LO or independent LOs) to show what an
  unsynchronized setup does to the data.

## Common mistakes

- Comparing phases between two devices because both run at 48 kHz.
- Forgetting that a software timestamp is the time the computer received a
  block, not the time it was sampled.
- Measuring a delay with segments that are short against it: H1 transfer
  functions are biased low by a delay that is not small compared with the
  segment length.

## Limitations

- The panel shows the synchronization settings but does not edit them; they
  come from templates, example projects or the [Python API](../python/api.md).
- Level triggering is not implemented; there is no hardware trigger
  distribution between devices beyond what a device reports.

## Related topics

[Acquisition](acquisition.md) · [Calibration](calibration.md) ·
[Measurements](measurements.md) · [Phase conventions](../reference/phase.md)
