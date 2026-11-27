# Hardware Abstraction

RES-EXP-003, RES-EXP-004. `src/core/experiments/Hardware.h`.

| Concept | Type |
|---|---|
| HardwareDevice | `HardwareDevice`: info, capabilities, status, configure, start acquisition / output, errors, disconnect |
| HardwareInterface / DeviceDriver | `DeviceDriver`: name, version, connection, availability (and licence), discover, open |
| DeviceCapabilities | `DeviceCapabilities`: input and output channels (quantity, range, complex), rates (discrete or a range), formats, tuning range, output power range, clocks, triggers, hardware timestamps, shared sample clock, frequency sweep, positioner, interlock, modes, reachable states |
| InputChannel / OutputChannel | `ChannelInfo` |
| ClockSource / TriggerSource | `ClockSource`, `TriggerSource` |
| DeviceConfiguration | `DeviceConfiguration`: rate, format, input / output channels (block order), clock, trigger, centre frequency, gain, block length, driver options |
| DeviceStatus | Disconnected, Discovering, Connecting, Connected, Configured, Acquiring, Transmitting, Error - a device lists the states it can reach |
| DeviceError | code, message, safety relevance, time |
| AcquisitionSession | blocks of samples (`SampleBlock`: frame counter, frames x channels, real or I/Q, hardware / software timestamps, frames lost before, end of stream) |
| OutputSession | running, stop, frames written; started only with a valid authorization |
| FrequencySweepInstrument | a VNA sweep (an output: needs an authorization) |
| Positioner | moves a probe; the run moves it through the scan |

## Discovery and connections

`DeviceRegistry` holds the drivers; `discover()` asks each available driver
(problems are collected, never fatal); `discoverAsync()` does it on its own
thread - the application never waits for it. `connect(id)` opens a device
once; the run discovers by itself when a device it needs is unknown. A
missing device never affects offline work: it fails the run that needs it,
at step 2, with the reason.

## Rules

- Readings are delivered in the channel's raw quantity: volts for a DAQ,
  full scale (FS) for an audio interface or a receiver. Physical units come
  only from a calibration.
- Nothing assumes devices share a clock, a rate or a trigger
  (`ACQUISITION_AND_SYNCHRONIZATION.md`).
- No vendor SDK is visible to the core or the application: drivers are in
  `src/hardware`.
