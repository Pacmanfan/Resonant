# Request: # PROMPT 23 — DYNAMIC MATERIAL STATE, NONLINEAR MATTER AND EVOLVING PHYSICAL SYS

**Request ID:** REQ-CHAT-20261126-021
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T23:47:10-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 23 — DYNAMIC MATERIAL STATE, NONLINEAR MATTER AND EVOLVING PHYSICAL SYSTEMS

## Project: RESONANT — Wave Field Engineering Studio

## Mission

Implement the next major scientific capability in RESONANT: **Dynamic Material State and Nonlinear Matter**.

RESONANT must move beyond treating materials as static objects with fixed physical properties.

In real physical systems, matter changes in response to energy, temperature, electromagnetic fields, mechanical loading, pressure, chemical interactions, deformation, and time.

Those changes can fundamentally alter how the system behaves.

A material can:
- Heat up and change its conductivity.
- Expand and alter its geometry.
- Deform and change its mechanical stiffness.
- Undergo a phase transition.
- Accumulate damage.
- Develop cracks.
- Change its magnetic properties.
- Exhibit hysteresis.
- Absorb energy and change its internal state.
- Develop nonlinear responses to increasing excitation.
- Change its resonance characteristics as its physical state evolves.

These effects are not independent.

A change in material state can alter geometry, energy absorption, wave propagation, resonance, mechanical response, and subsequent energy transfer.

RESONANT must gain the ability to represent these evolving relationships as part of its central physical simulation architecture.

The governing principle is:

**Energy and Matter Interaction → Material State Evolution → Changed Physical Properties → Changed System Response → Further Energy and Matter Interaction**

This creates a continuously evolving physical feedback system.

The objective is not to implement every possible nonlinear material model immediately. It is to establish a rigorous architecture for dynamic material state, implement meaningful validated models, and integrate them with the existing CEME, geometry, resonance, metamaterial, and multiscale systems.

---

# 1. Governing Architecture

Establish a first-class concept of **Dynamic Material State**.

A material definition describes what a material is.

A material state describes what that material is currently experiencing and how its physical characteristics have evolved.

These must be separate concepts.

For example, two objects may use the same base material definition but have different physical states because one has been heated, strained, magnetized, damaged, or otherwise altered.

The material state must be capable of influencing the constitutive properties used by the simulation.

Establish the following relationship:

**Material Definition + Current State + Physical History + Environmental Conditions → Constitutive Response**

And:

**Constitutive Response + Applied Physical Interactions → Updated Material State**

This must operate within the existing Coupled Energy–Matter Engine.

Do not create a separate dynamic-material simulation engine disconnected from CEME.

---

# 2. Audit Existing Architecture Before Implementation

Perform a comprehensive audit of:

- Material definitions and constitutive models.
- Prompt 22 multiscale material representations and lineage.
- CEME physical state management.
- Geometry and deformation capabilities.
- Mechanical and structural solvers.
- Thermal solvers.
- Electromagnetic and optical solvers.
- Acoustic and fluid solvers.
- Resonance and modal analysis.
- Metamaterial and effective-property systems.
- Time-domain simulation capabilities.
- Numerical integration methods.
- Mesh and field update mechanisms.
- Energy accounting and conservation infrastructure.
- Project serialization and versioning.
- Automation architecture.
- GUI material and simulation workflows.
- Existing validation and regression tests.

Determine which physical state variables are already supported.

Determine whether existing solvers support transient simulation, nonlinear constitutive behavior, or state-dependent material properties.

Do not infer capabilities from class names or interface declarations alone. Verify actual execution paths.

Create an architecture map showing how dynamic material state will be introduced into existing systems.

Reuse existing infrastructure wherever possible.

Do not create duplicate material registries, competing time-management systems, or parallel physical simulation engines.

---

# 3. Dynamic Material State Model

Implement a common state architecture capable of representing the evolving physical condition of a material.

A material state should support relevant combinations of:

### Thermal State
- Temperature.
- Temperature gradients.
- Thermal history.
- Thermal expansion.
- Thermal strain.
- Thermal gradients through the material.

### Mechanical State
- Stress.
- Strain.
- Strain rate.
- Plastic strain.
- Elastic strain.
- Deformation.
- Residual stress.
- Internal mechanical energy.

### Structural State
- Damage.
- Crack or fracture indicators.
- Porosity.
- Defect concentration.
- Grain or phase distribution where modeled.
- Microstructural evolution variables.

### Electromagnetic State
- Electric polarization.
- Magnetization.
- Magnetic domain state where supported.
- Charge-related state variables.
- Field-dependent material response.

### Chemical and Compositional State
- Composition.
- Concentration fields.
- Reaction progress.
- Diffusion state.
- Phase fractions.
- Other supported chemical state variables.

### History State
- Previous loading conditions.
- Previous temperatures.
- Maximum historical stress or strain where relevant.
- Accumulated fatigue.
- Hysteresis variables.
- Irreversible deformation.
- Accumulated damage.
- Other constitutive history variables.

