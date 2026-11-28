# Request: # PROMPT 26 — EXPERIMENTAL ENERGY–MATTER LABORATORY AND PHYSICS-BASED VALIDATION

**Request ID:** REQ-CHAT-20261126-023
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T23:57:49-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 26 — EXPERIMENTAL ENERGY–MATTER LABORATORY AND PHYSICS-BASED VALIDATION

## Project: RESONANT — Wave Field Engineering Studio

## Mission

Implement the next major scientific capability in RESONANT: **Experimental Energy–Matter Laboratory and Physics-Based Validation**.

RESONANT must evolve beyond computational physics and inverse design into a platform capable of connecting theoretical predictions, numerical simulations, physical measurements, and experimentally observed behavior.

The platform must establish a unified experimental framework in which physical systems can be characterized, measured, compared against simulations, and used to refine the models that describe them.

The governing concept becomes:

**Physical Theory → Computational Model → Predicted Behavior → Physical Experiment → Measurement → Comparison → Model Refinement → Improved Prediction**

This capability establishes an important new relationship between computational and experimental physics.

A simulation is a prediction based on a model.

An experiment is an observation of physical reality.

Neither should be silently substituted for the other.

RESONANT must preserve the distinction between:

- Theoretical predictions.
- Numerical simulation results.
- Surrogate predictions.
- Experimentally measured results.
- Derived experimental quantities.
- Calibrated model parameters.
- Independently validated predictions.

The objective is to establish an experimental science framework that allows RESONANT to investigate how physical systems actually behave, quantify differences between prediction and observation, identify possible causes of disagreement, and improve the underlying models.

This must integrate with the scientific architecture established in Prompts 17–25.

The experimental laboratory must not become a disconnected data-acquisition application.

It must become a first-class part of RESONANT's physics architecture.

---

# 1. Audit the Existing Architecture

Before implementation, conduct a comprehensive repository audit.

Inspect:

- Prompt 17 Shape–Function–Energy architecture.
- Prompt 18 CEME architecture.
- Prompt 19 geometry-driven energy transformation.
- Prompt 20 resonant structures and energy storage.
- Prompt 21 metamaterials and engineered effective properties.
- Prompt 22 multiscale material modeling.
- Prompt 23 dynamic material state and nonlinear matter.
- Prompt 24 energy, momentum, force, and conservation.
- Prompt 25 inverse design and optimization.
- Physical domain solvers.
- Simulation orchestration.
- Material models and material state.
- Geometry and CAD systems.
- Mesh generation.
- Solver convergence and numerical validation.
- Measurement or data-import facilities.
- Signal processing and numerical analysis.
- Calibration infrastructure.
- Parameter estimation.
- Uncertainty analysis.
- Project persistence.
- Visualization.
- Automation APIs.
- Existing AI or natural-language integration.

Determine:

1. Whether experimental datasets can already be imported.
2. Whether measurement metadata is preserved.
3. Whether measured data can be associated with a physical model.
4. Whether simulation and measurement results share compatible representations.
5. Whether calibration or parameter estimation already exists.
6. Whether uncertainty can be propagated through existing models.
7. Whether multiple experimental runs can be compared.
8. Whether simulation configurations and experimental configurations can be linked.
9. Whether physical quantities have consistent units and coordinate systems.
10. Whether existing infrastructure can support live or streaming measurements.

Produce an architecture map and an implementation plan.

Reuse existing systems wherever possible.

Do not duplicate the geometry, material, solver, CEME, optimization, or conservation infrastructure.

---

# 2. Establish the Experimental Laboratory Architecture

Create a common experimental framework.

The laboratory must support the complete lifecycle of an experiment:

1. Define an experimental objective.
2. Identify the physical system being investigated.
3. Define the experimental configuration.
4. Define the quantities to measure.
5. Associate the experiment with a physical model.
6. Establish measurement conditions.
7. Acquire or import measurements.
8. Validate measurement integrity.
9. Process the acquired data.
10. Extract relevant physical quantities.
11. Compare observations against predictions.
12. Quantify uncertainty.
13. Identify model discrepancies.
14. Calibrate model parameters where justified.
15. Re-evaluate the physical model.
16. Preserve the complete experimental record.

Experiments must be independently identifiable and reproducible.

Every experiment should contain:

- Experiment ID.
- Experiment name.
- Description.
- Physical domain.
- Experimental objective.
- Associated project.
- Associated geometry revision.
- Associated material revision.
- Associated simulation configuration.
- Instrument configuration.
- Measurement configuration.
- Environmental conditions.
- Acquisition configuration.
- Measurement datasets.
- Processing history.
- Calibration history.
- Analysis results.
- Uncertainty information.
- Experimental provenance.
- Date and time information.
- Operator or source information where available.

An experiment must never silently modify the original physical model.

---

# 3. Experimental Data Architecture

Establish a unified representation for experimental data.

Support both imported datasets and measurements acquired through connected instrumentation.

The data architecture must accommodate:

- Scalar measurements.
- Vector measurements.
- Tensor measurements.
- Time-series data.
- Frequency-domain data.
- Spatial field measurements.
- Spatially distributed sensor measurements.
- Images and camera measurements.
- Spectral measurements.
- Transient measurements.
- Environmental measurements.
- Synchronized measurements from multiple instruments.

Each measurement dataset must preserve:

