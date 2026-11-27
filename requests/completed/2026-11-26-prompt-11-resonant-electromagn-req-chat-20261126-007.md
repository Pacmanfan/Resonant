# Request: # PROMPT 11 — RESONANT: ELECTROMAGNETIC WAVE ENGINEERING, RF PROPAGATION AND ANT

**Request ID:** REQ-CHAT-20261126-007
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T15:34:55-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 11 — RESONANT: ELECTROMAGNETIC WAVE ENGINEERING, RF PROPAGATION AND ANTENNA ARRAY SIMULATION

You are continuing development of **RESONANT — Wave Field Engineering Studio**.

This milestone introduces the third major physics domain: **electromagnetic wave engineering**.

The objective is to make Resonant capable of designing, analyzing, simulating, and optimizing electromagnetic structures, RF antennas, antenna arrays, phased arrays, and electromagnetic propagation environments.

This includes conventional antenna engineering, near-field and far-field analysis, electromagnetic interference, field distribution, polarization, impedance, radiation patterns, and eventually full-wave electromagnetic simulation.

Build on the existing Resonant application, geometry, material, simulation, visualization, and solver architectures.

Do not replace the acoustic or mechanical subsystems. Electromagnetic physics must be a distinct domain with its own governing equations, constitutive material properties, boundary conditions, and numerical methods.

---

## 1. Mission and objectives

Implement a modular electromagnetic physics subsystem supporting:

- Electromagnetic wave propagation.
- Electric and magnetic field calculations.
- RF antenna modeling.
- Antenna radiation patterns.
- Near-field analysis.
- Far-field analysis.
- Antenna arrays.
- Phased arrays.
- Beam steering.
- Beamforming.
- Polarization analysis.
- Electromagnetic interference.
- Reflection and transmission.
- Diffraction and scattering.
- Material interactions.
- Resonant structures.
- Waveguides and transmission lines.
- Electromagnetic cavities.
- Impedance analysis.
- Frequency sweeps.
- Broadband and transient propagation.
- Electromagnetic field focusing.
- Metamaterials and engineered electromagnetic structures.

The initial priority is a useful RF antenna and array engineering workflow, followed by progressively more rigorous electromagnetic simulation.

The system should eventually support complex electromagnetic environments and real physical antenna structures.

Do not attempt to implement every electromagnetic solver or physical effect in one milestone.

Prioritize correct, independently validated capabilities.

## 2. Electromagnetic physics architecture

Create a dedicated electromagnetic physics subsystem.

Keep it independent from the acoustic and mechanical physics engines.

The subsystem should include:

- Electromagnetic domain data models.
- RF source definitions.
- Antenna models.
- Antenna array models.
- Electromagnetic materials.
- Propagation media.
- Boundary conditions.
- Electromagnetic geometry.
- Solver interfaces.
- Field evaluation.
- Radiation analysis.
- Frequency-domain analysis.
- Time-domain analysis.
- Optimization.
- Result storage.
- Visualization interfaces.

Reuse appropriate generic application infrastructure:

- Scene management.
- Geometry.
- Coordinate transforms.
- Units.
- Material metadata.
- Mesh infrastructure.
- Simulation jobs.
- Rendering.
- Project persistence.
- Result storage.

Do not reuse acoustic Green's functions, acoustic material properties, or mechanical solver formulations as substitutes for electromagnetic equations.

## 3. Electromagnetic material models

Extend the material system with electromagnetic properties.

Support:

- Relative permittivity.
- Relative permeability.
- Electrical conductivity.
- Dielectric loss.
- Magnetic loss.
- Complex permittivity.
- Complex permeability.
- Anisotropy where supported.
- Frequency dependence.
- Temperature dependence where available.
- Dispersion where supported.

Support initial materials such as:

- Vacuum.
- Air.
- Water.
- Copper.
- Aluminum.
- Silver.
- Brass.
- Steel.
- Glass.
- Acrylic.
- FR-4.
- PTFE.
- Common plastics.
- Ceramic substrates.
- Ferrite materials.
- Dielectric composites.

Material records must include:

- Source and provenance.
- Measurement conditions.
- Frequency validity range.
- Temperature validity range where available.
- Units and conventions.
- Confidence or quality metadata.

Do not invent authoritative material constants.

