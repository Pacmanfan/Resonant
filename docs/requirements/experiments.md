# Experimental Hardware, Measurement, Calibration and Simulation Correlation

**Owner of:** `RES-EXP-001..027`
**Source:** REQ-CHAT-20261126-008 (Prompt 12, "Experimental hardware, measurement acquisition, calibration and simulation correlation")
**Refines:** `product.md` (offline operation stays independent of hardware); `mechanics.md` RES-MECH-016 (measured responses);
`electromagnetics.md` RES-EM-022 (Touchstone)
**Decisions:** ADR-000213
**Documents:** `docs/experiments/`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-EXP-001 — One shared, domain-independent experiment framework

Experiments, hardware, acquisition, calibration, measurements, processing,
spatial data, comparison, uncertainty, execution, safety, storage and
reports live in `src/core/experiments`, which includes no physics domain.
The physical interpretation of measurements (sound pressure level,
accelerance and modes, return loss, patterns) lives in the domains, which
use the framework. Instrument drivers live in `src/hardware`, which the core
never includes.
*Verification:* `tools/check_layering.py` (layering suite).

### RES-EXP-002 — The experiment is a serializable definition, separate from its execution

An experiment holds an id, name, description, domain, project, devices and
their configurations, sensors, actuators, channel bindings, acquisition,
stimulus, trigger, synchronization, positions, environment, safety limits,
processing, execution history, results and notes. It is saved in the
project, several per project, created from templates, and validated without
any hardware. Run-time states are not persisted.
*Verification:* `Experiments.DefinitionRoundTripsAndValidatesWithoutHardware`, `ExperimentExamples.*`.

### RES-EXP-003 — A hardware abstraction without vendor dependencies

Drivers, devices, channels, clocks, triggers, configurations, statuses,
errors, acquisition and output sessions, frequency-sweep instruments and
positioners are interfaces in the core. Each device states its
capabilities (channels, ranges, rates, formats, tuning and power ranges,
clocks, triggers, timestamps, shared sample clock, reachable states).
Nothing assumes that devices share timing or sample rates.
*Verification:* `Experiments.SimulatedDevicesBehaveLikeInstruments`.

### RES-EXP-004 — Discovery is optional and never blocks

Device discovery runs on its own thread, reports problems per driver, and
is never needed for offline work; the application starts without it.
Devices report only the states their driver can establish.
*Verification:* `Experiments.SimulatedDevicesBehaveLikeInstruments`, `ExperimentsUiTest.WorkspaceActionsAndDialogs`.

### RES-EXP-005 — Sensors are first-class and are not channels

A sensor has an id, category, domain, pose in a named frame, measured
quantity, nominal sensitivity, range, noise, selected calibration and
uncertainty; a channel binding connects it to a device channel. A sensor
category measures only its quantities: stress is never measured directly
(it is inferred from strain with a stated material model).
*Verification:* `Experiments.DefinitionRoundTripsAndValidatesWithoutHardware`, `ExperimentDomains.StressIsInferredWithAStatedModel`.

### RES-EXP-006 — Actuators are first-class; commanded output is not physical output

An actuator states its command and output quantities, rated band and
amplitude, delay and calibration. A run records the commanded signal of
every driven output as a separate data category ("command"); the transfer
from command to physical output is a calibration or a measurement.
*Verification:* `Experiments.RunnerRefusesOutputWithoutAuthorizationAndRunsWithIt`, `ExperimentExamples.Mechanical`.

### RES-EXP-007 — Asynchronous acquisition that never drops samples silently

A reader thread and a recorder thread exchange blocks through a bounded
queue with backpressure. Frame-counter gaps, device-reported overruns,
timestamp discontinuities, invalid timestamps, clipping, saturation and
device errors are detected and recorded in the data quality; missing frames
are stored as NaN so every sample keeps its time. A display copies a live
view that never delays the recorder.
*Verification:* `Experiments.AcquisitionRecordsEverythingAndReportsFaults`.

### RES-EXP-008 — Timing and synchronization are explicit

Every record names its clock domain, whether timestamps were hardware or
software, and any timing correction applied. Phase-sensitive measurements
across devices need a shared external clock or a timing calibration; equal
nominal sample rates are never taken as coherence. On one device the
shared sample clock is a stated capability.
*Verification:* `Experiments.DefinitionRoundTripsAndValidatesWithoutHardware`, `ExperimentDomains.ArrayChannelCalibrationRecoversTheReceiverErrors`.

### RES-EXP-009 — Calibration records with an append-only history

A calibration record states what was calibrated, how, against what, the
result (scalar, frequency-dependent complex response, delay, phase,
position offset), its frequency and amplitude range, temperature,
uncertainty, validity and source. The library never changes or deletes a
record: a new calibration supersedes the previous one. A frequency-dependent
calibration is applied in the frequency domain and never reduced to a
scalar without a stated justification; it is never extrapolated. An
experiment selects the record that applies.
*Verification:* `Experiments.CalibrationHistoryIsAppendOnlyAndFrequencyDependent`, `Experiments.CalibrationProceduresRecoverKnownValues`.

