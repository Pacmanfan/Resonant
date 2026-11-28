# Request: # PROMPT 22 — MULTISCALE MATERIAL MODELING: FROM MACROSCOPIC BEHAVIOR TO MICROST

**Request ID:** REQ-CHAT-20261126-020
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T23:43:16-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 22 — MULTISCALE MATERIAL MODELING: FROM MACROSCOPIC BEHAVIOR TO MICROSTRUCTURE, MOLECULAR, ATOMIC AND QUANTUM PHYSICS

## Project: RESONANT — Wave Field Engineering Studio

## Mission

Implement the next major scientific capability in RESONANT: **Multiscale Material Modeling**.

RESONANT must gain the architectural ability to investigate, derive, and connect material behavior across multiple physical scales.

The objective is to establish a traceable modeling hierarchy that connects macroscopic engineering properties to the physical structures and interactions that produce them.

A material's macroscopic behavior is not independent of its internal organization. Elasticity, conductivity, thermal transport, optical response, magnetic behavior, damping, phase stability, strength, and other properties can emerge from structures and interactions at smaller scales.

RESONANT must be able to represent those relationships and, where computational capabilities and validated dependencies permit, calculate them.

This is not a request to build an entire quantum chemistry or molecular dynamics package from scratch. It is a request to establish the architecture, scientific interfaces, data models, coupling mechanisms, and at least one meaningful validated multiscale workflow that can expand over time.

The result must integrate with the existing Shape–Function–Energy architecture, Coupled Energy–Matter Engine, geometry-driven engineering, resonant structures, and metamaterial systems.

---

# 1. Governing Concept

Establish the following physical relationship as a central architectural principle:

**Atomic and Molecular Structure → Microstructure → Effective Material Properties → Macroscopic Physical Behavior → Functional Engineering Performance**

The reverse direction must also be supported:

**Macroscopic Conditions → Local Physical State → Microstructural Response → Molecular or Atomic Investigation**

This creates a two-way relationship between engineering-scale behavior and the physical mechanisms responsible for it.

For example:

A structure exhibits an unexpected thermal response.

RESONANT should eventually be able to:
1. Identify the region responsible for the behavior.
2. Extract its local temperature, stress, strain, field, or other relevant physical state.
3. Construct or identify an appropriate representative material region.
4. Investigate the material at a smaller scale.
5. Determine which physical mechanisms may explain the observed behavior.
6. Derive updated or alternative material properties.
7. Propagate those properties back into the engineering-scale model.
8. Recalculate the system and compare the resulting behavior.

This is the foundation of genuine multiscale engineering.

Do not assume that every engineering problem requires every scale. The system must select the appropriate physical model and scale for the question being investigated.

---

# 2. Audit the Existing Architecture Before Implementation

Before modifying the repository, perform a comprehensive audit.

Inspect:

- Material definitions and constitutive models.
- Material property databases and import mechanisms.
- Geometry and microstructure representations.
- Mesh generation and simulation domains.
- CEME architecture and physical state management.
- Existing electromagnetic, optical, acoustic, thermal, mechanical, and other supported solvers.
- Resonance and metamaterial systems.
- Existing numerical methods and optimization infrastructure.
- External solver integrations and dependencies.
- Serialization, project persistence, and versioning.
- GUI material editors and engineering workflows.
- Automation APIs and scripting interfaces.
- Existing scientific validation and benchmark tests.

Identify which capabilities already exist and which are missing.

Produce an architecture map showing where multiscale modeling should connect to the current system.

Do not create parallel material databases, duplicate geometry systems, or independent simulation frameworks when existing systems can be extended.

Preserve backward compatibility with existing projects.

Do not begin by installing a large collection of scientific packages. First determine what the environment already supports and which integrations provide meaningful capabilities.

---

# 3. Establish the Multiscale Modeling Hierarchy

Implement a formal hierarchy of material representations.

These must be distinct modeling levels, not merely different visualization resolutions.

## 3.1 Continuum and Macroscopic Scale

This is the conventional engineering material representation.

Examples:

- Elastic solids.
- Conductive materials.
- Dielectrics.
- Magnetic materials.
- Fluids.
- Thermal materials.
- Optical media.
- Composite engineering materials.

Properties may include:

- Density.
- Elastic modulus.
- Full stiffness and compliance tensors.
- Poisson's ratio.
- Thermal conductivity.
- Electrical conductivity.
- Heat capacity.
- Thermal expansion.
- Permittivity and permeability.
- Optical refractive index.
- Magnetic susceptibility.
- Viscosity.
- Diffusion coefficients.
- Damping and loss parameters.
- Strength and failure parameters.

Support isotropic, orthotropic, anisotropic, and other appropriate constitutive descriptions.

Properties must be capable of depending on temperature, pressure, composition, direction, frequency, deformation, and material state where the governing physics requires it.

