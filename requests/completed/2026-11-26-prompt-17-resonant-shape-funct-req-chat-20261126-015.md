# Request: # PROMPT 17 — RESONANT: Shape–Function–Energy Architecture
## Foundational Scien

**Request ID:** REQ-CHAT-20261126-015
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T23:22:11-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 17 — RESONANT: Shape–Function–Energy Architecture
## Foundational Scientific Vision, Geometry-Driven Physical Function, Energy–Matter Interaction, and Long-Term Platform Architecture

You are working on **RESONANT — Wave Field Engineering Studio**.

This is a major expansion and clarification of the scientific vision of RESONANT.

The goal is to evolve RESONANT beyond a collection of wave simulation tools into a comprehensive scientific and engineering platform for understanding, modeling, and designing the interactions between:

- Geometry
- Matter
- Material structure
- Energy
- Waves and fields
- Resonance
- Force and momentum
- Energy transfer and conversion
- Physical transformation

This prompt establishes a foundational principle that must influence future application architecture, scientific models, user workflows, documentation, and development priorities.

It does not authorize a wholesale rewrite of the application.

Preserve the existing architecture, domain solvers, optimization framework, Python integration, automation API, geometry systems, and application functionality. Extend them coherently.

---

# 1. Foundational design philosophy: Shape–Function–Energy

Establish the following as a central RESONANT design principle:

**The geometry and internal structure of an object can be the mechanism through which it performs its physical function.**

An engineered object is not merely a container for energy or a passive object exposed to external forces.

Its shape, dimensions, topology, boundaries, material composition, internal structure, and arrangement of components can determine how it interacts with energy and transforms physical behavior.

Examples:

- A microwave cavity confines electromagnetic energy and supports particular resonant modes.
- An optical resonator uses mirrors and optical path geometry to establish stable electromagnetic modes.
- An acoustic horn transforms acoustic impedance and controls wave propagation through its geometry.
- A parabolic reflector redirects incoming wave energy toward a focal region.
- A mechanical resonator stores and exchanges energy through its mass distribution, stiffness, and geometry.
- A phononic crystal controls elastic wave propagation through periodic material structure.
- A metamaterial uses deliberately engineered internal geometry to exhibit effective properties not ordinarily found in its constituent materials.
- A waveguide constrains propagation and supports particular modes according to its geometry and boundary conditions.

These are not simply shapes with labels attached.

Their functions arise from physical interactions involving geometry, materials, excitation, frequency, boundary conditions, and the surrounding environment.

RESONANT must model those interactions.

Do not implement shape-function relationships as superficial object categories or hardcoded behavior.

A geometry must acquire its physical behavior through an appropriate physical model.

# 2. Establish the Shape–Function–Energy model

Formalize the following relationship:

**Geometry + Material Properties + Excitation + Environment → Physical Interaction → Energy and Matter Response → Functional Behavior**

The model must account for:

### Geometry

- Shape.
- Dimensions.
- Topology.
- Surface curvature.
- Surface orientation.
- Internal cavities.
- Channels and passages.
- Layers.
- Interfaces.
- Periodic structures.
- Spatial distributions.
- Boundary conditions.
- Connections between components.
- Geometric tolerances.

### Material properties

- Mass density.
- Number density where relevant.
- Elasticity.
- Plasticity.
- Viscosity.
- Damping.
- Acoustic impedance.
- Electrical conductivity.
- Permittivity.
- Permeability.
- Thermal conductivity.
- Heat capacity.
- Optical properties.
- Magnetic properties.
- Anisotropy.
- Frequency dependence.
- Temperature dependence.
- Nonlinear response.

### Excitation and environment

- Frequency.
- Wavelength.
- Amplitude.
- Phase.
- Waveform.
- Direction.
- Polarization.
- Time dependence.
- Source geometry.
- Source impedance.
- Boundary conditions.
- Environmental pressure.
- Temperature.
- Surrounding medium.

### Physical interaction

- Reflection.
- Transmission.
- Refraction.
- Diffraction.
- Scattering.
- Absorption.
- Resonance.
- Interference.
- Coupling.
- Dissipation.
- Energy storage.
- Momentum transfer.
- Field concentration.
- Wave confinement.
- Material deformation.
- Heat generation.

### Functional behavior

The application must be capable of analyzing and designing structures that:

- Concentrate energy.
- Distribute energy.
- Redirect energy.
- Confine energy.
- Store energy.
- Convert energy.
- Transfer energy.
- Filter frequencies.
- Suppress unwanted modes.
- Amplify selected responses through resonance.
- Attenuate unwanted vibrations or waves.
- Produce controlled mechanical forces.
- Modify stress and strain distributions.
- Control thermal transfer.
- Influence effective material properties.
- Produce desired wave propagation characteristics.