Do not assume FR-4 or other manufactured composites have one universal dielectric constant or loss tangent.

Allow frequency-dependent material properties to be represented by suitable functions or tabulated data.

Clearly distinguish ideal conductors, finite-conductivity conductors, dielectric materials, and magnetic materials.

## 4. Maxwell equation foundation

Establish a mathematically correct electromagnetic physics foundation.

Use Maxwell's equations as the governing physical framework.

In differential form:

\[
\nabla\cdot\mathbf{D}=\rho
\]

\[
\nabla\cdot\mathbf{B}=0
\]

\[
\nabla\times\mathbf{E}
=
-\frac{\partial\mathbf{B}}{\partial t}
\]

\[
\nabla\times\mathbf{H}
=
\mathbf{J}
+
\frac{\partial\mathbf{D}}{\partial t}
\]

where:

- \(\mathbf{E}\) is electric field.
- \(\mathbf{H}\) is magnetic field.
- \(\mathbf{D}\) is electric flux density.
- \(\mathbf{B}\) is magnetic flux density.
- \(\mathbf{J}\) is current density.
- \(\rho\) is electric charge density.

Implement appropriate constitutive relationships:

\[
\mathbf{D}=\boldsymbol{\epsilon}\mathbf{E}
\]

\[
\mathbf{B}=\boldsymbol{\mu}\mathbf{H}
\]

\[
\mathbf{J}=\boldsymbol{\sigma}\mathbf{E}
\]

Use tensor formulations when anisotropic materials are supported.

Document the time-harmonic convention explicitly.

For frequency-domain solvers, define whether the implementation uses:

\[
e^{j\omega t}
\]

or:

\[
e^{-j\omega t}
\]

Maintain consistent phase, propagation, and complex-field conventions throughout the system.

Do not mix phasor conventions between solvers.

## 5. RF source model

Introduce a first-class **Electromagnetic Source** object.

Support:

- Stable ID.
- Position.
- Orientation.
- Frequency.
- Amplitude.
- Phase.
- Time delay.
- Waveform.
- Polarization.
- Source type.
- Active/inactive state.
- Optional physical dimensions.
- Optional source impedance.

Support initial idealized source models where mathematically appropriate.

Distinguish between:

- Mathematical point sources.
- Idealized current sources.
- Electric dipoles.
- Magnetic dipoles.
- Hertzian dipoles.
- Plane-wave excitation.
- Port excitation.
- Physical antenna models.

Do not represent a physical antenna as an idealized point source without identifying the approximation.

Prepare for real antenna feed structures and port definitions.

## 6. Antenna engineering subsystem

Introduce a dedicated **Antenna** scene object.

An antenna should support:

- Stable ID.
- Antenna category.
- Geometry.
- Feed position.
- Feed type.
- Operating frequency range.
- Polarization.
- Orientation.
- Impedance information.
- Radiation model.
- Material assignments.
- Optional measured data.
- Optional calibration information.

Implement initial antenna design models:

- Half-wave dipole.
- Monopole.
- Quarter-wave monopole.
- Simple loop antenna.
- Helical antenna.
- Patch antenna.
- Horn antenna.
- Yagi-Uda antenna.
- Basic aperture antenna.

The initial priority should be dipoles, monopoles, and simple antenna models with well-understood analytical references.

Other antenna types may initially be represented parametrically, but must not be presented as accurately simulated until their electromagnetic behavior is supported.

For each antenna, distinguish:

- Geometric design.
- Idealized analytical model.
- Numerical electromagnetic model.
- Measured antenna model.

Allow physical dimensions and operating frequency to be edited.

Provide useful initial design estimates based on wavelength, while clearly identifying their approximations.

## 7. Antenna feed and impedance modeling

Implement antenna feed definitions.

Support:

- Lumped-port concepts.
- Voltage excitation.
- Current excitation.
- Wave-port concepts where applicable.
- Feed position.
- Feed impedance.
- Reference impedance.
- Port orientation.

Introduce impedance analysis.

Support:

- Input impedance.
- Resistance.
- Reactance.
- Complex impedance.
- Reflection coefficient.
- Return loss.
- VSWR.
- Frequency-dependent impedance.

Use consistent reference impedance conventions.

For example, define the reflection coefficient:

\[
\Gamma=\frac{Z_{in}-Z_0}{Z_{in}+Z_0}
\]