## 3.2 Effective and Homogenized Material Scale

This level represents the averaged or emergent behavior of structured material.

It is particularly important for metamaterials, composites, porous structures, and engineered materials.

Examples:

- Effective permittivity.
- Effective permeability.
- Effective acoustic density.
- Effective bulk modulus.
- Effective elastic stiffness.
- Effective thermal conductivity.
- Effective electrical conductivity.
- Effective optical response.

Maintain a traceable relationship to the underlying microstructure.

A homogenized material must not become an unrelated material definition that loses its physical origin.

Integrate directly with Prompt 21.

## 3.3 Microstructure and Mesoscale

Represent the internal organization of materials.

Support appropriate combinations of:

- Grains and grain boundaries.
- Crystalline and amorphous phases.
- Inclusions.
- Precipitates.
- Voids and pores.
- Cracks.
- Dislocations.
- Defects.
- Interfaces.
- Domains and domain boundaries.
- Crystallographic orientation and texture.
- Layered structures.
- Composite phases.
- Fiber and particle reinforcement.
- Hierarchical structures.

Microstructure must be capable of having its own geometry, material constituents, spatial distribution, orientation, and physical properties.

Support imported microstructure data where practical.

Provide procedural generation for simple representative structures, without presenting synthetic structures as measured microstructures.

## 3.4 Molecular Scale

Establish the data model and integration interfaces for molecular representations.

Support the concepts required by molecular simulation:

- Molecular topology.
- Atoms and molecular species.
- Bonds.
- Bond angles.
- Dihedral and torsional interactions.
- Partial charges.
- Molecular geometry.
- Force fields.
- Intermolecular interactions.
- Periodic simulation cells.
- Molecular conformations.
- Molecular configuration and state.

Molecular models must distinguish between geometry, topology, interaction parameters, and simulation conditions.

## 3.5 Atomistic Scale

Establish atomistic material representations.

Support:

- Atomic positions.
- Chemical species.
- Atomic masses.
- Crystal structures.
- Lattice vectors.
- Unit cells.
- Supercells.
- Atomic bonds and coordination.
- Vacancies.
- Interstitial defects.
- Dislocations where supported.
- Grain boundaries.
- Surfaces and interfaces.
- Atomic substitutions and impurities.
- Periodic and non-periodic boundaries.

The system should be able to represent crystalline and non-crystalline structures.

Atomic models must be compatible with the material and geometry lineage system.

## 3.6 Electronic and Quantum Scale

Establish an architecture for electronic-structure and quantum-mechanical material calculations.

Where an actual supported solver is available, this level should eventually support calculations involving:

- Electronic structure.
- Band structures.
- Density of states.
- Charge density.
- Cohesive and binding energies.
- Dielectric response.
- Optical response.
- Magnetic properties.
- Phonon properties.
- Electronic and thermal transport inputs.
- Material stability and structural energetics.

Do not implement fake quantum calculations.

If a quantum solver is unavailable, establish the integration interface and explicitly identify the capability as unavailable rather than simulating results with arbitrary formulas or database values.

---

# 4. Multiscale Data Architecture

Create a unified architecture for managing relationships between scales.

Every representation must have:

- A unique identity.
- A material or structure identity.
- A defined physical scale.
- A representation type.
- Geometry or structural data.
- Constituent materials.
- Relevant physical state.
- Model assumptions.
- Boundary conditions.
- Source and provenance.
- Revision information.
- Validation status.
- Uncertainty information where available.

Establish explicit relationships between representations.

For example:

A macroscopic material may reference an effective material.

That effective material may reference an RVE.

The RVE may reference a microstructure containing several constituent phases.

Those phases may reference atomistic structures.

The atomistic structures may reference molecular or electronic-structure calculations.

The complete chain must remain navigable.

## 4.1 Material Lineage

Implement material lineage as a first-class system.

A derived property must retain its origin.

For every calculated or imported material property, record as applicable:

- Source material.
- Source structural representation.
- Scale at which it was calculated.
- Solver or calculation method.
- Solver version.
- Input parameters.
- Boundary conditions.
- Temperature and pressure.
- Composition.
- Material state.
- Convergence information.
- Numerical resolution.
- Statistical sampling information.
- Uncertainty.
- Date and calculation identity.
- Parent and derived revisions.

A property calculated from an atomistic model must remain linked to that model.

A property obtained from a database must be identified as database-sourced.

A fitted or calibrated property must be identified as fitted or calibrated.

A surrogate prediction must be identified as a surrogate prediction.

Do not present all material properties as equally authoritative.

## 4.2 Model Cards

Each material representation and calculation should have an associated model card.

The model card must explain:

- What physical system is represented.
- What scale is being modeled.
- What assumptions are made.
- Which equations or methods are used.
- Which parameters are required.
- Which effects are excluded.
- Which physical conditions are valid.
- How results should be interpreted.

