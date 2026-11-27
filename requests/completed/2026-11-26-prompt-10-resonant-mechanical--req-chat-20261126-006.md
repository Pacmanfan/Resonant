# Request: # PROMPT 10 — RESONANT: MECHANICAL WAVES, STRUCTURAL VIBRATION AND ELASTICITY SI

**Request ID:** REQ-CHAT-20261126-006
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T15:33:35-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 10 — RESONANT: MECHANICAL WAVES, STRUCTURAL VIBRATION AND ELASTICITY SIMULATION

You are continuing development of **RESONANT — Wave Field Engineering Studio**.

This milestone introduces the second major physics domain: **mechanical waves and structural vibration**.

The objective is to transform Resonant into a platform capable of designing, analyzing, and optimizing mechanical structures that transmit, reflect, concentrate, distribute, or suppress vibration and mechanical wave energy.

This includes vibration through solids, structural resonance, plates, membranes, beams, shells, mechanical actuators, and coupled structures.

Build on the existing application, geometry, material, simulation, visualization, and solver architectures.

Do not replace the existing acoustic subsystem. Mechanical physics must be implemented as a distinct domain with its own governing equations, material models, boundary conditions, and numerical methods.

---

## 1. Mission and objectives

Implement a modular mechanical physics subsystem supporting:

- Elastic wave propagation.
- Structural vibration.
- Mechanical resonance.
- Standing waves in solids.
- Flexural waves in plates.
- Longitudinal and transverse waves.
- Torsional vibration.
- Beam and rod vibration.
- Membrane vibration.
- Shell and plate vibration.
- Structural modal analysis.
- Forced response.
- Frequency response.
- Transient excitation.
- Damping and energy dissipation.
- Mechanical wave focusing and manipulation.
- Mechanical metamaterials and periodic structures.
- Coupled mechanical and acoustic behavior where supported.

The initial priority is a functional structural mechanics and modal analysis workflow.

The system should eventually support realistic mechanical wave-field engineering, including the ability to design structures that concentrate vibration at selected locations or suppress unwanted vibration elsewhere.

Do not attempt to implement every mechanical phenomenon in one milestone.

Prioritize scientifically validated capabilities.

## 2. Mechanical physics architecture

Create a dedicated mechanical physics subsystem.

Keep the mechanical domain independent from acoustic and electromagnetic physics.

The subsystem should include:

- Mechanical domain data models.
- Structural geometry.
- Mechanical materials.
- Mechanical boundary conditions.
- Structural meshes.
- Mechanical source definitions.
- Mechanical solver interfaces.
- Modal analysis.
- Forced-response analysis.
- Transient simulation.
- Mechanical field evaluation.
- Mechanical optimization.
- Result storage and visualization.

The mechanical subsystem may reuse generic application infrastructure, including:

- Scene management.
- Geometry.
- Units.
- Material metadata.
- Mesh infrastructure.
- Simulation job management.
- Rendering.
- Project persistence.
- Result storage.

Do not reuse acoustic equations or solver implementations where the underlying physics differs.

## 3. Mechanical material models

Extend the material system with mechanical properties.

Support:

- Density.
- Young's modulus.
- Shear modulus.
- Bulk modulus.
- Poisson's ratio.
- Damping.
- Elasticity.
- Anisotropy where supported.
- Frequency dependence where supported.
- Temperature dependence where supported.
- Plasticity as a future extension.
- Viscoelastic behavior as a future extension.

Support common engineering materials:

- Aluminum.
- Steel.
- Stainless steel.
- Copper.
- Brass.
- Titanium.
- Glass.
- Acrylic.
- ABS.
- PLA.
- Nylon.
- Polycarbonate.
- Rubber.
- Wood.
- Concrete.
- Ceramics.
- Composite materials.

Do not invent authoritative material constants.

Material records must identify:
- Source.
- Provenance.
- Measurement or reference conditions.
- Validity ranges.
- Units.
- Confidence or quality metadata where available.

Support isotropic materials initially.

Prepare the architecture for orthotropic and anisotropic materials, particularly fiber-reinforced composites.

Do not treat all plastics, metals, or composites as mechanically interchangeable.

## 4. Mechanical wave models

Implement the appropriate mechanical wave equations.

For an elastic solid, establish a mathematically correct continuum mechanics foundation.

