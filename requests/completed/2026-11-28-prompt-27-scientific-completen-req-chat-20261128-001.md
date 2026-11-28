# Request: # PROMPT 27 — SCIENTIFIC COMPLETENESS, MULTISCALE PHYSICS, AND RELEASE READINESS

**Request ID:** REQ-CHAT-20261128-001
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-28T11:25:20-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 27 — SCIENTIFIC COMPLETENESS, MULTISCALE PHYSICS, AND RELEASE READINESS

## Project: RESONANT — Wave Field Engineering Studio

## Mission

Implement the next major development milestone for RESONANT.

This milestone has two overarching objectives:

1. Close the outstanding scientific, computational, testing, and architectural gaps identified in the previous development cycles.
2. Establish a reproducible, professionally distributable application capable of supporting impressive, scientifically defensible demonstration videos.

This is not a cosmetic release-preparation exercise.

RESONANT must become a more complete engineering environment, capable of representing increasingly sophisticated physical structures, energy transformations, material architectures, and experimental observations.

All capabilities must be integrated into the existing architecture rather than implemented as isolated demonstrations.

Preserve the existing scientific standards, provenance requirements, uncertainty accounting, validation gates, and explicit model-validity boundaries.

---

# PART I — AUDIT AND ESTABLISH BASELINE

Before changing anything:

1. Read AGENTS.md and all applicable engineering instructions.
2. Inspect the current architecture and implementation status.
3. Review ADR-000227 and ADR-000228.
4. Review WORK-000024 and WORK-000025.
5. Review VER-000024 and VER-000025.
6. Inspect the outstanding work recorded in IMPLEMENTATION_STATUS.md.
7. Audit the existing demo system, automation framework, recording facilities, and any previous video-generation infrastructure.

Produce a gap analysis covering:

- Scientific capabilities.
- Multiphysics integration.
- Material representation.
- Interfaces and boundary conditions.
- Atomistic and multiscale integration.
- GUI and visualization.
- Testing and reliability.
- Packaging and deployment.
- Demo automation and video production.

Do not duplicate existing functionality.

# PART II — MULTIPHYSICS AND ENERGY CONVERSION

## 2.1 Unified energy-conversion architecture

Extend the existing coupled physics architecture to support additional energy-conversion mechanisms.

The architecture must support:

- Mechanical energy.
- Acoustic energy.
- Thermal energy.
- Electrical energy.
- Electromagnetic energy.
- Optical energy.
- Elastic strain energy.
- Dissipative energy.

Establish consistent energy accounting between interacting physical domains.

Every conversion mechanism must explicitly identify:

- Input energy.
- Output energy.
- Stored energy.
- Dissipated energy.
- Coupling coefficients.
- Boundary conditions.
- Assumptions.
- Validity limits.
- Uncertainty.

Energy must never appear or disappear through an undocumented numerical operation.

## 2.2 Piezoelectric multiphysics

Extend piezoelectric modeling beyond the existing single-mode cantilever.

Implement a generalizable coupled electromechanical formulation suitable for integration into the coupled engine.

Support, as appropriate:

- Piezoelectric constitutive relationships.
- Stress-charge and strain-charge formulations.
- Anisotropic piezoelectric coefficients.
- Electrical boundary conditions.
- Mechanical boundary conditions.
- Electrode geometry.
- Dielectric properties.
- Mechanical damping.
- Electrical loading.
- Frequency-dependent behavior.

Include:

- Piezoelectric plates.
- Piezoelectric beams.
- Cantilever structures.
- Layered piezoelectric composites.

Preserve the existing cantilever implementation as a reference model.

Validate against analytical solutions and established benchmark cases.

## 2.3 Thermoelectric conversion

Implement thermoelectric modeling.

Include:

- Seebeck effect.
- Peltier effect.
- Thomson effect where supported.
- Temperature-dependent electrical conductivity.
- Temperature-dependent thermal conductivity.
- Seebeck coefficient.
- Joule heating.
- Thermal and electrical boundary conditions.

Support thermoelectric generators and coolers.

Include energy and entropy accounting.

Validate against analytical thermoelectric models and established reference cases.

## 2.4 Magnetostrictive conversion

Implement magnetostrictive material behavior.

Include:

- Magnetic field excitation.
- Magnetostrictive strain.
- Mechanical response.
- Magnetic and mechanical coupling.
- Material anisotropy where supported.
- Hysteresis and losses where a validated model is available.

Support magnetostrictive actuators and energy harvesters.

Explicitly distinguish linear approximations from nonlinear constitutive models.

## 2.5 Photovoltaic conversion

Implement a physically grounded photovoltaic modeling capability.

Include:

- Incident optical energy.
- Spectral response.
- Absorption.
- Carrier generation.
- Electrical output.
- Temperature dependence.
- Equivalent-circuit models.