This is essential to scientific transparency.

---

# 5. Representative Volume Elements and Microstructure Modeling

Implement the Representative Volume Element (RVE) as a central engineering concept.

An RVE is a sufficiently representative portion of a material from which effective macroscopic behavior can be estimated under appropriate assumptions.

Support:

- RVE geometry.
- Multiple material phases.
- Spatial distributions.
- Grain structures.
- Orientation distributions.
- Inclusions and pores.
- Periodic and non-periodic arrangements.
- Boundary conditions.
- Meshing.
- Property calculations.
- Homogenization.

The system should support multiple RVE sizes and configurations.

It must be possible to investigate whether the calculated effective properties converge as the RVE becomes larger or more representative.

Do not assume that one small region is representative of every material.

## 5.1 RVE Boundary Conditions

Support the appropriate boundary-condition families where the solver permits:

- Periodic boundary conditions.
- Kinematic uniform boundary conditions.
- Static uniform boundary conditions.
- Other physically appropriate boundary conditions.

Document the assumptions and limitations of each.

Periodic boundary conditions must not be applied automatically to arbitrary non-periodic structures.

## 5.2 Computational Homogenization

Establish a pathway for deriving macroscopic properties from microscale simulations.

Where appropriate, support:

- Volume averaging.
- Field averaging.
- Energy-based homogenization.
- Effective tensor calculation.
- Constitutive response extraction.
- Statistical averaging.
- Sensitivity to RVE size and configuration.

For example, derive an effective elastic stiffness tensor from a heterogeneous microstructure rather than assigning an arbitrary effective modulus.

Where feasible, compare homogenized predictions against an explicitly modeled larger structure.

## 5.3 FE² and Hierarchical Methods

Investigate an architecture that could eventually support FE² or similar computational homogenization methods.

FE² conceptually couples a macroscopic finite-element calculation to microscale calculations at integration points.

Do not implement a superficial FE² label without actual two-scale coupling.

If the current numerical architecture cannot support this efficiently, implement the interfaces and document the future requirements.

The architecture should not prevent this capability from being introduced later.

---

# 6. Molecular and Atomistic Simulation Integration

Establish real scientific integrations where supported.

Do not reinvent mature scientific simulation packages unnecessarily.

Investigate appropriate options such as:

- LAMMPS for atomistic and molecular dynamics.
- OpenMM for molecular simulation.
- Other suitable molecular or atomistic engines already available or justified by the environment.

These are candidate integrations, not mandatory dependencies.

Select integrations based on:

- Existing repository architecture.
- Platform compatibility.
- Licensing.
- Installation complexity.
- Performance.
- Supported physical models.
- Data interoperability.
- Long-term maintainability.

## 6.1 Molecular Dynamics

Where a genuine molecular dynamics engine is available, support a meaningful configuration and execution workflow.

Relevant concepts include:

- Force fields.
- Interaction potentials.
- Initial atomic or molecular configurations.
- Integration time steps.
- Simulation duration.
- Temperature control.
- Pressure control.
- Ensembles.
- Equilibration.
- Statistical sampling.
- Periodic boundaries.
- Energy monitoring.

Support appropriate physical observables, potentially including:

- Radial distribution functions.
- Mean squared displacement.
- Diffusion coefficients.
- Atomic or molecular trajectories.
- Structural ordering.
- Temperature and pressure behavior.
- Elastic response.
- Thermal transport where the method and calculation support it.

Do not assume that every property can be reliably derived from a short simulation.

## 6.2 Force Fields and Potentials

Force fields and interaction potentials are scientific models, not generic tuning parameters.

Record:

- Force-field identity.
- Version.
- Parameter source.
- Chemical and material applicability.
- Known limitations.
- Validity range.
- Calibration provenance.

Prevent users from applying a force field to an incompatible chemical system without an explicit warning.

## 6.3 Atomistic Validation

Support appropriate validation checks:

- Energy minimization.
- Structural stability.
- Time-step sensitivity.
- Simulation-size sensitivity.
- Temperature equilibration.
- Energy conservation where applicable.
- Statistical convergence.
- Finite-size effects.

The system must distinguish between a simulation that executed successfully and a simulation that produced scientifically reliable results.

---

# 7. Electronic Structure and Quantum Integration

Establish a modular interface for quantum and electronic-structure calculations.

Investigate suitable existing packages where appropriate, including:

- Quantum ESPRESSO.
- CP2K.
- GPAW.
- Other compatible electronic-structure packages.

Do not require all of these.

Select a realistic integration path based on the actual environment, architecture, licensing, and available computational resources.

## 7.1 Calculation Configuration

Where supported, expose relevant calculation parameters:

- Atomic structure.
- Crystal lattice.
- Chemical composition.
- Electronic configuration.
- Exchange-correlation functional.
- Basis or plane-wave cutoff.
- Pseudopotentials.
- k-point sampling.
- Spin configuration.
- Convergence thresholds.
- Occupation and smearing methods.
- Relevant relativistic treatment.
- Boundary conditions.