A representative linear elastodynamic formulation is:

\[
\rho\frac{\partial^2\mathbf{u}}{\partial t^2}
=
\nabla\cdot\boldsymbol{\sigma}
+
\mathbf{f}
\]

where:

- \(\rho\) is material density.
- \(\mathbf{u}\) is displacement.
- \(\boldsymbol{\sigma}\) is the stress tensor.
- \(\mathbf{f}\) is body force per unit volume.

For linear elasticity:

\[
\boldsymbol{\sigma}
=
\mathbf{C}:\boldsymbol{\varepsilon}
\]

where:

- \(\mathbf{C}\) is the constitutive elasticity tensor.
- \(\boldsymbol{\varepsilon}\) is strain.

For isotropic small-strain elasticity, use an appropriate constitutive relationship based on Lamé parameters or equivalent elastic constants.

Implement consistent definitions and units.

Support:

- Longitudinal waves.
- Transverse/shear waves.
- Flexural waves where applicable.
- Torsional waves in suitable structures.

Distinguish bulk elastic waves from structural bending waves.

Do not use one simplified scalar wave equation to represent all mechanical vibration phenomena.

## 5. Mechanical source and actuator model

Introduce a first-class **Mechanical Actuator** object.

Each actuator should support:

- Stable ID.
- Position or attachment region.
- Orientation.
- Excitation frequency.
- Amplitude.
- Phase.
- Time delay.
- Waveform.
- Force or displacement excitation.
- Active/inactive state.
- Mechanical coupling properties.
- Optional physical dimensions.
- Optional actuator response model.

Support initial excitation types:

- Point force.
- Distributed force.
- Prescribed displacement.
- Prescribed velocity.
- Harmonic excitation.
- Transient pulse.
- Frequency sweep.

Prepare for physical actuator models such as:

- Electromagnetic shakers.
- Piezoelectric actuators.
- Voice-coil actuators.
- Ultrasonic mechanical transducers.
- Electromechanical vibration devices.

Do not assume an actuator's commanded signal directly produces an ideal mechanical force or displacement.

Keep actuator drive electronics and physical response as distinct concepts.

## 6. Structural geometry

Connect the mechanical subsystem to the existing engineering geometry architecture.

Support mechanical analysis of:

- Beams.
- Rods.
- Shafts.
- Plates.
- Membranes.
- Shells.
- Solid volumes.
- Frames.
- Trusses.
- Layered structures.
- Composite structures.

Implement appropriate structural representations.

Distinguish:
- CAD geometry.
- Engineering geometry.
- Structural idealization.
- Simulation mesh.

Allow users to identify appropriate idealizations.

For example:
- A slender structure may be represented as a beam.
- A thin structure may be represented as a plate or shell.
- A general solid requires a suitable volume formulation.

Do not automatically convert arbitrary geometry into a beam, plate, or shell without explaining the approximation.

Provide geometric properties such as:
- Length.
- Cross-sectional area.
- Thickness.
- Curvature.
- Section properties.
- Orientation.
- Material regions.

## 7. Structural analysis foundations

Implement a structural mechanics foundation capable of calculating mechanical deformation and vibration.

Support appropriate formulations for:

- Static structural response.
- Linear elasticity.
- Small displacement.
- Small strain.
- Dynamic structural response.

Static analysis is useful for establishing deformation and stiffness before vibration calculations.

Implement the necessary structural degrees of freedom and constraints for each selected element formulation.

Where a structural formulation is not implemented, do not expose it as a functioning solver option.

Clearly distinguish static structural analysis from dynamic wave propagation.

## 8. Modal analysis

Implement structural modal analysis.

This is a core capability.

Calculate natural frequencies and mode shapes using the appropriate generalized eigenvalue problem:

\[
\mathbf{K}\boldsymbol{\phi}
=
\omega^2\mathbf{M}\boldsymbol{\phi}
\]

where:

- \(\mathbf{K}\) is the structural stiffness matrix.
- \(\mathbf{M}\) is the mass matrix.
- \(\boldsymbol{\phi}\) is a mode shape.
- \(\omega\) is angular natural frequency.

Support:

- Natural frequencies.
- Mode shapes.
- Displacement distributions.
- Nodal regions.
- Antinodes.
- Modal participation where appropriate.

Allow users to select individual modes and visualize their deformation.

