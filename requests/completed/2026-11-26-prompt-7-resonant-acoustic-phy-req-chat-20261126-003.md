# Request: # PROMPT 7 — RESONANT: ACOUSTIC PHYSICS ENGINE AND WAVE-FIELD SIMULATION

You ar

**Request ID:** REQ-CHAT-20261126-003
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T15:13:49-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 7 — RESONANT: ACOUSTIC PHYSICS ENGINE AND WAVE-FIELD SIMULATION

You are continuing development of **RESONANT — Wave Field Engineering Studio**.

This milestone introduces the first real physics domain: **acoustic wave simulation**.

The objective is to transform the application from a general-purpose engineering scene editor into a functional acoustic wave-field design and experimentation platform.

This is not a visualization mockup. Implement real, mathematically grounded acoustic calculations and connect them to the existing scene, geometry, material, simulation, and rendering architectures.

Build on the existing codebase. Do not replace or unnecessarily redesign the application foundation.

---

## 1. Mission and scope

Implement a modular acoustic physics subsystem capable of:

- Defining acoustic emitters.
- Creating and configuring emitter arrays.
- Calculating acoustic propagation.
- Modeling interference and superposition.
- Calculating phase and time-delay compensation.
- Focusing acoustic energy at specified locations.
- Defining multiple target locations.
- Calculating acoustic fields across 2D and 3D regions.
- Visualizing pressure, amplitude, phase, and intensity.
- Comparing different emitter configurations.
- Saving and reproducing simulation configurations.

The initial implementation should prioritize frequency-domain analytical acoustics.

Do not attempt to implement a complete general-purpose acoustics solver in this milestone.

The architecture must permit later integration of FDTD, FEM, BEM, transient propagation, diffraction, reflections, and more advanced physical models.

## 2. Acoustic physics architecture

Create a dedicated acoustic physics module within the existing modular architecture.

Separate:

- Acoustic domain data.
- Acoustic source definitions.
- Propagation models.
- Field calculations.
- Focusing algorithms.
- Simulation configuration.
- Numerical execution.
- Result storage.
- Visualization interfaces.

Do not place physics calculations in Qt widgets or OpenGL rendering code.

The acoustic engine must be usable independently of the GUI.

Use Eigen for numerical operations where appropriate.

Keep solver implementations interchangeable through well-defined interfaces.

At minimum, define interfaces for:

- Analytical propagation.
- Point-source propagation.
- Directional-source propagation.
- Array-factor calculations.
- Acoustic field evaluation.
- Inverse field design.

The initial implementation should use the analytical propagation approach described below.

## 3. Acoustic emitter model

Introduce a first-class scene object called an **Acoustic Emitter**.

Each emitter must support:

- Stable ID.
- Position.
- Orientation.
- Operating frequency.
- Output amplitude.
- Initial phase.
- Time delay.
- Waveform designation.
- Directivity model.
- Active/inactive state.
- Optional physical dimensions.
- Optional assigned material or transducer profile.

Support at least these source models:

1. Idealized omnidirectional point source.
2. Directional point source.
3. Simple piston source approximation where practical.

Clearly distinguish idealized mathematical emitters from physical transducers.

Do not assume every physical transducer behaves like an ideal point source.

The emitter data model must eventually support measured frequency response, phase response, directivity, impedance, and calibration data.

### Emitter orientation

Emitter orientation must affect directional radiation models.

Define a consistent local forward axis and document it.

Use explicit coordinate transforms between local emitter space and world space.

## 4. Emitter arrays

Implement an **Acoustic Array** scene object.

An array is a collection of independently configurable emitters.

Support these parametric array layouts:

- Linear.
- Rectangular planar.
- Circular.
- Cylindrical.
- Spherical.
- Custom/manual placement.

Provide array configuration controls:

- Number of elements.
- Element spacing.
- Array dimensions.
- Array center.
- Array orientation.
- Element frequency.
- Element amplitude.
- Element phase.
- Element delay.
- Element activation.

