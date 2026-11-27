# Calibration Framework

RES-EXP-009, RES-EXP-010.

## Records

`CalibrationRecord`: id, the record it supersedes and its version; type
(amplitude, phase, timing, frequency, position, orientation, sensor
sensitivity, actuator transfer function, channel-to-channel); device,
channel, sensor or actuator; date, procedure, reference instrument,
operator, source; results - a scalar in a stated unit, a frequency-dependent
complex response (frequency, value, relative uncertainty per point), the
justification when a scalar stands for a frequency-dependent quantity, a
delay, a phase at a reference frequency, a position offset, a frequency
error; frequency and amplitude range; temperature (recorded or not);
validity; files; uncertainty budget; whether it was derived from a
simulated device.

## The library

`CalibrationLibrary` is append-only: `add` refuses an id that exists and a
second record superseding the same one; a new calibration is a new record
whose version follows the one it supersedes. `history(id)` gives the chain,
`latest(type, sensor)` the newest record not superseded. The experiment
selects the record each sensor uses (`Sensor::calibrationId`); nothing is
chosen automatically.

## Applying

Sensitivities are raw unit per physical unit (V/Pa, V/(m/s^2), FS/Pa, ...).

- Time series: divided by the binding's conditioning gain and the scalar
  sensitivity; refused for a frequency-dependent record without a stated
  scalar justification.
- Spectra and transfer functions: divided at each frequency by the complex
  sensitivity, interpolated linearly in magnitude and in unwrapped phase;
  frequencies outside the record fail (no extrapolation).
- `checkCalibration`: an expired record, a record with no value, or a table
  that does not cover the band is unusable (the run fails when calibration is
  required, otherwise the data stays uncalibrated); a scalar made at one
  frequency and applied across a band is a warning; a record from a
  simulated device is always flagged.

The calibrated result's unit follows the sensor's quantity; its uncertainty
combines the data's and the calibration's.

## Procedures

| Procedure | Function | Result |
|---|---|---|
| reference source (e.g. 94 dB, 1 kHz calibrator) | `sensitivityFromReference`, `acoustics::calibratorSensitivity` | scalar at the source's frequency (found within +-2 %), flat-top amplitude; noise component |
| comparison with a reference sensor | `responseByComparison` | frequency-dependent response from H1, bins below a coherence threshold left out |
| channel to channel | `channelToChannel` | gain ratio and phase at a frequency, delay (correlation peak plus cross-spectrum phase slope) |
| actuator | `actuatorTransferFunction` | command to physical output response and bulk delay |
| ultrasonic array elements | `acoustics::elementCalibration` | per-element amplitude and phase, propagation phase removed |
| RF array channels | `em::arrayChannelCalibration`, applied by `em::applyArrayCalibration` | per-channel complex gain; excitation pre-compensation |
