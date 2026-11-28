# Request: # PROMPT 19 — GEOMETRY-DRIVEN ENERGY TRANSFORMATION AND FUNCTIONAL STRUCTURES

#

**Request ID:** REQ-CHAT-20261126-017
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T23:29:53-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 19 — GEOMETRY-DRIVEN ENERGY TRANSFORMATION AND FUNCTIONAL STRUCTURES

## Project: RESONANT — Wave Field Engineering Studio

## Mission

Implement the next major engineering capability in RESONANT: **Geometry-Driven Energy Transformation and Functional Structures**.

This capability makes geometry a first-class physical design variable.

RESONANT must move beyond treating geometry as something that is merely displayed, imported, or converted into a simulation mesh. Geometry must become something the user can deliberately engineer to control how physical energy propagates, reflects, refracts, focuses, confines, resonates, couples, transfers, stores, dissipates, and interacts with matter.

The system must support both:

1. **Object-centric engineering:** The user creates or imports a structure, assigns materials and excitation conditions, and investigates its physical behavior.
2. **Function-centric engineering:** The user specifies a desired physical behavior, operating conditions, and constraints. RESONANT evaluates candidate geometries, materials, and configurations to identify structures that may achieve the desired result.

The governing design philosophy is:

Geometry + Material Properties + Excitation + Environment + Boundary Conditions
→ Physical Interaction
→ Energy and Matter Response
→ Functional Behavior

A shape does not possess a physical function merely because it has been assigned a label. Its behavior must emerge from the relevant physical equations, material constitutive models, operating conditions, boundaries, and environmental conditions.

This work must integrate with the Shape–Function–Energy architecture established in Prompt 17 and the Coupled Energy–Matter Engine (CEME) established in Prompt 18.

Do not duplicate those systems. Extend them through well-defined interfaces.

---

# 1. AUDIT FIRST

Before implementation, inspect the existing repository and establish the current state of:

- Geometry representation and CAD import.
- OpenCASCADE integration.
- Geometry editing and parameterization.
- Geometry-to-mesh pipeline.
- Material-region assignment.
- Boundary-condition assignment.
- Existing physics solvers.
- Resonance and modal analysis.
- Field visualization.
- Energy and momentum accounting.
- CEME interfaces and simulation orchestration.
- Optimization infrastructure.
- Project serialization.
- GUI workspaces and dockable panels.
- Automation and gRPC services.
- Existing documentation and roadmap.

Read AGENTS.md and all applicable development instructions.

Identify what already exists, what is partially implemented, what is missing, and what can be extended.

Produce an implementation plan based on actual repository evidence.

Do not invent existing capabilities. Do not create parallel implementations of existing functionality. Preserve working systems and existing project compatibility.

---

# 2. GEOMETRY AS A PHYSICAL DESIGN VARIABLE

Establish a geometry architecture that supports physical analysis and engineering optimization.

Geometry must be represented in a way that preserves more than its visible exterior surface.

Support the following concepts:

## 2.1 Geometric properties

- Dimensions and proportions.
- Surface curvature.
- Surface orientation and normals.
- Volume and enclosed volume.
- Surface area.
- Topology and connectivity.
- Openings and apertures.
- Cavities and internal volumes.
- Channels, ducts, and passages.
- Walls, shells, and thickness.
- Layers and interfaces.
- Periodic structures.
- Internal inclusions and resonant elements.
- Spatially varying cross-sections.
- Tapers and transitions.
- Component placement and orientation.
- Mechanical connections and constraints.
- Geometric tolerances.

## 2.2 Distinguish geometry representations

Keep these representations conceptually and technically distinct:

1. Visual geometry: what the user sees.
2. Engineering geometry: editable CAD and parameterized design geometry.
3. Physical geometry: the boundaries and material regions used by physical models.
4. Simulation geometry: the discretized representation used by a solver.
5. Functional geometry: the parameters and geometric relationships that control a desired physical response.

These representations must be traceable to one another.

A change to engineering geometry must invalidate affected downstream representations and results.