These are engineering objectives, not guaranteed outcomes. Every result must be supported by the applicable physical model.

# 3. Object-centric and function-centric engineering

RESONANT must support two complementary design workflows.

## Object-centric workflow

The user creates or imports an object.

They specify:

- Geometry.
- Materials.
- Internal structures.
- Excitation sources.
- Environment.
- Physical domain.
- Desired measurements.

RESONANT calculates the object's behavior.

## Function-centric workflow

The user begins with a desired physical outcome.

Examples:

- Create a cavity that supports a selected resonant frequency.
- Design a structure that concentrates acoustic energy at a particular location.
- Design an optical resonator with specified mode characteristics.
- Create a mechanical structure that suppresses vibration in a selected frequency band.
- Design a periodic structure that produces an elastic band gap.
- Create a material structure with a specified effective acoustic response.
- Design a geometry that distributes thermal energy uniformly.
- Produce a specified electromagnetic field distribution within a bounded region.

RESONANT searches the available design space for suitable:

- Geometry.
- Dimensions.
- Materials.
- Internal structure.
- Component arrangement.
- Excitation conditions.
- Operating parameters.

The optimization framework must be reusable across physical domains.

Do not duplicate optimization algorithms for each new domain when shared abstractions are appropriate.

However, domain-specific physical models and objective functions must remain mathematically correct.

# 4. Geometry becomes a physical engineering object

Audit the existing geometry and scene architecture.

Determine how geometry can represent more than visual surfaces.

Introduce the concept of **functional geometry**.

A functional geometry is an object whose shape and internal structure are deliberately designed to produce a physical response.

Examples include:

- Resonant cavities.
- Reflectors.
- Lenses.
- Waveguides.
- Horns.
- Channels.
- Acoustic impedance transformers.
- Optical resonators.
- Mechanical resonators.
- Phononic crystals.
- Electromagnetic periodic structures.
- Layered composites.
- Porous structures.
- Metamaterial unit cells.
- Coupled resonator networks.
- Engineered interfaces.

The system must distinguish:

1. Visual geometry.
2. Engineering geometry.
3. Physical boundary geometry.
4. Material regions.
5. Simulation mesh.
6. Functional or periodic internal structures.

A visual object must not automatically be assumed to have sufficient geometric fidelity for physical simulation.

Introduce a path for geometry to be analyzed and converted into the appropriate simulation representation.

Preserve the existing CAD import and geometry architecture.

# 5. Resonance as a cross-domain physical phenomenon

Establish resonance as a first-class cross-domain engineering concept.

Resonance must not be limited to acoustic frequency calculations.

Support the conceptual and architectural requirements for:

- Acoustic resonances.
- Electromagnetic cavity modes.
- Optical resonators.
- Mechanical structural modes.
- Elastic wave resonances.
- Electrical resonant circuits where appropriate.
- Coupled resonators.
- Distributed resonant structures.
- Local resonances in metamaterials.

Represent and analyze:

- Natural frequencies.
- Mode shapes.
- Modal energy.
- Damping.
- Quality factor.
- Bandwidth.
- Coupling.
- Mode splitting.
- Frequency response.
- Resonance localization.
- Spatial field distributions.
- Energy storage and dissipation.

Do not force these domains into an identical mathematical formulation.

Provide common application concepts where useful, while retaining the appropriate domain-specific equations and solvers.

# 6. Metamaterials and engineered structures

Establish metamaterial research as a major long-term capability.

The architecture must support structures whose effective physical properties emerge from deliberately engineered geometry and internal organization.

Investigate:

- Acoustic metamaterials.
- Electromagnetic metamaterials.
- Mechanical metamaterials.
- Phononic crystals.
- Photonic crystals.
- Periodic structures.
- Locally resonant structures.
- Subwavelength resonators.
- Layered composites.
- Anisotropic structures.
- Spatially graded structures.
- Architected lattices.
- Elastic and acoustic band gaps.
- Frequency-selective transmission.
- Effective constitutive parameters.
- Effective density.
- Effective stiffness.
- Effective impedance.
- Effective permittivity and permeability where applicable.
- Negative effective parameters where physically justified.

The application must distinguish between:

- Actual constituent material properties.
- Effective homogenized properties.
- Frequency-dependent effective behavior.
- Localized resonant effects.
- Full structural response.

Do not assign unusual effective properties merely because a structure is called a metamaterial.

They must be derived or validated using an appropriate model.

# 7. Coupled Energy–Matter Engine

Formalize the Coupled Energy–Matter Engine as a major central platform subsystem.