Do not require every material to carry every state variable.

State variables must be defined by the physical model that uses them.

## 3.1 Material State Identity

Every material state must be associated with:

- Its material definition.
- Its physical representation.
- Its spatial location or simulation region.
- Its current simulation time.
- Its previous state.
- Its relevant history variables.
- Its constitutive model.
- Its revision and provenance.

Where materials are spatially distributed, state must be capable of varying across the geometry or simulation mesh.

A material must not be forced to have one uniform temperature, stress, damage value, or phase fraction when the physics requires spatial variation.

## 3.2 Reversible and Irreversible State

Distinguish between reversible and irreversible changes.

Examples:

Reversible:
- Elastic deformation.
- Thermal expansion.
- Reversible polarization.
- Reversible magnetic response within the applicable model.

Irreversible:
- Plastic deformation.
- Fatigue accumulation.
- Permanent damage.
- Fracture.
- Plastic hysteresis.
- Irreversible phase transformation where applicable.

This distinction is essential for physically meaningful simulation.

---

# 4. Nonlinear Constitutive Modeling

Introduce a common architecture for nonlinear constitutive models.

A constitutive model describes how a material responds to physical loading and its current internal state.

It must be possible to implement different constitutive models without changing the entire CEME architecture.

Establish a consistent interface for:

- Current state.
- Applied physical conditions.
- Time increment.
- Constitutive response.
- Updated state.
- Internal variables.
- Tangent or linearized response where required.
- Convergence information.
- Energy contribution.
- Model validity.

The exact interface must respect the numerical requirements of the existing solvers.

Do not force electromagnetic, mechanical, thermal, acoustic, and fluid constitutive models into one inappropriate universal equation.

They may share software architecture while retaining their own physical equations.

## 4.1 Linear and Nonlinear Behavior

Support the distinction between:

- Linear constitutive response.
- Nonlinear reversible response.
- History-dependent response.
- Rate-dependent response.
- Hysteretic response.
- Irreversible response.
- Coupled multiphysics response.

A material may behave linearly within one operating region and nonlinearly outside it.

The system should allow models to specify their applicable range.

---

# 5. Mechanical Material Evolution

Implement the architecture for evolving mechanical material behavior.

This should become one of the primary applications of dynamic material state.

## 5.1 Elasticity

Support state-dependent elastic properties where appropriate.

Examples:

- Temperature-dependent Young's modulus.
- Temperature-dependent stiffness tensors.
- Anisotropic elasticity.
- Nonlinear elastic response.
- Hyperelastic behavior where supported.

Preserve the distinction between elastic deformation and permanent deformation.

## 5.2 Plasticity

Establish support for plastic material behavior.

Relevant concepts include:

- Yield criteria.
- Yield surfaces.
- Plastic flow.
- Hardening.
- Softening.
- Plastic strain.
- Residual stress.
- Loading and unloading behavior.

Potential models include:

- von Mises plasticity.
- Tresca plasticity.
- Isotropic hardening.
- Kinematic hardening.
- Combined hardening.

Implement models that are compatible with the existing mechanical solver.

Do not add model names without working constitutive calculations.

## 5.3 Viscoelasticity and Visco-Plasticity

Support time-dependent mechanical response where practical.

Relevant concepts include:

- Creep.
- Stress relaxation.
- Rate-dependent deformation.
- Delayed elastic recovery.
- Temperature-dependent viscosity.
- Viscoelastic hysteresis.

Potential reference models:

- Maxwell.
- Kelvin–Voigt.
- Standard linear solid.
- Generalized Maxwell models.

The implementation must distinguish instantaneous elastic response from time-dependent behavior.

## 5.4 Hyperelasticity

Where supported, investigate nonlinear elastic models for materials that can undergo large reversible deformation.

Examples:

- Rubber-like materials.
- Elastomers.
- Soft materials.

Potential constitutive models include Neo-Hookean and Mooney–Rivlin.

Do not apply small-strain assumptions to large-deformation problems.

---

# 6. Damage, Fracture and Fatigue

Introduce damage as an evolving material state.

Damage must influence the physical properties and response of the material.

It must not be merely a visual effect.

## 6.1 Damage Models

Support appropriate damage variables representing degradation.

Examples:

- Stiffness degradation.
- Strength degradation.
- Loss of load-bearing capacity.
- Progressive material deterioration.
- Localized damage.
- Distributed damage.

Damage evolution must depend on an appropriate physical loading or history criterion.

## 6.2 Fracture

Establish interfaces for fracture modeling.

Potential approaches include:

- Cohesive-zone models.
- Phase-field fracture.
- Continuum damage mechanics.
- Discrete crack representations.

Do not implement every approach simultaneously.

Select an approach that is compatible with the existing geometry, mesh, and numerical infrastructure.

A fracture model must account for the appropriate fracture energy or other physically meaningful dissipation mechanism.