Do not silently reuse stale meshes, field solutions, or derived properties.

## 2.3 Geometry revision and dependency tracking

Implement or extend revision tracking so the system can determine whether:

- A CAD model changed.
- A material region changed.
- A physical boundary changed.
- A mesh is stale.
- A field solution is stale.
- A functional objective is stale.
- An optimization result no longer applies.

Every geometry-dependent result must identify the geometry revision on which it was calculated.

---

# 3. FUNCTIONAL STRUCTURE MODEL

Introduce a functional structure abstraction.

A functional structure is an engineered physical object whose geometry, materials, excitation, and environment are configured to produce a measurable physical response.

It is not a hardcoded shape category.

A functional structure should reference:

- One or more geometric bodies.
- Material regions.
- Interfaces and boundaries.
- Physical domains.
- Excitation sources.
- Operating frequency or frequency range.
- Environmental conditions.
- Relevant physical constraints.
- Desired or measured response.
- Simulation configurations.
- Optimization parameters.
- Manufacturing constraints, where applicable.

Support structures that combine multiple physical functions.

For example, a single assembly might contain:

- An electromagnetic resonant cavity.
- A thermal management structure.
- A mechanically supported reflector.
- A cooling channel.
- A sensor or measurement region.

These components must be able to participate in the same physical system through existing CEME interfaces.

Do not require every structure to participate in every physics domain.

---

# 4. ENERGY TRANSFORMATION AND PHYSICAL INTERACTION

Implement a domain-aware framework for understanding how a structure transforms energy.

Do not create a universal energy equation that replaces domain-specific physics.

Use the appropriate existing or integrated solver for each physical domain.

The functional structure layer should coordinate and expose the resulting behavior.

Support the following categories.

## 4.1 Propagation and transmission

Analyze how energy propagates through or along a structure.

Examples:

- Waveguides.
- Acoustic ducts.
- Optical waveguides.
- Transmission structures.
- Mechanical waveguides.
- Layered media.
- Periodic media.

Expose relevant quantities such as:

- Propagating modes.
- Phase and group velocity where supported.
- Attenuation.
- Transmission.
- Reflection.
- Impedance.
- Dispersion.
- Field distribution.
- Energy flux.
- Power transmission.

## 4.2 Reflection and redirection

Analyze how geometry redirects incident energy.

Examples:

- Reflectors.
- Curved reflecting surfaces.
- Acoustic reflectors.
- Electromagnetic reflectors.
- Optical mirrors.
- Mechanical wave reflection boundaries.

Support investigation of:

- Reflection coefficients.
- Scattering.
- Directional redistribution.
- Interference.
- Standing-wave formation.
- Boundary effects.
- Energy carried away from the structure.

## 4.3 Focusing and concentration

Analyze structures that redistribute energy into a smaller spatial region.

Examples:

- Acoustic lenses.
- Electromagnetic lenses.
- Optical lenses.
- Reflective focusing geometries.
- Horns and impedance transformers.
- Concentrating waveguide structures.

Expose:

- Field amplitude.
- Intensity.
- Energy density.
- Power density.
- Focal location.
- Focal region dimensions.
- Spatial concentration ratio.
- Side lobes and unwanted secondary maxima.
- Efficiency and losses.

Distinguish carefully between field amplitude, intensity, power density, and total energy.

Do not describe an increase in local energy density as an increase in total energy.

Account for aperture, source power, losses, radiation, and conservation.

## 4.4 Confinement and storage

Analyze geometries that confine energy or support stored energy.

Examples:

- Microwave cavities.
- Optical resonators.
- Acoustic cavities.
- Mechanical resonators.
- Coupled resonant structures.
- Distributed resonant networks.

Expose:

- Resonant frequencies.
- Natural modes.
- Mode shapes.
- Modal energy.
- Stored energy.
- Leakage.
- Damping.
- Quality factor.
- Bandwidth.
- Coupling strength.
- Frequency response.

Distinguish energy stored in a resonator from energy continuously supplied by an external source.

