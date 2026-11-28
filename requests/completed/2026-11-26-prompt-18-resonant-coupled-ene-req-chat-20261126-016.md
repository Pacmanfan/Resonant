# Request: # PROMPT 18 — RESONANT Coupled Energy–Matter Engine
## Unified Multiphysics Arch

**Request ID:** REQ-CHAT-20261126-016
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T23:27:02-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 18 — RESONANT Coupled Energy–Matter Engine
## Unified Multiphysics Architecture, Cross-Domain Energy Transfer, Dynamic Material State, Conservation Laws, and Solver Coupling

You are working on **RESONANT — Wave Field Engineering Studio**.

This is a major architectural milestone.

The objective is to design and integrate the foundational architecture for the **Coupled Energy–Matter Engine (CEME)**.

RESONANT is evolving beyond independent wave and field simulators into a scientific engineering platform for understanding and designing the interactions between geometry, matter, energy, momentum, forces, and physical transformation.

The Coupled Energy–Matter Engine must become the architectural foundation that allows independently implemented physics domains to interact within a unified simulation.

This is not an instruction to replace existing physics solvers with one universal solver.

It is an instruction to create a scientifically rigorous framework through which specialized solvers can exchange information, interact, and evolve a shared physical system.

The central engineering principle is:

**Energy interacts with matter through geometry, material properties, boundaries, and physical fields. Those interactions change the state of matter, which can in turn change subsequent energy propagation and transfer.**

The engine must eventually support this entire feedback process.

---

# 1. Begin with a comprehensive architecture audit

Before implementation, inspect the existing RESONANT repository and documentation.

Review:

- Foundational application architecture.
- Scene and geometry architecture.
- Material system.
- Acoustic physics engine.
- Mechanical physics engine.
- Electromagnetic physics engine.
- Thermal capabilities, if present.
- Simulation framework.
- Solver interfaces.
- Simulation job management.
- Optimization framework.
- Python API.
- Protobuf/gRPC automation framework.
- Visualization architecture.
- Project storage.
- Experiment and measurement architecture.
- Existing physical quantity and unit conventions.

Read the existing Shape–Function–Energy documentation and the architectural work from Prompt 17 if already completed.

Do not assume documentation perfectly represents the current implementation. Verify the actual source code.

Create:

`docs/architecture/COUPLED_ENGINE_AUDIT.md`

Document:

1. Existing physical domains.
2. Existing solver capabilities.
3. Existing material abstractions.
4. Existing geometry and mesh representations.
5. Existing simulation data structures.
6. Existing unit conventions.
7. Existing job and scheduling infrastructure.
8. Existing optimization integration.
9. Existing serialization and project storage.
10. Existing automation interfaces.
11. Architectural gaps.
12. Integration risks.
13. Recommended migration strategy.

Do not duplicate functionality that already exists.

---

# 2. Establish the Coupled Energy–Matter Engine as a central subsystem

Create a clearly defined application subsystem:

**Coupled Energy–Matter Engine (CEME)**

Its responsibility is to coordinate physical interactions across independent simulation domains.

The engine must support:

- Multiple simultaneous physical domains.
- Independent domain-specific solvers.
- Cross-domain energy transfer.
- Momentum transfer.
- Force generation.
- Material state changes.
- Geometry changes.
- Dynamic boundary conditions.
- Time-dependent coupling.
- Spatially varying material properties.
- Energy and momentum accounting.
- Reproducible coupled simulations.

The engine must not become a monolithic collection of physical equations.

Its responsibilities should be orchestration, coupling, synchronization, state management, transfer, conservation auditing, and result provenance.

Physical equations remain implemented by their appropriate domain-specific models.

Create a modular architecture with clear ownership boundaries.

Recommended conceptual components:

```text
Coupled Energy–Matter Engine
│
├── Physical System Model
│
├── Simulation Domain Registry
│   ├── Electromagnetics
│   ├── Acoustics
│   ├── Mechanics
│   ├── Thermodynamics
│   ├── Fluid Dynamics
│   └── Future Domains
│
├── Material State Manager
│
├── Geometry State Manager
│
├── Physical Quantity Registry
│
├── Coupling Manager
│
├── Energy Transfer Manager
│
├── Momentum and Force Manager
│
├── Conservation Auditor
│
├── Time Coordination Manager
│
├── Spatial Mapping and Projection
│
├── Solver Orchestrator
│
├── Simulation State History
│
└── Provenance and Validation
```