- Dataset identity.
- Measurement identity.
- Physical quantity.
- Units.
- Coordinate system.
- Reference frame.
- Spatial location where applicable.
- Time reference.
- Sampling rate.
- Measurement bandwidth.
- Instrument identity.
- Instrument configuration.
- Calibration state.
- Resolution.
- Measurement range.
- Data format.
- Acquisition conditions.
- Processing history.
- Uncertainty information.
- Original source data.

Raw measurements must be immutable.

Processed datasets must retain a direct relationship to their original measurements.

Never overwrite raw experimental data with filtered, normalized, interpolated, or otherwise transformed data.

## 3.1 Supported Data Formats

Audit existing format support before introducing additional dependencies.

Where appropriate, support:

- CSV.
- TSV.
- JSON.
- HDF5.
- NetCDF.
- TDMS where supported.
- Touchstone S-parameter files.
- WAV.
- Standard image formats.
- Scientific image formats where supported.
- Binary measurement formats through explicit import adapters.

For every import, validate:

- File integrity.
- Expected data structure.
- Units.
- Numeric validity.
- Missing values.
- Timestamp consistency.
- Sampling metadata.
- Coordinate metadata.

Provide a clear import report.

Do not silently discard malformed or incomplete measurements.

## 3.2 Measurement Provenance

Every derived measurement must retain its ancestry.

For example:

Raw voltage measurement → Filtering → Spectral analysis → Resonance extraction → Experimental Q estimate.

The user must be able to inspect how a derived quantity was calculated.

---

# 4. Instrument Integration Architecture

Create an extensible instrument integration layer.

RESONANT should not be limited to a particular manufacturer or measurement device.

Define a common instrument interface supporting:

- Instrument discovery.
- Connection.
- Configuration.
- Measurement initiation.
- Measurement acquisition.
- Measurement termination.
- Status monitoring.
- Error reporting.
- Instrument metadata.
- Calibration metadata.
- Data transfer.

Potential instrument categories include:

### Electrical and Electromagnetic

- Oscilloscopes.
- Signal generators.
- Spectrum analyzers.
- Vector network analyzers.
- Impedance analyzers.
- RF power meters.
- Software-defined radios.
- Frequency counters.
- Lock-in amplifiers.

### Mechanical

- Load cells.
- Force sensors.
- Accelerometers.
- Displacement sensors.
- Strain gauges.
- Vibration analyzers.
- Laser vibrometers.

### Thermal

- Thermocouples.
- RTDs.
- Infrared cameras.
- Thermal imaging sensors.
- Heat-flux sensors.

### Acoustic

- Microphones.
- Hydrophones.
- Acoustic pressure sensors.
- Ultrasonic measurement systems.

### Optical

- Photodetectors.
- Optical power meters.
- Spectrometers.
- Interferometers.
- Cameras.
- Laser displacement systems.

### Environmental

- Temperature sensors.
- Pressure sensors.
- Humidity sensors.
- Magnetic field sensors.
- Environmental monitoring systems.

## 4.1 Instrument Abstraction

Use a common interface with device-specific adapters.

Support instrument communication through existing or appropriate protocols, including:

- VISA.
- SCPI.
- USB.
- Serial.
- Ethernet.
- TCP/IP.
- UDP.
- Vendor-supported APIs.
- Supported DAQ interfaces.

Do not implement every protocol merely to satisfy a checklist.

Prioritize the interfaces that fit the existing architecture and can be tested.

## 4.2 Instrument Capabilities

Every instrument adapter must declare its capabilities.

Examples:

- Measurement quantities.
- Supported ranges.
- Resolution.
- Bandwidth.
- Sampling rate.
- Trigger capabilities.
- Synchronization capabilities.
- Available acquisition modes.
- Configuration options.

Do not assume two instruments have equivalent measurement capabilities merely because they measure the same physical quantity.

## 4.3 Hardware Independence

The experimental framework must remain useful without connected instruments.

Users must be able to import datasets, perform analyses, compare simulations, and calibrate models using previously acquired measurements.

Instrument integration should extend the laboratory, not define its existence.

---

# 5. Experimental Configuration and Reproducibility

An experiment must describe the actual conditions under which measurements were obtained.

Support recording:

- Geometry.
- Material.
- Material state.
- Excitation.
- Frequency.
- Amplitude.
- Phase.
- Boundary conditions.
- Temperature.
- Pressure.
- Humidity.
- Applied forces.
- Mounting conditions.
- Sensor locations.
- Instrument settings.
- Environmental conditions.
- Time-dependent operating conditions.

The experimental configuration should be comparable to the corresponding simulation configuration.

However, do not assume the experimental system is identical to its nominal CAD representation.

Allow the user to record measured deviations such as:

- Actual dimensions.
- Manufacturing tolerances.
- Material batch.
- Measured material properties.
- Surface roughness.
- Assembly misalignment.
- Contact conditions.
- Sensor placement offsets.
- Fixture characteristics.

These deviations may be important sources of simulation disagreement.

Preserve both nominal and measured configurations.

---

# 6. Synchronization and Measurement Timing

Many experiments involve multiple sensors or instruments.

Implement a synchronization framework where supported.

Support:

- Common clock references.
- Trigger synchronization.
- Timestamp alignment.
- PPS synchronization where applicable.
- Phase synchronization.
- Multi-channel acquisition.
- Relative timing correction.
- Clock drift characterization.

Record the synchronization method and its uncertainty.

Distinguish:

- Simultaneous measurements.
- Trigger-aligned measurements.
- Timestamp-aligned measurements.
- Measurements synchronized only approximately.

Do not imply phase coherence or precise timing when the instruments do not support it.