Provide mode animation with adjustable playback speed.

Do not confuse a visually amplified mode shape with actual physical displacement.

Display the visualization scale factor separately from the physical amplitude.

For simple structures, compare numerical modal results against analytical solutions.

Examples include:
- Fixed-free beam.
- Simply supported beam.
- Fixed-fixed beam.
- Rectangular membrane.
- Simple plate configurations.

Document boundary-condition assumptions.

## 9. Frequency response and forced vibration

Implement forced-response analysis.

Allow the user to excite structures using mechanical actuators.

Support harmonic excitation and frequency sweeps.

Calculate frequency response at selected points and regions.

Display:

- Displacement amplitude.
- Velocity amplitude.
- Acceleration amplitude.
- Stress response where supported.
- Phase response.
- Resonance peaks.
- Damping effects.

Support frequency-response functions between actuator input and measurement locations.

Allow users to compare multiple actuator positions and excitation strategies.

Implement physically appropriate damping models.

Begin with practical linear damping formulations.

Clearly identify whether damping is:
- Modal.
- Proportional/Rayleigh.
- Material-based.
- Structural loss-factor based.

Do not apply arbitrary damping to obtain visually convenient results.

## 10. Transient mechanical simulation

Implement time-domain mechanical simulation.

Support:

- Impulse excitation.
- Short-duration force pulses.
- Sinusoidal excitation.
- Burst excitation.
- Sweeps.
- Custom sampled waveforms.

Calculate time-dependent:

- Displacement.
- Velocity.
- Acceleration.
- Stress and strain where supported.
- Mechanical energy quantities where supported.

Implement a numerically stable time integration method appropriate to the selected structural formulation.

Investigate methods such as:

- Newmark-beta.
- Generalized-alpha.
- Central difference.
- Other suitable explicit or implicit integration schemes.

The solver must expose its stability and accuracy limitations.

Support recording at selected locations and regions rather than requiring every degree of freedom to be stored at every timestep.

## 11. Mechanical field quantities

Introduce a mechanical field representation.

Support:

**Displacement**
- X displacement.
- Y displacement.
- Z displacement.
- Displacement magnitude.

**Velocity**
- Particle velocity components.
- Velocity magnitude.

**Acceleration**
- Acceleration components.
- Acceleration magnitude.

**Strain**
- Normal strain.
- Shear strain.
- Principal strain where supported.

**Stress**
- Normal stress.
- Shear stress.
- Principal stress where supported.
- Von Mises stress for appropriate isotropic material assumptions.

**Energy**
- Kinetic energy.
- Strain energy.
- Energy dissipation where supported.

Clearly distinguish vector quantities from scalar magnitudes.

Use SI units internally.

Do not present stress or strain results for a model that only calculates displacement.

## 12. Mechanical wave propagation

Implement mechanical wave propagation in appropriate structural models.

Support initial demonstrations of:

- Longitudinal propagation through rods.
- Transverse propagation through beams.
- Flexural propagation through plates.
- Membrane wave propagation.
- Standing-wave formation.
- Wave reflection at supported structural boundaries.

Support propagation through multiple connected structural regions where implemented.

Account for material transitions and mechanical impedance where the selected model supports them.

Do not assume a mechanical wave in a solid behaves like an acoustic wave in air.

Document the relevant wave speeds, modes, and dispersion relationships.

## 13. Mechanical wave manipulators

Connect the mechanical subsystem to the existing Wave Manipulator architecture.

Introduce mechanically focused design structures.

Support design concepts including:

- Mechanical horns.
- Tapered waveguides.
- Resonant plates.
- Resonant membranes.
- Mechanical resonators.
- Vibration concentrators.
- Mechanical impedance transformers.
- Phononic crystals.
- Periodic mechanical structures.
- Locally resonant metamaterials.
- Mechanical isolation structures.
- Vibration suppression structures.

The objective is to allow users to design geometry that controls mechanical energy propagation.

The physical model must account for the actual structural geometry, material properties, and boundary conditions.

Do not simply apply acoustic reflector assumptions to mechanical structures.

For example, a mechanical horn should be analyzed using an appropriate structural or waveguide model, not treated as a decorative tapered object.

## 14. Mechanical focusing and energy concentration

Introduce mechanical focusing and vibration concentration.

The user should be able to define:

- A target location.
- A target region.
- Desired displacement amplitude.
- Desired vibration frequency.
- Desired phase where applicable.
- Suppression regions.
- Mechanical constraints.

Support multiple actuator configurations.

Calculate the mechanical response produced by the selected structure and actuator arrangement.

Where mathematically appropriate, introduce inverse-design methods.

For a linear frequency-domain system, a general representation is:

\[
\mathbf{D}(\omega)\mathbf{u}=\mathbf{f}
\]

where:

- \(\mathbf{D}(\omega)\) is the dynamic stiffness operator.
- \(\mathbf{u}\) is the displacement response.
- \(\mathbf{f}\) is the applied excitation.

Use an appropriate formulation for the selected structural model.

Develop optimization approaches that can adjust actuator inputs or structural parameters to meet target displacement and suppression constraints.

Do not assume every target distribution is physically achievable.

Report residual error, constraints, and numerical limitations.

## 15. Mechanical metamaterials and phononic crystals

Prepare for mechanical metamaterial design.

Support geometric representation of:

- Periodic unit cells.
- Repeating lattices.
- Locally resonant inclusions.
- Layered periodic structures.
- Phononic crystal arrangements.

Prepare analysis capabilities for:

- Dispersion relations.
- Band structures.
- Stop bands.
- Pass bands.
- Wave attenuation.
- Mode localization.

For periodic structures, investigate appropriate unit-cell and Bloch-periodic formulations.

Do not claim a band gap based solely on observing low displacement in one transient simulation.

Validate band structure calculations against appropriate analytical or independent numerical references.

This is a major future capability; prioritize architecture and one tractable demonstration over an incomplete general metamaterials engine.

## 16. Mechanical and acoustic coupling

Prepare a coupled physics architecture.

Mechanical vibration can radiate acoustic energy, and acoustic pressure can excite mechanical structures.

Support the conceptual coupling between:

- Structural vibration.
- Acoustic pressure.
- Fluid loading.
- Transducer vibration.
- Acoustic radiation.

Potential applications include:
- Loudspeaker diaphragms.
- Ultrasonic transducers.
- Vibrating plates radiating sound.
- Acoustic excitation of structures.
- Coupled fluid-structure systems.

Do not implement one-way or two-way coupling without explicitly defining its physical assumptions.

For initial development, implement a documented one-way coupling demonstration if practical.

Prepare for two-way fluid-structure interaction as a later advanced capability.

## 17. Numerical solver strategy

Use appropriate numerical methods for mechanical problems.

Investigate and integrate established methods such as:

- Finite Element Method.
- Structural beam and shell elements.
- Finite Difference methods where appropriate.
- Spectral element methods.
- Modal superposition.
- Reduced-order models.
- Boundary element methods for selected radiation problems.

Open-source tools such as Gmsh and suitable established numerical libraries may be considered.

Investigate integration with mature external structural solvers where they provide meaningful value.

Do not assume the acoustic FDTD implementation is suitable for general solid mechanics.

For each solver, document:
- Governing equations.
- Degrees of freedom.
- Element formulations.
- Material assumptions.
- Boundary conditions.
- Numerical stability.
- Convergence requirements.
- Supported physical effects.

Begin with a validated linear-elastic structural FEM implementation or integration.

Do not attempt nonlinear contact, plasticity, or fracture mechanics in this milestone.

## 18. Mesh and resource management

Extend the simulation mesh infrastructure for structural mechanics.

Support:

- Beam meshes.
- Surface meshes.
- Shell meshes.
- Volume meshes.
- Material regions.
- Structural constraints.
- Actuator attachment regions.
- Probe locations.

Provide mesh quality checks.

Preflight estimates must consider:
- Degrees of freedom.
- Element count.
- Matrix storage.
- Solver memory.
- Expected computational cost.
- Eigenvalue count.
- Time integration requirements.

Support sparse matrix representations and sparse solvers.

Do not construct unnecessarily dense matrices for large structural systems.

Provide cancellation and progress reporting for expensive simulations.

## 19. Visualization

Extend the OpenGL viewport to visualize actual mechanical simulation results.

Implement:

**Structural visualization**
- Original geometry.
- Deformed geometry.
- Adjustable deformation scale.
- Displacement magnitude.
- Stress distribution.
- Strain distribution.
- Velocity and acceleration where available.

