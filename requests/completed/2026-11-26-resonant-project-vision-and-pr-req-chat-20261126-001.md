# Request: # RESONANT — Project Vision and Product Requirements

## Project Identity

**Nam

**Request ID:** REQ-CHAT-20261126-001
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T15:10:36-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # RESONANT — Project Vision and Product Requirements

## Project Identity

**Name:** Resonant  
**Product:** Wave Field Engineering Studio  
**Tagline:** Design. Simulate. Focus. Measure.

Resonant is a native desktop engineering application for designing, simulating, visualizing, optimizing, and experimentally investigating wave propagation and wave interactions in physical environments.

The application must support multiple physical wave domains through a shared underlying architecture:

1. Acoustic waves — audible sound, ultrasound, and infrasound.
2. Mechanical waves — vibration through solids, structures, surfaces, and membranes.
3. Electromagnetic waves — RF propagation, antenna arrays, and phased-array systems.
4. Future extensible domains — additional wave phenomena through modular physics implementations.

This is intended to become a serious engineering and scientific experimentation platform, not simply a phased-array calculator or audio visualization tool.

## 1. Core Product Vision

The central capability is the ability to create a three-dimensional physical environment, populate it with emitters and objects, define material properties, specify desired wave-field behavior, and calculate or simulate the resulting physical phenomena.

Users must be able to work in both 2D and 3D.

The application should support:

- Arbitrary emitter placement.
- Regular and irregular phased arrays.
- Individual emitter frequency, amplitude, phase, delay, and waveform configuration.
- Automatic calculation of individual emitter drive signals.
- Point, line, surface, and volumetric focusing targets.
- Passive wave manipulation through physical geometry.
- Active wave manipulation through emitter control.
- Combined active and passive wave manipulation.
- Physical material modeling.
- Wave propagation through and around objects.
- 3D field visualization.
- Simulation comparison.
- Experimental measurements and calibration.
- Automated optimization of emitter configurations and geometry.

## 2. Major Product Capabilities

### A. Three-dimensional modeling environment

Create an interactive 3D engineering workspace.

Users must be able to:

- Create and manipulate geometric objects.
- Place and orient emitters.
- Create arrays parametrically.
- Import external CAD geometry.
- Assign materials.
- Define targets and measurement locations.
- Create simulation regions.
- Define boundary conditions.
- Inspect objects and simulation results.

### B. Emitter and phased-array design

Support:

- Point sources.
- Directional sources.
- Speakers.
- Ultrasonic transducers.
- Mechanical vibration actuators.
- RF antenna elements.
- Custom emitter definitions.

Support linear, planar, circular, spherical, and arbitrary emitter arrangements.

Every emitter must have independently configurable properties.

### C. Inverse field design

Users must be able to specify a desired field and calculate the emitter inputs required to produce it.

Target types should eventually include:

- Single focal points.
- Multiple focal points.
- Lines.
- Planes.
- Curved surfaces.
- Three-dimensional volumes.
- Desired amplitude distributions.
- Desired phase distributions.
- Regions requiring field suppression.

Support analytical solutions and numerical optimization.

### D. Passive wave manipulation

This is a major product requirement.

Users must be able to create physical shapes that manipulate wave propagation without requiring independently controlled emitters.

Support modeling and eventual optimization of:

- Parabolic reflectors.
- Elliptical reflectors.
- Spherical reflectors.
- Acoustic lenses.
- Waveguides.
- Resonant cavities.
- Diffraction structures.
- Layered structures.
- Metamaterial structures.
- Custom parametric geometries.

Calculate the resulting physical field based on actual geometry and material properties.

Do not assume that a particular geometric shape automatically produces a particular focusing result. The simulation must account for the applicable physical model.

### E. Materials library

Create an extensible material library.

Acoustic properties:
- Density.
- Speed of sound.
- Acoustic impedance.
- Attenuation.
- Absorption.
- Dispersion.
- Frequency dependence.

