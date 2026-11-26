# Request: # PROMPT 9 — RESONANT: ADVANCED ACOUSTIC SIMULATION AND FULL-WAVE NUMERICAL SOLV

**Request ID:** REQ-CHAT-20261126-005
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T15:16:18-05:00
**Status:** inbox
**Attachments:** - none

## Original User Statement

> # PROMPT 9 — RESONANT: ADVANCED ACOUSTIC SIMULATION AND FULL-WAVE NUMERICAL SOLVERS

You are continuing development of **RESONANT — Wave Field Engineering Studio**.

This milestone advances the acoustic physics engine from analytical and approximate propagation models toward rigorous numerical simulation of real acoustic environments.

The objective is to simulate acoustic wave propagation and interaction with physical geometry, materials, boundaries, and complex environments.

This is a scientific computing milestone. Numerical accuracy, physical validity, solver stability, convergence, and reproducibility are more important than simply producing visually impressive simulations.

Build on the existing Resonant architecture, acoustic engine, Wave Manipulator subsystem, material library, simulation infrastructure, and visualization system.

Do not replace existing functionality. Extend it through modular solver implementations.

---

## 1. Mission and objectives

Implement an advanced acoustic simulation framework supporting multiple numerical methods.

The system should eventually handle:

- Wave propagation in complex environments.
- Diffraction around obstacles.
- Reflection from surfaces.
- Refraction through material boundaries.
- Transmission through structures.
- Scattering from objects.
- Acoustic absorption.
- Resonance.
- Standing waves.
- Broadband signals.
- Transient acoustic events.
- Nonuniform propagation media.
- Layered and composite materials.
- Complex acoustic boundaries.
- Passive and hybrid wave manipulation.

The initial priority is a validated full-wave simulation capability for selected acoustic configurations.

Do not attempt to implement every numerical method or physical effect simultaneously.

Establish the framework, select appropriate initial methods, validate them, and expand incrementally.

## 2. Solver architecture

Create a dedicated numerical acoustics subsystem.

Support multiple independent solver implementations behind a common execution and result-management framework.

Potential numerical methods include:

- Finite Difference Time Domain (FDTD).
- Finite Element Method (FEM).
- Boundary Element Method (BEM).
- Angular Spectrum Method.
- Pseudospectral methods.
- Modal analysis.
- Geometrical acoustics and ray tracing.

Each method must have its own appropriate mathematical formulation.

Do not force fundamentally different numerical methods into one oversized solver class.

Define common interfaces for:

- Solver identification.
- Supported physical models.
- Input validation.
- Geometry and mesh requirements.
- Material requirements.
- Boundary conditions.
- Frequency or time-domain configuration.
- Numerical resolution.
- Resource estimation.
- Execution.
- Progress reporting.
- Cancellation.
- Result generation.
- Validation and error reporting.

A solver must explicitly declare what physical phenomena it supports.

The application must not select a solver that cannot correctly represent the requested configuration.

## 3. Solver selection and applicability

Implement a solver selection and recommendation system.

Solver selection should consider:

- Physical domain.
- Frequency range.
- Wavelength.
- Geometry complexity.
- Simulation dimensions.
- Material variation.
- Boundary conditions.
- Required accuracy.
- Broadband versus narrowband requirements.
- Computational resources.
- Expected memory usage.

For example:

**FDTD**
- Appropriate for transient propagation and broadband behavior.
- Useful for reflections, scattering, and complex interactions.
- Requires careful spatial and temporal resolution.
- Can become expensive in large 3D domains.

**FEM**
- Appropriate for complex geometries and material distributions.
- Useful for frequency-domain acoustics and modal analysis.
- Requires mesh quality and appropriate element formulations.

**BEM**
- Particularly useful for certain exterior acoustic radiation and scattering problems.
- Can reduce the dimensionality of the discretized problem.
- May produce dense system matrices and substantial memory requirements.