and calculate VSWR where meaningful:

\[
VSWR=\frac{1+|\Gamma|}{1-|\Gamma|}
\]

Clearly distinguish ideal analytical estimates from numerically calculated feed impedance.

Prepare for multiport systems and S-parameter matrices.

## 8. Antenna radiation analysis

Implement antenna radiation analysis.

Support:

- Radiation patterns.
- Directivity.
- Gain.
- Realized gain where supported.
- Radiation efficiency.
- Polarization.
- Main-lobe direction.
- Beamwidth.
- Sidelobe levels.
- Front-to-back ratio.
- Near-field distribution.
- Far-field distribution.

Support 2D angular cuts:

- Azimuth.
- Elevation.

Support 3D radiation-pattern visualization.

Clearly distinguish:

- Directivity.
- Gain.
- Realized gain.
- Radiation efficiency.
- Relative normalized radiation pattern.

Do not label a normalized pattern as absolute gain.

Use appropriate definitions for radiated power and radiation intensity.

Include polarization information.

For example, support co-polarized and cross-polarized field components where the selected antenna model permits them.

## 9. Near-field and far-field analysis

Implement distinct near-field and far-field analysis capabilities.

Support:

- Reactive near-field visualization where calculated.
- Radiating near-field analysis.
- Far-field radiation patterns.
- Electric field distribution.
- Magnetic field distribution.
- Poynting vector.
- Time-averaged power flow.
- Polarization analysis.

Use physically appropriate far-field approximations and applicability criteria.

Do not assume every point outside a fixed distance is automatically in the far field.

For antenna radiation, provide the user with a documented far-field distance estimate based on antenna dimensions and wavelength where applicable.

Allow users to inspect field magnitude and phase across spatial planes and volumes.

Keep near-field and far-field quantities clearly identified.

## 10. Antenna arrays

Implement an **Electromagnetic Array** scene object.

Support:

- Linear arrays.
- Rectangular planar arrays.
- Circular arrays.
- Cylindrical arrays.
- Spherical arrays.
- Custom element placement.

Each array must support:

- Element count.
- Element spacing.
- Array geometry.
- Array orientation.
- Element type.
- Element frequency.
- Element amplitude.
- Element phase.
- Element delay.
- Element polarization.
- Individual element activation.
- Element feed configuration.

Allow individual antennas to be selected and modified after array generation.

Changing array geometry must not unnecessarily destroy per-element configurations.

Support element patterns and array geometry as separate concepts.

## 11. Array-factor calculations

Implement an array-factor engine.

For an idealized array, calculate the combined far-field response using element positions and complex excitation weights.

A representative expression is:

\[
AF(\hat{\mathbf{r}})
=
\sum_{n=1}^{N}
w_n e^{jk\hat{\mathbf{r}}\cdot\mathbf{r}_n}
\]

where:

- \(w_n\) is the complex excitation weight.
- \(k\) is the wavenumber.
- \(\hat{\mathbf{r}}\) is the observation direction.
- \(\mathbf{r}_n\) is the element position.

Combine the array factor with individual element patterns where appropriate.

Support:

- Uniform excitation.
- Amplitude tapering.
- Phase tapering.
- Steering phase.
- Element failure or deactivation.
- Custom excitation weights.

Clearly identify the assumptions of idealized array-factor modeling.

An array-factor model does not automatically account for:

- Mutual coupling.
- Element impedance changes.
- Feed-network effects.
- Surface currents.
- Structural interactions.
- Scattering from neighboring elements.

Do not present array-factor results as full-wave array simulations.

## 12. Beam steering and beamforming

Implement active beam steering.

Allow users to define a desired pointing direction.

Calculate the required per-element phase settings for narrowband steering.

Support:

- Azimuth steering.
- Elevation steering.
- Two-dimensional beam steering.
- Amplitude tapering.
- Phase tapering.
- Element weighting.
- Multiple array configurations.

Implement true time-delay configuration as a distinct control method.

Do not represent broadband time-delay steering as a fixed phase offset.

Support steering across a configurable frequency range.

Show:

- Desired steering direction.
- Calculated beam direction.
- Main-lobe direction.
- Beamwidth.
- Sidelobe level.
- Grating-lobe locations where identifiable.