## 4.5 Dissipation and conversion

Analyze how energy is absorbed, dissipated, or converted into another physical form.

Examples:

- Electromagnetic absorption and heating.
- Dielectric and conductive losses.
- Acoustic absorption.
- Viscous dissipation.
- Mechanical damping.
- Thermoelastic conversion.
- Fluid heating.
- Piezoelectric conversion where supported.

Connect to CEME so that energy transfers between domains are accounted for rather than counted twice.

## 4.6 Force and momentum transfer

Where supported by the underlying physics, calculate mechanical effects resulting from energy interactions.

Examples:

- Electromagnetic forces and Maxwell stress.
- Acoustic radiation force.
- Radiation pressure.
- Mechanical wave momentum transfer.
- Fluid pressure and viscous loading.
- Thermal expansion and associated mechanical stress.

Keep the different physical mechanisms distinct.

Do not introduce arbitrary geometry-to-force mappings or claim that a geometry produces a force without an appropriate physical model.

---

# 5. RESONANCE AND MODAL ENGINEERING

Resonance must be treated as a cross-domain engineering capability.

Integrate existing modal and resonance systems rather than creating a competing resonance subsystem.

Support analysis of:

- Electromagnetic cavity modes.
- Optical resonator modes.
- Acoustic modes.
- Mechanical natural modes.
- Elastic wave modes.
- Coupled resonators.
- Local resonances.
- Distributed resonant structures.
- Mode coupling and splitting.
- Mode localization.
- Frequency-dependent response.

For each supported model, expose appropriate quantities:

- Natural or resonant frequency.
- Mode shape.
- Spatial field distribution.
- Damping.
- Quality factor.
- Bandwidth.
- Stored energy.
- Coupling.
- Modal participation.
- Sensitivity to geometry changes.

Support geometry-driven resonance tuning.

The user should be able to change a cavity dimension, mirror separation, reflector curvature, wall thickness, or resonator spacing and observe the calculated effect on resonant behavior.

Do not assume that every structure is resonant. Clearly distinguish resonant, broadband, non-resonant, and mixed behavior.

---

# 6. REQUIRED REFERENCE ENGINEERING EXAMPLES

Implement these as real engineering examples using the underlying physical models.

They are not merely demonstration graphics.

## 6.1 Microwave cavity resonator

This is a primary reference case.

Allow the user to create or configure a microwave cavity with:

- Editable dimensions.
- Cavity shape.
- Conducting wall material.
- Wall conductivity.
- Dielectric inserts.
- Excitation port configuration.
- Port placement and orientation.
- Operating frequency range.
- Boundary conditions.
- Optional internal structures.

Analyze:

- Resonant modes.
- Resonant frequencies.
- Electric-field distribution.
- Magnetic-field distribution.
- Stored electromagnetic energy.
- Surface and volume losses where modeled.
- Quality factor.
- Bandwidth.
- Coupling and port response.
- Sensitivity to dimensions and material properties.

Show how geometry changes affect the modes and response.

Include a simple analytical or established benchmark comparison for a geometry where an analytical solution is available.

Validate the numerical solution against the analytical result within a documented tolerance.

## 6.2 Two-mirror optical resonator

Implement a reference optical resonator model.

Support:

- Mirror separation.
- Mirror curvature.
- Mirror reflectivity.
- Aperture.
- Optical wavelength.
- Alignment and positioning.
- Relevant optical losses.
- Stable and unstable resonator configurations.

Analyze:

- Resonator stability.
- Supported transverse modes where supported.
- Longitudinal mode spacing.
- Beam waist.
- Beam propagation.
- Mode distribution.
- Cavity losses.
- Stored optical energy and coupling where modeled.

Use an appropriate optical resonator model.

Do not represent a full electromagnetic optical solution as a paraxial approximation. Identify the assumptions and validity range of each model.

## 6.3 Acoustic horn and reflector

Implement a reference acoustic structure.

Support:

- Horn profile.
- Throat and mouth dimensions.
- Length.
- Cross-sectional profile.
- Material and wall properties.
- Source configuration.
- Operating frequency range.
- Acoustic boundary conditions.

Analyze:

- Pressure field.
- Particle velocity.
- Acoustic intensity.
- Impedance transformation.
- Reflection.
- Transmission.
- Radiation pattern.
- Directivity.
- Acoustic power transfer.

Allow the user to compare different horn profiles under equivalent source and operating conditions.

## 6.4 Mechanical resonator

Implement a reference mechanical resonant structure.

Support:

- Geometry.
- Material.
- Supports and constraints.
- Mass distribution.
- Excitation.
- Damping.
- Frequency range.

Analyze:

- Natural frequencies.
- Mode shapes.
- Displacement.
- Velocity.
- Stress and strain.
- Modal energy.
- Frequency response.
- Sensitivity to geometric changes.

## 6.5 Metamaterial unit cell

Implement a parameterized periodic unit-cell example.

Support:

- Unit-cell geometry.
- Periodicity.
- Material assignment.
- Internal resonators.
- Layered structures.
- Boundary conditions appropriate to the model.
- Frequency sweep.

Analyze, where supported:

- Dispersion relations.
- Band structure.
- Band gaps.
- Transmission.
- Reflection.
- Local resonances.
- Effective constitutive properties.
- Anisotropy.
- Frequency-dependent effective response.

Distinguish effective or homogenized properties from constituent material properties.

Do not assign negative effective density, stiffness, permittivity, or permeability by hand to make a result look like a metamaterial effect.

Derive such behavior from a valid model and clearly state the homogenization assumptions and frequency range.

---

# 7. GEOMETRY PARAMETERIZATION

Implement a reusable parameterization framework.

The user must be able to identify geometric parameters that control physical behavior.

Examples:

- Cavity length, width, and height.
- Mirror spacing and curvature.
- Lens curvature and thickness.
- Horn length and expansion profile.
- Resonator dimensions and spacing.
- Wall thickness.
- Channel dimensions.
- Unit-cell periodicity.
- Inclusion size and placement.
- Layer thickness.
- Structural support positions.

Support:

- Named parameters.
- Units and valid ranges.
- Default values.
- Constraints.
- Dependencies.
- Derived parameters.
- Parameter groups.
- Geometric constraints.
- Parameter sweeps.
- Revision tracking.

Parameter changes must produce predictable geometry updates.

Use the existing CAD kernel and parameterization capabilities where possible.

Avoid a separate geometry-editing engine unless the audit proves it is necessary.

---

# 8. FUNCTION-DRIVEN DESIGN

Introduce the architecture for designing a structure from a desired physical response.

The user should eventually be able to specify a goal such as:

- Produce a resonance near a target frequency.
- Maximize energy concentration in a specified region.
- Minimize reflection across a frequency band.
- Maximize acoustic directivity.
- Produce a desired mechanical mode.
- Create a band gap in a specified frequency range.
- Minimize unwanted energy leakage.
- Maximize energy transfer into a selected region.
- Control the spatial distribution of a field.
- Achieve a desired force or stress distribution subject to physical constraints.

The system must translate a goal into a measurable objective function.

Each objective must specify:

- Physical quantity.
- Units.
- Spatial region or boundary.
- Frequency or time range.
- Desired value or target range.
- Weighting.
- Constraints.
- Solver and model dependencies.
- Validity conditions.

Support multiple objectives and tradeoffs.

For example, a design that maximizes field concentration but creates excessive heating should not automatically be considered successful.

Use multi-objective optimization where appropriate.

## 8.1 Candidate design generation

Create an extensible architecture for generating and evaluating candidate designs.

Initially support:

- Parameter sweeps.
- Structured parameter sampling.
- Existing optimization algorithms.
- Constraint-based search.
- Sensitivity analysis.

Later approaches may include:

- Evolutionary optimization.
- Bayesian optimization.
- Surrogate models.
- Topology optimization.
- Gradient-based optimization.
- Adjoint methods.

Do not attempt to implement every optimization algorithm in this prompt.