These are conceptual responsibilities, not mandatory classes.

Use the existing architecture where possible.

---

# 3. Define the Physical System Model

Introduce a physical system representation that describes an engineered object or collection of interacting objects.

A physical system should contain:

- One or more geometries.
- Material regions.
- Interfaces and boundaries.
- Simulation domains.
- Excitation sources.
- Environmental conditions.
- Sensors and observation points.
- Domain-specific solver configurations.
- Coupling relationships.
- Initial conditions.
- Constraints.
- Simulation time configuration.

The system must represent relationships between objects, not just a flat collection of scene entities.

Examples:

- An electromagnetic source interacting with a dielectric cavity.
- An acoustic transducer coupled to a fluid and surrounding mechanical structure.
- A mechanical resonator with a temperature-dependent material.
- An electromagnetic resonator coupled to a thermally expanding structure.
- A multilayer metamaterial interacting with acoustic and electromagnetic waves.
- A fluid interacting with an elastic membrane.

The representation must be serializable into the RESONANT project format.

Every object must have a stable identity.

Physical relationships must survive save, load, rename, and project migration.

---

# 4. Establish a unified physical quantity system

This is a critical foundation.

RESONANT must be able to exchange physical information between domains without confusing units, definitions, or physical meaning.

Create a typed physical quantity architecture.

Potential categories include:

### Energy-related quantities

- Total energy.
- Kinetic energy.
- Potential energy.
- Internal energy.
- Thermal energy.
- Electromagnetic energy.
- Acoustic energy.
- Elastic strain energy.
- Energy density.
- Energy flux.
- Power.
- Power density.
- Dissipated energy.

### Momentum-related quantities

- Linear momentum.
- Angular momentum.
- Momentum density.
- Momentum flux.
- Mechanical force.
- Torque.
- Pressure.
- Stress.
- Radiation pressure where appropriate.

### Field-related quantities

- Electric field.
- Magnetic field.
- Electric displacement.
- Magnetic flux density.
- Acoustic pressure.
- Particle velocity.
- Fluid velocity.
- Mechanical displacement.
- Mechanical velocity.
- Temperature.
- Charge density.
- Current density.

### Material-related quantities

- Mass density.
- Number density.
- Charge density.
- Heat capacity.
- Thermal conductivity.
- Elastic moduli.
- Electrical conductivity.
- Permittivity.
- Permeability.
- Acoustic impedance.
- Damping.
- Phase.
- Composition.
- Damage state.

Do not represent all quantities as generic untyped floating-point arrays.

Use typed metadata that identifies:

- Physical quantity.
- SI unit.
- Spatial location.
- Coordinate frame.
- Time or frequency domain.
- Scalar, vector, tensor, or complex representation.
- Peak, RMS, instantaneous, averaged, or integrated meaning.
- Reference values where relevant.
- Validity range.
- Associated physical domain.
- Source solver.
- Material or geometry revision.

Avoid ambiguous field names.

Examples:

- `frequency_hz`
- `phase_radians`
- `position_meters`
- `temperature_kelvin`
- `pressure_pascal`
- `energy_density_joules_per_cubic_meter`
- `power_watts`

Use SI units internally.

Conversions should occur at explicit input and presentation boundaries.

A quantity must not be transferred between solvers unless its physical definition and interpretation are compatible.

---

# 5. Dynamic Material State Manager

Introduce a central material state management system.

A material must no longer be assumed to have one immutable collection of properties.

Its response can depend on the conditions it experiences.

The state may vary with:

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

The material state manager must distinguish:

1. Intrinsic material definitions.
2. Initial material state.
3. Current material state.
4. Evolving material state.
5. Effective or homogenized material properties.
6. Derived properties calculated by constitutive models.

Examples:

- Electrical conductivity changes with temperature.
- A material expands under heating.
- Elastic properties change with temperature.
- Magnetic response changes with field strength.
- A viscoelastic material responds differently at different frequencies.
- A phase transition changes density and other constitutive properties.
- Material damage changes stiffness and wave propagation.