For planar arrays, allow horizontal and vertical element counts and spacing to be configured independently.

For circular arrays, support radius and element count.

For spherical arrays, support a reasonable initial distribution algorithm and document its limitations.

Allow individual elements to be selected and modified after array generation.

Changing an array parameter should update its generated geometry and emitter positions predictably.

Do not destroy individual element settings unnecessarily when changing array parameters. Define and document how generated array configuration interacts with per-element overrides.

## 5. Acoustic material properties

Connect the acoustic subsystem to the existing material architecture.

Support acoustic properties including:

- Density.
- Speed of sound.
- Acoustic impedance.
- Attenuation.
- Absorption.
- Frequency dependence where available.
- Temperature dependence where available.

For the initial homogeneous-medium analytical solver, use an explicitly defined propagation medium.

Start with air and water as supported examples.

Allow the user to configure temperature and other relevant medium parameters when supported by the chosen model.

Do not assume that the properties of the material assigned to a solid object automatically define the propagation medium.

Distinguish:

- Propagation medium.
- Solid material.
- Reflector material.
- Transducer material.

For this milestone, solid geometry may be present in the scene without necessarily influencing the calculated acoustic field.

Clearly indicate when the simulation is using a free-field approximation and ignoring object interactions.

Do not pretend to simulate reflection, transmission, refraction, or scattering when those mechanisms are not implemented.

## 6. Analytical acoustic propagation

Implement a real frequency-domain acoustic propagation model.

For a harmonic point source in a homogeneous free-field medium, use an appropriate Green's function formulation.

A representative pressure-field model is:

\[
p_i(\mathbf{x}) =
A_i \frac{e^{-jkr_i}}{r_i}
\]

where:

- \(A_i\) represents the source's complex amplitude under the selected normalization.
- \(r_i\) is the distance from emitter \(i\) to the evaluation point.
- \(k=2\pi f/c\) is the acoustic wavenumber.
- \(f\) is frequency.
- \(c\) is sound speed.

The total complex pressure field is obtained by superposition:

\[
p(\mathbf{x})=\sum_{i=1}^{N}p_i(\mathbf{x})
\]

Implement this model with explicit documentation of:

- Source amplitude normalization.
- Pressure units.
- Phase convention.
- Frequency convention.
- Propagation assumptions.
- Validity limitations.
- Treatment of near-source singularities.

Do not use the simplified equation without considering source normalization and units.

Implement a consistent reference-amplitude convention so that changing emitter count does not silently change the meaning of displayed amplitude.

Provide an appropriate near-field handling strategy. Do not allow division by zero or numerical instability when an evaluation point coincides with an emitter.

### Propagation model options

The initial engine should support:

- Ideal spherical spreading.
- Optional attenuation.
- Optional source directivity.
- Complex phase propagation.
- Multiple-source superposition.

Use a well-defined propagation interface so additional models can be added later.

## 7. Acoustic field calculations

Implement a reusable acoustic field evaluator.

It must calculate field quantities at arbitrary world coordinates.

Support:

- Complex acoustic pressure.
- Pressure magnitude.
- Pressure phase.
- RMS pressure under the documented harmonic convention.
- Time-averaged intensity where the required assumptions are satisfied.

For a progressive plane-wave approximation in a homogeneous medium, intensity can be related to pressure using acoustic impedance. Do not blindly apply that relationship to arbitrary standing-wave or reactive near-field conditions.

Where the simple relationship is not valid, either implement the necessary particle-velocity calculation or clearly label the result as an approximation.

Maintain SI units internally.

The user interface should clearly distinguish:

- Pressure amplitude.
- RMS pressure.
- Sound pressure level.
- Intensity.
- Relative normalized field strength.

Never label relative amplitude as absolute acoustic pressure.

## 8. Acoustic focusing

Implement the first real active acoustic focusing system.

The user must be able to select a target point in the 3D viewport or enter its coordinates numerically.

