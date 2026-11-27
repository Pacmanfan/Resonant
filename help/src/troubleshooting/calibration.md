---
id: trouble.calibration
title: Calibration problems
category: Troubleshooting
order: 90
status: available
summary: Why a calibration is refused, expired, outside its band or not applied, and how to import a calibration record correctly.
keywords: [calibration, sensitivity, expired, band, unit, uncertainty, record]
related: [exp.calibration, trouble.acquisition, exp.uncertainty, exp.sensors, ref.data-formats]
---

A **calibration record** turns raw instrument readings (volts, or digital
full scale *FS*) into physical units, for example a microphone sensitivity in
FS/Pa. Resonant never assumes a sensitivity and never chooses a calibration
by itself: each sensor uses the record selected for it, and without one its
data stays in instrument units. Records are never changed or deleted; a new
calibration is a new record that supersedes the old one.

## Where calibrations live

The **Calibration** tab of the **Experimental Hardware** panel lists the
project's records (**Id**, **Version**, **Type**, **For**, **Value**,
**Valid until**, **Source**). Choose a **Sensor** and a **Calibration** and
press **Select For Sensor**; **Import Calibration...** adds a record from a
JSON file. The example projects in `examples/experiments/` contain records
for their simulated sensors (for example `cal-sim-mic-1`, 0.0158 FS/Pa).

> **Note** In this version a run does not add calibration records by
> itself: a calibration experiment (*Mechanical: shaker calibration*, *RF:
> array channel calibration*) produces measured data. A record enters the
> library by **Import Calibration...**, with an imported experiment archive,
> or through the Python API's experiment workspace.

## The import is refused

| Message | Fix |
|---|---|
| `<file> is not a calibration record (JSON): ...` | the file is not valid JSON |
| `calibration <id> exists already; a new calibration is a new record (calibration history is never overwritten)` | give the new record a new `id`, and name the old one in `supersedes` |
| `calibration <id> supersedes the unknown record <old>` | import the record it supersedes first, or clear `supersedes` |
| `calibration <old> is already superseded by <other>` | supersede the newest record of the chain instead |
| `calibration <id>: a response point is [frequencyHz, re, im, relativeUncertainty]` | each response row needs at least three numbers |
| `calibration <id>: the calibration response has two values at <f> Hz` | remove the duplicate frequency |
| `calibration <id>: unknown calibration type '<type>'` | use one of the types listed on [Data formats](../reference/data-formats.md) |

A minimal sensitivity record:

```json
{
  "id": "cal-mic-1-2026",
  "type": "sensor-sensitivity",
  "sensorId": "mic-1",
  "unit": "FS/Pa",
  "scalar": 0.0158,
  "scalarJustification": "flat within 0.2 dB from 100 Hz to 10 kHz (manufacturer chart)",
  "frequencyRangeHz": [100.0, 10000.0],
  "validUntilUtc": "2027-11-27T00:00:00Z",
  "source": "94 dB, 1 kHz calibrator",
  "uncertainty": {"components": [{"source": "sensor-calibration", "type": "calibration", "relative": 0.02}]}
}
```

## The run stops at step 4

Step 4 (*verify calibration status*) checks every selected record against
the band of the stimulus and today's date:

- `calibration '<id>' selected for '<binding>' is not in the calibration library`
- `calibration <id> holds no value`: neither a scalar nor a response table
- `calibration <id> expired on <date>`: import a current calibration
- `calibration <id> does not cover <low> ... <high> Hz`: the table does not
  reach the band of the measurement; no value is ever extrapolated

When the experiment requires calibrated sensors, such a problem fails the
run. Otherwise the run goes on and the warning ends in
`(the data stays uncalibrated)`. Validation already reports
`sensor '<id>' has no calibration selected and the experiment requires calibrated sensors`
as an error.

## Warnings worth reading

- `calibration <id> was made at <a> ... <b> Hz; applying it across <c> ... <d> Hz assumes a flat response there`:
  a single value is used outside the band it was measured in.
- `calibration <id> states no unit` or `states no uncertainty`: the
  calibrated result cannot carry a trustworthy unit or uncertainty.
- `calibration <id> was derived from a simulated device: it tests the procedure and calibrates nothing physical`.

## The calibration is not applied

| Message | Meaning |
|---|---|
| `calibration <id> is frequency dependent; apply it to a spectrum, not as one scalar (or state why a scalar is justified)` | a response table cannot scale a time series; process a spectrum, or state a `scalarJustification` |
| `calibration '<id>' is in <unit> but the data needs <unit>` | the record's unit does not match the sensor's quantity (for example V/Pa for FS data) |
| `'<id>' is already in physical units (<unit>)` | the data set was calibrated before |
| `'<id>' has dropped frames; calibrate a gap-free segment` | see [Acquisition errors](acquisition.md) |
| `calibration <id> covers <a> ... <b> Hz; <f> Hz lies outside (no extrapolation)` | a spectrum reaches beyond the table |

## Calibration procedures

The procedures of the calibration framework refuse unclear data rather than
return a doubtful value: `the calibration recording is too short (at least
1024 samples)`, `the recording does not show a clear tone near <f> Hz ...`,
`no frequency in <a> ... <b> Hz has a coherence of at least <c>` and
`the reference channel shows no signal at <f> Hz`. Record longer, raise the
reference level above the noise, or check the cabling.

## Related topics

- [Calibration](../experiments/calibration.md)
- [Acquisition errors](acquisition.md)
- [Uncertainty](../experiments/uncertainty.md)
- [Data formats](../reference/data-formats.md)