Begin with a validated equivalent-circuit model.

Do not represent semiconductor-level physics as solved unless the required transport equations are actually implemented.

## 2.6 Cross-domain energy accounting

Integrate these mechanisms into the existing energy ledger.

Demonstrate that energy can be followed through a chain of transformations.

For example:

Acoustic excitation → mechanical strain → piezoelectric conversion → electrical output → electrical load.

Every stage must have measurable quantities and an independently verifiable energy balance.

# PART III — THREE-DIMENSIONAL STRUCTURES AND INTERFACES

## 3.1 Three-dimensional layered structures

Address the missing full-shell modeling capability identified in ADR-000228.

Develop a geometry-aware structural modeling architecture for layered and coated three-dimensional bodies.

Support:

- Three-dimensional geometry.
- Layered shells.
- Coatings.
- Composite structures.
- Variable thickness.
- Material transitions.
- Curved interfaces.
- Spatially varying material properties.

Use an appropriate numerical method, preferably integrating an established finite-element architecture if compatible with the existing application.

Do not pretend that layered-plate theory is equivalent to a general three-dimensional shell solution.

Retain analytical plate and beam models as lower-fidelity reference methods.

## 3.2 Non-ideal interface physics

Implement a general interface representation.

Support progressively:

- Perfect bonding as an explicitly selected idealization.
- Finite normal stiffness.
- Finite tangential stiffness.
- Thermal contact resistance.
- Mechanical slip.
- Friction.
- Partial debonding.
- Interface failure criteria.

Every interface must have its own properties and provenance.

A perfect interface must never be silently assumed.

Where an interface model is unavailable, the solver must identify the limitation and refuse the unsupported analysis.

## 3.3 Functionally graded materials

Expand material-field representation.

Support:

- Spatially varying density.
- Spatially varying stiffness.
- Spatially varying thermal conductivity.
- Spatially varying electrical conductivity.
- Spatially varying dielectric properties.
- Spatially varying piezoelectric coefficients.
- Spatially varying magnetic properties.

Support directional and anisotropic material fields.

Each field must identify its coordinate system and tensor representation.

Support interpolation between measured, imported, and analytically defined fields.

# PART IV — MULTISCALE AND ATOMISTIC ARCHITECTURE

No atomistic engines are currently installed.

Do not make the application dependent on an unavailable external solver.

Instead, establish a formal multiscale integration architecture.

## 4.1 Solver integration

Design an adapter architecture capable of integrating appropriate external atomistic engines.

Investigate compatibility with:

- LAMMPS.
- GROMACS where appropriate.
- Other scientifically relevant atomistic engines.

Do not assume these engines are interchangeable.

Identify their respective domains of applicability.

## 4.2 Atomistic-to-continuum bridge

Design the data model for transferring information between scales.

Potential transferred quantities include:

- Elastic constants.
- Thermal conductivity.
- Density.
- Diffusion coefficients.
- Interfacial properties.
- Defect-related properties.
- Temperature-dependent constitutive behavior.

Preserve:

- Units.
- Coordinate systems.
- Temperature.
- Pressure.
- Composition.
- Simulation conditions.
- Statistical uncertainty.
- Provenance.

Do not generate fabricated atomistic results.

When an external engine is unavailable, report the missing dependency and provide a testable integration interface.

## 4.3 Microstructure representation

Establish foundations for:

- Grains.
- Grain boundaries.
- Defects.
- Inclusions.
- Porosity.
- Phase distributions.
- Multiphase materials.

Ensure microstructure can eventually influence continuum-scale material properties.

# PART V — MANUFACTURING AND DESIGN INTEGRATION

Integrate the expanded physical models with the existing functional architecture and manufacturing systems.

Support manufacturing-aware constraints involving:

- Minimum feature dimensions.
- Layer thickness.
- Material compatibility.
- Thermal processing.
- Residual stress.
- Interface integrity.
- Geometric limitations.
- Manufacturing tolerances.

Expand optimization to account for the new physics.

The optimizer must be capable of evaluating competing objectives, including:

- Energy conversion efficiency.
- Structural stiffness.
- Mass.
- Thermal performance.
- Electrical performance.
- Manufacturing feasibility.
- Reliability.

Preserve Pareto trade-off sets.

Never conceal infeasible designs by silently relaxing manufacturing constraints.

# PART VI — GEOMETRY AND VISUALIZATION

## 6.1 Three-dimensional material visualization

Implement the missing 3D visualization foundations.

Visualize:

- Material regions.
- Layer boundaries.
- Interfaces.
- Graded property fields.
- Directional properties.
- Stress fields.
- Strain fields.
- Temperature fields.
- Electric fields.
- Magnetic fields.
- Energy density.

Provide independent controls for each visualization layer.