Avoid exposing meaningless or unsupported parameters.

## 7.2 Derived Quantities

Where the selected solver supports them, extract:

- Total energy.
- Cohesive energy.
- Band structure.
- Density of states.
- Charge density.
- Electronic properties.
- Dielectric response.
- Optical response.
- Magnetic properties.
- Phonon information.

Record all calculation settings and convergence evidence.

## 7.3 Quantum Scientific Guardrails

Explicitly account for the limitations of quantum calculations.

These may include:

- Functional approximations.
- Pseudopotential limitations.
- Basis-set convergence.
- Plane-wave cutoff convergence.
- k-point convergence.
- Spin treatment.
- Relativistic effects.
- Finite-size effects.
- Structural relaxation.
- Temperature approximations.
- Missing many-body effects.

Do not imply that a DFT calculation automatically produces experimentally accurate properties.

The calculation must communicate its assumptions and limitations.

---

# 8. Property Derivation and Upscaling

This is one of the most important functions of Prompt 22.

RESONANT must be able to derive higher-level material properties from lower-level representations.

Establish a consistent property derivation framework.

Potential derived properties include:

### Mechanical

- Elastic stiffness tensors.
- Compliance tensors.
- Elastic moduli.
- Poisson's ratio.
- Anisotropic mechanical response.
- Thermal expansion.
- Material density.
- Damping parameters where physically supported.

### Thermal

- Thermal conductivity.
- Thermal diffusivity.
- Heat capacity.
- Thermal expansion.
- Directional thermal transport.

### Electrical and Electromagnetic

- Electrical conductivity.
- Effective permittivity.
- Effective permeability.
- Dielectric response.
- Frequency-dependent constitutive properties.

### Optical

- Refractive index.
- Optical dispersion.
- Absorption-related properties.
- Anisotropic optical response.

### Molecular and Transport

- Diffusion coefficients.
- Viscosity.
- Molecular mobility.
- Other transport properties supported by the selected simulation method.

### Structural and Material Stability

- Cohesive energy.
- Phase stability indicators.
- Defect energetics.
- Structural response.
- Other validated material descriptors.

Not every property must be available for every scale or material.

The system must identify which methods can legitimately derive each property.

## 8.1 Property Derivation Pipeline

Implement a pipeline:

1. Select source material representation.
2. Select the target property.
3. Select an appropriate physical method.
4. Validate model applicability.
5. Configure calculation conditions.
6. Execute calculation.
7. Assess numerical convergence.
8. Calculate the resulting property.
9. Estimate uncertainty where possible.
10. Record provenance and validity.
11. Make the derived property available to higher-scale models.

Derived properties must be versioned.

Changing the underlying microstructure or atomic structure must mark dependent properties as potentially stale.

## 8.2 State-Dependent Properties

Properties may depend on:

- Temperature.
- Pressure.
- Composition.
- Frequency.
- Direction.
- Strain.
- Defect concentration.
- Phase.
- Material history.
- Other relevant physical conditions.

Do not collapse these relationships into a single scalar when the physics requires a tensor, function, or state-dependent constitutive model.

---

# 9. Downscaling and Local Material Investigation

The architecture must support the opposite direction as well.

A macroscopic simulation may reveal a local region with unusual physical behavior.

RESONANT should eventually be able to use the local state to configure a smaller-scale investigation.

Examples:

- A localized thermal hot spot.
- A stress concentration.
- A region with unusual deformation.
- An electromagnetic field concentration.
- A region of high mechanical dissipation.
- A material interface with unusual energy transfer.
- A region exhibiting unexpected resonance behavior.

The system should be capable of extracting relevant local conditions and transferring them to a smaller-scale model.

Examples include:

- Local temperature.
- Stress and strain.
- Electric and magnetic fields.
- Local composition.
- Pressure.
- Deformation.
- Relevant boundary conditions.

Do not assume every macroscopic field can be transferred directly to every atomistic or molecular model.

The transfer must be physically appropriate and clearly documented.

Support mapping between coordinate systems, meshes, and representations where required.

---

# 10. Multiscale Coupling Strategies

Implement a modular coupling architecture.

Different physical problems require different coupling strategies.

Do not force every simulation into one universal coupling method.

Support the following architectural categories.

## 10.1 Sequential or Offline Coupling

One scale produces parameters consumed by another scale.

Example:

Atomistic simulation → elastic constants → continuum material model.

This should be the initial implementation priority because it provides a practical and verifiable multiscale pathway.

## 10.2 Iterative Coupling

A higher-scale model provides local conditions to a lower-scale model.

The lower-scale model calculates a response.

The higher-scale model is updated using that response.

Repeat until an appropriate convergence condition is satisfied.

