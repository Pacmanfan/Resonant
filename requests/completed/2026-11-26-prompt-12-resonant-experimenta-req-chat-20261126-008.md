# Request: # PROMPT 12 — RESONANT: EXPERIMENTAL HARDWARE, MEASUREMENT ACQUISITION, CALIBRAT

**Request ID:** REQ-CHAT-20261126-008
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T15:47:24-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 12 — RESONANT: EXPERIMENTAL HARDWARE, MEASUREMENT ACQUISITION, CALIBRATION AND SIMULATION CORRELATION

You are continuing development of **RESONANT — Wave Field Engineering Studio**.

This milestone introduces the unified experimental measurement and hardware integration architecture.

Resonant must evolve beyond offline simulation. It should eventually be capable of controlling physical equipment, acquiring measurements, calibrating hardware, reconstructing real-world wave fields, and comparing measured behavior against numerical predictions.

This must be a shared experimental framework serving all three existing physics domains:

1. Acoustic wave engineering.
2. Mechanical wave engineering.
3. Electromagnetic wave engineering.

Do not create three unrelated hardware frameworks.

Instead, establish a common experiment infrastructure with domain-specific instrument drivers, sensors, actuators, signal-processing pipelines, calibration models, and measurement interpretation.

The architecture must remain hardware-independent. Users must be able to design and simulate experiments without owning or connecting any physical equipment.

Do not make hardware connectivity mandatory for normal application operation.

---

## 1. Mission and objectives

Implement a unified experimental system supporting:

- Experiment definition and management.
- Hardware discovery and device registration.
- Instrument configuration.
- Sensor definitions.
- Actuator definitions.
- Measurement acquisition.
- Signal recording.
- Channel synchronization.
- Hardware calibration.
- Sensor calibration.
- Actuator characterization.
- Measurement preprocessing.
- Signal analysis.
- Spatial field reconstruction.
- Simulation-to-measurement comparison.
- Experimental uncertainty.
- Measurement provenance.
- Repeatable experiment execution.
- Hardware-independent offline testing.

The architecture must eventually support:

**Acoustic**
- Microphones.
- Microphone arrays.
- Ultrasonic microphones and receivers.
- Acoustic pressure probes.
- Speakers.
- Ultrasonic transducers.
- Multichannel audio interfaces.
- Signal generators and amplifiers.

**Mechanical**
- Accelerometers.
- Velocity sensors.
- Displacement sensors.
- Strain gauges.
- Load cells.
- Laser vibrometers.
- Mechanical shakers.
- Piezoelectric actuators.
- Multichannel data acquisition systems.

**Electromagnetic**
- Software-defined radios.
- Vector network analyzers.
- Spectrum analyzers.
- Signal generators.
- RF power meters.
- Antenna measurement systems.
- Near-field probes.
- RF field-strength sensors.
- Multichannel coherent RF receivers and transmitters.

Do not attempt to implement every device driver in this milestone.

Establish the architecture, implement a small number of useful integrations where feasible, and provide simulated hardware for development and testing.

## 2. Unified experiment architecture

Introduce a first-class **Experiment** object.

An experiment should contain:

- Stable experiment ID.
- Name.
- Description.
- Physics domain.
- Project association.
- Experiment configuration.
- Hardware configuration.
- Sensors.
- Actuators.
- Measurement channels.
- Calibration references.
- Acquisition settings.
- Trigger configuration.
- Synchronization configuration.
- Spatial measurement positions.
- Environmental conditions.
- Safety limits.
- Execution history.
- Result references.
- Notes and annotations.

An experiment must be serializable and reproducible.

Support multiple experiments inside one project.

Support experiment templates for common workflows.

Separate experiment definition from experiment execution.

A user should be able to create, configure, save, inspect, and validate an experiment without connected hardware.

## 3. Hardware abstraction layer

Implement a hardware abstraction layer independent of Qt widgets and individual physics domains.

Define common concepts:

- HardwareDevice.
- HardwareInterface.
- DeviceDriver.
- DeviceCapabilities.
- InputChannel.
- OutputChannel.
- ClockSource.
- TriggerSource.
- DeviceConfiguration.
- DeviceStatus.
- DeviceError.
- AcquisitionSession.
- OutputSession.