For transient and resonant systems, synchronization accuracy may be critical to the validity of the resulting analysis.

---

# 7. Measurement Calibration and Traceability

Establish a measurement calibration architecture.

Instrument calibration and physical model calibration are different operations and must remain distinct.

## 7.1 Instrument Calibration

Support recording:

- Instrument identity.
- Calibration date.
- Calibration reference.
- Calibration certificate or documentation.
- Calibration coefficients.
- Calibration validity period.
- Calibration uncertainty.
- Calibration history.

Where supported, allow instrument correction factors to be applied during measurement processing.

Preserve the original uncorrected readings.

## 7.2 Sensor Calibration

Support sensor-specific calibration models.

Examples:

- Voltage-to-field conversion.
- Strain-gauge conversion.
- Thermocouple conversion.
- Accelerometer sensitivity.
- Microphone sensitivity.
- Displacement sensor calibration.

Each conversion must preserve:

- Calibration equation.
- Coefficients.
- Units.
- Valid range.
- Uncertainty.
- Calibration provenance.

## 7.3 Calibration Traceability

Distinguish between:

- Factory calibration.
- Laboratory calibration.
- User calibration.
- Reference-standard calibration.
- Uncalibrated measurement.

Do not describe a measurement as traceable to a standard unless the required evidence exists.

---

# 8. Experimental Signal and Field Processing

Integrate the laboratory with existing numerical and signal-processing systems.

Support processing appropriate to the physical domain.

Potential capabilities include:

### Time-Domain Analysis

- Mean.
- RMS.
- Peak and peak-to-peak.
- Transient detection.
- Decay analysis.
- Envelope extraction.
- Cross-correlation.
- Autocorrelation.
- Time-delay estimation.

### Frequency-Domain Analysis

- FFT.
- Power spectral density.
- Cross-spectral density.
- Transfer functions.
- Frequency response.
- Harmonic analysis.
- Spectral peak detection.
- Resonance extraction.
- Bandwidth measurement.
- Phase analysis.

### Spatial Analysis

- Field reconstruction.
- Spatial interpolation.
- Vector field visualization.
- Gradient estimation.
- Divergence.
- Curl.
- Spatial correlation.
- Surface mapping.

### Mechanical Analysis

- Modal extraction.
- Natural frequency estimation.
- Damping estimation.
- Frequency response.
- Deflection measurement.
- Strain and stress derivation where supported.

### Thermal Analysis

- Temperature distribution.
- Thermal gradient.
- Heat-flux estimation.
- Transient heating and cooling.
- Thermal response time.

### Acoustic Analysis

- Sound pressure.
- Frequency response.
- Acoustic intensity where measurable.
- Resonance.
- Decay time.
- Spatial pressure distribution.

### Electromagnetic Analysis

- S-parameters.
- Reflection coefficient.
- Transmission coefficient.
- Impedance.
- Resonance.
- Gain.
- Field distribution.
- Phase response.
- Radiation characteristics where measurable.

All processing operations must be explicit and reproducible.

Record algorithm parameters and processing revisions.

Do not confuse measured quantities with quantities inferred from a mathematical model.

---

# 9. Simulation-to-Experiment Comparison

This is the central scientific requirement of Prompt 26.

Create a unified comparison framework between predicted and measured behavior.

The comparison engine must be able to associate experimental observations with corresponding physical simulation outputs.

Support comparison of:

- Resonant frequencies.
- Modal frequencies.
- Q factors.
- Transfer functions.
- Reflection and transmission.
- Spatial field distributions.
- Temperature distributions.
- Force distributions.
- Displacement.
- Stress and strain where measurable or inferable.
- Energy storage.
- Energy transfer.
- Dissipation.
- Transient responses.
- Material effective properties.

## 9.1 Comparison Requirements

Before comparing two datasets, establish:

- Compatible physical quantities.
- Compatible units.
- Compatible coordinate systems.
- Compatible reference frames.
- Compatible operating conditions.
- Compatible frequency ranges.
- Compatible time intervals.
- Compatible spatial regions.

Where conversion or alignment is necessary, record the operation.

Never compare incompatible datasets without warning.

## 9.2 Comparison Metrics

Support appropriate metrics including:

- Absolute error.
- Relative error.
- RMS error.
- Normalized RMS error.
- Mean absolute error.
- Maximum deviation.
- Correlation.
- Phase error.
- Frequency offset.
- Amplitude offset.
- Spectral similarity.
- Spatial field error.
- Temporal response error.

Metric selection must depend on the physical quantity and engineering objective.

Do not use a single generic error metric for every physical domain.

## 9.3 Visual Comparison

Provide overlays and comparative visualizations.

Examples:

- Simulated versus measured frequency response.
- Predicted versus measured resonance.
- Measured versus simulated field distribution.
- Simulated versus measured temperature.
- Predicted versus measured displacement.
- Predicted versus measured transient response.

Allow the user to inspect residuals and disagreement regions.

---

# 10. Experimental Uncertainty and Error Budgets

Experimental validation is incomplete without uncertainty analysis.

Establish a unified uncertainty framework.

Distinguish:

- Instrument uncertainty.
- Sensor uncertainty.
- Calibration uncertainty.
- Sampling uncertainty.
- Timing uncertainty.
- Spatial positioning uncertainty.
- Environmental uncertainty.
- Manufacturing variation.
- Material variation.
- Numerical simulation uncertainty.
- Model-form uncertainty.

Support uncertainty propagation where justified.

Methods may include:

- Analytical propagation.
- Monte Carlo simulation.
- Sensitivity-based propagation.
- Statistical estimation.
- Covariance analysis.

## 10.1 Error Budgets

Allow users to construct an error budget for an experiment.

For each contribution, record:

- Source.
- Magnitude.
- Distribution where known.
- Units.
- Correlation assumptions.
- Measurement method.
- Confidence or coverage information.
- Contribution to total uncertainty.

Do not assume independent uncertainty sources unless justified.

## 10.2 Measurement Confidence

Report uncertainty alongside experimental results.

Avoid false precision.

Distinguish between:

- Measurement resolution.
- Measurement accuracy.
- Repeatability.
- Reproducibility.
- Statistical confidence.
- Calibration uncertainty.

These concepts are related but not interchangeable.

---

# 11. Physical Model Calibration and Parameter Estimation

Establish a controlled pathway for using experiments to improve physical models.

The objective is to identify model parameters that better explain observed behavior.

Potential calibration targets include:

- Material parameters.
- Damping coefficients.
- Conductivity.
- Permittivity.
- Permeability.
- Elastic constants.
- Thermal properties.
- Acoustic properties.
- Boundary-condition parameters.
- Contact parameters.
- Resonator losses.
- Coupling coefficients.
- Effective metamaterial properties.

Calibration must use parameters that have a legitimate physical interpretation in the underlying model.

Do not permit arbitrary correction factors to conceal incorrect physical assumptions.

## 11.1 Parameter Estimation

Support:

- Least-squares estimation.
- Weighted least squares.
- Nonlinear parameter fitting.
- Maximum-likelihood estimation where justified.
- Bayesian parameter estimation where supported.
- Regularized parameter estimation.

Account for measurement uncertainty when selecting the fitting method.

## 11.2 Parameter Identifiability

A central requirement is determining whether available measurements can actually identify the desired parameters.

Support:

- Parameter correlation analysis.
- Identifiability assessment.
- Sensitivity analysis.
- Confidence intervals.
- Parameter covariance.
- Multiple-experiment fitting.

Do not claim that a parameter has been reliably determined if multiple parameter combinations explain the observations equally well.

## 11.3 Calibration Provenance

Every calibrated model revision must retain:

- Original model revision.
- Calibrated parameters.
- Original parameter values.
- Experimental datasets used.
- Fitting method.
- Optimization configuration.
- Residuals.
- Uncertainty.
- Identifiability information.
- Calibration date.
- Validation status.

Never overwrite the original model.

---

# 12. Independent Validation and Avoiding Overfitting

Calibration and validation must be distinct.

A model fitted against an experimental dataset has not automatically been independently validated by that same dataset.

Support:

- Calibration datasets.
- Validation datasets.
- Holdout measurements.
- Repeated experimental runs.
- Independent operating conditions.
- Cross-validation where appropriate.

Allow a model to be calibrated against one set of observations and tested against another.

Report:

- Calibration performance.
- Validation performance.
- Residual differences.
- Parameter uncertainty.
- Valid operating domain.

A calibrated model must not be presented as universally accurate outside its validated conditions.

---

# 13. Experimental Modal and Resonance Validation

Integrate directly with Prompt 20.

Resonant structures offer an especially important experimental validation opportunity.

Support experimental identification of:

- Natural frequencies.
- Driven resonance peaks.
- Mode shapes.
- Modal damping.
- Q factor.
- Mode coupling.
- Resonance bandwidth.
- Frequency-dependent losses.
- Transient buildup.
- Ring-down behavior.

Where supported, allow comparison between experimental and simulated mode shapes.

Use appropriate spatial and phase alignment.

## 13.1 Mode Identification

Distinguish between:

- Natural modes.
- Driven-response peaks.
- Coupled modes.
- Hybridized modes.
- Measurement artifacts.
- Instrument-induced resonances.

Do not automatically identify every spectral peak as a physical resonance of the structure.

## 13.2 Ring-Down Analysis

Support time-domain characterization of resonant decay.

Where the underlying assumptions are appropriate, estimate:

- Decay constant.
- Damping ratio.
- Q factor.
- Stored energy decay.
- Frequency stability.

Identify the fitting method and its validity assumptions.

## 13.3 Experimental Energy Storage

Where supported by actual measurements, estimate stored energy and dissipation.

Clearly distinguish directly measured quantities from model-derived estimates.

Do not claim direct energy measurement when energy has been inferred from voltage, current, displacement, or field measurements.

---

# 14. Experimental Validation of Metamaterials

Integrate with Prompt 21.

Support experimental characterization of metamaterial structures and engineered effective properties.

Potential measurements include:

- Reflection.
- Transmission.
- Dispersion.
- Band gaps.
- Phase velocity.
- Group velocity.
- Effective impedance.
- Effective density.
- Effective stiffness.
- Effective permittivity.
- Effective permeability.
- Anisotropy.
- Resonance behavior.

Where appropriate, compare experimental results against:

- Unit-cell simulations.
- Periodic boundary-condition simulations.
- Homogenized models.
- Macroscopic effective-property predictions.

## 14.1 Homogenization Validation

Evaluate whether the effective-property approximation is appropriate for the experimental scale and wavelength.

Identify:

- Unit-cell size.
- Wavelength.
- Operating frequency.
- Periodicity assumptions.
- Spatial dispersion concerns.
- Measurement limitations.

Do not assume that an effective-medium model remains valid outside its homogenization regime.

## 14.2 Measured Versus Derived Properties

Distinguish:

- Directly measured S-parameters.
- Retrieved effective properties.
- Simulated effective properties.
- Analytically derived effective properties.

Preserve the retrieval method and assumptions.

---

# 15. Multiscale Experimental Validation

Integrate with Prompt 22.

Experimental observations may be used to validate models at different physical scales.

Support relationships between:

- Macroscopic measurements.
- Mesoscopic measurements.
- Microstructure imaging.
- Microscopic material characterization.
- Molecular or atomic measurements where supported.

Potential data sources include:

- Optical microscopy.
- Electron microscopy.
- X-ray imaging.
- Tomography.
- Material spectroscopy.
- Diffraction measurements.
- Mechanical characterization.
- Thermal characterization.

The architecture should support measured microstructure data being associated with the corresponding multiscale material model.

For example:

1. Measure a physical unit cell.
2. Import its geometric characterization.
3. Associate the geometry with a microstructure model.
4. Extract material or structural parameters.
5. Calculate effective properties.
6. Compare those properties against macroscopic experimental measurements.
7. Update the material model where justified.

Preserve lineage across every scale.

Do not claim atomistic or quantum experimental validation merely because microscopy data is available.

---

# 16. Dynamic Material State and Experimental Observation

Integrate with Prompt 23.

Support experiments involving materials whose properties evolve over time or under changing conditions.

Examples:

- Thermal expansion.
- Phase transitions.
- Plastic deformation.
- Hysteresis.
- Fatigue.
- Damage.
- Stress-dependent properties.
- Temperature-dependent conductivity.
- Field-dependent material behavior.
- Nonlinear response.
- Viscoelasticity.

Allow measurements to be associated with material state.

For example:

- Temperature at measurement time.
- Applied stress.
- Strain history.
- Magnetic field.
- Electric field.
- Loading history.
- Heating and cooling cycles.
- Prior deformation.

The objective is to compare predicted material evolution against measured behavior.

Where supported, use experimental observations to refine dynamic constitutive models.

Preserve the distinction between observed state variables and inferred internal material states.

---

# 17. Conservation and Experimental Energy Accounting

Integrate with Prompt 24.

Experimental energy and momentum accounting introduces additional measurement considerations.

Support experimental accounting where sufficient measurements exist.

Examples:

- Electrical input and output power.
- Mechanical work.
- Heat transfer.
- Acoustic power.
- Electromagnetic power flow.
- Momentum transfer.
- Force and displacement relationships.
- Mass flow.
- Charge transfer.

Account for measurement uncertainty and unmeasured contributions.

## 17.1 Experimental Energy Balance

Where applicable, compare:

- Energy input.
- Energy stored.
- Energy transferred.
- Energy dissipated.
- Energy remaining in the system.

The experimental balance should include uncertainty bounds.

Do not force an exact balance when measurement uncertainty or unmeasured energy pathways prevent it.

## 17.2 Experimental Conservation Diagnostics

Identify possible discrepancies caused by:

- Missing sensors.
- Incorrect sensor calibration.
- Unmeasured energy pathways.
- Incomplete system boundaries.
- Incorrect synchronization.
- Sampling limitations.
- Sensor loading.
- Instrument bandwidth.
- Incorrect physical assumptions.

Distinguish measurement inconsistency from genuine model disagreement.

---

# 18. Experimental Support for Inverse Design

Integrate directly with Prompt 25.

Experimental results should be able to improve the reliability of future inverse-design operations.

Support:

- Calibration of models used by optimizers.
- Experimental validation of selected candidates.
- Comparison of candidate predictions against measured performance.
- Manufacturing deviation characterization.
- Experimental constraint refinement.
- Robustness-model improvement.
- Experimental response-surface generation.
- Surrogate-model training using verified experimental data.

## 18.1 Experimental Candidate Validation

A candidate promoted through inverse design may subsequently be fabricated and measured.

Allow the experiment to reference the exact candidate revision.

Compare:

- Predicted geometry.
- Manufactured geometry.
- Predicted material properties.
- Measured material properties.
- Predicted physical response.
- Measured physical response.

The resulting experimental validation record must remain linked to the original candidate.

## 18.2 Closed-Loop Design Refinement

Where appropriate, support:

1. Generate candidate through inverse design.
2. Validate candidate numerically.
3. Fabricate or assemble candidate.
4. Measure physical performance.
5. Compare against prediction.
6. Identify discrepancies.
7. Update calibrated model parameters.
8. Re-evaluate candidate.
9. Generate improved candidates.

This establishes an experimental feedback loop between physical manufacturing, measurement, simulation, and design discovery.

Do not automatically modify a validated design based on an unreviewed experimental result.

---

# 19. Experimental Visualization and Laboratory Workspace

Create a dedicated experimental laboratory workspace integrated into the existing application.

The workspace should provide:

### Experiment Explorer

- Experiment list.
- Experiment status.
- Associated physical system.
- Measurement datasets.
- Instrument information.
- Calibration status.
- Validation status.

### Measurement Viewer

- Raw measurement visualization.
- Processed data visualization.
- Time-domain plots.
- Frequency-domain plots.
- Spatial field maps.
- Measurement metadata.
- Uncertainty visualization.

### Simulation Comparison

- Predicted versus measured overlays.
- Residual plots.
- Spatial difference maps.
- Frequency-response comparisons.
- Modal comparisons.
- Transient comparisons.

### Calibration Workspace

- Model parameter selection.
- Parameter bounds.
- Fitting method.
- Calibration progress.
- Parameter estimates.
- Residual analysis.
- Identifiability.
- Confidence intervals.