**Angular Spectrum**
- Useful for propagation between parallel planes under appropriate assumptions.
- Particularly useful for planar source and field propagation.
- Not a general substitute for arbitrary three-dimensional obstacle interaction.

**Ray methods**
- Useful for certain high-frequency propagation scenarios.
- Cannot reliably capture all diffraction and interference effects.

These are general guidelines, not rigid rules. The application should explain the actual applicability and limitations of each implemented solver.

Do not silently substitute one solver for another when the requested physics is unsupported.

## 4. Finite Difference Time Domain solver

Prioritize a practical, validated acoustic FDTD implementation.

Implement the appropriate acoustic wave equations.

For a homogeneous medium, begin with the classical wave equation:

\[
\nabla^2 p(\mathbf{x},t)
=
\frac{1}{c^2}
\frac{\partial^2 p(\mathbf{x},t)}{\partial t^2}
\]

where:

- \(p\) is acoustic pressure.
- \(c\) is sound speed.
- \(\mathbf{x}\) is position.
- \(t\) is time.

For more complex materials, investigate an appropriate pressure-velocity formulation.

A first-order acoustic formulation may use:

\[
\frac{\partial \mathbf{v}}{\partial t}
=
-\frac{1}{\rho}\nabla p
\]

\[
\frac{\partial p}{\partial t}
=
-K\nabla\cdot\mathbf{v}+s
\]

where:

- \(\mathbf{v}\) is particle velocity.
- \(\rho\) is density.
- \(K\) is bulk modulus.
- \(s\) represents an appropriate source term.

Ensure the implementation uses a mathematically consistent formulation, including the treatment of source terms and material interfaces.

Requirements:

- Two-dimensional FDTD.
- Three-dimensional FDTD.
- Spatial discretization.
- Temporal discretization.
- Pressure and particle-velocity state.
- Source injection.
- Boundary conditions.
- Material properties.
- Simulation duration.
- Field recording.
- Energy monitoring.
- Stability monitoring.

Implement a documented numerical scheme.

Consider staggered-grid formulations where appropriate for pressure and particle velocity.

Do not implement a superficially convenient discretization without analyzing its stability and accuracy.

## 5. Numerical stability and resolution

Numerical stability is a fundamental requirement.

Implement a preflight analysis that evaluates spatial and temporal resolution.

For a given frequency and propagation speed:

\[
\lambda=\frac{c}{f}
\]

The spatial resolution must be sufficient to resolve the shortest relevant wavelength.

Allow the user to configure points per wavelength within the limits of the selected numerical method.

Do not use one universal resolution setting for every solver.

For explicit time-domain methods, enforce the applicable Courant-Friedrichs-Lewy stability condition.

For a conventional Cartesian grid, the maximum stable timestep depends on spatial resolution, dimensionality, and wave speed. Implement the correct condition for the actual discretization.

The preflight system must estimate:

- Grid dimensions.
- Total number of cells.
- Number of timesteps.
- Memory requirements.
- Expected computational work.
- Field recording requirements.
- Approximate execution time where a defensible estimate is possible.

Warn about:

- Insufficient wavelength resolution.
- Unstable timestep.
- Excessive simulation dimensions.
- Excessive memory consumption.
- Inadequate simulation duration.
- Insufficient observation time for the desired steady-state analysis.

Do not automatically reduce accuracy without telling the user.

Provide explicit choices to adjust resolution or accept a documented approximation.

## 6. Boundary conditions and absorbing layers

Implement a boundary-condition architecture.

Support, as appropriate to each solver:

- Rigid boundaries.
- Pressure-release boundaries.
- Impedance boundaries.
- Absorbing boundaries.
- Periodic boundaries.
- Symmetry boundaries.

For FDTD, implement a practical absorbing boundary approach.

Investigate Perfectly Matched Layers (PML) or an appropriate acoustic equivalent.

Requirements:

- Configurable absorbing-layer thickness.
- Damping profile.
- Boundary configuration.
- Reflection testing.
- Stability validation.

The user must be able to distinguish physical reflections from unwanted reflections caused by the edge of the computational domain.