Mechanical properties:
- Density.
- Young's modulus.
- Poisson's ratio.
- Damping.
- Elasticity.
- Anisotropy.
- Frequency-dependent mechanical response.

Electromagnetic properties:
- Relative permittivity.
- Relative permeability.
- Electrical conductivity.
- Dielectric loss.
- Frequency dependence.
- Anisotropy.

Support layered and composite materials.

Material properties should distinguish between measured, manufacturer-supplied, literature-derived, and theoretical values.

### F. Visualization

The 3D viewport is a central feature.

Support:

- Instantaneous wave fields.
- Time-averaged intensity or energy-related quantities.
- Phase visualization.
- Pressure visualization.
- Displacement visualization.
- Electric and magnetic field visualization.
- Volumetric rendering.
- Isosurfaces.
- 2D cross sections.
- Arbitrary clipping planes.
- Animated propagation.
- Field comparisons.

Visualization must use actual simulation data, not decorative approximations.

### G. Experiments and measurements

Users must eventually be able to connect physical hardware, generate signals, collect measurements, calibrate emitters, and compare measured results with simulated predictions.

Support reproducible experiment configurations.

## 3. Development Philosophy

Build Resonant incrementally, but establish the complete architectural foundation early.

Priorities:

1. Correct physical modeling.
2. Extensible software architecture.
3. Reproducible results.
4. Usable 3D engineering environment.
5. Performance and GPU acceleration.
6. Hardware independence.
7. Long-term maintainability.

Do not sacrifice architecture for superficial early demonstrations.

Do not implement complex physics prematurely.

## 4. Initial Release

V1 should provide a complete working acoustic simulation workflow.

Required capabilities:

- Native desktop application.
- Interactive 3D viewport.
- Geometry creation and import.
- Emitter placement.
- Parametric array generation.
- Initial acoustic material library.
- Analytical acoustic propagation.
- Phase and time-delay calculations.
- Point and basic region focusing.
- Initial inverse-design solver.
- 2D and 3D field visualization.
- Simulation configuration and reproducibility.
- Project save/load.
- Initial Python scripting interface.

The application architecture must anticipate future mechanical and electromagnetic simulation without requiring a fundamental redesign.

## 5. Non-Goals for V1

Do not attempt to implement all physical domains immediately.

Do not attempt to recreate a complete commercial CAD package.

Do not require physical hardware to use the initial application.

Do not represent simplified analytical calculations as full physical simulations.

Do not introduce complex GPU computing dependencies before a working CPU reference implementation exists.

## 6. Long-Term Vision

Resonant should eventually become a unified wave-field engineering laboratory.

A user should be able to:

1. Create a physical environment.
2. Design an emitter configuration.
3. Define the materials and geometry.
4. Specify a desired wave field.
5. Calculate the required emitter inputs.
6. Simulate propagation.
7. Optimize passive and active wave manipulation.
8. Visualize and analyze results.
9. Export signals to physical hardware.
10. Measure the real-world result.
11. Update the model using experimental data.

Treat this vision as the guiding product requirement for all architectural decisions.

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence low)
  - validation: PASS
  - classifier_reason: no framing in the request statement; body suggests feature: Build, Create, Design, Support
  - evidence: 'must support' [body]; 'capability' [body]; 'ability to' [body]; 'create' [body]; 'must be able' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: First ticket of the project, submitted by the user in chat: the project vision and product requirements document. Earlier bootstrap answers in the same session: C++ / Qt 6 Widgets, qmake, layout src/core (UI-free library) + src/app + tests/, Qt Test, greenfield, interactive processing only.

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

First ticket of the project, submitted by the user in chat: the project vision and product requirements document. Earlier bootstrap answers in the same session: C++ / Qt 6 Widgets, qmake, layout src/core (UI-free library) + src/app + tests/, Qt Test, greenfield, interactive processing only.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000001
- Evidence: pending