Provide warnings when element spacing and frequency create significant grating-lobe concerns.

## 13. Multiple beams and field constraints

Introduce an electromagnetic target-field representation.

Support:

- Point targets.
- Directional targets.
- Multiple beam directions.
- Desired field magnitude.
- Desired phase.
- Suppression directions.
- Spatial regions.
- Null placement.

For the initial implementation, support multiple directional targets where the selected array model permits.

Introduce inverse-design interfaces.

For sampled field constraints, construct a suitable complex propagation or array-response matrix:

\[
Aw\approx b
\]

Solve for complex element weights using a stable least-squares method.

Support regularization and configurable amplitude constraints.

Report residual error and identify infeasible target configurations.

Do not imply that an arbitrary target field can be achieved with a finite array.

## 14. Electromagnetic propagation

Implement initial propagation models.

Support appropriate analytical models for:

- Plane waves.
- Spherical waves.
- Idealized dipoles.
- Simple antenna radiation.
- Free-space propagation.

Where appropriate, calculate:

- Electric field.
- Magnetic field.
- Phase.
- Power density.
- Poynting vector.

Implement free-space path calculations with clearly documented assumptions.

Prepare for propagation through:

- Dielectric materials.
- Conductive materials.
- Layered media.
- Interfaces.
- Obstacles.
- Reflectors.
- Waveguides.

Do not treat free-space propagation models as substitutes for general electromagnetic interaction with geometry.

## 15. Electromagnetic material interactions

Implement the architecture for:

- Reflection.
- Transmission.
- Refraction.
- Scattering.
- Absorption.
- Conductor losses.
- Dielectric losses.
- Surface currents.

Support appropriate boundary conditions for electromagnetic solvers.

Distinguish:

- Perfect electric conductor.
- Perfect magnetic conductor where relevant.
- Finite-conductivity conductor.
- Dielectric interface.
- Impedance boundary.
- Periodic boundary.
- Radiation boundary.

For interfaces, implement physically consistent continuity conditions.

Do not use arbitrary reflection coefficients where the actual material and geometry should determine the interaction.

## 16. Numerical electromagnetic solvers

The analytical and array-factor models are only the beginning.

Establish a modular numerical solver architecture.

Investigate and integrate appropriate methods:

**Finite Difference Time Domain**
- Time-domain Maxwell equations.
- Broadband propagation.
- Transient electromagnetic fields.
- Scattering.
- Conductive and dielectric media.

**Finite Element Method**
- Frequency-domain electromagnetic analysis.
- Complex geometry.
- Material variation.
- Resonant structures.
- Waveguides and cavities.

**Method of Moments**
- Antenna radiation and scattering.
- Surface-current formulations.
- Conducting structures.
- Appropriate integral-equation problems.

**Finite Integration Technique**
- Time- and frequency-domain electromagnetic formulations where appropriate.

**Transmission-line and modal methods**
- Transmission lines.
- Waveguides.
- Resonant cavities.
- Multimode propagation.

Do not force all methods into one solver.

For each method, document:
- Governing equations.
- Supported dimensions.
- Material assumptions.
- Boundary conditions.
- Mesh requirements.
- Numerical stability.
- Convergence.
- Computational cost.

Prioritize integrating or implementing a validated full-wave electromagnetic method for a limited set of configurations.

Do not claim full-wave accuracy for an array-factor calculation.

## 17. Electromagnetic waveguides and resonant structures

Introduce electromagnetic waveguide and cavity objects.

Support geometry for:

- Rectangular waveguides.
- Circular waveguides.
- Coaxial structures.
- Parallel-plate waveguides.
- Dielectric waveguides.
- Resonant cavities.

Where appropriate, calculate analytical cutoff frequencies and modal properties.

For a rectangular waveguide, implement the relevant analytical mode calculations under documented assumptions.

Support:

- TE modes.
- TM modes.
- Propagation constants.
- Cutoff frequencies.
- Characteristic impedance where applicable.
- Mode visualization where supported.

Prepare for numerical analysis of complex waveguide geometry.

Distinguish analytical ideal-waveguide calculations from full numerical field simulation.

## 18. Transmission lines and RF networks

Introduce a transmission-line subsystem.

Support:

- Coaxial lines.
- Microstrip.
- Stripline.
- Coplanar waveguide.
- Ideal transmission-line models.