Use existing optimization infrastructure where possible.

## 8.2 Explainable design results

Every recommended candidate must explain:

- Which geometric parameters changed.
- Which material properties were used.
- Which operating conditions were assumed.
- Which solver and physical model were used.
- Which objective improved.
- Which objectives became worse.
- Which constraints were active.
- What physical mechanism explains the result.
- What uncertainty or validity limitations remain.

Do not present an optimizer's output as a proven physical result without validating it through the relevant solver.

---

# 9. GEOMETRY SENSITIVITY AND DESIGN EXPLORATION

Implement tools to understand how physical behavior changes as geometry changes.

Support:

- One-parameter sweeps.
- Multi-parameter sweeps.
- Local sensitivity analysis.
- Frequency-versus-geometry comparisons.
- Mode tracking across geometry revisions.
- Response surface visualization where appropriate.
- Parameter correlation.
- Tolerance sensitivity.
- Robustness evaluation.

A critical capability is understanding how a small dimensional change affects a resonant or functional response.

For example:

Changing cavity length may shift a resonant frequency. Changing mirror separation may affect optical mode spacing. Changing horn expansion may affect acoustic impedance and radiation pattern.

The software must calculate these effects using appropriate models rather than hardcoded rules.

When modes move, split, or exchange characteristics during a parameter sweep, implement appropriate mode-tracking logic or clearly flag ambiguous mode correspondence.

---

# 10. MANUFACTURABILITY AND ENGINEERING CONSTRAINTS

Functional structures must be physically realizable.

Introduce a manufacturing-constraint interface that can support:

- Minimum wall thickness.
- Minimum feature size.
- Minimum channel size.
- Minimum spacing.
- Material availability.
- Tolerance limits.
- Assembly constraints.
- Support and mounting requirements.
- Surface finish requirements where relevant.
- Fabrication process constraints.

Do not assume that a numerically optimal geometry is manufacturable.

Allow constraints to be associated with a manufacturing process, such as:

- CNC machining.
- Sheet fabrication.
- Additive manufacturing.
- Injection molding.
- Lamination.
- Composite fabrication.

Do not implement detailed manufacturing process simulation here.

The immediate goal is to ensure that design optimization can reject or penalize geometries that violate defined engineering constraints.

---

# 11. VISUALIZATION AND ENGINEERING INSPECTION

Extend the existing GUI without creating an unrelated application or duplicating the viewport.

The user should be able to inspect a functional structure and its physical response together.

Support:

## Geometry inspection

- CAD geometry.
- Material regions.
- Interfaces.
- Boundaries.
- Parameter labels.
- Dimensions.
- Selected features.
- Functional regions.
- Simulation mesh overlay.

## Physical field inspection

Where available:

- Electric field.
- Magnetic field.
- Acoustic pressure.
- Particle velocity.
- Acoustic intensity.
- Mechanical displacement.
- Stress and strain.
- Temperature.
- Energy density.
- Energy flux.
- Momentum flux.
- Force density.

## Engineering visualization

Provide:

- Field overlays on geometry.
- Cross-sectional inspection.
- Slice planes.
- Probe points.
- Region measurements.
- Frequency response plots.
- Modal visualizations.
- Geometry comparison.
- Before/after comparison.
- Parameter sweep plots.
- Optimization result comparison.

Support synchronized views of geometry, field response, and engineering metrics.

Changing the visualization must not trigger an unnecessary physics recalculation.

Clearly identify the displayed quantity, units, normalization, solver, geometry revision, and simulation time or frequency.

---

# 12. USER WORKFLOW

Build on the existing RESONANT workspace and dockable panel architecture.

Do not redesign the entire GUI.

Introduce a coherent functional-structure workflow:

1. Create or import geometry.
2. Define or select geometric parameters.
3. Assign material regions.
4. Configure physical domains.
5. Define boundaries and interfaces.
6. Add excitation sources.
7. Define the operating environment.
8. Select the desired physical analysis.
9. Validate the physical system.
10. Generate or update the simulation mesh.
11. Run the appropriate solver or coupled simulation.
12. Inspect field distributions and energy flow.
13. Review resonances, forces, and material response.
14. Sweep geometry parameters.
15. Optimize against a functional objective.
16. Compare candidate designs.
17. Save and document the resulting engineering design.

Make it possible to transition between object-centric and function-centric design without rebuilding the project.

---

# 13. CEME INTEGRATION

Use the existing Coupled Energy–Matter Engine as the coordinating architecture for multiphysics behavior.

Do not reproduce CEME's:

- Physical quantity registry.
- Material state management.
- Geometry state management.
- Coupling manager.
- Energy transfer records.
- Conservation auditing.
- Time coordination.
- Solver orchestration.
- Provenance system.

Functional structures should reference and configure these systems.

Examples:

- Microwave cavity absorption can produce thermal sources.
- Heating can change material properties.
- Temperature changes can shift resonant frequency.
- Thermal expansion can change cavity dimensions.
- Mechanical deformation can change acoustic or electromagnetic response.
- Acoustic pressure can excite structural modes.
- Structural vibration can radiate acoustic energy.

Where feedback exists, use the appropriate CEME coupling strategy.

Where there is no meaningful feedback, use a simpler one-way analysis.

Do not force coupled simulation when independent domain analysis is sufficient.

---

# 14. AUTOMATION AND PROGRAMMATIC ACCESS

Expose meaningful functional-structure capabilities through the existing application services and automation architecture.

The existing Protobuf contract remains authoritative.

Use the established package and naming conventions.

Potential operations include:

- Create functional structure.
- Import or reference geometry.
- List geometric parameters.
- Update geometric parameters.
- Assign material regions.
- Configure physical domains.
- Configure excitation and boundaries.
- Validate a functional structure.
- Generate or update a mesh.
- Run functional analysis.
- Run modal analysis.
- Run parameter sweep.
- Define a functional objective.
- Run geometry optimization.
- Inspect candidate designs.
- Compare candidate results.
- Retrieve physical response metrics.
- Retrieve energy and momentum accounting.
- Export engineering results.

Follow existing patterns for:

- Asynchronous jobs.
- Job status and cancellation.
- Progress reporting.
- Idempotency.
- Optimistic concurrency.
- Project revision tracking.
- Error handling.
- Capability discovery.
- Provenance.

Do not put large field arrays or simulation datasets into ordinary control-plane protobuf messages. Use the established scientific-data and bulk-transfer mechanisms.

GUI, Python, and automation must invoke the same underlying application services.

Do not create a second implementation of functional engineering logic for the automation interface.

---

# 15. DATA MODEL AND PROJECT SERIALIZATION

Functional structures, parameters, objectives, constraints, and analysis configurations must be persistent project resources.

Use stable UUIDs and the existing serialization conventions.

Persist:

- Structure identity.
- Geometry references and revisions.
- Parameter definitions and values.
- Material-region assignments.
- Physical domains.
- Excitation configurations.
- Boundary conditions.
- Environment.
- Functional objectives.
- Constraints.
- Optimization configuration.
- Analysis configuration.
- Simulation references.
- Result provenance.

Do not serialize transient solver state as if it were a durable engineering definition.

Ensure that projects created before this work continue to open correctly.

---

# 16. VALIDATION AND SCIENTIFIC CORRECTNESS

Scientific correctness is a release requirement.

Each reference structure must have a documented validation strategy.

Include:

## Microwave cavity

- Analytical resonant-frequency comparison where applicable.
- Mesh convergence.
- Boundary-condition verification.
- Energy balance.
- Material-loss behavior where modeled.

## Optical resonator

- Resonator stability checks.
- Analytical mode-spacing comparison.
- Beam parameter consistency.
- Documented paraxial assumptions.

## Acoustic horn

- Impedance and transmission behavior.
- Frequency-response comparison.
- Mesh convergence.
- Energy/power accounting.

## Mechanical resonator