Do not create one universal material model.

Create a common state and property interface that can host domain-specific constitutive laws.

Support spatially varying material properties.

Support material regions and interfaces within one physical object.

Define how material properties are interpolated and evaluated at solver locations.

Ensure changes to material state can invalidate and update dependent solver data correctly.

---

# 6. Geometry and physical state coupling

Geometry is not necessarily static during a simulation.

The engine must eventually support geometry changes caused by physical interactions.

Examples:

- Thermal expansion.
- Mechanical deformation.
- Fluid-structure interaction.
- Pressure-induced deformation.
- Elastic vibration.
- Plastic deformation.
- Phase-dependent volume changes.
- Structural damage.

Define a geometry state architecture that distinguishes:

- Reference geometry.
- Current geometry.
- Deformed geometry.
- Simulation mesh.
- Material-region geometry.
- Boundary geometry.

The engine must support updating solver geometry when physical state changes.

Plan for:

- Mesh deformation.
- Remeshing.
- Moving boundaries.
- Mesh-to-mesh transfer.
- Geometry validation.
- Contact and interface updates.
- Conservative transfer of relevant fields where possible.

Do not require every solver to use the same mesh.

Do not rebuild geometry unnecessarily when a field update does not require it.

Geometry updates must be versioned so dependent simulation data can detect stale geometry.

---

# 7. Coupling architecture

This is the central technical challenge.

Create a flexible coupling framework supporting several strategies.

## A. One-way coupling

One domain influences another, but feedback is not returned.

Example:

Electromagnetic absorption produces a thermal source term.

The thermal solver calculates temperature, but the electromagnetic solver does not update its material properties during that run.

## B. Two-way coupling

Both domains influence each other.

Example:

Electromagnetic heating changes temperature, temperature changes dielectric properties, and those changed properties alter electromagnetic field propagation.

## C. Sequential coupling

Solvers execute in a defined order and exchange results between stages.

Example:

1. Electromagnetic simulation.
2. Calculate absorbed power.
3. Thermal simulation.
4. Update material state.
5. Mechanical simulation.
6. Update geometry.
7. Recalculate electromagnetic response.

## D. Iterative coupling

Solvers exchange information repeatedly until a defined convergence criterion is satisfied.

## E. Fully time-dependent coupling

Domains advance together using compatible time coordination and data exchange.

Support the ability to use different numerical time steps.

A thermal solver may operate on a much slower timescale than an electromagnetic or acoustic solver.

Do not force all solvers to use the same time step.

Define interfaces for:

- Coupling inputs.
- Coupling outputs.
- Transfer operators.
- Update frequency.
- Synchronization.
- Convergence.
- Relaxation.
- Stability monitoring.
- Failure handling.
- Cancellation.

The coupling architecture must be extensible to future physical domains without requiring a redesign of the engine.

---

# 8. Define initial cross-domain coupling models

Create an architecture and implementation plan for the following interactions.

These are the initial reference cases for the engine.

## Electromagnetic ↔ Thermal

- Electromagnetic energy absorption.
- Joule heating.
- Dielectric losses.
- Magnetic losses where applicable.
- Spatial heat source generation.
- Temperature-dependent permittivity.
- Temperature-dependent conductivity.
- Temperature-dependent permeability where appropriate.

## Thermal ↔ Mechanical

- Thermal expansion.
- Thermal stress.
- Temperature-dependent elasticity.
- Temperature-dependent damping.
- Thermomechanical deformation.
- Thermal boundary conditions.

## Acoustic ↔ Mechanical

- Acoustic pressure acting on structures.
- Structural vibration generating acoustic waves.
- Acoustic-structure coupling.
- Fluid-structure interaction.
- Acoustic energy absorption.
- Boundary impedance.

## Electromagnetic ↔ Mechanical

- Lorentz forces.
- Maxwell stress.
- Electromagnetic pressure.
- Electromechanical coupling.
- Magnetostrictive effects where supported.
- Piezoelectric coupling where supported.

## Acoustic ↔ Thermal

- Absorption and dissipative heating.
- Thermoviscous effects.
- Temperature-dependent acoustic properties.
- Thermal-acoustic feedback.