Its purpose is to connect the existing physics domains and enable physical interactions between them.

The architecture must support exchanges of:

- Energy.
- Momentum.
- Force.
- Heat.
- Charge.
- Material state.
- Geometry.
- Stress.
- Strain.
- Fluid state.
- Relevant domain-specific physical quantities.

Example interaction:

1. An electromagnetic field propagates through a material.
2. The material absorbs part of the incident energy.
3. Absorbed energy produces local heating.
4. Temperature changes material properties.
5. Thermal expansion produces mechanical stress.
6. Deformation changes geometry.
7. Changed geometry alters the electromagnetic field.
8. The next simulation step accounts for the updated state.

This is a coupled feedback system.

The architecture must eventually support:

- One-way coupling.
- Two-way coupling.
- Multi-domain coupling.
- Sequential coupling.
- Iterative coupling.
- Fully time-dependent coupling where appropriate.

Define a consistent interface for coupling solvers without requiring them to share identical numerical methods.

Support different spatial and temporal resolutions.

Define interpolation, projection, transfer, synchronization, and conservation requirements.

Energy and momentum accounting must be explicit.

Do not claim conservation merely because quantities have been transferred between solvers.

# 8. Material state as an evolving physical entity

Introduce the architectural concept of a dynamic material state.

A material is not necessarily represented by one fixed set of properties.

Its physical response may depend on:

- Position.
- Time.
- Temperature.
- Pressure.
- Stress.
- Strain.
- Frequency.
- Electric field.
- Magnetic field.
- Composition.
- Phase.
- Microstructure.
- Damage.
- Deformation history.

Define how the application represents material properties that vary across space and time.

Preserve domain-specific constitutive models.

The architecture must accommodate:

- Linear materials.
- Nonlinear materials.
- Anisotropic materials.
- Dispersive materials.
- Temperature-dependent materials.
- Viscoelastic materials.
- Plastic materials.
- Porous materials.
- Composite materials.
- Phase-changing materials.
- Materials with evolving internal structure.

# 9. Multiscale scientific modeling

Establish a future path connecting macroscopic engineering simulation to material microstructure and microscopic physics.

Define the following modeling scales:

### Macroscopic

- Entire objects.
- Engineering components.
- Cavities.
- Waveguides.
- Structures.
- Large-scale field distributions.

### Mesoscopic

- Grain structures.
- Pores.
- Fibers.
- Inclusions.
- Layered composites.
- Periodic unit cells.
- Internal interfaces.

### Molecular

- Molecular arrangements.
- Bonding interactions.
- Molecular dynamics.
- Local material response.

### Atomic

- Crystal structures.
- Lattice vibrations.
- Defects.
- Dislocations.
- Phonons.
- Electronic behavior.

### Quantum

- Electronic states.
- Quantum transitions.
- Quantum interactions.
- Material properties derived from electronic structure.

Do not implement every scale in this milestone.

Define a modular integration architecture that can use established external scientific packages where appropriate.

Investigate appropriate interfaces for molecular dynamics and electronic-structure tools.

Determine how microscopic results could inform macroscopic material models.

For example, microscopic calculations may help estimate:

- Elastic constants.
- Thermal conductivity.
- Dielectric properties.
- Magnetic properties.
- Phonon behavior.
- Material response under different conditions.

Clearly distinguish measured properties, theoretical predictions, computed properties, and homogenized approximations.

# 10. Visualization requirements

Extend the long-term visualization vision.

RESONANT must ultimately make it possible to see how energy interacts with an object's geometry and internal structure.

Plan for visualization of:

- Field distributions.
- Energy density.
- Energy flux.
- Momentum density and flux where defined.
- Wave propagation.
- Standing waves.
- Resonant modes.
- Mode shapes.
- Stress and strain.
- Temperature.
- Material phase.
- Charge and current distributions.
- Acoustic pressure.
- Electromagnetic fields.
- Mechanical displacement.
- Material deformation.
- Microstructure.
- Periodic unit cells.
- Band structures.
- Spatially varying effective properties.

Support synchronized visualization of multiple physical quantities.

The user should be able to inspect the same location in the object across different physical domains.

Do not create visualizations that imply unsupported physical accuracy.

# 11. Documentation updates

Audit all existing project documentation before making changes.

The project already has foundational documents covering:

- Project vision.
- Architecture.
- Geometry and materials.
- Physics architecture.
- Acoustic simulation.
- Mechanical simulation.
- Electromagnetic simulation.
- Optimization.
- Python and automation.
- Linux packaging.

Preserve existing documentation and update it where appropriate.

Create or update the following:

