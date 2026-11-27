# Device Driver Framework

RES-EXP-005 (the framework), RES-EXP-019 (simulated drivers), RES-EXP-020 (real drivers).

## Writing a driver

1. Implement `DeviceDriver` (in `src/hardware` for a real instrument API):
   `availability()` says whether its library is usable and its licence;
   `discover()` lists `DeviceInfo` without opening anything slow;
   `open()` returns a `HardwareDevice`.
2. Implement `HardwareDevice`: report honest `capabilities()` (only what was
   verified on the device), check every configuration, deliver blocks with a
   monotonic frame counter (a jump is a reported gap) and timestamps, report
   overruns in `droppedBefore` when the device knows the count, and fail
   `read()` on disconnection.
3. Outputs: check `OutputAuthority::instance().isValid(authorization)` before
   starting and for every block; stop at once when it fails (released,
   revoked, emergency stop).
4. Register it in `hardware::registerHardwareDrivers` behind a build switch
   (`hardware.pri` / `CMakeLists.txt`, `RESONANT_WITH_...`), and list it in
   `driverBuildInfo()`.
5. Document it in `HARDWARE_INTEGRATIONS.md`: hardware, dependencies,
   licence, tested and untested operations. A device is not "supported"
   because a protocol exists.

## Built-in drivers

| Driver | Library | In this version |
|---|---|---|
| simulated | part of the core | six simulated instruments (below) |
| alsa | libasound (LGPL-2.1) | capture and playback |
| soapysdr | SoapySDR (BSL-1.0) | discovery and capabilities only |

The simulated instruments are drivers like any other; their models and
options are in `HARDWARE_INTEGRATIONS.md`.

## Investigated, not implemented

PortAudio (ALSA covers Linux audio interfaces directly), UHD and libiio
(no device to test; SoapySDR modules exist for both), SCPI / VISA
(no instrument to test; the abstraction admits them as network or USB
drivers), generic serial. Each can be added as a driver without changes to
the framework.