## Fluid ↔ Mechanical

- Fluid pressure and viscous loading.
- Moving boundaries.
- Elastic structures interacting with fluid flow.
- Structural response changing the flow field.

For each coupling, document:

- Governing equations.
- Required physical inputs.
- Produced outputs.
- Material dependencies.
- Numerical requirements.
- Conservation considerations.
- Appropriate solvers.
- Validation benchmarks.
- Known limitations.

Do not claim a coupling is implemented until the underlying equations and transfer mechanism are actually implemented and validated.

---

# 9. Energy and momentum accounting

Introduce a dedicated conservation auditor.

This is a foundational scientific requirement.

Every coupled simulation should be able to account for energy entering, leaving, being stored, converted, and dissipated.

Define an energy accounting framework that can distinguish:

- External energy input.
- Energy entering through boundaries.
- Energy leaving through boundaries.
- Stored field energy.
- Kinetic energy.
- Elastic energy.
- Internal and thermal energy.
- Converted energy.
- Dissipated energy.
- Numerical energy imbalance.

The accounting framework must respect the relevant physics.

Do not blindly add quantities that have different definitions or overlapping contributions.

For example, an electromagnetic field solver may report stored field energy and power flow, while a thermal solver reports internal energy and heat flux. Coupling must not double-count energy transferred between them.

Introduce explicit energy transfer records.

Each transfer should identify:

- Source domain.
- Destination domain.
- Physical mechanism.
- Quantity and units.
- Spatial region.
- Time interval.
- Transfer amount.
- Associated solver.
- Numerical approximation.
- Uncertainty or known limitation.

Establish a conservation residual for each applicable conservation law.

Where appropriate, calculate:

- Energy balance error.
- Momentum balance error.
- Mass balance error.
- Charge conservation error.

Do not assume all physical systems conserve every quantity under arbitrary boundary conditions.

External work, open boundaries, sources, sinks, and dissipative mechanisms must be represented explicitly.

The application must be able to identify whether a discrepancy originates from:

- Physical dissipation.
- External work.
- Boundary transfer.
- Solver error.
- Interpolation error.
- Coupling error.
- Incomplete accounting.

Create:

`docs/physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md`

---

# 10. Time coordination and numerical stability

Coupled simulation introduces numerical risks.

Design a time coordination manager.

Support:

- Independent domain time steps.
- Common synchronization points.
- Subcycling.
- Adaptive time stepping where supported.
- Event-driven updates.
- Iterative coupling.
- Convergence detection.
- Relaxation.
- Stability checks.
- Checkpointing.

Define how a coupled simulation responds when one solver:

- Fails.
- Diverges.
- Cannot converge.
- Runs out of memory.
- Produces invalid values.
- Requires a smaller time step.
- Is cancelled.

The engine must not silently accept invalid solver results.

Preserve partial results when safe and meaningful.

Every simulation must record its time coordination configuration.

Do not impose an unnecessarily sophisticated time integrator on every solver.

Choose methods appropriate to each domain.

---

# 11. Spatial mapping and data transfer

Different solvers may use:

- Different meshes.
- Different coordinate systems.
- Different element types.
- Different spatial resolutions.
- Different field representations.
- Different time grids.

Create a spatial mapping and projection architecture.

Support:

- Mesh-to-mesh mapping.
- Interpolation.
- Conservative transfer.
- Boundary-to-volume mapping.
- Surface field transfer.
- Volume source transfer.
- Coordinate transformations.
- Unit conversion.
- Temporal interpolation.
- Spatial averaging.
- Region-based aggregation.

Distinguish between interpolation and physically conservative transfer.

Where conservation is required, provide appropriate transfer methods and tests.

Support nonmatching meshes.

Do not force all physical domains to share one universal mesh.

---

# 12. Solver orchestration and simulation execution

The Coupled Energy–Matter Engine must integrate with the existing simulation job framework.

A coupled simulation should be represented as an execution graph.

For example:

```text
Initial Material State
        |
        v
Electromagnetic Solver
        |
        v
Absorbed Power Distribution
        |
        v
Thermal Solver
        |
        v
Updated Temperature
        |
        v
Material State Update
        |
        v
Mechanical Solver
        |
        v
Updated Geometry
        |
        v
Electromagnetic Solver
        |
        v
Convergence Evaluation
        |
        +------> Repeat if required
        |
        v
Final Results and Conservation Audit
```