**Modal visualization**
- Selected mode shape.
- Animated vibration.
- Nodal regions.
- Antinodes.
- Frequency labels.

**Wave visualization**
- Transient mechanical waves.
- Standing waves.
- Flexural wave patterns.
- Propagation through structures.

**Analysis visualization**
- Frequency-response curves.
- Mode frequency tables.
- Probe waveforms.
- Stress and displacement plots.
- Energy distribution.
- Comparison of simulation configurations.

Do not display unsupported field quantities.

Always distinguish the undeformed physical geometry from the visually amplified deformation.

Visualization must use stored numerical results and should not trigger recalculation when only display settings change.

## 20. Experimental measurement and calibration preparation

Prepare mechanical simulations for future experimental comparison.

Introduce data structures for:

- Accelerometer measurements.
- Displacement measurements.
- Strain gauge measurements.
- Force measurements.
- Frequency-response measurements.
- Sensor position and orientation.
- Calibration information.

Support importing measured data from documented formats such as CSV.

Allow measured and simulated frequency-response curves to be compared.

Do not claim automated hardware acquisition in this milestone.

The architecture must eventually support sensor calibration and experimental model updating.

## 21. Performance and computational backends

Begin with a correct CPU implementation.

Investigate:

- Sparse matrix optimization.
- Eigenvalue solver performance.
- OpenMP.
- SIMD.
- GPU compute.
- Reduced-order modeling.
- Modal superposition.

Keep the mechanical physics model independent of its execution backend.

Validate accelerated implementations against the reference CPU implementation.

Do not compromise numerical accuracy without explicitly reporting it.

## 22. Example projects

Create reproducible demonstration projects.

**Example A — Vibrating beam**
- Aluminum or steel beam.
- Defined support conditions.
- Modal analysis.
- First several natural frequencies.
- Mode-shape visualization.

**Example B — Vibrating plate**
- Rectangular plate.
- Configurable thickness.
- Modal analysis.
- Nodal and antinodal visualization.

**Example C — Forced resonance**
- Structural model.
- Mechanical actuator.
- Frequency sweep.
- Frequency-response plot.
- Resonance identification.

**Example D — Transient vibration**
- Mechanical impulse.
- Time-domain simulation.
- Displacement and velocity visualization.
- Probe recordings.

**Example E — Mechanical wave propagation**
- Rod or beam.
- Transient excitation.
- Propagating and reflected waves.

**Example F — Vibration concentration**
- Structural geometry.
- Multiple actuators.
- Defined target location.
- Optimized or adjusted actuator inputs.
- Measured target response.

**Example G — Phononic structure**
- Periodic mechanical unit cell.
- Dispersion or band-structure analysis where implemented.
- Stop-band demonstration.

All examples must use actual implemented mechanical calculations.

## 23. Validation and numerical credibility

Implement a rigorous mechanical validation program.

Validate:

- Linear elasticity.
- Beam deformation.
- Plate deformation.
- Natural frequencies.
- Mode shapes.
- Forced harmonic response.
- Damping behavior.
- Transient integration.
- Energy behavior.
- Structural wave propagation.
- Material transitions where supported.
- Mesh convergence.
- Time-step convergence.
- Eigenvalue solver accuracy.

Use analytical solutions for simple structures wherever possible.

Compare against independently established reference results.

Document:
- Numerical error.
- Mesh resolution.
- Solver tolerances.
- Boundary conditions.
- Material parameters.
- Validity assumptions.

Do not use visually plausible deformation as proof of mechanical accuracy.

## 24. Documentation

Create or update:

- MECHANICAL_PHYSICS.md
- STRUCTURAL_MECHANICS.md
- MECHANICAL_MATERIALS.md
- MODAL_ANALYSIS.md
- FORCED_RESPONSE.md
- TRANSIENT_VIBRATION.md
- MECHANICAL_WAVE_PROPAGATION.md
- MECHANICAL_FOCUSING.md
- PHONONIC_STRUCTURES.md
- FLUID_STRUCTURE_COUPLING.md
- MECHANICAL_VALIDATION.md
- IMPLEMENTATION_STATUS.md
- DECISIONS.md

Document equations, material assumptions, element formulations, boundary conditions, numerical methods, and limitations.

## 25. Implementation sequence

**Phase A — Mechanical domain architecture**
- Domain models.
- Material properties.
- Actuator models.
- Structural representations.
- Solver interfaces.