Provide calculations for:

- Characteristic impedance.
- Propagation constant.
- Wavelength in the medium.
- Phase velocity.
- Attenuation.
- Reflection coefficient.
- Standing-wave ratio.
- Input impedance.
- Matching networks.

Prepare for multiport network analysis.

Support S-parameter data import and export where practical.

Allow users to connect antenna ports and transmission-line components conceptually.

Do not turn Resonant into a full circuit simulator unless a later milestone justifies that expansion.

## 19. Electromagnetic field visualization

Connect electromagnetic results to the OpenGL rendering subsystem.

Visualize actual numerical or analytical field results.

Implement:

**Electric field**
- Ex, Ey, Ez.
- Magnitude.
- Phase.
- Polarization.

**Magnetic field**
- Hx, Hy, Hz.
- Magnitude.
- Phase.

**Power flow**
- Poynting vector.
- Power density.
- Directional flow.

**Antenna visualization**
- 2D radiation patterns.
- 3D radiation patterns.
- Near-field slices.
- Far-field angular maps.
- Array element excitation.
- Beam direction.

**Wave propagation**
- Animated electromagnetic fields.
- Phase progression.
- Transient propagation.
- Field interaction with supported structures.

Provide selectable field components, color scales, normalization, and visualization controls.

Do not use decorative radiation lobes or generic wave graphics as substitutes for calculated results.

Ensure field visualization remains separate from the numerical solver.

## 20. RF measurements and experimental calibration preparation

Prepare the electromagnetic subsystem for real RF measurement data.

Introduce data models for:

- Antenna measurements.
- S-parameters.
- Return loss.
- VSWR.
- Radiation patterns.
- Field-strength measurements.
- Frequency sweeps.
- Phase measurements.
- Antenna calibration.
- Measurement uncertainty.

Support importing standard or documented formats where practical.

Allow simulated and measured results to be compared.

Prepare for future integration with:

- Vector network analyzers.
- Spectrum analyzers.
- Signal generators.
- Software-defined radios.
- RF power meters.
- Near-field probes.
- Antenna measurement systems.

Do not implement uncontrolled RF hardware transmission in this milestone.

The architecture should support safe, explicitly configured measurement and calibration workflows.

## 21. Electromagnetic optimization

Introduce inverse-design and optimization interfaces.

Support optimization variables such as:

- Antenna dimensions.
- Element spacing.
- Array geometry.
- Element amplitude.
- Element phase.
- Element delay.
- Feed position.
- Material properties.
- Matching-network parameters.
- Reflector geometry.

Support objectives such as:

- Maximize gain in a target direction.
- Improve impedance matching.
- Reduce sidelobes.
- Improve beamwidth.
- Place radiation nulls.
- Improve efficiency.
- Reduce unwanted radiation.
- Shape near-field distributions.

Use appropriate optimization methods.

Do not optimize geometry using a simplified model and imply that the result is guaranteed to work in a full-wave physical design.

Clearly identify the model used for every optimization run.

## 22. Performance and compute backends

Begin with a correct CPU reference implementation.

Investigate:

- Sparse matrix methods.
- Eigen-based linear algebra.
- OpenMP.
- SIMD.
- CUDA.
- OpenCL or other suitable compute backends.
- GPU-accelerated FDTD.
- GPU-accelerated field evaluation.

Do not make the electromagnetic physics engine dependent on one GPU vendor.

Keep solver physics independent from compute execution.

Validate accelerated results against CPU reference calculations.

Report precision, convergence, and numerical tolerances.

## 23. Resource management and numerical preflight

Electromagnetic simulations can become computationally expensive, particularly at high frequencies and in three dimensions.

Implement preflight estimates for:

- Wavelength.
- Mesh resolution.
- Grid dimensions.
- Number of degrees of freedom.
- Time-step count.
- Memory.
- Matrix storage.
- Expected computational workload.
- Field recording requirements.

For explicit time-domain solvers, enforce the appropriate stability condition.

For frequency-domain solvers, estimate matrix and memory requirements before assembly.

Warn about:
- Insufficient wavelength resolution.
- Invalid material frequency ranges.
- Unstable timesteps.
- Excessive mesh sizes.
- Inadequate boundary treatment.
- Insufficient simulation duration.
- Unresolved geometric features.