The actual execution graph must be constructed from the selected physical models and coupling configuration.

Support:

- Dependency graphs.
- Solver execution ordering.
- Parallel execution where physically valid.
- Intermediate result storage.
- Checkpoints.
- Cancellation.
- Progress reporting.
- Resource estimates.
- Failure propagation.
- Reproducibility.

Long-running coupled simulations must execute asynchronously.

Do not hold GUI or gRPC requests open for the duration of long simulations.

Use the existing unified job system.

Expose coupled simulation operations through the existing automation services and Protobuf contracts.

Do not create a separate automation interface.

---

# 13. Physical system configuration and user workflow

Design a future user experience for configuring coupled simulations.

The user should be able to:

1. Create or open a physical system.
2. Add or import geometry.
3. Assign materials.
4. Select physical domains.
5. Add excitation sources.
6. Select relevant solvers.
7. Define domain coupling.
8. Configure simulation conditions.
9. Inspect resource estimates.
10. Validate the model.
11. Run the simulation.
12. Inspect individual domain results.
13. Inspect cross-domain interactions.
14. Inspect energy and momentum accounting.
15. Compare results with uncoupled simulations.

The interface should make coupling relationships visible.

Plan a graphical representation of the simulation dependency graph.

The user should be able to inspect which domains exchange information and why.

Do not implement a generic visual programming environment as a substitute for the physical system model.

The graph must represent actual solver and physical dependencies.

---

# 14. Visualization of coupled physical behavior

The existing 3D viewport must support synchronized inspection of multiple physical quantities.

Plan for:

- Electromagnetic field overlays.
- Acoustic pressure overlays.
- Mechanical deformation.
- Stress and strain.
- Temperature distributions.
- Energy density.
- Energy flux.
- Momentum-related quantities where defined.
- Material phase.
- Changing material properties.
- Moving boundaries.
- Time-dependent geometry.
- Energy transfer paths.

The user should be able to select a point or region and inspect its physical state across domains.

For example:

At one location, display:

- Electromagnetic field strength.
- Absorbed power density.
- Temperature.
- Mechanical stress.
- Displacement.
- Local material properties.

All values must correspond to the same simulation time or clearly indicate their time offsets.

Visualization must remain separate from numerical computation.

Changing the visualization must not force a new simulation.

---

# 15. Result provenance and reproducibility

A coupled simulation has many interacting dependencies.

Record:

- Project version.
- Geometry revision.
- Material definitions.
- Material state.
- Domain configurations.
- Solver names and versions.
- Solver numerical settings.
- Mesh versions.
- Coupling definitions.
- Transfer operators.
- Time coordination.
- Boundary conditions.
- Initial conditions.
- External sources.
- Convergence settings.
- Conservation audit results.
- Software build identifier.
- Relevant dependencies.

The result must be reproducible to the extent permitted by the numerical methods and execution environment.

Support saving intermediate states and resuming where technically safe.

Do not allow results to appear valid after their geometry, material state, or solver dependencies have changed without clearly identifying them as stale.

---

# 16. Scientific validation

Create a dedicated validation plan.

Begin with simple analytical and established benchmark cases.

Required reference cases include:

### Electromagnetic to thermal

A simple dielectric or conductive object receiving electromagnetic energy.

Verify absorbed power, heating distribution, and total energy accounting against an appropriate analytical or independently validated reference.

### Thermal to mechanical

A homogeneous object undergoing uniform thermal expansion.

Verify deformation and thermal stress under clearly defined boundary conditions.

### Acoustic to mechanical

A simple elastic structure subjected to acoustic pressure.

Compare the response with an appropriate analytical or established reference solution.

### Mechanical to acoustic

A vibrating surface radiating sound.

Compare predicted acoustic output with a known analytical case.

### Coupled iterative feedback

A simplified temperature-dependent material whose electromagnetic response changes as it heats.

Verify convergence and demonstrate that the feedback is physically consistent.

For every test:

- Define assumptions.
- Define governing equations.
- Define expected behavior.
- Define numerical tolerances.
- Compare against reference results.
- Record error.
- Explain deviations.

Do not validate a coupled engine merely by checking that its components execute.

Validate the physical result.

---

# 17. Testing architecture

Create tests for:

- Physical quantity typing.
- Unit consistency.
- Coordinate transformations.
- Material state updates.
- Geometry state versioning.
- Solver registration.
- Coupling registration.
- One-way coupling.
- Two-way coupling.
- Sequential execution.
- Iterative convergence.
- Time synchronization.
- Spatial transfer.
- Conservative mapping.
- Energy accounting.
- Momentum accounting.
- Failure propagation.
- Cancellation.
- Checkpointing.
- Project serialization.
- Automation API.
- Result provenance.

Include regression tests for existing independent acoustic, mechanical, and electromagnetic simulations.

A new coupling architecture must not break existing standalone simulations.

Use synthetic and analytical test cases where appropriate.

---

# 18. Automation and extensibility

The engine must integrate with the existing RESONANT automation architecture.

Expose meaningful operations through the existing Protobuf/gRPC contracts.

Examples:

- Create physical system.
- Add simulation domain.
- Assign material.
- Configure coupling.
- Inspect coupling graph.
- Validate physical system.
- Estimate simulation resources.
- Start coupled simulation.
- Query simulation state.
- Inspect transfer records.
- Inspect conservation results.
- Cancel simulation.
- Retrieve results.
- Save simulation configuration.

Follow the existing contract versioning, capability discovery, job management, events, and error-handling conventions.

The engine must also be accessible through the existing Python API.

Do not create parallel business logic for GUI, Python, or remote automation.

All clients must use the same application services.

---

# 19. Implementation strategy

Do not attempt to implement every multiphysics interaction at once.

Use this phased strategy.

## Phase A — Architecture and interfaces

- Audit existing systems.
- Define engine responsibilities.
- Define physical quantity types.
- Define material state interfaces.
- Define coupling contracts.
- Define geometry state interfaces.
- Define conservation interfaces.
- Document the architecture.

## Phase B — Core engine infrastructure

Implement:

- Physical system representation.
- Domain registry.
- Coupling registry.
- Material state management.
- Geometry state versioning.
- Solver orchestration.
- Result provenance.

## Phase C — Energy and momentum accounting

Implement:

- Transfer records.
- Energy ledgers.
- Conservation residuals.
- Accounting visualization and reporting.
- Analytical tests.

## Phase D — First working coupling

Implement one complete, validated coupling path:

Electromagnetic → Thermal.

Start with a deliberately simple physical case.

Demonstrate that absorbed energy produces a thermal source and that the resulting temperature distribution is physically reasonable.

## Phase E — Two-way coupling

Extend the first example to include temperature-dependent electromagnetic material properties.

Demonstrate iterative feedback and convergence.

## Phase F — Thermomechanical coupling

Connect thermal state to mechanical response.

Demonstrate thermal expansion and stress.

## Phase G — Additional domain interactions

Introduce acoustic-mechanical, electromagnetic-mechanical, and other validated couplings.

## Phase H — User workflow and automation

Expose coupled simulation configuration and execution through the GUI, Python, and automation services.

## Phase I — Advanced coupling

Introduce nonmatching meshes, multirate simulation, moving geometry, nonlinear coupling, and additional domains as justified.

---

# 20. Documentation deliverables

Create or update:

1. `docs/architecture/COUPLED_ENGINE_ARCHITECTURE.md`
2. `docs/architecture/PHYSICAL_SYSTEM_MODEL.md`
3. `docs/physics/PHYSICAL_QUANTITY_SYSTEM.md`
4. `docs/materials/MATERIAL_STATE_ARCHITECTURE.md`
5. `docs/physics/COUPLING_ARCHITECTURE.md`
6. `docs/physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md`
7. `docs/physics/SPATIAL_AND_TEMPORAL_TRANSFER.md`
8. `docs/physics/COUPLED_SIMULATION_EXECUTION.md`
9. `docs/physics/COUPLED_SIMULATION_VALIDATION.md`
10. `docs/roadmap/COUPLED_ENGINE_ROADMAP.md`