Support color scales with explicit units and numerical ranges.

Do not use visual interpolation to imply physical resolution that the underlying solver does not possess.

## 6.2 CAD integration

Establish CAD export capabilities.

Support appropriate exchange formats, prioritizing STEP and STL where suitable.

Preserve material-region boundaries where the target format permits.

Document information that cannot survive export.

## 6.3 Matter editor

Expand the existing Matter panel.

Provide editing capabilities for:

- Material regions.
- Layer stacks.
- Interface properties.
- Graded material fields.
- Functional assignments.
- Manufacturing processes.

All edits must create traceable revisions.

Never silently modify an existing material record.

# PART VII — EXPERIMENTAL LABORATORY

Build upon the existing experimental laboratory.

Extend measurement comparison to the additional physical domains.

Support appropriate measurements involving:

- Acoustic response.
- Mechanical vibration.
- Electrical output.
- Temperature.
- Thermal gradients.
- Magnetic response.
- Optical response.

Maintain the existing read-only original-file policy.

Expand calibration where appropriate.

Preserve the requirement that calibration validation must use independent data.

Add cross-domain experimental energy-balance reports.

Ensure synthetic measurements remain explicitly labeled synthetic.

# PART VIII — RELIABILITY AND TESTING

Address the outstanding reliability issues.

## 8.1 Flaky cancellation test

Investigate and eliminate the flakiness in:

Jobs/RunningJobIsCancelledPromptly

Identify the actual race condition or synchronization defect.

Do not merely increase timeouts or disable the test.

Add deterministic regression coverage.

## 8.2 C++ gRPC transport

Resolve the outstanding dependency requirement:

libgrpc++-dev  
protobuf-compiler-grpc

Check whether the packages are installed.

If not, document the required system installation and request appropriate authorization rather than silently skipping the integration.

Build and test the C++ transport.

Verify interoperability with existing services and MCP tools.

## 8.3 Test coverage

Expand testing across:

- Physics unit tests.
- Analytical benchmarks.
- Coupled-system integration.
- Material architecture.
- Interface models.
- Measurement ingestion.
- Calibration.
- Optimization.
- Manufacturing constraints.
- GUI workflows.
- Service interfaces.
- MCP tools.
- Serialization.
- Reproducibility.

Every new solver must have at least one independently calculated reference result.

Add conservation and dimensional-consistency checks.

# PART IX — PACKAGING AND DISTRIBUTION

This work requires a PROCESS-CHANGE request because the existing package test lane and Deployment section of AGENTS.md need modification.

## 9.1 Process change

Create the required PROCESS-CHANGE request.

Review and update the deployment process to accommodate:

- Clean-machine testing.
- Reproducible builds.
- Runtime dependency discovery.
- Qt libraries and plugins.
- C++ dependencies.
- Resources.
- Offline documentation.
- Example projects.
- Demonstration assets.

Preserve the project's existing engineering governance.

## 9.2 Linux distribution

Produce a self-contained Linux distribution.

Target a ZIP package that includes:

- Application executable.
- Required Qt runtime libraries.
- Required plugins.
- Resources.
- Supporting libraries.
- Offline HTML documentation.
- Example projects.
- Reference data.
- Demonstration scenarios.

Use linuxdeployqt or an appropriate alternative.

The package must not require root privileges, installation scripts, or manual environment configuration to launch.

Document the supported Linux compatibility baseline.

## 9.3 Clean-machine validation

