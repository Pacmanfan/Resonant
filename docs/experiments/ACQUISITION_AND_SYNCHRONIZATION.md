# Acquisition and Synchronization

RES-EXP-007, RES-EXP-008, RES-EXP-026.

## The engine

```
device --(reader thread)--> bounded queue --(recorder thread)--> store (stream per channel)
                                                   |--> memory (for processing)
                                                   '--> live view (try-lock: never waits)
```

- **Backpressure.** When the queue is full the reader waits; the device then
  buffers and reports an overrun if its own buffer fills. If the recorder
  stays behind longer than `maxBackpressureSeconds`, the block is counted as
  dropped, with a note - never silently discarded.
- **Integrity.** A jump of the frame counter or a device-reported overrun is
  a gap (frames, count, position); a counter going back or a non-finite or
  decreasing timestamp is an invalid timestamp; a hardware timestamp off the
  frame count by more than two sample periods is a discontinuity; readings at
  `clipFraction` of the channel range are clipped, a run of them saturation;
  device errors end the acquisition. Missing frames are stored as NaN so
  every stored sample keeps its time; processing refuses data with gaps
  until a gap-free segment is chosen.
- **Statistics.** Blocks and frames read, recorded and dropped, gaps, queue
  occupancy and its maximum, acquisition rate, disk throughput, live-view
  updates skipped because a display was busy.
- **Stopping.** A stop request stops the device and closes the data sets as
  incomplete; what was recorded stays readable.

## Clocks and synchronization

Every record states its clock domain (`<device>:<clock source>`), whether
its timestamps are hardware or software, and any timing correction applied.

| Situation | What Resonant assumes |
|---|---|
| channels of one device | one sample clock when the device says so (`sharedSampleClock`) |
| several devices, `shared-clock` | each device must be configured on an external reference clock |
| several devices, `timing-calibration` | delays from the named timing / channel-to-channel calibrations are recorded per channel |
| several devices, anything else | independent clocks: phase and delay between them are not meaningful |

A phase-sensitive experiment across devices without a shared clock or a
timing calibration does not validate: equal nominal sample rates are not
coherence. Clock drift appears in the simulated devices as `clockDriftPpm`
and in the timestamps; channel delays are estimated by `channelToChannel`.

## Triggers

Immediate and software triggers start at once; an external (hardware)
trigger needs a device with a hardware trigger input. Level triggering is
not implemented in this version and is refused at step 8.