Follow existing documentation conventions if the repository has already established equivalent locations.

Update the primary architecture and project vision to reference the Coupled Energy–Matter Engine.

Provide diagrams for:

- Engine architecture.
- Domain coupling.
- Material state evolution.
- Energy transfer.
- Geometry feedback.
- Solver execution.
- Conservation accounting.

Distinguish implemented, planned, experimental, and future functionality.

---

# 21. Engineering constraints

- C++20.
- Existing Qt 6 application architecture.
- Existing CMake build system.
- Existing simulation job infrastructure.
- Existing Protobuf/gRPC automation architecture.
- Existing Python API.
- Existing project serialization.
- SI units internally.
- Domain-specific physics and numerical formulations.
- Physics independent of rendering.
- Modular solver interfaces.
- Optional dependencies where appropriate.
- Reproducible simulation provenance.
- Scientific validation required.

Do not:

- Replace every solver with a universal solver.
- Force all domains to use one mesh.
- Force all domains to use one time step.
- Treat all physical quantities as interchangeable floating-point arrays.
- Double-count energy during transfer.
- Assume all materials are linear or static.
- Assume all geometry is fixed.
- Claim conservation without calculating and checking it.
- Implement arbitrary physical behavior to make a demonstration look convincing.
- Introduce a separate automation or project system.
- Rewrite stable existing components without evidence that they must change.

---

# 22. Final acceptance criteria

| ID | Acceptance criterion |
|---|---|
| CEME-001 | Existing architecture has been audited |
| CEME-002 | Central Coupled Energy–Matter Engine architecture is documented |
| CEME-003 | Physical system representation is defined |
| CEME-004 | Typed physical quantity architecture is defined |
| CEME-005 | Material state architecture is defined |
| CEME-006 | Geometry state architecture is defined |
| CEME-007 | Domain registration and coupling contracts are defined |
| CEME-008 | One-way coupling is supported |
| CEME-009 | Two-way coupling architecture is supported |
| CEME-010 | Sequential and iterative coupling are supported |
| CEME-011 | Independent domain time steps are addressed |
| CEME-012 | Spatial mapping architecture is defined |
| CEME-013 | Energy transfer accounting is implemented or explicitly staged |
| CEME-014 | Conservation auditing is implemented or explicitly staged |
| CEME-015 | Solver orchestration integrates with existing job infrastructure |
| CEME-016 | Project serialization and provenance are addressed |
| CEME-017 | Existing standalone solvers remain functional |
| CEME-018 | First validated electromagnetic-to-thermal coupling is implemented |
| CEME-019 | Temperature-dependent feedback is implemented and validated |
| CEME-020 | Automation and Python integration are addressed |
| CEME-021 | Required documentation is complete |
| CEME-022 | Analytical and regression tests pass |

Mark each criterion PASS, FAIL, BLOCKED, or NOT APPLICABLE.

Do not mark architectural designs as implemented functionality.

Do not claim physical coupling is working until the actual equations, data exchange, and validation tests exist.

---

# 23. Final report

Provide:

1. Architecture audit findings.
2. New engine architecture.
3. New interfaces and data models.
4. Existing components reused.
5. Components modified.
6. Components added.
7. Couplings actually implemented.
8. Couplings only designed or planned.
9. Energy and momentum accounting results.
10. Scientific validation results.
11. Test results.
12. Performance and resource implications.
13. Known limitations.
14. Recommended next development milestone.

The ultimate objective is to give RESONANT a common scientific engine through which geometry, materials, energy fields, momentum, forces, and evolving physical states can interact.

**RESONANT must ultimately be able to model not only what happens when energy encounters matter, but how that interaction changes matter—and how the changed matter subsequently transforms the energy acting upon it.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: - (heuristic)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence low)
  - validation: PASS
  - classifier_reason: no framing in the request statement; body suggests feature: Add, Create, Design, Evolving
  - evidence: 'milestone' [body]; 'objective' [body]; 'integrate' [body]; 'evolving' [body]; 'must become' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: User: 'add this as a ticket. process it in order' (queued after REQ-CHAT-20261126-015)

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

User: 'add this as a ticket. process it in order' (queued after REQ-CHAT-20261126-015)

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000017
- Evidence: pending