### RES-EXP-010 — Calibration procedures

Sensitivity from a reference source (acoustic calibrator), frequency
response by comparison with a reference sensor, channel-to-channel gain,
phase and delay, actuator transfer functions and RF array channel
calibration derive records from measurements, with their uncertainty.
*Verification:* `Experiments.CalibrationProceduresRecoverKnownValues`, `ExperimentDomains.CalibratorGivesTheMicrophoneSensitivity`,
`ExperimentDomains.ArrayChannelCalibrationRecoversTheReceiverErrors`.

### RES-EXP-011 — One measurement record for every kind of data

A measurement record holds its identity, experiment, run, binding, sensor,
device and channel, domain, kind (time series, spectrum, power spectrum,
transfer function, network parameters, scalar, vector), category (raw,
calibrated, processed, simulated, reconstructed, command), quantity and
unit, time base, pose, data reference, native format, calibration, sources,
processing history, uncertainty, quality and acquisition settings. Raw
records have no history; derived records name their sources.
*Verification:* `Experiments.ProcessingIsReproducibleAndArchivesReproduceIt`.

### RES-EXP-012 — Explicit, reproducible processing

Processing is a chain of named operations with parameters (segment,
calibrate, spectrum, PSD, transfer function with coherence, impulse
response, filters, decimation, resampling, correlation, delay, RMS, tone,
average, frequency-domain integration, scale with a stated reason). Each
result records every parameter it used; nothing is windowed, filtered,
normalized or converted unless a step asks for it. The same sources and
step always give the same derived id and values.
*Verification:* `ExperimentDsp.*`, `Experiments.ProcessingIsReproducibleAndArchivesReproduceIt`.

### RES-EXP-013 — Spatial measurements and honest reconstruction

Positions come from fixed sensors, imported lists, linear, planar,
volumetric and angular scans and probe arrays. A reconstruction marks every
node as measured, interpolated (within a stated radius) or empty; nothing
is extrapolated. Sparse measurements are not presented as a measured field.
*Verification:* `Experiments.ScansAndReconstructionMarkWhatWasMeasured`.

### RES-EXP-014 — Comparison of measurement and simulation

Only compatible quantities are compared; an uncalibrated reading is compared
in shape only and only on request. Frequency series are compared over the
overlap of their ranges (no extrapolation); spatial data at the measured
positions in the simulation's frame. Metrics: RMS and maximum level error,
phase error, normalized mean square error, correlation, peak frequencies,
the fraction within the measurement uncertainty; domain metrics (focus
location, width and sidelobes, mode pairing and MAC, S11 error, pattern
difference, beam pointing, beamwidth).
*Verification:* `Experiments.ComparisonRefusesIncompatibleQuantitiesAndComparesOnlyTheOverlap`, `ExperimentDomains.*`.

### RES-EXP-015 — Uncertainty is carried, not discarded

Uncertainty budgets list components by source and type (instrument
specification, calibration, statistical, model, numerical simulation),
combine them by root-sum-square under a stated assumption, expand them with
a coverage factor, and are propagated into calibrated and processed data.
*Verification:* `Experiments.UncertaintyBudgetsCombineByRootSumSquare`.

### RES-EXP-016 — The execution engine

A run validates, verifies devices, capabilities, calibrations and
synchronization, checks safety and authorization, configures, arms,
executes, records, verifies integrity, processes and reports, through the
states Draft, Validating, Ready, Armed, Running, Stopping, Completed, Failed
and Cancelled. A failed check stops the run before anything is output; a
device error ends it without processing; cancellation stops outputs first
and keeps the recorded data, marked incomplete. Runs are background jobs.
*Verification:* `Experiments.RunnerRefusesOutputWithoutAuthorizationAndRunsWithIt`, `Experiments.CancellationAndEmergencyStopEndRunsCleanly`.

### RES-EXP-017 — Physical output needs an explicit authorization

No output (speaker, transducer, shaker, actuator, RF transmitter, network
analyser test port) starts without an authorization granted by a named
operator with an acknowledgement, for one experiment, within its safety
limits (amplitude, band, duty cycle, duration, channels, interlock,
transmit power). It covers one run, expires unused, and is never saved:
opening a project or loading an experiment authorizes nothing. A changed
experiment needs a new authorization. The emergency stop is latched, stops
every output at its next block and revokes every authorization. Acoustic,
mechanical and RF warnings are shown; a software limit is not called a
hardware interlock or regulatory compliance.
*Verification:* `Experiments.OutputNeedsAnExplicitAuthorizationWithinTheLimits`, `Experiments.RunnerRefusesOutputWithoutAuthorizationAndRunsWithIt`,
`ExperimentsUiTest.OpeningAProjectAuthorizesNothingAndOutputsNeedAuthorization`, `ExperimentsUiTest.EmergencyStopEndsARunningExperiment`.

### RES-EXP-018 — Storage, provenance and archives