## 6.3 Fatigue

Establish an architecture for cyclic damage accumulation.

Relevant concepts include:

- Stress amplitude.
- Strain amplitude.
- Mean stress.
- Load cycles.
- Fatigue damage.
- Crack initiation.
- Crack propagation.
- Variable-amplitude loading.

Potential models include appropriately qualified S–N and strain-life relationships, Miner-type accumulation, and more advanced approaches where supported.

Clearly distinguish empirical fatigue models from physically resolved fracture simulations.

## 6.4 Damage Feedback

Damage must be capable of changing:

- Elastic stiffness.
- Strength.
- Thermal transport.
- Electrical properties where appropriate.
- Acoustic response.
- Resonance characteristics.
- Energy dissipation.

Only implement property changes that are supported by the selected material model.

---

# 7. Phase Transitions and Evolving Microstructure

Introduce material phase evolution.

Materials may change their internal phase composition in response to temperature, pressure, fields, composition, and other physical conditions.

Examples:

- Solid–liquid transitions.
- Solid–solid phase transitions.
- Crystallization.
- Melting.
- Solidification.
- Glass transitions.
- Magnetic phase transitions.
- Structural phase transformations.
- Transformation-induced deformation.

## 7.1 Phase Representation

Support materials with multiple phases.

A phase representation should be capable of recording:

- Phase identity.
- Phase fraction.
- Relevant thermodynamic properties.
- Spatial distribution.
- Transformation conditions.
- Transformation history.
- Associated material properties.

## 7.2 Phase Evolution

Support appropriate phase evolution models.

Potential approaches include:

- Equilibrium phase selection.
- Kinetic transformation models.
- Nucleation and growth models.
- Phase-field methods.
- Empirical transformation kinetics.

Do not confuse an equilibrium phase diagram with a time-dependent phase transformation simulation.

The system must distinguish between predicting which phase is thermodynamically favored and calculating how quickly or where the transformation occurs.

## 7.3 Latent Heat and Energy Transfer

Phase transformations can absorb or release energy.

Where supported, incorporate:

- Latent heat.
- Transformation enthalpy.
- Temperature-dependent heat capacity.
- Phase-dependent conductivity.
- Transformation-related strain.
- Other relevant energy terms.

The energy transferred during a phase transition must be accounted for within CEME.

---

# 8. Thermal Evolution and Thermo-Mechanical Feedback

Establish a complete connection between thermal and mechanical state evolution.

A material may heat, expand, deform, develop stress, change stiffness, and subsequently alter its energy absorption.

Support appropriate combinations of:

- Temperature-dependent material properties.
- Thermal expansion.
- Thermal stress.
- Thermal strain.
- Thermal gradients.
- Heat capacity changes.
- Temperature-dependent conductivity.
- Thermal softening.
- Thermal damage.
- Phase-change effects.

The system must support feedback:

**Energy Absorption → Temperature Increase → Material Property Change → Changed Physical Response → Changed Energy Absorption**

Example:

An electromagnetic structure absorbs energy.

The material heats up.

Its electrical conductivity and permittivity change.

The electromagnetic field distribution changes.

The energy absorption pattern changes again.

This is a coupled physical process, not a one-time correction to a static material property.

Implement a meaningful thermo-physical feedback workflow where the existing solvers permit it.

---

# 9. Nonlinear Electromagnetic, Optical and Magnetic Materials

Dynamic material behavior must not be limited to mechanical materials.

Establish interfaces for nonlinear electromagnetic and optical constitutive behavior.

Where supported, investigate:

## 9.1 Nonlinear Dielectrics

- Field-dependent permittivity.
- Nonlinear polarization.
- Dielectric saturation.
- Dielectric loss.
- Hysteresis where appropriate.

## 9.2 Nonlinear Optical Materials

- Intensity-dependent refractive index.
- Kerr-type nonlinear response.
- Harmonic generation.
- Nonlinear absorption.
- Saturable absorption where supported.
- Other domain-appropriate nonlinear optical effects.

## 9.3 Magnetic Materials

- Nonlinear magnetization.
- Hysteresis.
- Magnetic saturation.
- Remanence.
- Coercivity.
- Magnetic loss.
- Temperature-dependent magnetic response.

Potential models include appropriate B–H constitutive relationships and hysteresis models.

## 9.4 Frequency- and State-Dependent Response

The system must be capable of representing material response that depends on both frequency and current state.

For example, a material's electromagnetic properties may change with temperature, applied field, or internal phase.

Do not assume a constant scalar permittivity or permeability when a dispersive, tensorial, nonlinear, or history-dependent model is required.

---

# 10. Nonlinear Acoustic, Fluid and Other Physical Domains

Extend dynamic-state architecture to other domains where the existing solver infrastructure supports it.

Potential areas include:

### Acoustic Materials
- Amplitude-dependent response.
- Nonlinear acoustic propagation.
- Temperature-dependent acoustic properties.
- Viscoelastic acoustic losses.
- State-dependent absorption.