Use the existing resonant-clean/* Docker images.

Test the distribution in a clean environment.

Verify:

- Startup.
- GUI rendering.
- Physics calculations.
- Example loading.
- Data import.
- Saving and reopening projects.
- Service availability.
- MCP integration where supported.
- Offline operation.
- Resource discovery.

Record all missing dependencies and failures.

Do not declare the distribution complete until the clean-machine tests pass.

# PART X — LICENSING AND RELEASE GOVERNANCE

The project's own license remains an owner decision.

Do not independently select or impose a license.

Prepare a concise licensing decision document identifying:

- Available licensing choices.
- Consequences for commercial distribution.
- Compatibility with incorporated dependencies.
- LGPL source-distribution obligations.
- Required notices.
- Source availability requirements.

Preserve LGPL obligations, including publishing corresponding LGPL source code with any applicable release.

Do not finalize release licensing until the owner approves the decision.

## ADR bookkeeping

Investigate ADR-000217, which was allocated but never used.

Determine whether it should be formally recorded as unused or reassigned under the project's ADR conventions.

Do not silently reuse decision numbers.

# PART XI — DEMONSTRATION SYSTEM READINESS

We are deliberately not producing the final demonstration videos in this milestone.

However, audit the existing demonstration system and ensure that the application is ready to support the next phase.

Establish or verify reusable demonstration scenarios for:

1. Electric Bell.
2. Energy-Focusing Metamaterial.
3. Resonant Pendulum.

Each scenario should eventually support:

- Reproducible project setup.
- Deterministic simulation configuration.
- Controlled input excitation.
- Observable physical quantities.
- Synchronized visualization.
- Simulation and measurement comparison.
- Energy accounting.
- Exportable numerical results.
- Screenshots.
- Interaction traces.
- Video recording.

The demonstration system must distinguish actual application output from post-production graphics.

Do not create synthetic measurements that could be mistaken for experimental results.

Prepare the infrastructure for 60–90 second flagship demonstration videos.

The Electric Bell should eventually demonstrate:

Bell excitation → acoustic field → Helmholtz resonance → piezoelectric cantilever → electrical output.

The Energy-Focusing Metamaterial should demonstrate geometry-driven wave interaction, focusing, and energy redistribution.

The Resonant Pendulum should demonstrate coupled dynamic behavior, resonance, energy transfer, and dissipation.

These are future demonstration deliverables, not excuses to bypass missing physics.

# PART XII — DOCUMENTATION AND COMPLETION

Update:

- Architecture documentation.
- Physics documentation.
- Model-validity documentation.
- API documentation.
- MCP documentation.
- Installation and deployment documentation.
- Example projects.
- IMPLEMENTATION_STATUS.md.

Create appropriate ADRs, WORK records, and VER evidence.

For every capability, distinguish:

1. Implemented and verified.
2. Implemented with limitations.
3. Architecturally prepared but not implemented.
4. Dependent on unavailable external software.
5. Not supported.

Do not describe architectural preparation as completed scientific functionality.

# EXECUTION STRATEGY

This is a substantial milestone.

Break the work into coherent implementation phases and use the existing request-management and completion-gate framework.

Do not attempt a superficial implementation of every feature.

Prioritize correctness and integration.

Suggested order:

1. Baseline audit and process-change request.
2. Reliability and dependency resolution.
3. Three-dimensional geometry and interface foundations.
4. Expanded multiphysics and energy conversion.
5. Material fields and multiscale architecture.
6. Manufacturing and optimization integration.
7. Visualization and CAD integration.
8. Experimental laboratory expansion.
9. Packaging and clean-machine validation.
10. Documentation and final audit.

Use parallel agents where useful, but establish clear ownership and integration boundaries.

Every completed request must pass the appropriate T2 verification tier, including:

- Core tests.
- GUI tests.
- CMake build.
- CTest.
- Relevant audits.
- Completion gate.

Commit and push completed work according to project conventions.

Do not leave untracked implementation changes.

## FINAL ACCEPTANCE CRITERIA

The milestone is complete only when:

- New physical mechanisms are genuinely implemented and validated.
- The coupled engine accounts for energy across supported domains.
- Three-dimensional layered structures have an explicit modeling pathway.
- Non-ideal interfaces are represented without hidden idealizations.
- Material fields support directional and spatial variation.
- Multiscale integration has a documented, testable architecture.
- Manufacturing constraints remain integrated with optimization.
- Three-dimensional visualization and CAD exchange are operational.
- Experimental validation supports the expanded physics.
- The cancellation test is reliable.
- The gRPC transport is built and verified.
- The deployment process is documented and approved.
- The clean-machine package tests pass.
- Licensing obligations are documented and the owner decision is recorded.
- The demonstration infrastructure is audited and ready for the next phase.

At completion, provide a comprehensive engineering report.

Include:

- Implemented capabilities.
- Outstanding limitations.
- Benchmark results.
- Test results.
- Packaging results.
- External dependencies.
- ADRs and work records.
- Commit identifiers.
- Demonstration readiness assessment.

**The ultimate objective is to make RESONANT an engineering environment in which sophisticated physical structures can be conceived, modeled, optimized, experimentally evaluated, and ultimately demonstrated with scientific credibility.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Implement, capabilities, milestone, must become
  - evidence: 'Implement' [head]; 'milestone' [head]; 'milestone' [head]; 'objectives' [head]; 'must become' [head]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: Pasted by the user as one message after the status report on Demo Studio. Processed in phases as separate requests (the prompt's execution strategy): this request carries the full statement and the baseline deliverables (Part I gap analysis, Part X licensing decision document and ADR-000217 bookkeeping, the Part XI demonstration-system audit, the phase plan); each later phase is its own request referring to this one.

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

Pasted by the user as one message after the status report on Demo Studio. Processed in phases as separate requests (the prompt's execution strategy): this request carries the full statement and the baseline deliverables (Part I gap analysis, Part X licensing decision document and ADR-000217 bookkeeping, the Part XI demonstration-system audit, the phase plan); each later phase is its own request referring to this one.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000026
- Evidence: pending