Each hardware device must expose its capabilities.

Examples:

- Number of input channels.
- Number of output channels.
- Sample rates.
- Supported frequencies.
- Resolution.
- Input ranges.
- Output ranges.
- Clock capabilities.
- Trigger capabilities.
- Synchronization capabilities.
- Supported operating modes.

Do not assume all devices support the same timing, sample rate, or synchronization model.

Keep hardware drivers behind stable interfaces.

The application must not depend directly on a specific vendor SDK.

## 4. Device discovery and management

Create a hardware management interface.

Support:

- Device discovery.
- Device identification.
- Device registration.
- Device configuration.
- Device status.
- Connection status.
- Capability inspection.
- Device disconnection.
- Error reporting.
- Driver version reporting.

Support different connection types:

- USB.
- Ethernet.
- PCIe.
- Serial.
- Network APIs.
- Vendor-specific SDKs.

Device discovery must be optional and must not block application startup.

Do not allow a missing hardware device to prevent offline simulation.

Implement clear device status states:

- Disconnected.
- Discovering.
- Connecting.
- Connected.
- Configured.
- Acquiring.
- Transmitting.
- Error.

Only expose states that the device driver can actually establish.

## 5. Instrument driver architecture

Create a plugin-friendly instrument driver framework.

Each driver should implement the relevant subset of supported capabilities.

Investigate appropriate integration options:

- VISA-compatible instruments.
- SCPI.
- USB device APIs.
- Vendor SDKs.
- ALSA and PortAudio for audio.
- SoapySDR for software-defined radios.
- UHD where applicable.
- libiio where applicable.
- Network-based instrument protocols.
- Generic serial interfaces.

Do not assume a single protocol covers every instrument.

For each proposed integration, inspect:

- Licensing.
- Platform support.
- API stability.
- Device compatibility.
- Dependency availability.
- Redistribution restrictions.

Implement a mock instrument driver for automated testing.

Do not introduce unnecessary vendor dependencies into the core application.

## 6. Sensor architecture

Introduce a first-class **Sensor** object.

A sensor should contain:

- Stable ID.
- Sensor category.
- Physics domain.
- Position.
- Orientation.
- Coordinate frame.
- Measurement type.
- Measurement units.
- Sensitivity.
- Frequency response.
- Dynamic range.
- Noise characteristics.
- Calibration reference.
- Uncertainty.
- Connected device.
- Input channel.
- Status.

Support point sensors and spatially distributed sensor arrays.

Distinguish a physical sensor from the instrument channel that acquires its output.

For example, a microphone and its audio interface input are separate objects.

Support sensor positioning within the scene.

Allow measured values to be associated with exact spatial coordinates and coordinate frames.

## 7. Actuator architecture

Introduce a first-class **Actuator** object.

Support:

- Physical speakers.
- Ultrasonic transducers.
- Mechanical shakers.
- Piezoelectric actuators.
- RF transmitters.
- Antenna elements.
- Generic controlled sources.

An actuator should contain:

- Stable ID.
- Physics domain.
- Position.
- Orientation.
- Output type.
- Operating range.
- Frequency response.
- Amplitude limits.
- Phase response.
- Delay.
- Calibration.
- Connected hardware.
- Output channel.
- Safety limits.

Distinguish commanded output from actual physical output.

An output command does not guarantee that the physical actuator produces the requested field.

Model the transfer characteristics between command signal and physical response.

## 8. Acquisition and recording engine

Implement a dedicated measurement acquisition subsystem.

Support:

- Single-channel acquisition.
- Multichannel acquisition.
- Continuous recording.
- Triggered recording.
- Finite-duration acquisition.
- Frequency sweeps.
- Repeated acquisitions.
- Segmented acquisition where supported.
- Streaming acquisition.
- Configurable buffering.
- Data recording to disk.

Each acquisition must retain:

- Device identity.
- Channel identity.
- Sample rate.
- Sample format.
- Time reference.
- Start time.
- Duration.
- Trigger information.
- Calibration state.
- Measurement units.
- Data quality metadata.

