# Hardware Integrations

RES-EXP-019, RES-EXP-020. What each driver does, what was tested, and what was not.

## ALSA (driver `alsa`)

| | |
|---|---|
| Hardware | Linux audio interfaces through ALSA PCM devices (`hw:`, `plughw:`, `default`, `pulse`, `null`, ...) |
| Library | libasound (ALSA 1.2.11 here), LGPL-2.1, dynamically linked |
| Build | `pkg-config alsa`; off with qmake `CONFIG+=no_alsa` / CMake `-DRESONANT_WITH_ALSA=OFF` |
| Platform | Linux |
| Discovery | `snd_device_name_hint` (names only; nothing is opened) |
| Capabilities | probed when opened: channels (up to 32), standard rates, 16-bit / 32-bit / float formats |
| Capture | interleaved frames delivered in full-scale units; overruns recovered and the lost frames estimated from the elapsed time (ALSA does not count them), reported as a gap |
| Playback | with a valid authorization, checked for every period; stops at once when it ends |
| Tested | discovery, capability probing, capture of 4800 frames and authorized playback on the `null` device (`HardwareDrivers.AlsaNullDeviceCapturesAndPlaysBackOnlyWithAuthorization`); refusal without authorization |
| Not tested | a physical interface (none was used: tests never open a microphone or play sound), hardware timestamps, rates outside the standard list |

ALSA delivers digital full scale, not volts or pascals: physical units come
from a calibration (a calibrator recording gives FS/Pa).

## SoapySDR (driver `soapysdr`)

| | |
|---|---|
| Hardware | SDRs with a SoapySDR module (RTL-SDR, USRP through UHD, ...) |
| Library | SoapySDR 0.8.1, BSL-1.0, dynamically linked |
| Build | `pkg-config SoapySDR`; off with `CONFIG+=no_soapysdr` / `-DRESONANT_WITH_SOAPYSDR=OFF` |
| Implemented | discovery (`Device::enumerate`), capabilities on open (RX channels, sample rates, tuning range, clock sources, hardware time); module loader messages are collected, not printed |
| Not implemented | streaming and transmission: no SDR was available to test them, so the device refuses acquisition with that reason |
| Tested | discovery runs without failing the registry when no SDR is attached (`HardwareDrivers.SoapySdrDiscoveryReportsWithoutClaimingStreaming`) |

An SDR reports levels relative to its full scale; absolute power or field
strength needs a calibrated chain (`em::RfMeasurementChain`).

## Simulated instruments (driver `simulated`)

Always built. The data is generated from synthetic models, marked
`simulatedDevice`, and labelled in reports. Options are set in the device's
configuration (`DeviceConfiguration.options`).

| Device | Model | Options |
|---|---|---|
| `sim:audio-8x2` | 8 inputs, 2 outputs, 44.1-192 kHz, FS. Outputs drive loudspeakers (2 Pa at 1 m per FS, 80 Hz second-order high-pass) radiating into a free field (1/r, delay r / 343 m/s); inputs 0-6 microphones (0.0158 FS/Pa); input 7 the electrical loopback of output 0 | `speakerPositions`, `micPositions`, `speakerHighPassHz`, `speakerPaPerFsAt1m`, `micFsPerPa`, `speedOfSound`, `gainMismatch[8]`, `channelDelaySamples[8]`, `noiseRms`, `probeInput` (the input that follows the positioner) |
| `sim:daq-4x1` | 4 inputs, 1 output, +-10 V, 1-102.4 kHz, hardware timestamps. Output drives a shaker (10 N/V, 5 Hz high-pass) on a structure with modal accelerances (default 120 Hz zeta 0.02, 410 Hz zeta 0.015); input 0 load cell (22.5 mV/N), inputs 1-3 accelerometers (10.2 mV/(m/s^2)); interlock | `modes` [{frequencyHz, dampingRatio, modalMass, shape[4]}], `shakerNewtonPerVolt`, `loadCellVoltPerNewton`, `accelerometerVoltPerMs2[3]`, `noiseRmsVolt`, `clockDriftPpm`, `interlockClosed`, `requireInterlock` |
| `sim:rf-rx-4` | 4 I/Q channels, 1-20 MS/s, 70 MHz-6 GHz; receives the carriers of `sim:siggen` (path gain -40 dB, -10 dBm full scale), channel phases 0/37/-52/101 deg and gains 0/-0.4/0.3/-0.8 dB by default; coherent LO or independent LOs | `channelPhaseDeg[4]`, `channelGainDb[4]`, `coherent`, `noiseRms`, `pathGainDb`, `elementSpacing`, `sourceAzimuthDeg`, `autPattern` (a cos^1.5 antenna pattern seen from the probe's azimuth) |
| `sim:siggen` | RF source 9 kHz-6 GHz, -120..+20 dBm; its carrier exists only while its authorization is valid | - |
| `sim:vna-2` | 2-port VNA 100 kHz-6 GHz, -40..0 dBm; DUT: series RLC (73 ohm, 1 GHz, Q 10) on port 1, matched line (1.5 dB, 2 ns) between ports; trace noise | `dutResonanceHz`, `dutQ`, `dutResistance`, `lineLossDb`, `lineDelaySeconds`, `traceNoise` |
| `sim:positioner` | moves the probe (+-5 m) | `travel` |

Faults on every streaming device: `dropAfterBlocks` + `dropFrames` (an
overrun gap), `disconnectAfterBlocks`, `errorAfterBlocks`,
`timestampGlitchAfterBlocks` (a 1 ms jump), `realTime` (pace blocks at the
sample rate; otherwise as fast as requested), `seed`.

## Data files

Touchstone 1.1 and 2.0 files of any port count are imported as raw
network-parameter measurements (`em::importTouchstoneMeasurement`), keeping
the reference impedances, frequency unit, number format, version and
comments. CSV frequency responses (mechanics) and calibration records
(JSON) are imported too.

## Licences for distribution

HDF5 (BSD-style), ALSA (LGPL-2.1, dynamic) and SoapySDR (BSL-1.0, dynamic)
are optional; a distribution that includes them ships their shared
libraries and licence texts.