Do not silently lower resolution or alter solver settings.

## 24. Reproducibility and data management

Every electromagnetic simulation must retain:

- Project ID.
- Simulation ID.
- Solver name and version.
- Electromagnetic formulation.
- Frequency or frequency range.
- Material definitions.
- Antenna and source configurations.
- Array configuration.
- Boundary conditions.
- Mesh configuration.
- Numerical settings.
- Execution backend.
- Precision settings.
- Result metadata.
- Model assumptions.

Keep large numerical results separate from project metadata.

Use HDF5 or another appropriate scientific data format for large field datasets.

Support reproducible reruns and comparison between simulation versions.

## 25. Validation and scientific credibility

Implement a serious electromagnetic validation program.

Validate:

- Free-space plane-wave propagation.
- Spherical-wave propagation where applicable.
- Dipole radiation.
- Monopole radiation.
- Antenna polarization.
- Radiation patterns.
- Array-factor calculations.
- Beam steering.
- Grating-lobe predictions.
- Waveguide cutoff frequencies.
- Transmission-line impedance.
- Reflection and transmission at material interfaces.
- Conductor losses where implemented.
- Resonant cavity behavior.
- Numerical solver convergence.

Use analytical solutions and independent reference calculations.

For full-wave solvers, perform mesh-refinement and numerical convergence studies.

Compare against trusted external tools or published benchmark problems where possible.

Document:
- Error metrics.
- Solver tolerances.
- Mesh resolution.
- Boundary conditions.
- Material data.
- Validity assumptions.

Do not use visual similarity as proof of electromagnetic accuracy.

## 26. Example projects

Create reproducible example projects.

**Example A — Dipole antenna**
- Half-wave dipole.
- Configurable frequency.
- Radiation pattern.
- Polarization.
- Impedance analysis where supported.

**Example B — Monopole antenna**
- Quarter-wave monopole.
- Ground-plane representation.
- Radiation pattern.
- Analytical comparison.

**Example C — Linear antenna array**
- Configurable element count and spacing.
- Array-factor calculation.
- Radiation-pattern visualization.
- Sidelobe analysis.

**Example D — Phased-array beam steering**
- Planar array.
- Adjustable steering direction.
- Calculated element phase.
- Beam direction verification.
- Grating-lobe analysis.

**Example E — Near-field visualization**
- Antenna model.
- Spatial field region.
- Electric and magnetic field visualization.
- Probe measurements.

**Example F — Rectangular waveguide**
- Configurable dimensions.
- TE/TM mode analysis.
- Cutoff frequency calculation.
- Field visualization where supported.

**Example G — Electromagnetic full-wave validation**
- A tractable antenna or scattering geometry.
- Numerical solution.
- Analytical or independent reference.
- Mesh-convergence demonstration.

All examples must use actual implemented calculations.

Clearly identify whether results are analytical, array-factor, numerical full-wave, or imported measurement data.

## 27. Documentation

Create or update:

- ELECTROMAGNETIC_PHYSICS.md
- MAXWELL_EQUATIONS.md
- ELECTROMAGNETIC_MATERIALS.md
- ANTENNA_ENGINEERING.md
- ANTENNA_ARRAYS.md
- ARRAY_FACTOR_AND_BEAMFORMING.md
- NEAR_FIELD_FAR_FIELD.md
- RF_IMPEDANCE_AND_S_PARAMETERS.md
- TRANSMISSION_LINES.md
- WAVEGUIDES_AND_CAVITIES.md
- ELECTROMAGNETIC_SOLVERS.md
- ELECTROMAGNETIC_VALIDATION.md
- PERFORMANCE_AND_GPU.md
- IMPLEMENTATION_STATUS.md
- DECISIONS.md

Document governing equations, conventions, units, material assumptions, solver capabilities, numerical limitations, and validation results.

## 28. Implementation sequence

**Phase A — Electromagnetic domain architecture**
- Domain models.
- Material properties.
- Source models.
- Antenna models.
- Solver interfaces.

**Phase B — Analytical RF foundation**
- Free-space propagation.
- Dipole models.
- Monopole models.
- Field calculations.
- Radiation patterns.

**Phase C — Antenna array system**
- Parametric array geometry.
- Array factor.
- Element weighting.
- Beam steering.
- Sidelobe analysis.