1. `docs/vision/SHAPE_FUNCTION_ENERGY.md`
2. `docs/physics/COUPLED_ENERGY_MATTER.md`
3. `docs/physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md`
4. `docs/materials/MATERIAL_STATE_ARCHITECTURE.md`
5. `docs/geometry/FUNCTIONAL_GEOMETRY.md`
6. `docs/physics/RESONANT_STRUCTURES.md`
7. `docs/materials/METAMATERIALS_AND_ENGINEERED_STRUCTURES.md`
8. `docs/physics/MULTISCALE_MODELING.md`
9. `docs/architecture/COUPLED_MULTIPHYSICS_ARCHITECTURE.md`
10. `docs/roadmap/ENERGY_MATTER_RESEARCH_ROADMAP.md`

If the repository uses a different documentation organization, follow its established conventions and update these paths accordingly.

Update the primary project vision and architecture documents to reference the new direction.

Add a clear conceptual explanation of the Shape–Function–Energy principle.

Include examples and diagrams.

Distinguish current capabilities, planned capabilities, research prototypes, and theoretical future directions.

Do not document planned functionality as implemented.

# 12. Relationship with existing architecture

Audit how the new vision maps to existing systems.

Explicitly map it to:

- Scene management.
- Geometry.
- Materials.
- Acoustic physics.
- Mechanical physics.
- Electromagnetic physics.
- Simulation framework.
- Optimization.
- Python API.
- Automation framework.
- Visualization.
- Project storage.
- Measurement and experiments.
- Help and documentation.

Identify missing abstractions and architectural gaps.

Do not introduce duplicate systems.

Do not create a separate application or parallel simulation framework.

The existing domain-specific solvers must remain independent where their physical equations and numerical methods require it.

The Coupled Energy–Matter Engine should coordinate these systems, not replace them.

# 13. Development roadmap

Create a staged roadmap for implementing the new direction.

Recommended sequence:

1. Shape–Function–Energy architecture and documentation.
2. Unified material state and physical quantity conventions.
3. Energy and momentum accounting.
4. Coupled solver interfaces.
5. One-way and two-way multiphysics coupling.
6. Geometry-driven resonant systems.
7. Metamaterial and periodic structure modeling.
8. Microstructure and homogenization.
9. Multiscale external solver integration.
10. Dynamic material response.
11. Function-driven inverse design.
12. Experimental validation.

For each stage, define:

- Goals.
- Required architectural work.
- Dependencies.
- Expected capabilities.
- Scientific risks.
- Validation requirements.
- Acceptance criteria.

Identify which existing components can be reused and which need extension.

# 14. Engineering constraints

- C++20 and existing Qt 6 architecture remain authoritative.
- Preserve CMake and the current build system.
- Preserve the Protobuf/gRPC automation contract.
- Preserve Python integration.
- Preserve the existing domain-specific solver architecture.
- Keep scientific computation independent of visualization.
- Keep optional scientific dependencies modular.
- Do not introduce unnecessary mandatory dependencies.
- Evaluate licenses and redistribution implications.
- Use SI units internally.
- Preserve coordinate, phase, frequency, and field conventions.
- Maintain reproducible simulation provenance.
- Require scientific validation.
- Avoid arbitrary hardcoded physical behavior.
- Avoid placeholder implementations that claim to perform real physics.

Do not start a wholesale implementation of the Coupled Energy–Matter Engine in this milestone.

This prompt establishes and documents the scientific architecture and development direction.

If a small architectural correction is essential to prevent an inconsistency, make the minimal justified change and document it.

# 15. Required final report

At completion, report:

1. Documentation created.
2. Existing documentation updated.
3. Architecture gaps identified.
4. Existing systems that can be reused.
5. Proposed new abstractions.
6. Recommended implementation sequence.
7. Scientific modeling risks.
8. External scientific packages worth evaluating.
9. Tests performed.
10. Any implementation changes made.

The result must be a coherent scientific vision that can guide the next several years of RESONANT development.

**The ultimate goal is to make RESONANT a platform where we can design an object's physical function through its geometry, material structure, and interaction with energy—and predict, understand, and engineer the resulting behavior.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: - (heuristic)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence low)
  - validation: PASS
  - classifier_reason: no framing in the request statement; body suggests feature: Acceptance criteria, Add, Create, Extend
  - evidence: 'evolve' [body]; 'framework' [body]; 'integration' [body]; 'Extend' [body]; 'implement' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: User: 'add this as a ticket. Process in order' (queued after REQ-CHAT-20261126-013; REQ-CHAT-20261126-014 is in processing)

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

User: 'add this as a ticket. Process in order' (queued after REQ-CHAT-20261126-013; REQ-CHAT-20261126-014 is in processing)

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000016
- Evidence: pending