Do not represent an arbitrary damped boundary as a mathematically validated PML.

Document limitations and supported configurations.

## 7. Complex material interactions

Extend acoustic material simulation beyond homogeneous free-field propagation.

Support spatially varying:

- Density.
- Sound speed.
- Bulk modulus.
- Acoustic impedance.
- Attenuation.
- Absorption.
- Frequency-dependent behavior where supported.

Implement appropriate treatment of material interfaces.

Support:

- Reflection.
- Transmission.
- Refraction.
- Scattering.
- Energy absorption.

The implementation must respect the relevant physical continuity and boundary conditions.

Do not approximate a material interface by simply changing a color or multiplying field amplitude by an arbitrary coefficient.

Support layered materials and multiple material regions.

Prepare for anisotropic and dispersive materials, but do not claim those capabilities until an appropriate formulation exists.

Track the provenance and validity range of material parameters.

## 8. Geometry-to-simulation integration

Integrate numerical solvers with the existing engineering geometry and mesh systems.

The system must distinguish:

- Visual geometry.
- Parametric engineering geometry.
- Simulation geometry.
- Numerical mesh.
- Material regions.
- Boundary surfaces.

Provide geometry preparation tools.

Support:

- Geometry validation.
- Mesh generation.
- Mesh refinement.
- Surface and volume meshing.
- Material-region assignment.
- Boundary assignment.
- Mesh inspection.

Investigate integration with Gmsh and other established meshing tools where appropriate.

Do not assume the display mesh is suitable for numerical simulation.

Mesh generation must respect physical requirements and the selected solver.

Provide mesh quality metrics and diagnostics.

For curved geometry, consider the effects of geometric approximation on numerical accuracy.

## 9. Diffraction and scattering

Implement physically grounded diffraction and scattering behavior through the selected numerical methods.

The system should be capable of evaluating:

- Diffraction around edges.
- Diffraction through apertures.
- Scattering from obstacles.
- Scattering from curved surfaces.
- Shadow regions.
- Near-field behavior.
- Interference between direct and scattered waves.

Create validation examples with analytically or independently understood behavior.

Do not introduce arbitrary diffraction correction factors into the analytical solver and label them full-wave simulation.

Where an approximate model is used, identify it explicitly.

## 10. Broadband and transient acoustics

Introduce time-domain source definitions.

Support:

- Continuous sinusoidal signals.
- Pulsed sinusoidal signals.
- Gaussian pulses.
- Broadband pulses.
- Frequency sweeps.
- Custom sampled waveforms.

Support configurable:

- Amplitude.
- Center frequency.
- Bandwidth.
- Duration.
- Phase.
- Delay.
- Repetition.
- Source envelope.

Allow emitters to operate with different waveforms where the selected solver supports them.

Implement true time-domain delay behavior.

Do not approximate broadband delay using a single frequency-dependent phase value.

Provide waveform visualization and source diagnostics.

## 11. Frequency-domain analysis

Implement analysis tools for extracting frequency-domain behavior from transient simulations.

Use FFT processing where appropriate.

Support:

- Frequency spectra.
- Complex pressure versus frequency.
- Magnitude response.
- Phase response.
- Frequency sweeps.
- Resonance identification.
- Frequency-dependent field analysis.

Allow users to inspect selected probe locations across frequency.

Provide clear information about:

- Sampling rate.
- Observation duration.
- Frequency resolution.
- Windowing.
- Aliasing.
- Spectral leakage.

Do not imply a frequency resolution better than the sampled observation permits.

Reuse the existing FFT infrastructure where suitable, or integrate a validated library such as FFTW.

## 12. Resonance and modal analysis

Introduce the foundations for acoustic modal analysis.

Support appropriate analytical and numerical approaches for:

- Enclosed cavities.
- Resonant chambers.
- Waveguides.
- Standing-wave configurations.

Calculate or extract:

- Resonant frequencies.
- Pressure modes.
- Particle-velocity modes.
- Nodal regions.
- Antinodes.
- Modal field distributions.