### Fluids
- Temperature-dependent viscosity.
- Non-Newtonian response.
- Pressure-dependent density.
- Phase changes.
- Flow-dependent constitutive behavior.

### Thermal Materials
- Temperature-dependent conductivity.
- Nonlinear heat capacity.
- State-dependent transport.
- Anisotropic thermal evolution.

These capabilities must remain domain-specific.

Do not claim nonlinear acoustic or fluid simulation if the current solver cannot execute it.

Prioritize the domains already supported by RESONANT.

---

# 11. Hysteresis and Memory Effects

Hysteresis deserves its own architectural treatment.

Some materials respond not only to their current conditions but also to the path by which they arrived there.

Examples:

- Magnetic hysteresis.
- Mechanical plastic hysteresis.
- Ferroelectric polarization.
- Viscoelastic response.
- Thermal hysteresis.
- Phase transformation hysteresis.

The system must be able to represent internal variables that preserve relevant physical history.

A material's response at a given temperature or field may differ depending on its previous state.

Do not implement hysteresis as a graphical loop disconnected from the constitutive calculation.

The hysteresis model must calculate the actual response and preserve the state necessary for subsequent time steps.

---

# 12. Material State Evolution Engine

Implement a common architecture responsible for coordinating material state updates.

The state evolution process should follow a controlled sequence:

1. Retrieve the current material definition.
2. Retrieve the current material state.
3. Obtain relevant physical fields and environmental conditions.
4. Identify applicable constitutive models.
5. Calculate constitutive response.
6. Evaluate state evolution.
7. Apply appropriate numerical updates.
8. Check convergence and model validity.
9. Calculate relevant energy contributions.
10. Commit the updated state.
11. Notify dependent physical systems.
12. Record the state transition and provenance.

State updates must be coordinated with the solver's numerical integration method.

Avoid updating a material state independently of the physical time step in a way that produces inconsistent field solutions.

## 12.1 State Update Contracts

Every evolving material model should define:

- Required input variables.
- Required history variables.
- Constitutive response.
- State evolution equations.
- Numerical update method.
- Validity conditions.
- Convergence criteria.
- Energy contributions.
- Failure behavior.

## 12.2 Spatial State

Where appropriate, material state must be stored spatially.

For a meshed object, different regions may have different:

- Temperatures.
- Strains.
- Damage values.
- Phase fractions.
- Magnetic states.
- Other internal variables.

Support state interpolation and mapping using appropriate numerical methods.

## 12.3 Time Integration

Use the existing simulation time infrastructure where possible.

Support appropriate methods for:

- Explicit integration.
- Implicit integration.
- Adaptive time stepping.
- Nonlinear iteration.
- State rollback.
- Checkpointing.

Do not assume one integration method is appropriate for every material model.

## 12.4 Numerical Stability

State evolution can introduce numerical instability.

Implement appropriate checks for:

- Excessive state changes.
- Nonphysical negative quantities.
- Invalid phase fractions.
- Constitutive divergence.
- Unstable time steps.
- Failed nonlinear iterations.
- Nonphysical energy generation.

Where possible, provide adaptive step reduction or controlled rollback.

Never silently clamp physically meaningful variables simply to make a calculation complete.

---

# 13. Energy Accounting and Conservation

Dynamic material evolution must integrate with the existing CEME energy framework.

Material state changes may involve:

- Stored elastic energy.
- Plastic dissipation.
- Viscous dissipation.
- Fracture energy.
- Thermal energy.
- Latent heat.
- Magnetic energy.
- Dielectric energy.
- Other domain-specific stored or dissipated energy.

The material update process must expose the relevant energy changes to CEME.

For every supported model, identify which terms represent:

- Reversible energy storage.
- Irreversible energy dissipation.
- Energy transferred to another physical domain.
- Energy supplied externally.
- Energy released during state evolution.

Do not double-count energy already accounted for by the underlying solver.

Do not use a universal material energy equation that replaces domain-specific physical laws.

Prompt 24 will establish a more comprehensive unified energy, momentum, force, and conservation framework.

Prompt 23 must provide the interfaces and accounting information that framework will require.

---

# 14. Feedback Into Geometry and Functional Structures

Dynamic material state must be capable of affecting physical geometry.

Examples include:

- Thermal expansion.
- Elastic deformation.
- Plastic deformation.
- Swelling.
- Shrinkage.
- Phase-transformation strain.
- Damage-induced geometry changes.
- Fracture.

The geometry system must distinguish between:

- Original reference geometry.
- Current deformed geometry.
- Updated physical geometry.
- Visual representation.

Where supported, updated geometry must be reflected in the simulation domain.

Changes in geometry can alter:

- Field distributions.
- Boundary conditions.
- Resonance.
- Energy confinement.
- Mechanical response.
- Wave propagation.
- Material interaction.

Integrate with Prompt 19's geometry architecture.

