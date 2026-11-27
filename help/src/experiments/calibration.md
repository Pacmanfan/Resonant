---
id: exp.calibration
title: Calibration
category: Experiments and Hardware
order: 60
status: available
summary: Calibration records, the append-only library, how a calibration turns instrument readings into physical units, and the checks a run makes.
keywords: [calibration, sensitivity, calibrator, library, units, traceability]
related: [exp.sensors, exp.uncertainty, exp.measurements, trouble.calibration]
---

Instruments deliver volts or digital full scale. A **calibration record**
says how a reading relates to a physical quantity, how it was obtained and
how well it is known. Resonant applies a calibration only when an experiment
selects it for a sensor and a processing step asks for it.

## The calibration record

| Field | Meaning |
|---|---|
| type | amplitude, phase, timing, frequency, position, orientation, sensor-sensitivity, actuator-transfer-function, channel-to-channel |
| for | a device and channel, a sensor, or an actuator |
| date, procedure, reference instrument, operator, source | where the value comes from |
| value | a scalar in a stated unit (for example 0.05 V/Pa), or a frequency-dependent complex response (frequency in Hz, real, imaginary, relative uncertainty per point) |
| scalar justification | why one scalar may stand for a frequency-dependent quantity |
| delay, phase, position offset, frequency error | the results of timing, phase, position and frequency calibrations |
| frequency and amplitude range, temperature | the conditions it was made in |
| valid until | its expiry date (UTC) |
| uncertainty | its uncertainty budget |
| from a simulated device | always flagged when true |

## The library is append-only

Records are never changed or deleted. A new calibration is a new record that
**supersedes** the previous one; its version follows that record's. A record
id that exists is refused, and a record can be superseded only once. The
experiment selects the record each sensor uses: nothing is chosen
automatically, not even the newest one.

## In the application

The **Calibration** tab of the Experimental Hardware dock lists every record
with **Id**, **Version**, **Type**, **For**, **Value** (the scalar and unit, or
the number of points), **Valid until** (*not stated* when empty) and
**Source** (*(simulated)* when made on a simulated device).

- To use a record: choose the **Sensor** and the **Calibration**, then press
  **Select For Sensor**. This changes the experiment definition (its revision
  increases, it can be undone) and drops an output authorization the
  experiment had. Choose *(none)* to use no calibration.
- To add a record: **Import Calibration...** reads a calibration record in
  JSON. The file name is kept in the record.

A minimal scalar record:

```json
{"id": "cal-mic-1-2026", "type": "sensor-sensitivity", "sensorId": "mic-1",
 "scalar": 0.0102, "unit": "V/Pa", "frequencyRangeHz": [250, 1250],
 "procedure": "94 dB calibrator at 1 kHz", "validUntilUtc": "2027-11-01T00:00:00Z",
 "supersedes": "cal-mic-1-2025"}
```

## Applying a calibration

Sensitivities are raw unit per physical unit (V/Pa, V/(m/s²), FS/Pa).

```math
x(t) = x_raw(t) / (g · S)
X(f) = X_raw(f) / (g · S(f))
```

`g` is the binding's conditioning gain and `S` the scalar sensitivity, or
`S(f)` the complex response interpolated linearly in magnitude and in
unwrapped phase. A time series is divided by a scalar only: a
frequency-dependent record without a scalar justification is refused there.
A spectrum or transfer function outside the record's frequencies fails: there
is no extrapolation. The result's unit follows the sensor's quantity, and its
uncertainty combines the data's and the calibration's.

## Checks at step 4 of a run

- A selected record that is not in the library fails the run.
- A record that has expired, holds no value, or whose table does not cover the
  band is **unusable**: the run fails when the experiment requires
  calibration; otherwise the data stays uncalibrated and a warning says so.
- A scalar made at one frequency and applied across a band is a warning (it
  assumes a flat response there); so are a missing unit, a missing
  uncertainty, and every record from a simulated device.

## Procedures

| Procedure | Result |
|---|---|
| reference source (for example a 94 dB, 1 kHz calibrator) | a scalar sensitivity at the source's frequency (the tone is searched within ±2 % of the stated frequency), with a noise component |
| comparison with a reference sensor | a frequency-dependent response from H1; bins below a coherence threshold left out |
| channel to channel | gain ratio, phase at a frequency, delay |
| actuator | command-to-output response and bulk delay |
| ultrasonic array elements | amplitude and phase per element, propagation phase removed |
| RF array channels | complex gain per channel; the excitation can be pre-compensated |

These procedures are part of Resonant's experiment framework and are verified
by its tests. In this version the application runs the measurements they
need (the shaker calibration, ultrasonic element and RF array calibration
templates) but has no command that turns a result into a calibration record:
import the record as JSON instead.

## Worked example

A 94 dB calibrator produces 1 Pa RMS:

```math
L_p = 20 · log10(p_rms / 20 µPa) = 94 dB  →  p_rms ≈ 1.00 Pa
S = V_rms / p_rms
```

A microphone reading 10.2 mV RMS on it has a sensitivity of 10.2 mV/Pa. In
the automated tests a 1003 Hz tone of 1 Pa through 0.05 V/Pa gives 0.05 V/Pa
within 0.1 % (the frequency within 0.5 Hz), and a 94 dB calibrator through
0.02 FS/Pa gives 0.02 FS/Pa within 10⁻⁵; a silent recording is refused.

The example projects carry records such as `cal-sim-mic-1` (0.0158 FS/Pa) and
`cal-sim-load-cell` (0.0225 V/N), taken from the simulated devices' models:
they calibrate the simulated chain only and are flagged in every report.

## Related topics

[Sensors](sensors.md) · [Uncertainty](uncertainty.md) ·
[Synchronization](synchronization.md) · [Calibration problems](../troubleshooting/calibration.md)