Support common sample formats, including appropriate integer and floating-point representations.

Do not silently discard samples.

Detect and report:

- Buffer overruns.
- Dropped samples.
- Clock discontinuities.
- Invalid timestamps.
- Clipping.
- Saturation.
- Device errors.

Acquisition must run asynchronously without blocking the GUI.

## 9. Timing and synchronization

Synchronization is a critical requirement.

Support several timing models:

- Independent device clocks.
- Shared reference clocks.
- Shared trigger signals.
- Hardware timestamps.
- Software timestamps.
- Synchronized sample clocks.
- External PPS or equivalent timing references where appropriate.

Represent clock domains explicitly.

Track:

- Sample-clock frequency.
- Clock source.
- Reference frequency.
- Trigger source.
- Time offset.
- Clock drift.
- Synchronization uncertainty.

Do not claim coherent acquisition simply because multiple devices report the same nominal sample rate.

For phase-sensitive experiments, require a suitable synchronization and calibration strategy.

Support per-channel timing correction.

For RF and phased-array experiments, prepare for shared frequency references, coherent local oscillators, and phase calibration.

For acoustic and mechanical experiments, support synchronized multichannel acquisition and output.

## 10. Calibration framework

Implement a unified calibration system.

A calibration record should include:

- Calibration ID.
- Device.
- Channel.
- Sensor or actuator.
- Calibration type.
- Date.
- Calibration procedure.
- Reference instrument.
- Calibration coefficients.
- Frequency range.
- Amplitude range.
- Temperature conditions.
- Uncertainty.
- Validity period.
- Operator or source metadata.
- Calibration file references.

Support:

- Amplitude calibration.
- Phase calibration.
- Timing calibration.
- Frequency calibration.
- Position calibration.
- Orientation calibration.
- Sensor sensitivity calibration.
- Actuator transfer-function calibration.
- Channel-to-channel calibration.

Allow calibration data to be frequency-dependent.

Do not reduce a complex frequency-dependent calibration into one scalar value unless that approximation is explicitly justified.

Maintain calibration history.

Never overwrite an old calibration record when a new calibration is performed.

Allow the user to select which calibration applies to a particular experiment.

## 11. Acoustic experimental workflows

Implement acoustic-specific measurement and output models.

Support:

- Microphone sensitivity.
- Acoustic pressure.
- Sound pressure level.
- Frequency response.
- Phase response.
- Microphone position and orientation.
- Speaker output response.
- Ultrasonic transducer response.
- Multichannel microphone acquisition.

Support acoustic measurements such as:

- Time-domain pressure.
- RMS pressure.
- Frequency spectrum.
- Phase.
- Transfer functions.
- Impulse response.
- Frequency response.
- Spatial pressure distributions.

Support acoustic experiment templates:

**A. Speaker characterization**
- Sweep a speaker.
- Record microphone response.
- Calculate frequency response.
- Compare against the expected response.

**B. Acoustic field mapping**
- Move or select microphones at spatial coordinates.
- Record pressure and phase.
- Construct a spatial measurement map.

**C. Ultrasonic array calibration**
- Characterize each transducer.
- Measure amplitude and phase.
- Determine channel timing.
- Store per-element calibration.

**D. Acoustic focusing validation**
- Configure a simulated target focus.
- Execute a supported physical output experiment.
- Measure pressure at target and comparison locations.
- Compare predicted and measured results.

Clearly distinguish acoustic pressure measurements from calculated intensity or power.

## 12. Mechanical experimental workflows

Implement mechanical-specific measurement and output models.

Support:

- Accelerometer sensitivity.
- Displacement sensor calibration.
- Velocity sensor calibration.
- Strain measurements.
- Force measurements.
- Load-cell calibration.
- Sensor orientation.
- Mechanical actuator response.

Support measured quantities:

- Displacement.
- Velocity.
- Acceleration.
- Force.
- Stress where appropriately inferred.
- Strain.
- Frequency response.
- Phase.
- Resonance.
- Damping.

Do not imply that stress can be directly measured by an ordinary accelerometer.

Support mechanical experiment templates:

**A. Structural modal testing**
- Excite a structure.
- Record synchronized responses.
- Calculate frequency response functions.
- Identify resonant modes where supported.

**B. Vibration characterization**
- Apply a controlled excitation.
- Measure acceleration or displacement.
- Compare against simulation.

**C. Mechanical actuator calibration**
- Characterize actuator output.
- Determine frequency response and phase.
- Store calibration.

**D. Structural vibration mapping**
- Associate measurements with physical positions.
- Construct spatial mode or response visualizations.

## 13. Electromagnetic experimental workflows

Implement RF-specific measurement models.

Support:

- Complex RF samples.
- I/Q data.
- Frequency sweeps.
- S-parameters.
- Spectrum measurements.
- RF power.
- Antenna patterns.
- Near-field measurements.
- Phase measurements.
- Channel calibration.

Support importing Touchstone files:

- .s1p
- .s2p
- .s3p
- .s4p
- Higher-port formats where supported.

Preserve:
- Reference impedance.
- Frequency units.
- Magnitude/phase or real/imaginary representation.
- Port ordering.
- File metadata.

Prepare for SDR integration using appropriate hardware abstraction.

Support RF experiment templates:

**A. Antenna impedance measurement**
- Import or acquire S11.
- Display impedance.
- Calculate return loss.
- Calculate VSWR.
- Compare against simulated antenna results.

**B. Antenna array channel calibration**
- Acquire per-element measurements.
- Determine relative phase and amplitude.
- Store calibration.
- Apply calibration to array excitation calculations.

**C. RF field mapping**
- Associate probe measurements with spatial coordinates.
- Display measured field distributions.

**D. Antenna radiation-pattern experiment**
- Record angular measurements.
- Associate samples with antenna orientation.
- Construct measured radiation patterns.
- Compare against simulation.

Do not assume an SDR provides calibrated absolute field strength.

Account for receiver gain, antenna factor, cable loss, frequency response, and other relevant measurement-chain characteristics.

## 14. Measurement data model

Introduce a unified measurement representation.

A measurement must include:

- Stable ID.
- Experiment ID.
- Measurement channel.
- Sensor ID.
- Physics domain.
- Quantity.
- Units.
- Timestamp or time base.
- Spatial coordinates where applicable.
- Coordinate frame.
- Sample data reference.
- Calibration reference.
- Uncertainty.
- Quality indicators.
- Acquisition configuration.

Support both:

- Time-series data.
- Frequency-domain data.
- Complex-valued measurements.
- Spatial measurement datasets.
- Scalar measurements.
- Vector measurements.

Do not force all measurements into one scalar time-series abstraction.

Preserve original raw data.

Derived and processed measurements must retain links to their source data and processing history.

## 15. Signal processing pipeline

Create a reusable signal-processing framework.

Support:

- Windowing.
- FFT.
- Spectral estimation.
- Averaging.
- Filtering.
- Decimation.
- Resampling.
- Cross-correlation.
- Autocorrelation.
- Transfer-function estimation.
- Phase extraction.
- Coherence estimation.
- Impulse-response estimation.

Use appropriate algorithms for each measurement type.

Keep the raw acquisition immutable.

Store processing parameters and derived results separately.

Support processing chains that can be reproduced.

Distinguish calibrated measurements from raw instrument readings.

Avoid silently applying windowing, normalization, filtering, or unit conversions.

## 16. Spatial measurement and field reconstruction

Introduce a measurement-position system.

Support:

- Fixed sensor locations.
- Manually entered coordinates.
- Imported coordinate lists.
- Linear measurement scans.
- Planar scans.
- Volumetric scans.
- Angular measurement sweeps.
- Probe arrays.

Associate each measurement with its actual position and orientation.

Implement spatial field reconstruction appropriate to the available data.

Support:

- Point-cloud visualization.
- Measured field slices.
- Interpolation where justified.
- Measured-versus-simulated field comparison.

Clearly identify interpolated regions.

Do not imply that sparse measurements constitute a fully measured continuous field.

Prepare for future automated scanning systems and motorized positioning hardware.

## 17. Simulation-to-measurement correlation

This is one of the most important capabilities in Resonant.

Implement a unified comparison framework.

Allow users to compare:

- Simulated fields.
- Measured fields.
- Calibrated fields.
- Reconstructed fields.
- Multiple experiments.
- Different calibration versions.

Support comparison metrics appropriate to the physics domain.

Examples:

**Acoustic**
- Pressure magnitude error.
- Phase error.
- Frequency response error.
- Focal location.
- Focal width.
- Sidelobe levels.

**Mechanical**
- Displacement error.
- Acceleration error.
- Resonance-frequency error.
- Modal correlation.
- Frequency response error.

**Electromagnetic**
- Complex field error.
- Magnitude error.
- Phase error.
- Radiation-pattern difference.
- S-parameter error.
- Beam-pointing error.

Do not compare incompatible quantities or units.

Require appropriate spatial, temporal, frequency, and coordinate alignment.

Report uncertainty and measurement limitations.

## 18. Experimental uncertainty

Introduce uncertainty representation throughout the measurement system.

Support uncertainty sources such as:

- Sensor calibration.
- Instrument accuracy.
- Quantization.
- Noise.
- Position uncertainty.
- Orientation uncertainty.
- Clock uncertainty.
- Temperature variation.
- Repeatability.
- Material uncertainty.

Do not present measurements as infinitely precise.

Where feasible, propagate uncertainty into derived measurements and simulation comparisons.

Distinguish:
- Instrument specifications.
- Calibration uncertainty.
- Statistical uncertainty.
- Model uncertainty.
- Numerical simulation uncertainty.

Provide clear metadata and visual indicators where uncertainty is available.

## 19. Experiment execution engine

Implement an asynchronous experiment execution framework.

Support a configurable workflow:

1. Validate the experiment.
2. Verify device availability.
3. Verify hardware capabilities.
4. Verify calibration status.
5. Verify synchronization requirements.
6. Check safety limits.
7. Configure instruments.
8. Arm triggers.
9. Execute acquisition and permitted outputs.
10. Record data.
11. Verify data integrity.
12. Process results.
13. Generate a report.

Use explicit experiment states:

- Draft.
- Validating.
- Ready.
- Armed.
- Running.
- Stopping.
- Completed.
- Failed.
- Cancelled.

Allow users to inspect execution progress.

Provide clear error reporting and recovery.

Do not silently continue after critical acquisition or synchronization failures.

Support cancellation without corrupting saved measurement data.

## 20. Hardware safety and output authorization

Physical output must be explicitly controlled.

Implement safety checks before enabling actuators or transmitters.

Support configurable:

- Output amplitude limits.
- Frequency limits.
- Duty-cycle limits.
- Duration limits.
- Channel limits.
- Hardware interlocks where available.
- Emergency stop behavior.

For acoustic and ultrasonic experiments, provide appropriate exposure and equipment safety warnings.

For mechanical systems, account for actuator and structural limits.

For RF systems, provide configurable transmit-power limits and regulatory compliance warnings.

Do not assume that a software warning establishes legal or regulatory compliance.

Do not automatically transmit RF signals or activate physical actuators when opening a project or loading an experiment.

Require an explicit user action to authorize physical output.

Where supported, provide a prominent stop control.

Treat hardware errors and loss of control communications as safety-relevant events.

## 21. Experiment UI and workflow

Integrate the experiment framework into the existing Qt6 interface.

Create an **Experimental Hardware** workspace.

Include:

- Device browser.
- Device status panel.
- Experiment hierarchy.
- Sensor list.
- Actuator list.
- Channel configuration.
- Calibration manager.
- Acquisition controls.
- Live measurement display.
- Measurement browser.
- Experiment results.
- Simulation comparison.

Provide a guided experiment setup workflow without making the interface cumbersome.

Allow users to inspect all relevant hardware and measurement metadata.

Support dockable panels and saved workspace layouts.

Live acquisition displays must not block the GUI.

## 22. Data storage and provenance

Use the existing project storage architecture.

Store:

- Experiment definitions.
- Device configurations.
- Sensor and actuator definitions.
- Calibration records.
- Acquisition metadata.
- Raw measurements.
- Derived measurements.
- Processing histories.
- Comparison results.
- Experiment reports.