For simple geometries, compare numerical results with analytical estimates.

For complex geometries, document the eigenvalue formulation and solver assumptions.

Do not confuse transient frequency-response peaks with rigorously calculated eigenmodes.

## 13. Nonuniform and environmental media

Extend the propagation-medium model.

Support spatial variation in:

- Sound speed.
- Density.
- Temperature.
- Attenuation.
- Flow conditions where an appropriate model exists.

Prepare for environmental acoustics involving:

- Temperature gradients.
- Layered media.
- Air-water interfaces.
- Material transitions.
- Moving media.
- Background flow.

Begin with static spatially varying material properties.

More advanced flow and moving-medium models may be deferred.

The engine must identify when its assumptions of a stationary homogeneous medium are no longer valid.

## 14. Advanced acoustic effects

Design the architecture to accommodate additional phenomena.

Potential future models include:

- Nonlinear acoustic propagation.
- Harmonic generation.
- Shock formation.
- Acoustic streaming.
- Cavitation.
- Thermoviscous losses.
- Thermoacoustic effects.
- Nonlinear transducer behavior.
- Strongly dispersive media.

Do not implement simplified versions and imply that they are physically complete.

For each future effect, document the physical equations, required material parameters, solver implications, and validation needs before implementation.

## 15. Passive and hybrid structure simulation

Connect the advanced acoustic solvers to the Wave Manipulator system from Prompt 8.

The solver must be able to represent supported physical interactions involving:

- Parabolic reflectors.
- Elliptical reflectors.
- Spherical reflectors.
- Acoustic lenses.
- Waveguides.
- Resonant cavities.
- Layered structures.
- Arbitrary obstacles.

Support active and passive combinations.

For example:

- A planar emitter array radiating toward a reflector.
- A point source illuminating an acoustic lens.
- An emitter array operating inside a resonant cavity.
- A waveguide directing acoustic energy toward a target.
- Multiple sources interacting with a complex structure.

The geometry and material properties must actually participate in the numerical calculation.

Do not overlay an idealized free-field result on a physical structure and call it a coupled simulation.

## 16. Solver performance and compute backends

Implement an extensible computational backend architecture.

Begin with a correct CPU reference implementation.

Investigate:

- OpenMP parallelism.
- SIMD optimization.
- Cache-aware memory layouts.
- GPU compute.
- CUDA.
- OpenCL or other suitable cross-platform approaches.

Do not make the physics engine dependent on a specific GPU vendor.

The numerical model must remain independent of its execution backend.

If GPU acceleration is introduced:

- Compare against the CPU reference.
- Verify numerical agreement within documented tolerances.
- Test boundary behavior.
- Test long-duration stability.
- Test cancellation and resource cleanup.

GPU acceleration must not be used to conceal numerical errors or reduced precision without disclosure.

Provide a backend selection mechanism where multiple backends exist.

## 17. Simulation resource management

Large numerical simulations can consume substantial memory and compute.

Implement robust resource estimation and job management.

Support:

- Preflight estimates.
- Memory limits.
- Configurable field recording.
- Selective probe recording.
- Region-of-interest storage.
- Cancellation.
- Checkpointing where practical.
- Simulation resumption where safely supported.

Avoid storing every field value at every timestep by default.

Provide configurable recording strategies:

- Final field only.
- Selected timesteps.
- Fixed intervals.
- Selected probes.
- Region of interest.
- Full time history when explicitly requested.

Use HDF5 or another appropriate binary scientific data format for large numerical results.

Keep project metadata and large numerical datasets separate.

## 18. Visualization

Extend the visualization system to support actual numerical simulation data.

Implement:

**Time-domain visualization**
- Animated instantaneous pressure.
- Particle velocity where available.
- Wave propagation.
- Reflection and scattering.
- Transient pulse propagation.
- Standing-wave development.

**Frequency-domain visualization**
- Pressure magnitude.
- Phase.
- RMS pressure.
- Intensity where correctly supported.
- Frequency response.
- Resonant modes.