For an emitter at \(\mathbf{x}_i\) and target \(\mathbf{x}_t\):

\[
r_i=\|\mathbf{x}_t-\mathbf{x}_i\|
\]

Calculate propagation distance and phase compensation for every emitter.

Support two distinct control strategies:

**Narrowband phase focusing**
- Calculate per-element phase compensation at a selected frequency.
- Respect the engine's phase sign convention.
- Allow amplitude control separately.

**True time-delay focusing**
- Calculate relative propagation delays.
- Support broadband-compatible delay control conceptually and in the source configuration.
- Do not represent a fixed phase shift as a true broadband time delay.

Provide a configurable focus operation.

The user should be able to:

1. Select an array.
2. Specify a target location.
3. Choose phase or time-delay control.
4. Calculate element settings.
5. Apply the settings.
6. Run a field simulation.
7. Inspect the resulting focus.

Do not guarantee a perfect focus. The actual field must be calculated from the propagation model.

Support optional amplitude weighting and tapering.

## 9. Multiple targets and field constraints

Introduce a target-field model.

Targets should eventually support:

- Single points.
- Multiple points.
- Lines.
- Planes.
- Surfaces.
- Volumetric regions.
- Desired pressure amplitude.
- Desired phase.
- Field suppression regions.

For this milestone, implement point targets and multiple point targets.

Provide the data model for the additional target types without pretending they are already solved.

Allow users to assign target priorities or weights where appropriate.

Design the target representation so future inverse-design algorithms can use it directly.

## 10. Initial inverse-design solver

Implement a first inverse-design capability for multiple target points.

Construct a propagation matrix:

\[
H_{mi}=G(\mathbf{x}_m,\mathbf{x}_i)
\]

where \(G\) is the selected propagation response between emitter \(i\) and target point \(m\).

Solve for complex emitter weights:

\[
Hw \approx b
\]

where:

- \(H\) is the propagation matrix.
- \(w\) is the vector of emitter drive weights.
- \(b\) is the desired complex field at the targets.

Implement a least-squares solution using Eigen.

Support regularization where appropriate:

\[
\min_w \|Hw-b\|_2^2+\lambda\|w\|_2^2
\]

Requirements:

- Numerically stable solution.
- Configurable regularization.
- Error reporting.
- Residual measurement.
- Per-element complex weights.
- Conversion into amplitude and phase settings.
- Configurable amplitude limits.
- Clear indication when requested targets cannot be achieved accurately.

Do not use explicit matrix inversion when a numerically stable decomposition or solver is appropriate.

For this milestone, prioritize amplitude-and-phase control. Phase-only optimization can be added as a later enhancement.

## 11. Simulation region and sampling

Introduce a simulation-region object.

Support:

- 2D XY plane.
- 2D XZ plane.
- 2D YZ plane.
- Arbitrary 2D plane where practical.
- 3D rectangular volume.

Allow the user to configure:

- Region origin.
- Dimensions.
- Sampling resolution.
- Frequency.
- Propagation medium.
- Selected emitters or array.
- Calculation model.

Provide preflight validation.

Estimate:

- Number of evaluation points.
- Memory consumption.
- Expected computational workload.
- Potentially excessive resolution.

Do not allow an accidental enormous simulation to exhaust system memory.

Implement cancellation and progress reporting for long-running field calculations.

The GUI must remain responsive.

## 12. Rendering and visualization

Connect calculated acoustic fields to the OpenGL rendering subsystem.

The viewport must visualize actual numerical results.

Implement:

**2D field visualization**
- Pressure magnitude.
- Pressure phase.
- Relative intensity.
- Cross-sectional field maps.

**3D field visualization**
- Sampled volumetric field.
- Slices through the volume.
- Adjustable slice positions.
- Isosurfaces or other useful volumetric representations where practical.

**Animation**
- Harmonic instantaneous pressure visualization.
- Animation driven by calculated complex field values.
- User-adjustable animation frequency or playback speed.

