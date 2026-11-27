# Experiment Validation

Evidence for RES-EXP-001..027. Every row is an automated test of the `core`
suite (`tests/core/test_experiments.cpp`, `test_experiment_dsp.cpp`,
`test_experiment_domains.cpp`, `test_experiment_examples.cpp`,
`test_hardware_drivers.cpp`) or the `app` suite
(`tests/app/test_experiments_ui.cpp`); tolerances are the ones the tests
assert. Results on simulated devices verify the measurement chain and the
procedures against the models the devices were given; they are not
measurements of anything physical.

## Signal processing

| Check | Reference | Tolerance |
|---|---|---|
| Hann coherent gain, noise bandwidth; rectangular, Blackman-Harris, flat top | 0.5, 1.5 bins; 1, 2.00, 3.77 bins | 1e-9, 1e-3, 0.01-0.02 |
| Welch density of white noise integrates to the variance | Parseval | 2 % |
| Power of a sinusoid of amplitude 2 | A^2/2 = 2 | 1 % |
| H1 of a known resonator (300 Hz, zeta 0.05) with output noise, 20-1000 Hz | exact discrete response | 0.3 dB, 0.03 rad; coherence > 0.98 |
| Coherence of unrelated signals | ~1/n_d | mean < 0.05 |
| Impulse response of a 5-sample delay | peak at 5, height 1 | exact index, 1 % |
| Tone amplitude and phase | 0.3, 0.7 rad | 1e-6 |
| Delay of 37.3 samples in band-limited noise | 37.3 | 0.02 samples |
| FIR low-pass: pass band (phase kept with delay compensation), stop band | input / 0 | 2e-3 / 1e-4 RMS |
| Decimation and resampling keep a 500 Hz tone | amplitude 1 | 5e-3, 3e-3 |

## Framework

| Check | Result |
|---|---|
| Experiment JSON round trip; validation without hardware | identical; errors for stress from an accelerometer, a stimulus at Nyquist, an unknown output, phase comparison across devices without synchronization |
| Stimulus: amplitude, delay of 10 samples and gain 2, reproducible noise, invalid sweep | exact |
| Uncertainty RSS, expansion, per type, JSON | 1e-12 |
| Calibration library: append-only, supersession, history, latest, interpolation, no extrapolation, no scalar for a frequency-dependent record, expiry | as specified |
| Sensitivity from a 1003 Hz calibrator tone at 1 Pa through 0.05 V/Pa | 0.05 V/Pa within 0.1 %, 1003 Hz within 0.5 Hz; a silent recording is refused |
| Channel to channel: gain 0.5, delay 3 samples | 1e-3, 0.05 samples, 1e-3 rad |
| Safety: limits, operator and acknowledgement required, single use, release, expiry, latched emergency stop, RF power limit and warning | as specified |
| Simulated devices: discovery (also asynchronous), single connection, configuration checks, output refused with a forged authorization | as specified |
| Acquisition: 8192 frames on two channels, stored data equal to recorded, raw data not writable again | exact |
| Injected overrun of 300 frames, timestamp jump | 300 frames in 1 gap as NaN; >= 1 discontinuity |
| Injected disconnection | failed, data kept and marked incomplete (4 x 512 frames), device in Error |
| Runner: output refused without authorization (stops at step 6, nothing recorded); with it the states pass Validating..Armed..Running..Completed; command recorded; report written; authorization spent; changed experiment refused | as specified |
| Cancellation and emergency stop during a real-time run | Cancelled; data incomplete but readable; device not transmitting |
| Project round trip of experiments, calibrations, records; no authorization in the file; no key without experiments | identical |
| Scans (planar 12, angular 12 with 360 = 0, CSV import), reconstruction coverage (measured / interpolated value 1.5 / empty) | exact |
| Comparison: overlap only (10 of 11 points), 1 dB and 0.1 rad errors, incompatible quantities refused, relative comparison only on request | 1e-9 |
| Processing determinism, reproduction from the store and from an exported, imported archive; merge refuses duplicates | ids equal, values 1e-9 |

## Domain workflows (simulated devices)

| Check | Model value | Tolerance / result |
|---|---|---|
| Speaker characterization H1 at 1 and 4 kHz | 2 Pa/FS x 0.0158 FS/Pa / 1 m | 0.3 dB, coherence > 0.99 |
| Calibrated response vs free-field prediction above 1 kHz | ideal flat source | RMS 0.3 dB, 10 deg; at 100 Hz the loudspeaker's high-pass shows (> 1.2 dB) |
| Calibrator 94 dB at 1 kHz through 0.02 FS/Pa | 0.02 FS/Pa | 1e-5; level uncertainty propagated |
| Field mapping vs free-field point source (in shape) | - | RMS 0.2 dB, 3 deg; the uncalibrated comparison is refused unless relative |
| Focusing with two delayed loudspeakers | target 0.2 m | nearest microphone; predicted focus at the same position |
| Modal test: resonances | 120 Hz (zeta 0.02), 410 Hz | 2 Hz; damping 0.006; distinct operating shapes (MAC < 0.9); pairing within 3 % |
| Accelerance integrated twice | m/N | unit |
| Stress from strain | E epsilon | exact, reason recorded |
| Touchstone 2-port order, 2.0 with per-port references and 12_21 order, 3-port on several lines, round trips, refusals | exact | 1e-11 |
| VNA S11 of the RLC DUT | best at 1 GHz, return loss -20 log10(23/123) = 14.56 dB, VSWR 1.46 | 3 MHz, 0.2 dB, 0.02; vs the same RLC model RMS < 0.1 dB |
| Array channel calibration | 0/37/-52/101 deg, 0/-0.4/0.3/-0.8 dB | 0.5 deg, 0.05 dB; excitation pre-compensated |
| Radiation pattern from a 72-angle scan | cos^1.5: peak 0 deg, beamwidth 74.9 deg | one step (5 deg), 2 deg |
| RF chain: power and field strength only when calibrated | - | as specified |

## Hardware drivers and the application

| Check | Result |
|---|---|
| ALSA `null`: capture 4800 frames of silence; playback refused without, accepted with, an authorization | as specified |
| SoapySDR discovery without devices; no streaming claimed | as specified |
| Example projects (acoustic 4, mechanical 3, RF 3 experiments) validate and complete; the Touchstone example imports | all |
| GUI: discovery in the background; the authorization dialog; opening a project authorizes nothing; run refused without authorization, then run as a job; data next to the project; markers; comparison; report; save and reopen | as specified |
| GUI: emergency stop during a run; latched until reset | as specified |
| GUI: templates (undo / redo), Touchstone import, comparison refused without an antenna | as specified |