**Spatial visualization**
- XY, XZ, and YZ slices.
- Arbitrary cross sections.
- Volume rendering.
- Isosurfaces.
- Probe locations.
- Geometry and material boundaries.

**Analysis visualization**
- Time-domain waveforms.
- Frequency spectra.
- Frequency-response curves.
- Pressure versus position.
- Probe comparisons.
- Simulation comparisons.

Visualization must operate on stored numerical results.

Do not require recalculation when the user changes color maps, camera position, or visualization controls.

Clearly identify whether a displayed result is transient, steady-state, harmonic, RMS, or time-averaged.

## 19. Validation and scientific credibility

This milestone requires a serious validation program.

Implement tests against known analytical solutions and independent reference cases.

At minimum, validate:

- Homogeneous wave propagation.
- Spherical spreading where applicable.
- Plane-wave propagation.
- Phase progression.
- Two-source interference.
- Standing waves.
- Reflection from ideal boundaries.
- Transmission across material interfaces.
- Refraction.
- Diffraction through an aperture.
- Scattering from a simple obstacle.
- Resonance in simple cavities.
- Absorbing boundary performance.
- Numerical stability.
- Energy behavior.
- Mesh convergence.
- Temporal convergence.

Perform mesh-refinement studies.

Perform timestep-refinement studies where relevant.

Compare numerical results against analytical expectations and independently validated reference implementations.

Document error metrics and tolerances.

Do not claim scientific accuracy based solely on visual plausibility.

## 20. Example projects

Create reproducible projects demonstrating the actual implemented capabilities.

**Example A — Free-field propagation**
- Single emitter.
- Transient pulse.
- 2D and 3D propagation.
- Pressure visualization.
- Probe recording.

**Example B — Reflection**
- Source and rigid reflector.
- Incident and reflected waves.
- Standing-wave formation where applicable.

**Example C — Diffraction**
- Source.
- Barrier with aperture.
- Diffraction into the shadow region.
- Field comparison.

**Example D — Material interface**
- Two media with different acoustic properties.
- Reflection and transmission.
- Refraction where supported.

**Example E — Resonant cavity**
- Enclosed geometry.
- Resonant response.
- Mode or frequency-response visualization.

**Example F — Passive focusing**
- Physical reflector or lens.
- Numerical propagation.
- Focal-region analysis.

**Example G — Hybrid focusing**
- Active emitter array.
- Passive wave manipulator.
- Full numerical field evaluation.

All examples must be calculated using the actual implemented solver.

Clearly identify solver type, numerical resolution, physical assumptions, and limitations.

## 21. Documentation

Create or update:

- ADVANCED_ACOUSTICS.md
- FDTD_SOLVER.md
- NUMERICAL_ACCURACY.md
- BOUNDARY_CONDITIONS.md
- MATERIAL_INTERACTIONS.md
- BROADBAND_SIMULATION.md
- FREQUENCY_ANALYSIS.md
- MODAL_ANALYSIS.md
- SOLVER_SELECTION.md
- PERFORMANCE_AND_GPU.md
- ACOUSTIC_VALIDATION.md
- IMPLEMENTATION_STATUS.md
- DECISIONS.md

Document mathematical formulations, discretization, stability criteria, boundary behavior, mesh requirements, and validation results.

Include solver-specific limitations.

## 22. Implementation sequence

Work in the following stages.

**Phase A — Solver framework**
- Solver interfaces.
- Capability declarations.
- Solver selection.
- Numerical configuration.
- Resource estimation.

**Phase B — 2D FDTD**
- Wave equation.
- Source injection.
- Boundary conditions.
- Field recording.
- Validation.

**Phase C — 3D FDTD**
- Three-dimensional grid.
- Memory estimation.
- Stability controls.
- Simulation execution.
- Validation.

**Phase D — Material interactions**
- Spatial material properties.
- Interfaces.
- Reflection.
- Transmission.
- Absorption.
- Refraction.