### Experimental Report

Generate a report containing:

- Experimental objective.
- Experimental configuration.
- Instrument information.
- Calibration status.
- Measurement conditions.
- Raw dataset references.
- Processing history.
- Simulation configuration.
- Comparison results.
- Error metrics.
- Uncertainty analysis.
- Calibration results.
- Validation conclusions.
- Known limitations.

Preserve the existing application design language.

Do not overwhelm the main engineering interface with experimental controls.

Use dedicated workflows and contextual panels.

---

# 20. Live and Streaming Measurements

Where supported by the existing architecture, implement live measurement acquisition.

Support:

- Live instrument data.
- Streaming sensor data.
- Multi-channel acquisition.
- Real-time visualization.
- Measurement status.
- Buffer management.
- Acquisition interruption.
- Data integrity checks.
- Timestamp preservation.

The live measurement architecture must not block the main GUI thread.

Use existing asynchronous or job-management infrastructure where possible.

Support recording live measurements into persistent experimental datasets.

Do not require live acquisition for basic experimental analysis.

Imported datasets must remain fully supported.

---

# 21. AI-Assisted Experimental Analysis

If RESONANT already has an AI or natural-language interface, integrate it with the laboratory.

The assistant should be able to help users:

- Interpret experimental objectives.
- Identify relevant measurement quantities.
- Suggest suitable instruments based on declared capabilities.
- Identify missing metadata.
- Suggest measurement configurations.
- Explain differences between predictions and observations.
- Identify possible sources of discrepancy.
- Recommend additional measurements.
- Explain calibration results.
- Suggest experiments that distinguish competing physical explanations.

For example:

"The measured resonant frequency is approximately 4% below the predicted value, and the measured Q factor is substantially lower. Investigate possible explanations involving material loss, dimensional tolerances, coupling, and boundary conditions."

The AI should be able to inspect the actual experimental and simulation records and identify relevant evidence.

However:

- It must not invent measurements.
- It must not silently alter model parameters.
- It must not declare a model validated without supporting evidence.
- It must distinguish hypotheses from established findings.
- It must identify missing information.
- It must preserve the user's control over model changes.

If the repository does not contain a suitable AI integration, establish a clean API for future use without introducing an unnecessary external dependency.

---

# 22. Automation and Programmatic Access

Integrate the experimental laboratory with the existing automation architecture.

Expose operations for:

- Create experiment.
- Retrieve experiment.
- Update experimental configuration.
- Import measurement dataset.
- Discover instruments.
- Connect instrument.
- Configure instrument.
- Start acquisition.
- Stop acquisition.
- Retrieve acquisition status.
- Retrieve measurement data.
- Process measurement data.
- Extract physical quantities.
- Compare simulation and experiment.
- Calculate uncertainty.
- Create calibration job.
- Inspect calibration results.
- Validate a model.
- Generate experimental reports.
- Export experimental datasets.

Long-running acquisition, processing, and calibration jobs must use existing job-management infrastructure where available.

Automation must preserve the same provenance, uncertainty, and validation information as the GUI.

---

# 23. Persistence and Experimental Reproducibility

Experimental data must be treated as a permanent scientific record.

Persist:

- Experiment definitions.
- Experimental configurations.
- Instrument configurations.
- Calibration records.
- Raw datasets.
- Processed datasets.
- Processing history.
- Simulation references.
- Comparison results.
- Error metrics.
- Uncertainty models.
- Calibration results.
- Validation datasets.
- Reports.
- Provenance.

Raw measurement datasets must remain immutable.

Support recovery of interrupted acquisitions where the instrument and acquisition method permit it.

Support interrupted calibration recovery where feasible.

Preserve compatibility with existing project persistence and versioning.

Avoid storing unnecessarily large raw datasets inside ordinary project metadata when the existing architecture supports external or referenced data storage.

Ensure that referenced datasets remain portable and recoverable.

---

# 24. Scientific Validation and Benchmark Experiments

Create a serious experimental validation test suite.

Use both synthetic datasets with known ground truth and actual physical measurements where available.

Synthetic datasets are appropriate for verifying algorithms and uncertainty calculations, but they must never be presented as actual experimental evidence.

## 24.1 Resonator Validation

Create or import a known resonator measurement dataset.

Verify:

- Resonance extraction.
- Q-factor estimation.
- Bandwidth calculation.
- Phase response.
- Simulation comparison.
- Measurement uncertainty.

## 24.2 Frequency Response Validation

Use a known frequency-response dataset.

Verify:

- Amplitude alignment.
- Phase alignment.
- Frequency-axis consistency.
- Residual calculations.
- Comparison metrics.

## 24.3 Material Parameter Calibration

Use a physical model with known parameters and synthetic observations.

Verify that the calibration framework can recover the parameters within documented uncertainty.

Test parameter identifiability.

## 24.4 Spatial Field Comparison

Use a known spatial field distribution.

Verify:

- Coordinate transformation.
- Spatial alignment.
- Interpolation.
- Field comparison.
- Spatial residual visualization.

## 24.5 Thermal Validation

Where a thermal solver exists, compare a known thermal simulation against a corresponding measured or synthetic temperature dataset.

Verify uncertainty-aware comparison.

## 24.6 Conservation Validation

Construct an experimental energy balance with known measurement uncertainty.

Verify:

- Input and output accounting.
- Uncertainty propagation.
- Missing contribution detection.
- Conservation discrepancy reporting.

## 24.7 Invalid Dataset Handling