Use HDF5 or an appropriate scientific data format for large measurement arrays.

Use project metadata for relationships and configuration.

Never overwrite raw measurements during processing.

Support experiment export and import.

Make it possible to archive a complete experiment and reproduce its analysis without requiring the original hardware.

## 23. Hardware simulation and automated testing

Implement simulated hardware drivers.

The simulated devices must provide realistic interface behavior, not merely return constant values.

Create mock devices for:

- Multichannel audio acquisition.
- Microphone input.
- Mechanical accelerometer.
- Mechanical actuator.
- RF receiver.
- RF signal generator.
- VNA measurement data.

Generate test signals with known properties.

Include:
- Noise.
- Frequency response.
- Phase offset.
- Timing offset.
- Gain mismatch.
- Clipping.
- Dropped samples.
- Device disconnection.
- Acquisition errors.

Use these devices to test synchronization, calibration, acquisition, and processing without physical equipment.

## 24. Initial real hardware integrations

After the architecture and simulated drivers are working, investigate practical integrations.

Prioritize integrations based on existing project needs and development environment availability.

Potential starting points:

- ALSA or PortAudio for audio devices.
- SoapySDR for SDR devices.
- libiio for compatible RF hardware.
- SCPI/VISA for laboratory instruments.
- Generic file import for VNA and measurement data.

Do not require all integrations to be completed in this milestone.

For each integration, document:
- Supported hardware.
- Required dependencies.
- Installation requirements.
- Licensing.
- Platform support.
- Tested operations.
- Unsupported operations.

Do not claim a device is supported merely because a generic protocol exists.

## 25. Experiment reporting

Implement experiment reports.

A report should include:

- Experiment description.
- Physics domain.
- Hardware used.
- Device configurations.
- Sensor and actuator details.
- Calibration information.
- Acquisition parameters.
- Environmental conditions.
- Processing methods.
- Measurement results.
- Simulation comparisons.
- Uncertainty.
- Known limitations.

Support export to a suitable document format.

Reports must identify raw, calibrated, processed, simulated, and reconstructed data distinctly.

## 26. Performance and reliability

Measurement acquisition must be robust.

Separate:

- Hardware communication.
- Acquisition.
- Buffer management.
- Disk recording.
- Processing.
- Visualization.

Avoid copying large arrays unnecessarily.

Use bounded queues and explicit backpressure behavior.

Do not let visualization performance determine acquisition reliability.

Support long-running experiments.

Monitor:
- Buffer occupancy.
- Acquisition rate.
- Disk write rate.
- Dropped samples.
- Device health.
- Processing backlog.

Ensure graceful handling of application shutdown and device disconnection.

## 27. Documentation

Create or update:

- EXPERIMENT_ARCHITECTURE.md
- HARDWARE_ABSTRACTION.md
- DEVICE_DRIVER_FRAMEWORK.md
- SENSOR_AND_ACTUATOR_MODELS.md
- MEASUREMENT_DATA_MODEL.md
- ACQUISITION_AND_SYNCHRONIZATION.md
- CALIBRATION_FRAMEWORK.md
- SIGNAL_PROCESSING_PIPELINE.md
- SPATIAL_MEASUREMENT.md
- SIMULATION_MEASUREMENT_CORRELATION.md
- EXPERIMENT_SAFETY.md
- HARDWARE_INTEGRATIONS.md
- EXPERIMENT_VALIDATION.md
- IMPLEMENTATION_STATUS.md
- DECISIONS.md

Document supported hardware, synchronization limitations, calibration conventions, measurement units, uncertainty, and safety assumptions.

## 28. Implementation sequence

**Phase A — Experiment architecture**
- Experiment objects.
- Measurement data model.
- Hardware abstraction.
- Sensor and actuator models.
- Serialization.

**Phase B — Simulated hardware**
- Mock devices.
- Acquisition engine.
- Error simulation.
- Automated tests.

**Phase C — Calibration framework**
- Calibration records.
- Amplitude calibration.
- Phase calibration.
- Timing calibration.
- Sensor and actuator response.

**Phase D — Measurement acquisition**
- Multichannel acquisition.
- Buffer management.
- Triggering.
- Timestamping.
- Recording.