## 10.3 Hierarchical Coupling

A sequence of models operates at different scales.

For example:

Quantum calculation → atomistic model → microstructure model → effective material → macroscopic engineering model.

Each transition must preserve provenance and uncertainty.

## 10.4 Concurrent Coupling

Investigate future support for concurrent atomistic-continuum or other concurrent multiscale approaches.

Do not implement this as a placeholder that claims actual coupling.

Only implement when the numerical methods, solver integrations, and physical interfaces genuinely support it.

## 10.5 Coupling Contracts

Every coupling method must define:

- Source scale.
- Target scale.
- Transferred quantities.
- Coordinate transformations.
- Physical assumptions.
- Boundary conditions.
- Update frequency.
- Convergence criteria.
- Failure handling.
- Uncertainty propagation.
- Provenance.

---

# 11. Connection to Dynamic Material State — Prompt 23

Prepare the architecture for evolving material properties.

Materials do not always remain static during operation.

Examples include:

- Phase transformations.
- Thermal expansion.
- Plastic deformation.
- Damage accumulation.
- Fatigue.
- Aging.
- Diffusion.
- Chemical reactions.
- Microstructural evolution.
- Defect formation.
- Grain growth.
- Changes in molecular configuration.

Prompt 22 must establish the state and lineage interfaces required for these future capabilities.

A material representation must be capable of distinguishing:

- Initial state.
- Current state.
- Derived state.
- Historical state.
- Predicted future state.

Preserve the relationship between material state and its derived properties.

Do not implement an incomplete dynamic material engine here. Prompt 23 will address that directly.

---

# 12. Integration With the Existing RESONANT Architecture

This capability must be integrated into the existing engineering architecture.

## 12.1 Prompt 17 — Shape–Function–Energy

Material structure must be connected to functional behavior.

The material model should help explain why a geometry produces a particular physical response.

## 12.2 Prompt 18 — Coupled Energy–Matter Engine

Material properties must be available to CEME.

CEME must be able to consume derived material properties and appropriate state-dependent constitutive models.

Material calculations must not become disconnected from the central physical simulation framework.

## 12.3 Prompt 19 — Geometry-Driven Energy Transformation

Geometry and material properties must remain linked.

The material's internal structure may influence how the larger geometry transforms, confines, transfers, or dissipates energy.

## 12.4 Prompt 20 — Resonant Structures and Energy Storage

Multiscale-derived properties must be available to resonance and modal analysis.

Examples:

- Atomic structure influences elastic stiffness.
- Microstructure influences mechanical damping.
- Material composition influences dielectric response.
- Temperature influences resonant frequency.
- Microstructural losses influence quality factor.

These relationships must be physically derived or supplied by validated models.

## 12.5 Prompt 21 — Metamaterials

This integration is particularly important.

A metamaterial's effective properties must remain connected to its underlying structure and constituents.

For example:

1. Define the constituent material.
2. Obtain or derive its physical properties.
3. Construct the metamaterial unit cell.
4. Model the internal geometry and arrangement.
5. Calculate the emergent response.
6. Derive effective material properties.
7. Analyze wave propagation and resonance.
8. Compare the effective model against an explicitly modeled structure.

The system must retain the complete derivation chain.

---

# 13. Reference Engineering Workflows

Implement at least one complete, scientifically defensible multiscale workflow. Prefer multiple examples where the existing solver infrastructure makes them practical.

## Example A — Crystalline Material

Construct or import a simple crystalline structure.

Where a supported atomistic or electronic-structure solver exists:

1. Define the crystal lattice and atomic positions.
2. Configure the calculation.
3. Establish structural stability.
4. Calculate a relevant physical property.
5. Derive a higher-level material parameter.
6. Import that parameter into a continuum material definition.
7. Run a macroscopic engineering calculation.
8. Preserve the full lineage.

If a genuine atomistic or quantum calculation cannot be executed in the current environment, do not claim this workflow is complete.

## Example B — Polycrystalline Material

Construct an RVE containing multiple grains.

Allow variation in:

- Grain size.
- Grain shape.
- Crystallographic orientation.
- Phase distribution.
- Grain-boundary properties.

Calculate an effective property, such as elastic stiffness or thermal conductivity, using an appropriate numerical method.

Investigate sensitivity to RVE size and orientation distribution.

## Example C — Composite or Porous Material

Construct a heterogeneous RVE containing multiple phases, inclusions, or pores.

Calculate effective thermal or mechanical properties.

Compare against a simplified homogeneous approximation.

Demonstrate that microstructure can materially influence the derived properties.

## Example D — Multiscale Metamaterial

Connect a constituent material to a structured unit cell.

Derive or import constituent properties.

Calculate the unit-cell response.

Extract effective properties.

Use those properties in a larger-scale model.

Connect the result to Prompt 21's metamaterial and band-structure systems.

## Example E — Cross-Scale Engineering Investigation