Test:

- Missing units.
- Invalid timestamps.
- Malformed files.
- Inconsistent sampling rates.
- Missing channels.
- NaN and infinite values.
- Incompatible coordinate systems.
- Incomplete instrument metadata.

Verify that errors are explicit and raw data is preserved.

## 24.8 Calibration and Independent Validation

Use separate calibration and validation datasets.

Verify that a model can improve its fit to calibration observations without incorrectly claiming independent validation.

---

# 25. Implementation Strategy

Implement this capability in deliberate phases.

## Phase 1 — Architecture Audit and Experimental Data Model

- Audit existing infrastructure.
- Establish experiment identity and persistence.
- Define measurement and dataset representations.
- Establish immutable raw-data handling.
- Define instrument abstraction.
- Define experimental provenance.

## Phase 2 — Data Import and Processing

- Implement dataset import.
- Validate metadata and units.
- Implement processing provenance.
- Integrate existing numerical and signal-processing systems.
- Implement measurement visualization.

This is the first major functional milestone.

## Phase 3 — Simulation-to-Experiment Comparison

- Associate experiments with simulation revisions.
- Implement physical quantity matching.
- Implement coordinate and time alignment.
- Implement comparison metrics.
- Implement residual visualization.

This is the central scientific milestone.

## Phase 4 — Calibration and Parameter Estimation

- Implement parameter estimation.
- Implement uncertainty-aware fitting.
- Implement identifiability analysis.
- Preserve calibrated model revisions.
- Separate calibration from validation.

## Phase 5 — Instrument Integration

- Implement the common instrument interface.
- Implement one or more practical adapters.
- Add acquisition and status monitoring.
- Add synchronization metadata.
- Add live visualization where supported.

## Phase 6 — Domain-Specific Experimental Integration

- Integrate resonance measurements.
- Integrate metamaterial characterization.
- Integrate material characterization.
- Integrate thermal and mechanical measurements where supported.
- Integrate conservation accounting.

## Phase 7 — Inverse Design Feedback

- Link experiments to inverse-design candidates.
- Support experimental candidate validation.
- Integrate verified experimental data into model calibration.
- Support controlled closed-loop design refinement.

## Phase 8 — GUI, Automation, and Reporting

- Implement laboratory workspace.
- Implement experiment explorer.
- Implement measurement viewer.
- Implement calibration workspace.
- Implement reports.
- Integrate automation APIs.

## Phase 9 — Scientific Validation and Documentation

- Run synthetic benchmark experiments.
- Run available physical measurement tests.
- Verify data integrity.
- Verify reproducibility.
- Verify uncertainty handling.
- Verify calibration and independent validation.
- Document supported capabilities and limitations.

Prioritize a complete, usable experimental data and simulation-comparison workflow before attempting extensive hardware integrations or autonomous experimental discovery.

---

# 26. Explicit Scientific and Engineering Guardrails

These requirements are mandatory.

1. Do not confuse simulated data with experimental measurements.
2. Do not present synthetic benchmark data as physical experimental evidence.
3. Do not overwrite raw measurement data.
4. Do not discard measurement provenance.
5. Do not compare incompatible physical quantities without explicit conversion.
6. Do not ignore coordinate-system differences.
7. Do not imply phase synchronization without evidence.
8. Do not confuse instrument calibration with physical model calibration.
9. Do not claim measurement traceability without supporting documentation.
10. Do not ignore measurement uncertainty.
11. Do not assume uncertainty sources are independent without justification.
12. Do not confuse measurement resolution with measurement accuracy.
13. Do not calibrate against a dataset and call the same dataset independent validation.
14. Do not claim parameter identifiability without examining it.
15. Do not permit arbitrary correction factors to conceal incorrect physics.
16. Do not automatically modify physical models based on unreviewed measurements.
17. Do not treat every spectral peak as a structural resonance.
18. Do not infer direct energy measurement from indirect sensor measurements without identifying the derivation.
19. Do not claim physical validation outside the experimentally supported operating range.
20. Do not assume nominal CAD geometry exactly represents a manufactured specimen.
21. Do not claim experimental conservation when unmeasured pathways remain significant.
22. Do not allow live acquisition to block the GUI.
23. Do not make hardware integration mandatory for imported-data workflows.
24. Do not create duplicate experiment, material, geometry, or solver infrastructure.
25. Preserve the distinction between prediction, measurement, calibration, and independent validation.

**Experimental evidence must be traceable, uncertainty-aware, and scientifically interpretable.**

---

# 27. Acceptance Criteria

Prompt 26 is complete when the following conditions are met.

### Experimental Architecture

- [ ] A common experiment representation exists.
- [ ] Measurement datasets have stable identities.
- [ ] Raw data is immutable.
- [ ] Processed datasets preserve provenance.
- [ ] Experimental configurations are persistent.
- [ ] Experiments can reference physical model revisions.

### Data Acquisition and Import

- [ ] At least one practical measurement import workflow works.
- [ ] Dataset metadata is validated.
- [ ] Units and coordinate systems are preserved.
- [ ] Invalid datasets are reported clearly.
- [ ] Imported data can be analyzed without connected hardware.

### Instrument Integration

- [ ] A reusable instrument abstraction exists.
- [ ] Instrument capabilities are represented.
- [ ] At least one practical instrument adapter works where hardware or a testable interface is available.
- [ ] Acquisition does not block the GUI.
- [ ] Instrument status and errors are visible.

### Simulation Comparison

