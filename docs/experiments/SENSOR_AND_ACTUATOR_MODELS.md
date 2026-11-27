# Sensor and Actuator Models

RES-EXP-005, RES-EXP-006.

## Sensors

A sensor is a physical transducer (`Sensor`): id, name, category, domain,
pose (position and sensitive axis in a named frame, with position and
orientation uncertainty), measured quantity, nominal (data-sheet)
sensitivity, dynamic range, noise floor, frequency response note, selected
calibration, uncertainty and status.

Categories and what they measure directly:

| Category | Measures |
|---|---|
| microphone, ultrasonic receiver, pressure probe | pressure |
| accelerometer | acceleration |
| velocity sensor | velocity |
| displacement sensor | displacement |
| strain gauge | strain |
| load cell | force |
| laser vibrometer | velocity, displacement |
| RF receiver channel | full scale, voltage |
| antenna | voltage, power, full scale |
| near-field probe | voltage, electric field, magnetic field |
| field-strength sensor | electric field |
| RF power sensor | power |
| generic voltage | voltage, full scale |

No category measures stress: stress is inferred from strain with a stated
material model (`mechanics::inferStressStep`: uniaxial Hooke's law, the
reason recorded with the result). Validation rejects a sensor that claims to
measure what its category does not.

## Channels and bindings

A sensor is not a device channel. A `ChannelBinding` connects a sensor (or
an actuator) to a device channel, with the conditioning gain between them
(an amplifier, V/V), a channel calibration and whether it is the reference
of a transfer-function measurement. The sensor's data is recorded on the
binding; its pose is in every record.

## Actuators

An actuator (`Actuator`): id, category (speaker, ultrasonic transducer,
shaker, piezo actuator, RF transmitter, antenna element, generic source),
domain, pose, command quantity (what the channel delivers: V, FS, power),
output quantity (pressure, force, field), rated band and command amplitude,
nominal delay, calibration (command to output transfer function).

Commanded output is not physical output. Each run records the commanded
signal of every driven output as a data set of category `command`; a
transfer function from it to a calibrated sensor (`mechanics::shakerCalibrationExperiment`,
`actuatorTransferFunction`) is the actuator's characterization.

## Stimulus

`Stimulus`: sine, exponential (log) sweep, linear chirp, white noise
(portable generator, reproducible), single Hann pulse, RF carrier; peak
amplitude in the command unit, band, duration, duty cycle (one burst),
raised-cosine fades, per-binding delay (a true time shift) and gain (array
steering and focusing).