Provide a demonstration in which a macroscopic model uses a derived material property and allows the user to navigate back to the underlying representation and calculation.

This example should demonstrate the value of the complete architecture, not merely display several disconnected simulation results.

---

# 14. Material Database and External Data

Extend material import and data management to support multiscale representations.

Potential sources include:

- Crystallographic structure databases.
- Materials property databases.
- Molecular structure files.
- Atomistic structure files.
- Experimental microstructure data.
- User-generated material definitions.
- Existing simulation outputs.

Support appropriate file formats where practical, such as:

- CIF.
- XYZ.
- PDB.
- POSCAR.
- Other formats justified by the selected solver integrations.

Do not assume every format contains all necessary physical information.

Imported data must be validated and accompanied by source information.

Do not silently convert missing information into fabricated defaults.

Imported material properties must be distinguished from calculated properties.

---

# 15. Uncertainty and Scientific Confidence

Multiscale modeling introduces uncertainty at every transition.

The system must not treat calculated material properties as exact merely because a solver returned a number.

Track uncertainty arising from:

- Model assumptions.
- Incomplete microstructure information.
- Limited sampling.
- RVE size.
- Statistical variation.
- Force-field limitations.
- Quantum approximations.
- Numerical convergence.
- Measurement uncertainty.
- Property-fitting procedures.
- Scale-transfer assumptions.

Where feasible, propagate uncertainty through property derivation and homogenization.

At minimum, the architecture must preserve uncertainty information and avoid discarding it when transferring a property between scales.

Distinguish:

- Numerically converged.
- Statistically converged.
- Experimentally validated.
- Model-calibrated.
- Database-sourced.
- Predicted but unvalidated.

These are not interchangeable statuses.

---

# 16. Visualization and User Experience

Multiscale modeling must be accessible without making RESONANT feel like a collection of unrelated scientific applications.

Create a coherent workflow around the material itself.

## 16.1 Material Scale Explorer

Provide a way to navigate between material representations.

For example:

Macroscopic material → Effective material → Microstructure → Grain → Atomic structure → Electronic structure.

The interface should make it clear which scale is currently active.

Show relationships between parent and derived representations.

## 16.2 Structural Visualization

Where appropriate, visualize:

- Microstructure.
- Grain boundaries.
- Phase distributions.
- Inclusions.
- Porosity.
- Atomic positions.
- Molecular structures.
- Crystal lattices.
- Defects.
- Simulation cells.

Use existing geometry and rendering systems wherever possible.

Do not build an unrelated visualization framework.

## 16.3 Property Comparison

Provide comparison tools for:

- Constituent versus effective properties.
- Different microstructures.
- Different RVE sizes.
- Different temperatures.
- Different compositions.
- Different structural configurations.
- Different calculation methods.

Clearly identify calculated, imported, fitted, and estimated results.

## 16.4 Provenance Navigation

Users should be able to select a material property and discover:

- Where it came from.
- Which model produced it.
- Which source structure was used.
- Which calculation conditions applied.
- Whether the result is still current.
- What uncertainty and limitations apply.

## 16.5 Engineering Workflow

The user should be able to:

1. Select or create a material.
2. Choose the relevant modeling scale.
3. Import or construct the representation.
4. Configure a physical model.
5. Execute an analysis.
6. Inspect the results.
7. Derive a property.
8. Promote that property to a higher-scale material.
9. Assign the material to an engineering geometry.
10. Run a CEME or other supported simulation.
11. Navigate back to the underlying material calculation.

The workflow must preserve existing RESONANT usability and visual consistency.

---

# 17. Automation and Programmatic Access

Expose multiscale functionality through the existing automation architecture.

Do not create a separate automation subsystem.

Provide programmatic access to relevant operations, including:

- Create material representation.
- Import structure.
- Create microstructure.
- Configure RVE.
- Configure atomistic or molecular calculation.
- Configure electronic-structure calculation where available.
- Execute supported calculations.
- Inspect convergence.
- Extract properties.
- Perform homogenization.
- Create derived material.
- Inspect lineage.
- Compare properties.
- Validate model applicability.
- Export results.

Automation operations must return structured results.

Failures must identify the stage and cause.

Do not report a calculation as successful if it only created a configuration or submitted an incomplete job.

---

# 18. Performance and Resource Management

Multiscale calculations can be computationally expensive.

The architecture must account for:

- CPU and GPU availability.
- Memory requirements.
- Simulation cell size.
- Atom count.
- Mesh resolution.
- Solver execution time.
- Parallel execution.
- Calculation cancellation.
- Checkpointing.
- Result caching.
- Intermediate data storage.

Use existing job management and resource infrastructure where possible.

Provide resource estimates when a calculation's approximate cost can be determined.

Do not start large calculations without making their expected resource requirements reasonably clear.

Allow calculations to be interrupted and resumed where the underlying solver supports it.