- Analytical or established modal benchmark.
- Boundary-condition verification.
- Mesh convergence.
- Modal consistency.

## Metamaterial unit cell

- Periodic boundary-condition verification.
- Dispersion or band-structure benchmark.
- Frequency-range and homogenization validity.
- Comparison with a full-structure model where practical.

For all analyses:

- Record solver and model versions.
- Record geometry and material revisions.
- Record boundary conditions and excitation.
- Record mesh and convergence information.
- Record approximations and assumptions.
- Identify unsupported physical effects.
- Report numerical failures explicitly.

Do not silently substitute an approximate model for a more complete physical model.

The user must be able to see what physics was actually calculated.

---

# 17. TESTING REQUIREMENTS

Add tests for:

- Geometry parameterization.
- Parameter dependency updates.
- Geometry revision tracking.
- Stale mesh detection.
- Stale result detection.
- Material-region association.
- Boundary association.
- Functional structure serialization.
- Functional objective serialization.
- Constraint evaluation.
- Parameter sweeps.
- Sensitivity calculations.
- Candidate comparison.
- Solver integration.
- CEME integration.
- Automation operations.
- Project compatibility.

Add regression tests for all existing geometry and physics functionality.

Every reference engineering example must have at least one automated validation test appropriate to its model.

Include failure tests for:

- Invalid geometry.
- Missing materials.
- Invalid boundary conditions.
- Unsupported solver configurations.
- Mesh generation failure.
- Non-convergence.
- Invalid optimization bounds.
- Stale geometry during a job.
- Cancellation.
- Missing or incompatible solver capabilities.

Do not mark a capability complete merely because a GUI panel or data structure exists.

---

# 18. IMPLEMENTATION PHASES

Implement in carefully scoped phases.

## Phase A — Architecture and audit

- Inspect existing systems.
- Document integration points.
- Identify reusable components.
- Define functional structure and parameterization contracts.
- Identify missing geometry capabilities.

## Phase B — Geometry-driven engineering foundation

- Implement or extend functional structure resources.
- Implement parameter definitions and dependencies.
- Implement geometry revision tracking.
- Implement physical geometry and material-region associations.
- Integrate existing CAD and meshing infrastructure.

## Phase C — Reference engineering models

Prioritize in this order:

1. Microwave cavity.
2. Two-mirror optical resonator.
3. Acoustic horn and reflector.
4. Mechanical resonator.
5. Metamaterial unit cell.

Reuse existing solvers and external scientific packages where appropriate.

Do not implement a new general-purpose solver simply to complete a reference example.

## Phase D — Parameter exploration

- Parameter sweeps.
- Sensitivity analysis.
- Mode tracking.
- Geometry comparisons.
- Response plots.

## Phase E — Function-driven design foundation

- Objective definitions.
- Constraints.
- Candidate generation.
- Candidate evaluation.
- Explainable results.

## Phase F — GUI and automation

- Integrate the workflow into the existing application.
- Expose application services.
- Add automation operations.
- Add project serialization and documentation.

## Phase G — Validation and release readiness

- Execute scientific benchmarks.
- Run regression suites.
- Test project compatibility.
- Test automation.
- Document limitations and unsupported physics.

If a phase depends on missing functionality from Prompt 18, implement the necessary interface or dependency cleanly and report the limitation. Do not fabricate a result to make the phase appear complete.

---

# 19. PERFORMANCE AND ARCHITECTURAL REQUIREMENTS

- Keep physics computation independent of rendering.
- Preserve CPU reference implementations.
- Use existing CUDA acceleration where appropriate.
- Keep geometry and solver operations asynchronous when expensive.
- Reuse mesh and solver results only when their dependencies are unchanged.
- Avoid unnecessary remeshing.
- Support cancellation and progress reporting.
- Avoid copying large field datasets unnecessarily.
- Preserve modular solver architecture.
- Keep domain-specific physical equations separate.
- Avoid monolithic functional-structure classes.
- Do not make the GUI responsible for physics.
- Do not duplicate CEME or optimization logic.
- Keep external scientific dependencies optional where practical.
- Document license and redistribution implications of new dependencies.