Do not implement geometry changes as visual-only transformations.

If the solver cannot update its mesh or physical domain dynamically, document that limitation and expose the appropriate interface for future work.

---

# 15. Feedback Into Resonance and Metamaterials

This is an especially important integration.

A material's evolving state can alter its resonance and effective material properties.

Examples:

- Temperature shifts resonant frequency.
- Mechanical deformation changes cavity dimensions.
- Damage changes mechanical stiffness and modal frequencies.
- Magnetic state changes permeability.
- Dielectric nonlinearity changes electromagnetic resonance.
- Phase transformation changes refractive index.
- Material loss changes Q factor.
- Microstructural changes alter effective metamaterial response.

Integrate with Prompts 20 and 21.

## 15.1 Dynamic Resonance

Support re-evaluation of resonance characteristics when material state changes.

Where applicable, update:

- Natural frequencies.
- Resonant frequencies.
- Mode shapes.
- Damping.
- Q factor.
- Coupling.
- Energy storage.
- Resonant response.

Do not automatically recompute expensive modal analyses after every insignificant state change.

Use appropriate dependency tracking and update criteria.

## 15.2 Dynamic Metamaterials

Effective material properties must be capable of changing as their constituents or microstructure evolve.

For example:

- Temperature-dependent unit-cell behavior.
- Deformation-dependent band structures.
- Damage-dependent effective stiffness.
- State-dependent electromagnetic response.

Preserve the relationship between the effective material and its underlying unit cell.

A dynamically changed constituent must invalidate or update dependent effective properties as appropriate.

---

# 16. Connection to Multiscale Material Modeling — Prompt 22

Dynamic state evolution must be compatible with the multiscale hierarchy established in Prompt 22.

Material changes may occur at different scales.

Examples:

- Atomic defects form or migrate.
- Molecular configurations change.
- Grain boundaries evolve.
- Phase fractions change.
- Microcracks develop.
- Effective material properties change.
- Macroscopic behavior changes.

Establish interfaces for state transfer across scales.

For example:

1. A macroscopic model identifies a region under elevated temperature and stress.
2. The relevant local state is transferred to an RVE or lower-scale representation.
3. A supported lower-scale model evaluates structural or material changes.
4. The resulting property changes are derived.
5. The updated properties are propagated to the higher-scale model.

Do not implement unrestricted real-time atomistic-to-continuum coupling unless the numerical methods and solver infrastructure genuinely support it.

Start with sequential or iterative coupling where practical.

Every transferred state must preserve units, coordinate conventions, physical assumptions, and provenance.

---

# 17. Nonlinear Resonance and Dynamic Material Interaction

Connect nonlinear material behavior to Prompt 20's resonance architecture.

Support appropriate investigations involving:

- Amplitude-dependent resonance.
- Nonlinear stiffness.
- Nonlinear damping.
- Frequency shifts.
- Hysteretic response.
- Bistability.
- Jump phenomena.
- Parametric excitation.
- Nonlinear mode interaction.
- Temperature-dependent resonance.
- State-dependent resonance.

Potential reference systems include:

- Duffing-type mechanical resonator.
- Nonlinear dielectric resonator.
- Magnetically nonlinear resonant structure.
- Thermally shifting electromagnetic cavity.
- Coupled resonators with state-dependent material properties.

Distinguish between:

- Linear eigenmode analysis.
- Nonlinear steady-state response.
- Transient nonlinear simulation.
- Stability analysis.

A nonlinear frequency response must not be presented as though it were simply a linear eigenfrequency calculation.

---

# 18. Reference Engineering Demonstrations

Implement practical demonstrations that prove dynamic material behavior is integrated into the physical simulation framework.

Prefer examples that reuse existing solvers.

## Example A — Thermally Evolving Electrical Conductor

Create a conductive object subjected to a defined heating condition.

Demonstrate:

1. Initial material properties.
2. Energy absorption or thermal input.
3. Temperature evolution.
4. Temperature-dependent conductivity.
5. Updated electrical response.
6. Feedback into subsequent energy absorption.
7. Recorded energy and material state.

This should be a priority example if the existing electromagnetic and thermal solvers support it.

## Example B — Thermo-Mechanical Expansion

Create a structure subjected to a temperature change.

Demonstrate:

1. Initial geometry and material state.
2. Thermal evolution.
3. Thermal expansion.
4. Thermal strain.
5. Induced mechanical stress where constrained.
6. Updated physical geometry where supported.
7. Changes in subsequent mechanical or physical response.

## Example C — Plastic Deformation

Create a mechanical specimen under increasing load.

Demonstrate:

1. Initial elastic response.
2. Yield onset.
3. Plastic deformation.
4. Unloading.
5. Residual deformation.
6. History-dependent subsequent loading.

The result must distinguish elastic and plastic strain.

## Example D — Damage and Resonance Shift

Create a mechanical resonator or supported structural model.

Introduce progressive material damage.

Demonstrate:

1. Initial stiffness and modal characteristics.
2. Damage accumulation.
3. Changes in material response.
4. Changes in resonant behavior.
5. Changes in stored and dissipated energy.

Do not fabricate damage-to-frequency relationships. Calculate them through the implemented physical model.

## Example E — Phase Transformation

Create a simple material model that undergoes a supported phase transition.

Demonstrate:

1. Initial phase.
2. Transformation condition.
3. Phase evolution.
4. Energy absorption or release.
5. Changes in material properties.
6. Resulting physical response.

An equilibrium phase selection alone does not qualify as a dynamic transformation simulation.

## Example F — Hysteretic Material

Implement one genuine hysteretic constitutive model.

Demonstrate a loading cycle and unloading cycle.

Verify that the subsequent material response depends on the internal state and loading history.

The hysteresis loop must arise from the constitutive model.

---

# 19. Visualization and User Experience

Dynamic material state should be understandable without overwhelming the existing interface.

Extend existing material and simulation workflows.

## 19.1 Material State Inspector

Provide an interface for examining:

- Current material state.
- Initial state.
- State history.
- Constitutive model.
- Active internal variables.
- Current physical conditions.
- Validity and convergence status.

## 19.2 Spatial State Visualization

Where supported, visualize state variables across the geometry.

Examples:

- Temperature maps.
- Stress and strain maps.
- Plastic deformation.
- Damage.
- Phase fractions.
- Magnetic state.
- Other supported internal variables.

Use existing visualization infrastructure.

## 19.3 Time Evolution

Provide time-dependent views of material state.

Allow users to inspect:

- Temperature versus time.
- Stress versus time.
- Strain versus time.
- Damage accumulation.
- Phase fraction evolution.
- Material property changes.
- Energy storage and dissipation.

Where useful, compare multiple state variables on synchronized timelines.

## 19.4 Material State History

Allow users to navigate previous material states.

The user should be able to inspect how the current state developed.

Where simulation checkpointing supports it, provide a way to restore or branch from a prior state without corrupting the original project.

## 19.5 Material Model Configuration

Provide a clear workflow for selecting and configuring nonlinear material models.

Display:

- Required parameters.
- Units.
- Validity ranges.
- Assumptions.
- Unsupported features.
- Relevant warnings.

Do not expose a large collection of poorly explained numerical parameters.

---

# 20. Automation and Programmatic Access

Extend the existing automation architecture.

Expose supported operations such as:

- Create dynamic material state.
- Inspect material state.
- Select constitutive model.
- Configure nonlinear material behavior.
- Apply physical loading.
- Advance material state.
- Run transient simulation.
- Inspect internal variables.
- Query damage.
- Query phase state.
- Inspect material history.
- Extract energy contributions.
- Compare material states.
- Recalculate dependent material properties.
- Invalidate dependent results.
- Restore supported checkpoints.

Automation must use the same physical models as the GUI.

Do not create a separate simplified implementation for scripted workflows.

Results must include structured information about:

- Simulation time.
- Material identity.
- State identity.
- Constitutive model.
- Updated variables.
- Convergence.
- Energy changes.
- Warnings and limitations.

---

# 21. Persistence, History and Reproducibility

Dynamic material state creates a significant persistence requirement.

A saved simulation must preserve enough information to reproduce or continue its physical evolution.

Persist:

- Initial material definitions.
- Initial material states.
- Constitutive models.
- Model parameters.
- State variables.
- Internal history variables.
- Time-step information.
- Relevant solver state.
- Material property dependencies.
- Energy contributions.
- Simulation checkpoints.
- State history where configured.
- Provenance and validation information.

Do not save only the final visual state.

A material with hysteresis, plastic deformation, damage, or other history-dependent behavior cannot necessarily be reconstructed from its current geometry and temperature alone.

Preserve the internal variables required by the model.

Implement efficient storage.

Avoid storing every full simulation field at every time step when checkpointing or selective history retention would be more appropriate.

Maintain project compatibility and version migrations.

---

# 22. Performance and Numerical Robustness

Dynamic nonlinear simulations can be substantially more expensive than static analyses.

Account for:

- Nonlinear iteration costs.
- Time-step size.
- Mesh resolution.
- State-variable count.
- Material update frequency.
- Solver coupling frequency.
- Memory use.
- Checkpoint storage.
- Visualization overhead.

Implement:

- Appropriate update scheduling.
- Adaptive time stepping where supported.
- Nonlinear convergence checks.
- State rollback.
- Checkpointing.
- Cancellation.
- Resource reporting.
- Dependency-aware recomputation.

Avoid unnecessary recomputation of material properties or resonance analyses.

A material state update should invalidate only the calculations that genuinely depend on it.

When a simulation fails to converge, report the physical and numerical context of the failure.

Do not silently accept an unconverged state.

---

# 23. Scientific Validation and Benchmarking

Validation is mandatory.

Tests must establish that the implemented constitutive models produce physically meaningful behavior.