Keep external solver processes isolated and manage their lifecycle safely.

---

# 19. Persistence and Project Integrity

Persist all multiscale entities and relationships.

A saved project must retain:

- Material representations.
- Microstructures.
- RVE configurations.
- Atomic and molecular structures.
- Calculation configurations.
- Calculation results.
- Derived properties.
- Provenance.
- Uncertainty.
- Validation state.
- Parent-child relationships.
- Revision dependencies.

Implement dependency tracking.

If a source representation changes, identify downstream results that may be invalidated.

Do not silently reuse stale properties.

Preserve existing project files and migration compatibility.

A project should remain understandable even when an optional external solver is unavailable on another machine.

---

# 20. Scientific Validation and Benchmarking

Scientific validation is mandatory.

Build tests that verify the calculations, not merely that the software executes.

## 20.1 Architecture Tests

Verify:

- Each scale has a distinct representation.
- Relationships between scales are preserved.
- Material lineage survives save and reload.
- Derived properties retain provenance.
- Source revisions invalidate dependent results appropriately.
- Unsupported scale transitions are rejected or clearly identified.

## 20.2 Microstructure and Homogenization Tests

Use simple benchmark structures with known or independently verified behavior.

Examples:

- A homogeneous RVE should recover the constituent property under appropriate conditions.
- A simple layered composite should reproduce the expected directional difference in effective response.
- A porous material should demonstrate the expected influence of porosity under a defined model.
- RVE results should be examined for convergence with increasing size where appropriate.

Use analytical solutions or independently verified reference results where possible.

## 20.3 Atomistic Tests

Where an atomistic engine is integrated, include a small benchmark that validates:

- Structure import.
- Energy minimization.
- Stability.
- Energy behavior.
- A selected physical observable.
- Sensitivity to relevant numerical parameters.

## 20.4 Quantum Tests

Only where a genuine quantum solver is integrated, include a small benchmark with documented reference results.

Verify calculation configuration, convergence, and extracted quantities.

Do not make quantum benchmarks mandatory for an environment that does not support quantum calculations.

## 20.5 Integration Tests

Verify that derived material properties can be consumed by existing engineering and CEME systems.

At least one end-to-end test must demonstrate:

**Source representation → Calculation → Derived property → Higher-scale material → Engineering simulation**

---

# 21. Implementation Strategy

Implement this capability in deliberate phases.

Do not attempt to build every scale and every solver integration simultaneously.

## Phase 1 — Architecture Audit and Data Model

- Audit the existing architecture.
- Establish the multiscale representation hierarchy.
- Define material lineage.
- Define model cards.
- Define derived-property records.
- Define uncertainty and validation metadata.
- Establish revision and dependency tracking.

## Phase 2 — Microstructure and RVE

- Implement microstructure representations.
- Implement RVE geometry and configuration.
- Support multiple constituent phases.
- Implement an appropriate homogenization pathway.
- Validate against simple reference cases.

This phase should deliver the first substantial new scientific capability.

## Phase 3 — Atomistic and Molecular Integration

- Evaluate available scientific engines.
- Select a practical integration.
- Implement structure import and validation.
- Implement solver configuration and execution.
- Extract at least one useful physical property.
- Preserve full calculation provenance.

If no suitable solver is available, implement the integration contract and clearly document the limitation rather than creating a fake simulation.

## Phase 4 — Electronic and Quantum Integration

- Evaluate the available environment.
- Select a suitable electronic-structure integration if justified.
- Implement configuration, execution, and result extraction.
- Establish convergence reporting.
- Preserve scientific assumptions and limitations.

This phase may be deferred if the environment cannot support it.

## Phase 5 — Property Upscaling and Downscaling

- Implement property derivation.
- Implement scale-transfer contracts.
- Implement uncertainty preservation.
- Implement local-state extraction and mapping interfaces.
- Connect derived properties to higher-scale materials.

## Phase 6 — CEME and Engineering Integration

- Make derived properties available to CEME.
- Integrate with existing material assignments.
- Connect to geometry-driven engineering.
- Connect to resonance and metamaterial analysis.
- Verify that changes in material lineage are reflected in dependent engineering models.

## Phase 7 — User Experience and Automation

- Implement material scale navigation.
- Implement structural visualization.
- Implement property comparison.
- Implement provenance navigation.
- Expose supported operations through existing automation.
- Add example projects and guided workflows.

## Phase 8 — Validation and Documentation

- Run the complete test suite.
- Validate all implemented scientific pathways.
- Benchmark performance.
- Document supported scales and limitations.
- Produce an engineering completion report.

---

# 22. Explicit Scientific Guardrails

These rules are mandatory.