Measurement data is stored next to the project (HDF5 when built with it,
else a binary format), created exclusively and made read-only when
finished; the project holds the records. Archives export an experiment with
its calibrations, records and data; importing one and re-running its
processing reproduces the derived data without the hardware.
*Verification:* `Experiments.AcquisitionRecordsEverythingAndReportsFaults`, `Experiments.BinaryStoreKeepsTheSameGuarantees`,
`Experiments.ProjectsKeepExperimentsButNeverAuthorizations`, `Experiments.ProcessingIsReproducibleAndArchivesReproduceIt`.

### RES-EXP-019 — Simulated instruments behave like instruments

Simulated devices (audio interface with a free-field acoustic path, vibration
DAQ with a modal structure and a shaker, coherent RF receiver, RF signal
generator, two-port VNA, positioner) stream on a frame clock, need
configuring, refuse output without authorization and inject noise, gain
mismatch, timing and phase offsets, clipping, dropped samples, timestamp
glitches, disconnection and acquisition errors on request. Their data is
marked as generated, not measured.
*Verification:* `Experiments.SimulatedDevicesBehaveLikeInstruments`, `Experiments.AcquisitionRecordsEverythingAndReportsFaults`, `ExperimentDomains.*`.

### RES-EXP-020 — Real hardware integrations, stated as tested

ALSA capture and playback on Linux audio interfaces (tested on the ALSA
"null" device: no microphone and no loudspeaker are used by tests) and
SoapySDR discovery and capability inspection (streaming not implemented)
are built when their libraries are found, and are optional. Each
integration documents its hardware, dependencies, licence and tested and
untested operations.
*Verification:* `HardwareDrivers.*`.

### RES-EXP-021 — Acoustic workflows

Speaker characterization, field mapping, ultrasonic element
characterization and focusing validation; sound pressure and its level,
microphone calibration with a calibrator, the free-field prediction at
measured positions, focus metrics.
*Verification:* `ExperimentDomains.SpeakerCharacterizationMeasuresTheSimulatedChain`, `ExperimentDomains.FieldMappingAgreesWithTheAcousticModelInShape`,
`ExperimentDomains.FocusingPutsTheMaximumAtTheTarget`.

### RES-EXP-022 — Mechanical workflows

Modal testing (accelerance FRFs, resonances identified by peak picking and
half-power bandwidth, operating shapes, MAC, pairing with simulated modes),
vibration characterization, shaker calibration, frequency-domain
integration, stress inferred from strain.
*Verification:* `ExperimentDomains.ModalTestIdentifiesTheSimulatedModes`, `ExperimentDomains.StressIsInferredWithAStatedModel`.

### RES-EXP-023 — RF workflows and N-port Touchstone

Antenna impedance with a VNA (return loss, VSWR, impedance, comparison with
the simulated impedance), array channel calibration applied to array
excitations, radiation patterns from angular scans; Touchstone 1.1 and 2.0
files of any port count, keeping reference impedances, units, format, port
order and comments; absolute received power and field strength only with a
calibrated measurement chain.
*Verification:* `ExperimentDomains.TouchstoneFilesOfAnyPortCount`, `ExperimentDomains.AntennaImpedanceMeasurementOnTheSimulatedVna`,
`ExperimentDomains.ArrayChannelCalibrationRecoversTheReceiverErrors`, `ExperimentDomains.RadiationPatternFromAnAngularScan`.

### RES-EXP-024 — Reports

A run produces a self-contained HTML report (description, domain, hardware
and configuration, sensors and actuators, calibrations, acquisition and
timing, environment, execution, processing, results with categories and
uncertainty, comparisons, warnings and limitations) that labels raw,
calibrated, processed, simulated and reconstructed data and data from
simulated devices.
*Verification:* `Experiments.RunnerRefusesOutputWithoutAuthorizationAndRunsWithIt`.

### RES-EXP-025 — The Experimental Hardware workspace

A dock with devices, experiments, calibrations, the run (safety, explicit
authorization, run, stop, emergency stop, progress, live display) and
measurements (data, comparison, reports, archives); an Experiments menu with
the emergency stop; sensors, actuators, scan positions and measured points
in the viewport; `--run-experiment` and `--authorize-output` on the command
line.
*Verification:* `ExperimentsUiTest.*`.

### RES-EXP-026 — Reliability

Device communication, acquisition, buffering, recording, processing and
display are separated; buffer occupancy, rates, dropped samples and disk
throughput are monitored; outputs stop on loss of control; shutdown and
disconnection close recordings cleanly.
*Verification:* `Experiments.AcquisitionRecordsEverythingAndReportsFaults`, `Experiments.CancellationAndEmergencyStopEndRunsCleanly`.

### RES-EXP-027 — Examples and documentation

Example projects for acoustic, mechanical and RF experiments on the
simulated devices (each validates and completes), a labelled synthetic
two-port Touchstone file, and the documents of `docs/experiments/`.
*Verification:* `ExperimentExamples.*`; `scripts/doc_integrity.py`.