---

# 20. DOCUMENTATION

Create or update documentation covering:

- Functional structure architecture.
- Geometry parameterization.
- Geometry-to-physics relationships.
- Energy transformation mechanisms.
- Resonant structure engineering.
- Function-driven design.
- Geometry sensitivity and optimization.
- Manufacturing constraints.
- Scientific assumptions and limitations.
- Reference engineering examples.
- Automation API.
- Validation methodology.

Update the Shape–Function–Energy documentation established in Prompt 17.

Update the CEME documentation established in Prompt 18 where new integration contracts are required.

Include diagrams of the architecture and representative workflows.

Keep documentation synchronized with actual implementation. Clearly distinguish implemented functionality, partial functionality, and future roadmap items.

---

# 21. ACCEPTANCE CRITERIA

This work is complete only when the following are demonstrated:

1. A user can create or import a physical engineering geometry.
2. The user can define and modify meaningful geometric parameters.
3. Geometry changes correctly invalidate dependent meshes and results.
4. Material regions and physical boundaries remain associated with the intended geometry.
5. At least the microwave cavity reference model is implemented and scientifically validated.
6. The optical resonator, acoustic horn, mechanical resonator, and metamaterial examples have implemented model paths or clearly documented dependency-backed limitations; they must not be represented as completed simulations if only visual examples exist.
7. The system can analyze geometry-driven changes in a physical response.
8. The user can execute parameter sweeps and compare results.
9. The user can define at least one measurable functional objective and evaluate candidate designs against it.
10. Results identify the actual physical model, assumptions, solver, geometry revision, and material revision.
11. CEME integration works where energy–matter feedback is required.
12. GUI, Python, and automation use the same application services.
13. Existing projects remain compatible.
14. Existing geometry and physics regression tests pass.
15. Scientific validation tests pass within documented tolerances.
16. Documentation accurately reflects implementation and limitations.

---

# 22. FINAL ENGINEERING REPORT

At completion, provide a detailed report containing:

- Repository audit findings.
- Existing capabilities reused.
- New architecture introduced.
- Files and modules changed.
- Reference models implemented.
- Solvers and external packages integrated.
- Scientific equations and approximations used.
- Validation results and tolerances.
- Test results.
- Performance observations.
- Known limitations.
- Incomplete work and reasons.
- New dependencies and license implications.
- Automation API additions.
- GUI workflow changes.
- Recommended next development prompt.

Do not claim that a feature is implemented unless it is present, integrated, and tested.

Do not conceal failed tests or unsupported physics.

## Final directive

The objective is to make RESONANT a genuine geometry-driven engineering environment.

A user should be able to ask:

- What happens if I change this shape?
- How does this geometry transform the energy entering it?
- Where does the energy go?
- What is stored, transmitted, reflected, concentrated, converted, or dissipated?
- What forces or material changes result?
- How does the structure behave at different frequencies?
- What geometry would produce the physical response I want?
- What tradeoffs and manufacturing constraints determine whether that design is practical?

RESONANT should progressively answer those questions through rigorous physical models, explainable engineering analysis, and controlled design exploration.

**Do not build a collection of decorative shape presets. Build the foundation for engineering physical behavior through geometry.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: - (heuristic)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Implement, capability, creates, first-class
  - evidence: 'Implement' [head]; 'capability' [head]; 'capability' [head]; 'first-class' [head]; 'must become' [head]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: User: 'add this ticket and process in order' (queued after REQ-CHAT-20261126-016)

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context


### Follow-up 2026-11-27T04:50:30.915012+00:00

> The user pasted Prompt 19 (Geometry-Driven Energy Transformation and Functional Structures) a second time with 'add this and implement in order'. The text matches this request section for section (sections 1-22, same acceptance criteria), so no new request was created; it is processed here, in its existing place in the queue.


## Follow-up Context

User: 'add this ticket and process in order' (queued after REQ-CHAT-20261126-016)

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000018
- Evidence: pending
