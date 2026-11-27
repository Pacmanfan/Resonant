---
id: trouble.acquisition
title: Acquisition errors
category: Troubleshooting
order: 80
status: available
summary: Read where an experiment run stopped, what the data quality line means, and how to fix gaps, clipping, synchronization and trigger errors.
keywords: [acquisition, experiment, gaps, clipping, overrun, trigger, quality]
related: [exp.acquisition, exp.synchronization, trouble.devices, trouble.calibration, exp.safety]
---

An experiment run goes through 13 steps, from *validate the experiment* to
*generate the report*. The **Run** tab of the **Experimental Hardware**
panel shows the state and the current step while it runs
(`running - step 9: ...`), the list of steps with their status and message,
and the live statistics (`N frames read, N dropped, buffer N / N, N kB/s to
disk`). A run either completes, or stops at one step with the reason, for
example `failed: step 3 (verify hardware capabilities) failed: <device> does
not support <rate> Hz`. Nothing is retried or filled in silently.

## The run does not start

| Message | What to do |
|---|---|
| `save the project first: measurements are stored next to the project file` | save the project in a writable folder; data goes to `<name>.data/` beside it |
| `the experiment drives physical outputs: authorize the output first (nothing was output)` | **Authorize Output...**, then **Run** ([Safety](../experiments/safety.md)) |
| `an experiment is running` | one run at a time; wait, or press **Stop** |
| `Error: ...` lines under **Validation (without hardware)** | the definition is invalid; the run fails at step 1 |

Frequent validation errors:

- `the stimulus reaches <f> Hz, at or above the Nyquist frequency <fs/2> Hz of the acquisition`:
  raise the sample rate or lower the stimulus.
- `the acquisition needs a positive sample rate, duration and block length`
  and `the acquisition buffer needs at least 2 blocks`.
- `phase-sensitive measurement across N devices needs a shared sample clock or a timing calibration; equal nominal sample rates do not make acquisition coherent`.

## Steps 2 to 8: before any sample is read

| Step | Typical failure |
|---|---|
| 2 verify device availability | `device '<id>' was not found by discovery ...` ([Device connection failures](devices.md)) |
| 3 verify hardware capabilities | `<device> does not support <rate> Hz`, `<device> has no input channel '<id>'` |
| 4 verify calibration status | a selected calibration is missing, expired or does not cover the band ([Calibration problems](calibration.md)) |
| 5 verify synchronization | `<device> does not sample its channels on one clock; phase comparison needs a timing calibration`; `shared-clock strategy, but <device> runs on clock 'internal', not an external reference` |
| 6 check safety limits and output authorization | the output exceeds the limits, or the authorization is expired, used, for another experiment, or the experiment changed after it was granted |
| 8 arm triggers | `level triggering is not implemented in this version; use immediate, software or external triggering`; `<device> has no hardware trigger input` |

## Step 9: the acquisition fails

A failure during acquisition ends the run with
`acquisition failed: <reason> (the data recorded before the failure is kept, marked incomplete, and was not processed)`.
Reasons include `device error: ...` (a device error always ends the
acquisition), a stalled or disconnected device, the emergency stop, and
`the device delivered a block of N channel(s) and M frame(s) that does not match the configuration`.
**Stop** ends a run with `cancelled; outputs stopped; the data recorded so far is kept, marked incomplete`.
A failure after the acquisition, in step 12, reads
`processing failed: <reason> (the raw data is complete and kept)`.

## The data quality line

Every data set on the **Measurements** tab has a **Quality** line. A clean
recording reads `complete; no dropped samples, gaps, clipping or timestamp problems`.
Otherwise it lists what happened, for example:

```text
INCOMPLETE; 1024 dropped frame(s) in 1 gap(s); 37 clipped sample(s) (saturated)
```

| Problem | Cause | Remedy |
|---|---|---|
| dropped frames, gaps | the device or the recorder fell behind: a capture overrun, or the recorder was behind for more than 2 s | close other programs, use a larger block length, a faster disk, or a lower rate |
| clipped samples | readings at 99.9 % of the channel's range; three in a row count as saturation | lower the input gain or the stimulus amplitude |
| timestamp discontinuities, invalid timestamps | the device's timestamps disagree with the frame count, go back, or are not finite | check the device clock; treat timing results with care |
| device error | the device reported an error | see the device's message |

Missing frames are stored as NaN, so every stored sample keeps its time.
Processing then refuses the data set:
`'<id>' has dropped frames (NaN); take a gap-free segment first`.
Process a gap-free segment, or run the measurement again.

## The data cannot be shown

The measurement text ends with `Data:` and the reason when a data set cannot
be read, for example `measurement <id> was not recorded to disk` (the
acquisition did not record to disk) or `the measurement data set ... is
missing from ...`. Data is stored in `<project name>.data/measurements/`
next to the project file and is found from the project file's name and
folder. **File > Save As...** under another name or in another folder does
not move that folder: copy it beside the new project file and rename it to
match. A build without HDF5 cannot read `.h5` data, and a build with HDF5
does not read `.rmd` data ([Missing dependencies](dependencies.md)).

## Simulated devices

Data from the simulated instruments is generated, not measured; every
record says `Acquired from a SIMULATED device: generated data, not a
measurement.` Use them to rehearse a procedure and to reproduce the faults
above before a real instrument is connected.

## Related topics

- [Acquisition](../experiments/acquisition.md)
- [Synchronization](../experiments/synchronization.md)
- [Device connection failures](devices.md)
- [Calibration problems](calibration.md)