**Phase E — Advanced boundaries**
- Impedance boundaries.
- Absorbing boundaries.
- PML or validated equivalent.
- Reflection testing.

**Phase F — Broadband and transient analysis**
- Source waveforms.
- Time-domain delays.
- FFT analysis.
- Frequency response.

**Phase G — Geometry and numerical integration**
- Mesh generation.
- Material regions.
- Complex geometry.
- Passive structure simulation.

**Phase H — Additional solver integrations**
- Angular Spectrum.
- FEM.
- BEM.
- Modal analysis.

Only integrate methods where they provide real value and can be validated.

**Phase I — Performance**
- CPU optimization.
- Parallelization.
- GPU investigation.
- Memory optimization.

**Phase J — Verification**
- Analytical tests.
- Convergence tests.
- Example projects.
- Documentation.
- Performance reports.

Do not attempt to finish every solver in one implementation pass.

Prioritize a robust 2D FDTD implementation before attempting an expansive collection of partially functional solvers.

## 23. Acceptance criteria

This milestone is complete only when:

1. The application has a functioning modular numerical acoustic solver framework.
2. At least one full-wave numerical solver is implemented and validated.
3. Two-dimensional transient acoustic simulation works.
4. Three-dimensional simulation is implemented if resource and validation requirements can be met; otherwise its remaining work is explicitly documented.
5. Spatial and temporal resolution are configurable.
6. Numerical stability is checked before execution.
7. Boundary conditions are implemented and tested.
8. Material interfaces produce physically meaningful results.
9. Transient acoustic sources work.
10. Broadband frequency analysis works where implemented.
11. Numerical results can be inspected in the viewport.
12. Geometry and materials participate in supported simulations.
13. At least one passive Wave Manipulator is simulated using a suitable numerical method.
14. Results are reproducible.
15. Mesh and timestep convergence are documented.
16. Automated tests pass.
17. Resource consumption is controlled and reported.
18. Existing analytical and active focusing capabilities remain functional.
19. Documentation accurately distinguishes implemented physics from future capabilities.

## 24. Critical development rules

- Numerical correctness takes priority over feature count.
- Do not claim full-wave accuracy for approximate methods.
- Do not claim unsupported material behavior.
- Do not ignore CFL stability or wavelength resolution.
- Do not hide numerical instability.
- Do not use visualization to imply physical behavior that was not calculated.
- Do not silently lower simulation resolution.
- Do not treat all boundary conditions as equivalent.
- Do not force different numerical methods into a single formulation.
- Preserve the CPU reference implementation.
- Validate accelerated implementations independently.
- Maintain reproducibility and solver metadata.
- Keep the application responsive during computation.
- Preserve all previously implemented capabilities.
- Record significant technical decisions and known limitations.

**The goal is to give Resonant a scientifically credible acoustic simulation engine capable of modeling real wave behavior in complex environments. We are moving beyond idealized point sources and simple interference into numerical acoustics that can support serious engineering design and physical experimentation.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Implement, capability, framework, implement
  - evidence: 'milestone' [body]; 'objective' [body]; 'milestone' [body]; 'Build' [body]; 'Extend' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: Fifth ticket, submitted by the user in chat (submit-only; the user asked not to claim or process tickets yet). Depends on REQ-CHAT-20261126-004 (passive wave manipulation, 'Prompt 8') and REQ-CHAT-20261126-003 (acoustic physics engine, 'Prompt 7'), which depend on REQ-CHAT-20261126-002 (V0.1 application foundation). Product vision: REQ-CHAT-20261126-001.

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

Fifth ticket, submitted by the user in chat (submit-only; the user asked not to claim or process tickets yet). Depends on REQ-CHAT-20261126-004 (passive wave manipulation, 'Prompt 8') and REQ-CHAT-20261126-003 (acoustic physics engine, 'Prompt 7'), which depend on REQ-CHAT-20261126-002 (V0.1 application foundation). Product vision: REQ-CHAT-20261126-001.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: pending
- Evidence: pending