**Phase E — Signal processing**
- FFT.
- Filtering.
- Transfer functions.
- Cross-correlation.
- Coherence.
- Derived data storage.

**Phase F — Spatial measurement**
- Sensor positioning.
- Scan definitions.
- Spatial datasets.
- Field reconstruction.

**Phase G — Simulation correlation**
- Measurement import.
- Coordinate alignment.
- Frequency alignment.
- Comparison metrics.
- Uncertainty.

**Phase H — Domain-specific workflows**
- Acoustic experiments.
- Mechanical experiments.
- RF experiments.

**Phase I — Initial hardware integration**
- Prioritized drivers.
- Device discovery.
- Configuration.
- Real acquisition testing.

**Phase J — Experiment execution and reporting**
- Execution state machine.
- Safety checks.
- Reports.
- Archival and reproducibility.

**Phase K — Verification**
- Automated tests.
- Simulated-device testing.
- Hardware testing where available.
- Long-duration reliability tests.
- Documentation.

Work incrementally. Keep offline operation fully functional throughout development.

## 29. Acceptance criteria

This milestone is complete only when:

1. Experiments can be created, saved, and reopened.
2. The hardware abstraction layer is independent of the physics domains.
3. Sensors and actuators are represented as first-class objects.
4. Simulated hardware can be discovered and configured.
5. Multichannel acquisition works with simulated devices.
6. Acquisition runs asynchronously.
7. Raw measurements are preserved.
8. Timing and synchronization metadata are recorded.
9. Calibration records can be created, versioned, and applied.
10. Frequency-dependent calibration is supported where appropriate.
11. Measurement processing is reproducible.
12. Spatial measurements can be visualized.
13. Measurements can be compared with simulation results.
14. Uncertainty and data quality are represented.
15. Hardware failures are detected and reported.
16. Physical output requires explicit authorization.
17. At least one practical hardware integration is implemented if the development environment supports it.
18. Example experiments exist for acoustic, mechanical, and electromagnetic domains.
19. Automated tests cover acquisition, calibration, synchronization, and error handling.
20. Existing offline simulation workflows remain fully functional.

## 30. Critical development rules

- Build one shared experimental framework.
- Keep physics-specific measurement interpretation in the appropriate domain.
- Never confuse a sensor with its acquisition channel.
- Never confuse commanded actuator output with measured physical output.
- Preserve raw measurements.
- Keep calibration history.
- Track timing and synchronization explicitly.
- Never claim coherent acquisition without evidence.
- Never claim absolute RF field strength without a calibrated measurement chain.
- Never silently discard samples.
- Do not let GUI performance compromise acquisition.
- Do not activate physical outputs automatically.
- Treat hardware errors as potentially safety-critical.
- Make offline operation independent of physical hardware.
- Preserve reproducibility and measurement provenance.

**The goal is to make Resonant an experimental engineering platform: design a wave system, simulate its behavior, connect real instruments, measure the physical result, calibrate the system, reconstruct the field, and quantitatively determine how well reality matches the model.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence medium)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Implement
  - evidence: 'milestone' [body]; 'introduces' [body]; 'integration' [body]; 'evolve' [body]; 'framework' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: Eighth ticket, submitted by the user in chat while REQ-CHAT-20261126-002 was being processed, with the instruction to process it in order (after REQ-CHAT-20261126-007). Shared experimental framework for the acoustic (003-005), mechanical (006) and electromagnetic (007) domains. Standing operator instructions for all tickets: keep qmake support in addition to CMake; Qt user interfaces use .ui files. Safety note for whoever processes it: no physical output may be activated automatically; developme

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

Eighth ticket, submitted by the user in chat while REQ-CHAT-20261126-002 was being processed, with the instruction to process it in order (after REQ-CHAT-20261126-007). Shared experimental framework for the acoustic (003-005), mechanical (006) and electromagnetic (007) domains. Standing operator instructions for all tickets: keep qmake support in addition to CMake; Qt user interfaces use .ui files. Safety note for whoever processes it: no physical output may be activated automatically; development and tests use simulated devices.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000011
- Evidence: pending