Animation must not require recalculating the entire field at every frame. Use the stored frequency-domain solution where applicable.

Support color-scale controls and normalization modes.

Clearly identify whether the visualization shows absolute or normalized values.

Keep numerical computation separate from rendering. Changing camera position, color map, or slice visibility must not trigger unnecessary physics recalculation.

## 13. Analysis and measurements

Introduce an initial acoustic analysis panel.

Display:

- Selected frequency.
- Propagation medium.
- Number of active emitters.
- Simulation dimensions.
- Sampling resolution.
- Maximum pressure magnitude.
- RMS pressure where supported.
- Relative peak location.
- Target-point values.
- Solver execution time.
- Model assumptions.

Implement point probes.

A user should be able to place a probe in the scene and inspect calculated field quantities at that location.

Allow probe results to be exported in a documented machine-readable format.

## 14. Simulation reproducibility

Every simulation result must retain enough metadata to reproduce it.

Store:

- Project ID.
- Simulation ID.
- Solver type and version.
- Acoustic model.
- Frequency.
- Medium properties.
- Emitter configurations.
- Array configuration.
- Target definitions.
- Simulation-region definition.
- Sampling resolution.
- Numerical parameters.
- Execution time.
- Model assumptions.
- Relevant software version.

Changing the scene after a simulation must not silently alter the historical result.

Results should be associated with the configuration that produced them.

Allow the user to run multiple simulations and compare results.

## 15. Performance and numerical quality

The initial solver should be optimized sensibly, without premature complexity.

Requirements:

- Use efficient numerical loops.
- Use Eigen where appropriate.
- Avoid unnecessary allocations.
- Parallelize independent field evaluation points where beneficial.
- Keep computation off the GUI thread.
- Support cancellation.
- Monitor memory use.
- Avoid excessive copies of large field arrays.

Implement numerical validation tests.

Include tests for:

- Single-source spherical propagation.
- Distance-dependent amplitude behavior.
- Phase progression.
- Two-source constructive interference.
- Two-source destructive interference.
- Symmetric array behavior.
- Focus delay calculations.
- Focus phase calculations.
- Multiple-target least squares.
- Regularization behavior.
- Field-grid dimensions.
- Numerical stability near emitters.

Use analytical expectations or independently derived reference calculations wherever possible.

Do not validate a calculation solely by comparing two code paths that share the same implementation.

## 16. User interface integration

Integrate acoustic functionality into the existing application.

Add suitable workspace panels or dockable views for:

- Acoustic emitter properties.
- Array configuration.
- Acoustic medium selection.
- Target definition.
- Focusing controls.
- Simulation configuration.
- Field analysis.
- Probe inspection.

Context-sensitive properties should appear when an acoustic emitter, array, target, or probe is selected.

Support sensible workflows rather than requiring the user to navigate multiple unrelated dialogs.

The interface must clearly communicate which objects are emitters, which are targets, which define simulation regions, and which are visualization objects.

## 17. Example projects

Create a small set of real, reproducible example projects.

At minimum:

**Example A — Two-source interference**
- Two coherent acoustic emitters.
- Configurable separation.
- Demonstration of constructive and destructive interference.

**Example B — Linear array**
- Parametric linear array.
- Adjustable frequency and element spacing.
- Field visualization.

**Example C — Focused planar array**
- Planar array.
- Single point target.
- Calculated phase compensation.
- Field simulation showing the resulting focus.

**Example D — Multiple target points**
- Planar array.
- Multiple target locations.
- Inverse-design solution.
- Field comparison against the requested target values.

All examples must use real calculations from the implemented engine.

Clearly label assumptions and limitations.

## 18. Documentation

Create or update:

- ACOUSTIC_ENGINE.md
- ACOUSTIC_MODELS.md
- ACOUSTIC_ARRAYS.md
- ACOUSTIC_FOCUSING.md
- ACOUSTIC_INVERSE_DESIGN.md
- ACOUSTIC_VALIDATION.md
- IMPLEMENTATION_STATUS.md
- DECISIONS.md