1. Do not describe a continuum material calculation as atomistic simulation.
2. Do not describe a molecular dynamics calculation as quantum simulation.
3. Do not describe database retrieval as a first-principles calculation.
4. Do not fabricate atomic structures, force-field parameters, quantum results, or material properties.
5. Do not claim a solver integration is functional until it has executed a real calculation.
6. Do not treat numerical convergence as proof of experimental accuracy.
7. Do not assume that effective material properties are independent of frequency, temperature, direction, or microstructure.
8. Do not assume that every material can be represented by a single isotropic scalar property.
9. Do not transfer properties between scales without documenting the physical assumptions.
10. Do not discard source lineage when creating an effective or derived material.
11. Do not silently substitute an approximate calculation when the requested method is unavailable.
12. Do not create an artificial universal multiscale equation that replaces domain-specific physics.
13. Do not require all modeling scales to be active for every engineering simulation.
14. Do not allow stale derived properties to appear current.
15. Do not claim scientific validation without a documented benchmark or experimental comparison.

A calculation may be useful without being definitive. RESONANT must communicate that distinction honestly.

---

# 23. Acceptance Criteria

Prompt 22 is complete when the following conditions are satisfied.

### Architecture

- [ ] Multiscale material representations exist as distinct, connected entities.
- [ ] Material lineage is preserved across all implemented scales.
- [ ] Derived properties retain their calculation provenance.
- [ ] Model assumptions and validity information are stored.
- [ ] Revision and dependency tracking work.

### Microstructure

- [ ] Microstructure can represent multiple constituent phases.
- [ ] An RVE can be constructed or imported.
- [ ] At least one effective property can be calculated through a genuine numerical or analytical method.
- [ ] At least one result is compared against a reference or independently verified expectation.
- [ ] RVE sensitivity or convergence is investigated.

### Lower-Scale Integration

- [ ] At least one genuine atomistic or molecular integration is operational, if supported by the environment.
- [ ] If unavailable, the missing capability and its integration requirements are explicitly documented.
- [ ] No unsupported simulation capability is presented as implemented.
- [ ] Electronic or quantum integration is operational only if a real solver has been integrated and validated.

### Property Derivation

- [ ] At least one complete property derivation pathway exists.
- [ ] Derived properties can be assigned to higher-scale material representations.
- [ ] Provenance and uncertainty are preserved.
- [ ] Stale dependencies can be identified.

### RESONANT Integration

- [ ] Derived materials can be used by existing engineering workflows.
- [ ] CEME can consume the implemented material properties.
- [ ] Integration with geometry, resonance, and metamaterial systems is established where relevant.
- [ ] Existing projects remain compatible.

### User Experience and Automation

- [ ] Users can inspect material representations and their relationships.
- [ ] Users can identify the origin of a derived property.
- [ ] Supported multiscale operations are accessible through the existing automation architecture.
- [ ] Example projects demonstrate the implemented workflow.

### Quality

- [ ] Scientific benchmark tests pass.
- [ ] Existing regression tests pass.
- [ ] Documentation identifies supported capabilities and limitations.
- [ ] No fabricated scientific capabilities or results are present.

**Important:** Acceptance does not require every scale to be fully implemented. It requires a genuine, validated multiscale architecture and at least one complete cross-scale pathway. Unsupported scales must be explicitly identified as future integrations.

---

# 24. Final Engineering Report

When implementation is complete, provide a detailed report containing:

1. Architecture audit findings.
2. Existing systems reused.
3. New data structures and interfaces.
4. Implemented modeling scales.
5. External solvers integrated.
6. Scientific methods implemented.
7. Property derivation pathways.
8. Homogenization and RVE capabilities.
9. Lineage and uncertainty implementation.
10. CEME and engineering integration.
11. GUI and automation changes.
12. Scientific benchmarks and results.
13. Performance and resource observations.
14. Test results.
15. Known limitations.
16. Deferred capabilities.
17. Recommended next steps.

Clearly distinguish completed implementation from planned architecture.

Include relevant source files, modules, APIs, and tests.

---

# Final Directive

The purpose of this work is to make RESONANT capable of connecting engineering behavior to the physical organization of matter.

A material should no longer be treated merely as a collection of engineering constants.

It should be possible to investigate how those properties emerge from its structure, how that structure influences energy and matter interactions, and how the resulting behavior manifests in an engineered system.

The architecture must allow an engineer to move between scales without losing physical meaning, calculation provenance, or scientific accountability.

**We are building a system that can investigate not only how a material behaves, but why it behaves that way.**

Implement this as a cohesive extension of RESONANT, not as a separate scientific application.

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: - (heuristic)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Implement, ability to, build an , capabilities
  - evidence: 'Implement' [head]; 'capability' [head]; 'ability to' [head]; 'objective' [head]; 'must be able' [head]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: User: 'add this prompt and perform it in order' (queued after REQ-CHAT-20261126-019)

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

User: 'add this prompt and perform it in order' (queued after REQ-CHAT-20261126-019)

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000021
- Evidence: pending