**Phase B — Structural mechanics foundation**
- Linear elasticity.
- Structural mesh.
- Boundary conditions.
- Static analysis.
- Validation.

**Phase C — Modal analysis**
- Mass matrix.
- Stiffness matrix.
- Eigenvalue solution.
- Mode visualization.
- Analytical comparisons.

**Phase D — Forced response**
- Harmonic excitation.
- Frequency sweeps.
- Damping.
- Frequency-response plots.

**Phase E — Transient vibration**
- Time integration.
- Actuator waveforms.
- Displacement and velocity recording.
- Probe support.

**Phase F — Mechanical wave propagation**
- Beam and rod waves.
- Plate and membrane models where practical.
- Wave reflection.
- Standing-wave behavior.

**Phase G — Mechanical wave manipulation**
- Resonators.
- Mechanical horns.
- Vibration concentrators.
- Phononic structures.

**Phase H — Optimization and coupling**
- Actuator optimization.
- Structural parameter optimization.
- Initial acoustic coupling.

**Phase I — Performance and validation**
- Sparse solvers.
- Resource management.
- Numerical validation.
- Example projects.
- Documentation.

Work incrementally. A validated structural FEM and modal-analysis workflow is more valuable than numerous partially implemented mechanical features.

## 26. Acceptance criteria

This milestone is complete only when:

1. Mechanical physics is a distinct modular domain.
2. Mechanical material properties are supported.
3. Mechanical actuator objects can be created and configured.
4. Structural geometry can be prepared for mechanical analysis.
5. Linear-elastic structural analysis works.
6. Modal analysis calculates natural frequencies and mode shapes.
7. Forced-response analysis works for supported structures.
8. Frequency sweeps produce meaningful response curves.
9. At least one transient vibration simulation works.
10. Mechanical field quantities are visualized correctly.
11. Results distinguish physical displacement from visualization scaling.
12. Mechanical wave behavior is demonstrated in at least one validated structural model.
13. The architecture supports future mechanical wave manipulation.
14. At least one target-driven vibration optimization demonstration works if the selected model supports it.
15. Results retain reproducibility metadata.
16. Analytical or independent numerical validation is documented.
17. Automated tests pass.
18. Simulation resource consumption is estimated and controlled.
19. Existing acoustic capabilities remain functional.
20. Documentation clearly identifies implemented and deferred capabilities.

## 27. Critical development rules

- Mechanical physics must remain separate from acoustic physics.
- Do not treat solid mechanics as a scalar acoustic wave equation.
- Do not confuse structural modal analysis with transient wave propagation.
- Do not claim unsupported anisotropic, nonlinear, or viscoelastic behavior.
- Do not fabricate material properties.
- Do not ignore structural boundary conditions.
- Do not display stress or strain if the solver does not calculate them.
- Do not present amplified deformation as actual displacement.
- Do not use dense numerical methods unnecessarily.
- Validate numerical methods against independent references.
- Preserve the existing acoustic subsystem.
- Keep solver and rendering architectures modular.
- Maintain reproducibility.
- Record important architectural decisions.

**The goal is to make Resonant a serious mechanical wave and structural vibration engineering environment: model structures, calculate their resonances, excite them, observe how mechanical energy propagates, and design geometries and actuator arrangements that control the resulting vibration.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Implement, ability to, capabilities, implement
  - evidence: 'milestone' [body]; 'introduces' [body]; 'objective' [body]; 'Build' [body]; 'Implement' [head]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: Sixth ticket, submitted by the user in chat while REQ-CHAT-20261126-002 was being processed, with the instruction to process it in order (after REQ-CHAT-20261126-005). Second physics domain; builds on the application foundation (002), the wave-manipulator architecture (004) and the solver framework (005). Standing operator instructions for all tickets: keep qmake support in addition to CMake; Qt user interfaces use .ui files.

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

Sixth ticket, submitted by the user in chat while REQ-CHAT-20261126-002 was being processed, with the instruction to process it in order (after REQ-CHAT-20261126-005). Second physics domain; builds on the application foundation (002), the wave-manipulator architecture (004) and the solver framework (005). Standing operator instructions for all tickets: keep qmake support in addition to CMake; Qt user interfaces use .ui files.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000006
- Evidence: pending