Document mathematical conventions, units, assumptions, numerical methods, and known limitations.

Include equations and examples.

## 19. Implementation sequence

Work in manageable stages.

**Phase A — Acoustic domain model**
- Acoustic emitters.
- Acoustic arrays.
- Medium properties.
- Target points.
- Simulation configuration.

**Phase B — Analytical propagation**
- Green's function.
- Spherical spreading.
- Phase propagation.
- Attenuation.
- Directivity.
- Superposition.

**Phase C — Array generation**
- Linear.
- Rectangular planar.
- Circular.
- Spherical.
- Custom.

**Phase D — Focusing**
- Point-target phase compensation.
- Time-delay compensation.
- Apply settings to arrays.
- Validate resulting field.

**Phase E — Field evaluation**
- 2D sampling.
- 3D sampling.
- Simulation job integration.
- Progress and cancellation.

**Phase F — Inverse design**
- Propagation matrix.
- Least-squares solution.
- Regularization.
- Multiple target points.
- Result validation.

**Phase G — Visualization**
- 2D field maps.
- 3D field visualization.
- Phase display.
- Animation.
- Probe support.

**Phase H — Verification**
- Unit tests.
- Analytical validation.
- Example projects.
- Performance checks.
- Documentation.

Do not skip validation to accelerate implementation.

## 20. Acceptance criteria

This milestone is complete only when:

1. A user can create an acoustic emitter.
2. A user can create a parametric acoustic array.
3. Individual emitters can be configured.
4. A user can configure the propagation medium.
5. The engine calculates a real complex acoustic field.
6. Two-source interference behaves correctly.
7. The user can create and select a target point.
8. The engine calculates focusing phase compensation.
9. The user can apply focusing settings to an array.
10. A simulation demonstrates the resulting field.
11. The user can calculate 2D field maps.
12. The user can calculate a manageable 3D field volume.
13. The user can inspect pressure and phase.
14. The user can run a multiple-target inverse-design calculation.
15. Simulation jobs support progress and cancellation.
16. Results retain reproducibility metadata.
17. Example projects work.
18. Automated tests pass.
19. Numerical validation results are documented.
20. The existing application foundation remains functional.

## 21. Critical development rules

- Implement actual acoustic mathematics, not visual approximations presented as physics.
- Keep acoustic physics independent from GUI and rendering.
- Do not claim solid geometry influences acoustic propagation until the relevant interaction model exists.
- Do not confuse phase shifts with true time delays.
- Do not confuse pressure amplitude with RMS pressure or intensity.
- Do not assume all transducers are ideal point sources.
- Do not silently normalize absolute quantities into relative values.
- Do not fabricate material properties or experimental results.
- Preserve reproducibility.
- Record architectural decisions.
- Keep the implementation modular and testable.
- Do not break previously working functionality.

**The goal is a functional acoustic wave-field engineering system: create sources, configure arrays, calculate propagation, focus energy, solve multiple targets, and inspect the resulting three-dimensional field.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Creating, Implement, implement, integration
  - evidence: 'milestone' [body]; 'introduces' [body]; 'objective' [body]; 'Implement' [body]; 'Build' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: Third ticket, submitted by the user in chat (submit-only; the user asked not to claim or process tickets yet). Depends on REQ-CHAT-20261126-002 (V0.1 application foundation), which must be completed first: this ticket builds on its scene, geometry, material, simulation-job and rendering subsystems. Product vision: REQ-CHAT-20261126-001.

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

Third ticket, submitted by the user in chat (submit-only; the user asked not to claim or process tickets yet). Depends on REQ-CHAT-20261126-002 (V0.1 application foundation), which must be completed first: this ticket builds on its scene, geometry, material, simulation-job and rendering subsystems. Product vision: REQ-CHAT-20261126-001.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000003
- Evidence: pending