## 23.1 State Architecture

Verify:

- Material definitions and states are separate.
- State variables are associated with the correct material and spatial region.
- History-dependent variables persist.
- Revisions invalidate dependent results.
- State history survives serialization.
- State rollback restores the appropriate physical state.

## 23.2 Mechanical Models

Where implemented, validate:

- Linear elastic response.
- Thermal expansion.
- Yield onset.
- Plastic deformation.
- Unloading and residual strain.
- Viscoelastic time response.
- Damage evolution.

Use analytical solutions or independently verified benchmarks.

## 23.3 Thermal and Phase Models

Where implemented, validate:

- Thermal evolution.
- Temperature-dependent properties.
- Thermal expansion.
- Energy associated with phase transitions.
- Phase fraction evolution.
- Relevant limiting cases.

## 23.4 Electromagnetic and Magnetic Models

Where implemented, validate:

- Nonlinear constitutive response.
- Hysteresis.
- Saturation.
- Field-dependent material properties.
- Energy storage and loss.

## 23.5 Energy Accounting

Verify that:

- Reversible energy storage is calculated consistently.
- Irreversible dissipation is nonnegative where required by the model.
- Energy transfer between coupled domains is accounted for.
- Energy is not double-counted.
- Energy balance errors are measured and reported.

## 23.6 Regression and Integration

Verify that:

- Existing linear simulations still behave correctly.
- Static material models remain compatible.
- Prompt 22 material lineage remains intact.
- Prompt 20 resonance analysis can consume updated properties.
- Prompt 21 effective materials can be updated when underlying constituents change.
- CEME can coordinate supported dynamic material updates.

Do not claim universal energy conservation for numerical models that have not implemented or validated the necessary accounting.

---

# 24. Implementation Strategy

Implement this capability in deliberate phases.

## Phase 1 — Architecture Audit and Dynamic State Framework

- Audit existing material and CEME systems.
- Establish material state identity.
- Separate material definitions from current states.
- Establish state history.
- Define constitutive update contracts.
- Define spatial state interfaces.
- Establish persistence and dependency tracking.

## Phase 2 — Nonlinear Mechanical Materials

- Implement the constitutive architecture.
- Add a validated nonlinear mechanical model.
- Add plastic or viscoelastic behavior where practical.
- Validate loading and unloading.
- Preserve internal history variables.

## Phase 3 — Thermal and Thermo-Mechanical Evolution

- Implement temperature-dependent material properties.
- Connect thermal and mechanical state.
- Support thermal expansion and thermal stress where appropriate.
- Validate the feedback process.
- Connect thermal energy contributions to CEME.

## Phase 4 — Damage and Phase Evolution

- Implement an initial damage model.
- Establish fracture interfaces.
- Implement an appropriate phase evolution model where practical.
- Account for energy associated with irreversible changes.
- Validate state evolution.

## Phase 5 — Nonlinear Electromagnetic and Other Domains

- Extend the architecture to supported electromagnetic, optical, magnetic, acoustic, and fluid domains.
- Implement at least one genuine nonlinear constitutive model in an additional domain if existing solver infrastructure supports it.
- Preserve domain-specific equations.

## Phase 6 — Geometry, Resonance and Metamaterial Integration

- Connect state changes to physical geometry.
- Update dependent material properties.
- Integrate with resonance and modal analysis.
- Integrate with metamaterial effective-property derivation.
- Implement dependency-aware recalculation.

## Phase 7 — Multiscale and Automation Integration

- Connect state changes to Prompt 22 lineage.
- Establish state-transfer interfaces.
- Extend automation.
- Implement state inspection and history visualization.
- Add reference engineering projects.

## Phase 8 — Validation and Documentation

- Run all scientific benchmarks.
- Run regression tests.
- Evaluate performance.
- Document supported models.
- Document limitations.
- Produce the engineering completion report.

Do not implement every possible nonlinear model in one pass.

Prioritize a small number of complete, validated physical models over a large number of superficial model definitions.

---

# 25. Explicit Scientific Guardrails

These requirements are mandatory.

1. Do not treat dynamic material state as a visual animation.
2. Do not treat material properties as automatically constant throughout a simulation.
3. Do not confuse reversible and irreversible deformation.
4. Do not implement hysteresis as a graphical effect.
5. Do not apply a mechanical constitutive equation to an unrelated physical domain.
6. Do not claim fracture simulation when only a damage indicator is calculated.
7. Do not claim dynamic phase transformation when only equilibrium phase selection is implemented.
8. Do not claim atomistic microstructural evolution without an actual lower-scale model.
9. Do not create arbitrary property changes to make a demonstration appear interesting.
10. Do not silently clamp unstable or nonphysical state variables.
11. Do not discard history variables required by a constitutive model.
12. Do not allow unconverged state updates to be presented as valid results.
13. Do not double-count energy contributions across coupled solvers.
14. Do not update geometry visually without updating the physical simulation representation when geometry changes affect the physics.
15. Do not automatically recompute expensive analyses without dependency and significance checks.
16. Do not claim a nonlinear capability unless its governing model and numerical implementation actually exist.
17. Do not force every material or solver to support every state variable.
18. Do not sacrifice existing linear simulation behavior when adding nonlinear capabilities.