**Phase D — RF analysis**
- Feed impedance.
- Reflection coefficient.
- Return loss.
- VSWR.
- Polarization.
- Near-field and far-field analysis.

**Phase E — Transmission lines and waveguides**
- Analytical models.
- Characteristic impedance.
- Cutoff frequencies.
- Mode calculations.

**Phase F — Full-wave solver framework**
- Solver interfaces.
- Material interactions.
- Boundary conditions.
- Mesh integration.
- Resource estimation.

**Phase G — Full-wave numerical implementation or integration**
- Prioritized FDTD, FEM, or Method of Moments implementation.
- Field calculation.
- Validation.
- Example geometry.

**Phase H — Advanced array analysis**
- More realistic element patterns.
- Mutual coupling where supported.
- Multiple targets.
- Inverse design.

**Phase I — Optimization and experimental preparation**
- Antenna geometry optimization.
- Array optimization.
- Measurement import.
- Calibration structures.

**Phase J — Verification**
- Analytical comparisons.
- Numerical convergence.
- Example projects.
- Performance tests.
- Documentation.

Work incrementally.

A validated antenna and phased-array workflow is more valuable than a broad collection of incomplete electromagnetic features.

## 29. Acceptance criteria

This milestone is complete only when:

1. Electromagnetic physics is implemented as a distinct modular domain.
2. Electromagnetic material properties are supported.
3. RF source objects can be created and configured.
4. Initial antenna models can be designed.
5. Analytical antenna radiation models work.
6. Radiation patterns can be visualized.
7. Near-field and far-field analysis are clearly distinguished.
8. Antenna arrays can be generated parametrically.
9. Array-factor calculations are validated.
10. Beam steering produces the expected pointing direction.
11. Grating-lobe warnings work for supported configurations.
12. Basic impedance analysis works.
13. Transmission-line and waveguide analytical calculations work where implemented.
14. At least one full-wave electromagnetic numerical solver is implemented or successfully integrated and validated.
15. Actual electromagnetic field components can be visualized.
16. Material and geometry interactions are supported by the selected numerical solver.
17. Results retain reproducibility metadata.
18. Numerical accuracy is independently evaluated.
19. Automated tests pass.
20. Existing acoustic and mechanical capabilities remain functional.
21. Documentation distinguishes analytical models from full-wave results.

## 30. Critical development rules

- Electromagnetic physics must remain separate from acoustic and mechanical physics.
- Use Maxwell's equations as the physical foundation.
- Maintain consistent phasor and phase conventions.
- Do not confuse array-factor calculations with full-wave simulations.
- Do not confuse gain, directivity, and realized gain.
- Do not label normalized fields as absolute field strength.
- Do not invent material properties.
- Do not ignore antenna feed and impedance effects when claiming realistic antenna performance.
- Do not claim mutual coupling is modeled unless it is actually calculated.
- Do not claim far-field accuracy for near-field approximations.
- Do not silently reduce mesh resolution.
- Validate numerical methods independently.
- Preserve the existing application and physics subsystems.
- Keep numerical computation separate from visualization.
- Maintain reproducibility.
- Record architectural decisions and known limitations.

**The goal is to make Resonant a capable RF and electromagnetic engineering environment: design antennas, create arrays, steer beams, calculate electromagnetic fields, analyze propagation, model physical interactions, and progressively advance toward full-wave electromagnetic simulation.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Implement, capabilities, implement, milestone
  - evidence: 'milestone' [body]; 'introduces' [body]; 'objective' [body]; 'Build' [body]; 'Implement' [head]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: Seventh ticket, submitted by the user in chat while REQ-CHAT-20261126-002 was being processed, with the instruction to process it in order (after REQ-CHAT-20261126-006). Third physics domain; builds on the application foundation (002) and the solver framework (005). Standing operator instructions for all tickets: keep qmake support in addition to CMake; Qt user interfaces use .ui files.

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

Seventh ticket, submitted by the user in chat while REQ-CHAT-20261126-002 was being processed, with the instruction to process it in order (after REQ-CHAT-20261126-006). Third physics domain; builds on the application foundation (002) and the solver framework (005). Standing operator instructions for all tickets: keep qmake support in addition to CMake; Qt user interfaces use .ui files.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000008
- Evidence: pending