- [ ] Simulation and experimental data can be associated.
- [ ] Compatible quantities can be compared.
- [ ] Unit and coordinate alignment is supported.
- [ ] Comparison metrics are implemented.
- [ ] Residuals can be visualized.
- [ ] Comparison provenance is preserved.

### Calibration and Validation

- [ ] At least one physical model parameter can be calibrated using data.
- [ ] Calibration uncertainty is represented.
- [ ] Parameter identifiability is assessed where applicable.
- [ ] Calibration and validation datasets remain distinct.
- [ ] Calibrated model revisions preserve their ancestry.

### Scientific Analysis

- [ ] Experimental resonance analysis is available where supported.
- [ ] Uncertainty analysis is implemented.
- [ ] Experimental energy accounting is supported where sufficient data exists.
- [ ] Synthetic benchmark tests have known ground truth.
- [ ] No synthetic dataset is presented as physical evidence.

### Integration

- [ ] Experimental results integrate with CEME.
- [ ] Geometry and material revisions can be referenced.
- [ ] Resonance analysis integrates with Prompt 20.
- [ ] Metamaterial characterization integrates with Prompt 21 where supported.
- [ ] Multiscale experimental lineage integrates with Prompt 22 where supported.
- [ ] Dynamic material-state observations integrate with Prompt 23 where supported.
- [ ] Conservation analysis integrates with Prompt 24.
- [ ] Inverse-design candidates can reference experimental validation from Prompt 25.

### User Experience

- [ ] A laboratory workspace exists.
- [ ] Experiments can be created and inspected.
- [ ] Measurement data can be imported and visualized.
- [ ] Simulation comparisons can be inspected.
- [ ] Calibration results can be reviewed.
- [ ] Experimental reports can be generated.

### Scientific Quality

- [ ] Raw data integrity is preserved.
- [ ] Measurement uncertainty is not silently discarded.
- [ ] Calibration and independent validation are distinguished.
- [ ] At least two meaningful validation benchmarks pass.
- [ ] Known limitations are documented.
- [ ] No unsupported claims of experimental validation are made.

Completion does not require universal instrument support, autonomous experimentation, or experimental characterization across every physical domain.

It requires a reusable experimental architecture, reliable data ingestion, meaningful simulation-to-experiment comparison, controlled model calibration, and a scientifically defensible validation workflow.

---

# 28. Final Engineering Report

When implementation is complete, provide a detailed engineering report containing:

1. Existing architecture audit.
2. Experimental laboratory architecture.
3. Experimental data model.
4. Raw-data preservation and provenance.
5. Supported data formats.
6. Instrument abstraction.
7. Implemented instrument adapters.
8. Acquisition and synchronization.
9. Measurement calibration.
10. Signal and field processing.
11. Simulation-to-experiment comparison.
12. Uncertainty and error budgets.
13. Parameter estimation.
14. Identifiability analysis.
15. Calibration and independent validation.
16. Resonance validation.
17. Metamaterial characterization.
18. Multiscale experimental integration.
19. Dynamic material-state integration.
20. Experimental conservation accounting.
21. Inverse-design feedback integration.
22. GUI implementation.
23. Automation integration.
24. Persistence and reproducibility.
25. Benchmark results.
26. Test results.
27. Performance observations.
28. Known limitations.
29. Deferred capabilities.
30. Recommended next engineering steps.

Identify the relevant modules, interfaces, source files, and tests.

Clearly distinguish:

- Fully implemented capabilities.
- Partially implemented capabilities.
- Interfaces established for future hardware.
- Capabilities requiring additional physical instrumentation.
- Capabilities requiring additional physical domain solvers.

Do not report an instrument integration as operational unless it has been tested against a real instrument or a clearly identified hardware test environment.

---

# Final Directive

RESONANT must become a platform in which computational predictions and experimental observations inform one another.

The platform should be able to describe a physical system, predict its behavior, investigate that behavior through physical measurement, compare the results, quantify uncertainty, and refine its understanding of the system.

The deeper objective is to create a continuous scientific relationship between theory, simulation, experiment, and engineering design.

Prompt 25 gave RESONANT the ability to discover candidate physical structures from desired functions.

Prompt 26 gives RESONANT the ability to determine whether those structures actually behave as predicted.

Together, these capabilities establish a powerful closed-loop scientific framework:

**Model → Predict → Design → Fabricate → Measure → Compare → Learn → Refine → Discover Again.**

This is not simply a laboratory data viewer.

It is the experimental foundation of a physics-driven engineering discovery platform.

Implement it as a cohesive extension of the existing architecture.

Preserve scientific rigor, measurement traceability, uncertainty awareness, reproducibility, and the distinction between simulated predictions and experimentally demonstrated physical behavior.

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: - (heuristic)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Implement, capability, evolve, framework
  - evidence: 'Implement' [head]; 'capability' [head]; 'evolve' [head]; 'must establish' [head]; 'framework' [head]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: User: 'add this prompt and process in order' (queued after REQ-CHAT-20261126-022, Prompt 24). Prompt 25 (inverse design and optimization), which this prompt builds on, has not been received; Prompt 12 (REQ-CHAT-20261126-008, being processed) builds the experiment and hardware framework this prompt extends.

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

User: 'add this prompt and process in order' (queued after REQ-CHAT-20261126-022, Prompt 24). Prompt 25 (inverse design and optimization), which this prompt builds on, has not been received; Prompt 12 (REQ-CHAT-20261126-008, being processed) builds the experiment and hardware framework this prompt extends.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000024
- Evidence: pending