The system must remain scientifically honest about what it can and cannot simulate.

---

# 26. Acceptance Criteria

Prompt 23 is complete when the following conditions are satisfied.

### Dynamic Material Architecture

- [ ] Material definitions and material states are separate entities.
- [ ] Material states can contain physical and historical variables.
- [ ] State updates are coordinated with the simulation time step.
- [ ] Spatially varying material states are supported where appropriate.
- [ ] State history survives persistence and reload.
- [ ] State-dependent properties can be consumed by existing solvers.

### Nonlinear Constitutive Modeling

- [ ] A genuine nonlinear constitutive model is implemented.
- [ ] At least one history-dependent model is implemented and validated.
- [ ] Loading and unloading behavior is correctly distinguished.
- [ ] Internal variables are persisted.
- [ ] Numerical convergence and failure are reported.

### Thermal and Mechanical Integration

- [ ] Temperature-dependent properties are supported where appropriate.
- [ ] At least one thermo-mechanical feedback workflow is operational if the existing solvers permit it.
- [ ] Energy contributions are exposed to CEME.
- [ ] Geometry updates are physically integrated where supported.

### Damage and Phase Evolution

- [ ] Damage architecture exists.
- [ ] At least one genuine damage model is operational, or its implementation boundary is explicitly documented.
- [ ] Phase evolution architecture exists.
- [ ] Any implemented phase transition includes appropriate state and energy handling.
- [ ] Unsupported fracture or transformation capabilities are not misrepresented.

### Cross-System Integration

- [ ] Material state updates can affect supported engineering calculations.
- [ ] Resonance analysis can consume updated material properties.
- [ ] Metamaterial dependencies can be invalidated or updated appropriately.
- [ ] Prompt 22 material lineage is preserved.
- [ ] CEME coordinates the implemented dynamic physical interactions.

### User Experience and Automation

- [ ] Users can inspect material state.
- [ ] Users can inspect relevant state history.
- [ ] Users can visualize supported spatial state variables.
- [ ] Supported operations are accessible through existing automation.
- [ ] Example projects demonstrate genuine dynamic material behavior.

### Scientific Quality

- [ ] Constitutive benchmark tests pass.
- [ ] Energy accounting tests pass for the implemented models.
- [ ] Numerical stability and convergence tests pass.
- [ ] Existing linear simulation regression tests pass.
- [ ] Documentation clearly identifies implemented models and limitations.

**Completion does not require every nonlinear physical domain or every damage and phase model to be implemented. It requires a functioning dynamic material-state architecture and multiple meaningful, validated examples that demonstrate its integration with RESONANT.**

---

# 27. Final Engineering Report

When implementation is complete, provide a detailed report covering:

1. Existing architecture audit.
2. Dynamic material state architecture.
3. Constitutive model interfaces.
4. Implemented nonlinear material models.
5. Mechanical behavior.
6. Thermal and thermo-mechanical behavior.
7. Damage and fracture capabilities.
8. Phase evolution capabilities.
9. Nonlinear electromagnetic and other supported domains.
10. Material state history and persistence.
11. Geometry integration.
12. Resonance and metamaterial integration.
13. CEME energy accounting.
14. Automation and GUI changes.
15. Scientific benchmarks and results.
16. Numerical stability and performance.
17. Test results.
18. Known limitations.
19. Deferred capabilities.
20. Recommended next engineering steps.

Identify the relevant source files, modules, interfaces, and tests.

Clearly distinguish implemented capabilities from architectural interfaces and future work.

---

# Final Directive

RESONANT must now be capable of representing matter as something that evolves.

A material is not simply assigned a stiffness, conductivity, density, or permittivity and then left unchanged while energy passes through it.

It experiences the physical world.

It heats, expands, deforms, absorbs energy, changes phase, accumulates damage, develops internal stresses, and alters its response.

Those changes influence the fields and structures around it.

The surrounding system then responds to the changed material.

This is a feedback process.

**The central objective is to make the material itself an active participant in the simulation.**

Implement this as a cohesive extension of the existing RESONANT architecture, with rigorous physical models, meaningful numerical integration, traceable state evolution, and scientific validation.

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: - (heuristic)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Develop, EVOLVING, Implement, ability to
  - evidence: 'EVOLVING' [head]; 'Implement' [head]; 'capability' [head]; 'Develop' [head]; 'Develop' [head]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: User: 'add this and perform in order' (queued after REQ-CHAT-20261126-020, Prompt 22)

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

User: 'add this and perform in order' (queued after REQ-CHAT-20261126-020, Prompt 22)

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000022
- Evidence: pending
