# Request: # FOUNDATIONAL ARCHITECTURE PROMPT — FUNCTIONALLY ARCHITECTED MATTER AND EMERGEN

**Request ID:** REQ-CHAT-20261127-001
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-27T00:16:34-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # FOUNDATIONAL ARCHITECTURE PROMPT — FUNCTIONALLY ARCHITECTED MATTER AND EMERGENT PHYSICAL SYSTEMS

## Project: RESONANT — Wave Field Engineering Studio

## Initiative: Functionally Architected Matter (FAM)

## Mission

Establish a foundational architectural capability within RESONANT that fundamentally expands how the platform conceives, represents, designs, analyzes, and optimizes physical matter.

RESONANT must evolve beyond designing objects from selected materials and predetermined geometries.

It must support the design of **functionally architected physical systems in which atomic composition, material phases, microstructure, interfaces, spatial organization, geometry, and operating conditions are deliberately coordinated to produce desired physical behavior.**

The central principle is:

**Matter is not merely the substance from which an engineered object is made. The organization of matter is itself an engineering variable.**

A material's composition, crystal structure, orientation, interfaces, internal architecture, and macroscopic geometry can work together to produce physical properties and functions that are unavailable from the constituent materials in isolation.

RESONANT must provide an architectural foundation for designing these systems.

This initiative is not limited to energy harvesting, metamaterials, smart materials, or conventional composites.

It encompasses the broader engineering of physical behavior through the deliberate organization of matter.

The objective is to establish a platform capable of discovering and engineering structures whose intended functions are inherent in their physical construction.

---

# 1. FOUNDATIONAL DESIGN PHILOSOPHY

Establish Functionally Architected Matter as a first-class architectural concept within RESONANT.

The platform must recognize that the physical behavior of an engineered system emerges from interacting variables across multiple scales.

These include:

1. Elemental composition.
2. Atomic bonding and electronic structure.
3. Molecular organization.
4. Crystal structure and orientation.
5. Material phases and phase boundaries.
6. Grain structure, defects, and microstructure.
7. Nanoscale features and interfaces.
8. Material layers and functional regions.
9. Mesoscale architecture and periodic structures.
10. Macroscopic geometry and topology.
11. Environmental and operational conditions.
12. Dynamic material state and history.

These variables must not be treated as independent design categories.

Their interactions determine the effective behavior of the complete physical system.

For example:

- Crystal orientation influences piezoelectric response.
- Microstructure influences mechanical strength and electrical conductivity.
- Interface properties influence stress transfer and charge transport.
- Layer thickness influences mechanical resonance and thermal resistance.
- Geometric topology influences wave propagation and energy localization.
- Electrical loading influences electromechanical damping.
- Temperature influences material properties and interface behavior.

RESONANT must establish the infrastructure to represent and investigate these dependencies.

## 1.1 Function is embedded in physical organization

The intended function of a structure should influence its material composition, architecture, and geometry from the beginning of the design process.

Do not assume that an engineer must first select a conventional material and then construct a shape from it.

Instead, support a function-driven approach:

Desired physical outcome → candidate physical mechanisms → material selection → material architecture → interface design → geometry → coupled simulation → optimization → manufacturing → experimental validation.

The design process must also support the reverse direction.

An engineer should be able to begin with an existing material system or structure and investigate what functions it can perform.

## 1.2 Material properties are not necessarily additive

Do not assume that a combination of materials will behave as a simple weighted average of their individual properties.

Interfaces, topology, anisotropy, periodicity, phase interactions, and scale-dependent effects can produce emergent effective behavior.

RESONANT must distinguish:

- Intrinsic constituent properties.
- Interface properties.
- Effective properties of composite structures.
- Geometry-dependent properties.
- Frequency-dependent properties.
- Temperature-dependent properties.
- Nonlinear and state-dependent properties.
- Emergent behavior resulting from coupled interactions.

## 1.3 Energy transformation is a central application

The architecture must explicitly support structures designed to transform energy between physical domains.

Examples include:

- Mechanical → electrical.
- Acoustic → mechanical → electrical.
- Thermal → electrical.
- Electrical → mechanical.
- Magnetic → mechanical.
- Optical → electrical.
- Mechanical → thermal.
- Electrical → optical.
- Fluid → mechanical → electrical.
- Electromagnetic → mechanical.
- Chemical → electrical.

Support multistage and cascaded transformations.

The architecture must not assume that every conversion mechanism is resonant. Resonance is an important design mechanism where applicable, but thermal conduction, chemical reactions, static field effects, and other physical processes require their appropriate governing models.

Every transformation must obey conservation laws and account for conversion losses.

---

# 2. ESTABLISH A UNIFIED MATERIAL ARCHITECTURE MODEL

Create a first-class data model for functionally architected matter.

This model must describe an engineered material system as a hierarchical physical object rather than a flat list of materials.

The architecture should support the following entities.

## 2.1 Composition

Represent the constituent materials and their composition.

Support:

- Elements.
- Compounds.
- Alloys.
- Ceramics.
- Polymers.
- Semiconductors.
- Dielectrics.
- Conductors.
- Magnetic materials.
- Piezoelectric materials.
- Ferroelectric materials.
- Magnetostrictive materials.
- Thermoelectric materials.
- Photonic and optical materials.
- Composite constituents.
- Novel and experimentally characterized materials.

Represent composition at the appropriate level of detail.

Do not require atomic-level composition for every engineering problem.

A conventional engineering material can be represented through experimentally characterized bulk properties, while advanced investigations can use more detailed structural descriptions.

## 2.2 Material phases

Support multiple material phases within a single engineered system.

Represent:

- Phase identity.
- Phase fraction.
- Spatial distribution.
- Phase boundaries.
- Phase transitions.
- Temperature dependence.
- Pressure dependence.
- Field dependence.
- Metastable states where supported.

Connect phase behavior to the Dynamic Material State architecture.

## 2.3 Crystalline and microstructural organization

Provide a representation for:

- Crystal structures.
- Lattice parameters.
- Crystal orientation.
- Grain boundaries.
- Grain size distributions.
- Defects.
- Dislocations.
- Vacancies.
- Inclusions.
- Porosity.
- Domain structures.
- Anisotropic properties.
- Texture and orientation distributions.

The architecture must allow material properties to depend on microstructural organization.

It must not pretend that all such properties are available from a simple material database lookup.

## 2.4 Nanoscale and molecular architecture

Establish extensible support for:

- Thin films.
- Nanoparticles.
- Nanowires.
- Nanotubes.
- Molecular assemblies.
- Layered nanoscale structures.
- Surface functionalization.
- Quantum-confined structures.
- Molecular orientation.
- Nanoscale inclusions.
- Surface and interface effects.

These representations should connect to appropriate specialized simulation models.

Do not attempt to solve every nanoscale problem through the continuum solver.

## 2.5 Functional regions

Allow a single physical object to contain multiple spatially defined functional regions.

Each region must be able to specify:

- Geometry or spatial extent.
- Material composition.
- Material phase.
- Microstructural properties.
- Orientation.
- Anisotropy.
- Functional role.
- Operating conditions.
- Interfaces with adjacent regions.
- Manufacturing method.
- Property uncertainty.

A functional region may be a homogeneous material, a composite, a graded material, a patterned film, or a complex internal architecture.

## 2.6 Spatially varying material properties

Support continuous and discontinuous material-property distributions.

Examples:

- Gradually changing composition.
- Functionally graded materials.
- Spatially varying elastic modulus.
- Spatially varying thermal conductivity.
- Spatially varying permittivity.
- Magnetic permeability distributions.
- Piezoelectric coefficient distributions.
- Density distributions.
- Anisotropic orientation fields.

The underlying data model must be extensible enough to support field-based property descriptions rather than relying exclusively on discrete material assignments.

---

# 3. INTERFACE ENGINEERING — A FIRST-CLASS PHYSICAL DOMAIN

This is a particularly important part of the initiative.

RESONANT must recognize that interfaces between materials can determine the performance of the complete structure.

An interface is not merely a geometric boundary.

It can have its own mechanical, electrical, thermal, magnetic, chemical, and structural behavior.

Create an Interface Engineering subsystem.

## 3.1 Interface properties

Support representations of:

- Mechanical bonding.
- Adhesion.
- Interfacial stiffness.
- Thermal boundary resistance.
- Electrical contact resistance.
- Charge transfer.
- Dielectric boundary behavior.
- Diffusion.
- Interfacial reactions.
- Surface energy.
- Friction.
- Delamination.
- Residual stress.
- Interfacial defects.
- Chemical compatibility.

## 3.2 Interface behavior

Allow different interface models depending on the physical problem.

Examples:

- Perfect mechanical bonding.
- Finite-stiffness bonding.
- Cohesive-zone behavior.
- Frictional contact.
- Thermal contact resistance.
- Electrical contact resistance.
- Dielectric interfaces.
- Charge-transfer interfaces.
- Diffusive interfaces.
- Reactive interfaces.

Interface models must be selected based on physical assumptions and solver capabilities.

## 3.3 Interface optimization

Allow RESONANT to investigate whether changing an interface improves the overall physical function.

For example:

- Improve mechanical strain transfer into a piezoelectric film.
- Reduce thermal leakage between functional regions.
- Improve electrical charge collection.
- Prevent unwanted charge leakage.
- Reduce mechanical delamination.
- Control interfacial damping.
- Improve compatibility between materials with different thermal expansion coefficients.

Interface design must participate in inverse-design and multi-objective optimization.

---

# 4. MATERIAL ARCHITECTURE AND SPATIAL ORGANIZATION

Develop a system that allows engineers to construct and modify complex material architectures.

Support more than conventional flat layers.

## 4.1 Layered structures

Represent:

- Multilayer stacks.
- Thin-film assemblies.
- Alternating material layers.
- Conductive/dielectric stacks.
- Piezoelectric/electrode assemblies.
- Thermal barrier layers.
- Magnetic/electrical functional layers.
- Protective and encapsulation layers.

Each layer must have independent material properties, thickness, orientation, and interface definitions.

## 4.2 Patterned structures

Support:

- Periodic patterns.
- Aperiodic patterns.
- Locally patterned regions.
- Segmented electrodes.
- Embedded material regions.
- Spatially varying inclusions.
- Resonator arrays.
- Defect structures.
- Gradient architectures.

## 4.3 Three-dimensional material architectures

Support complex internal structures such as:

- Lattices.
- Cellular materials.
- Porous structures.
- Interlocking material regions.
- Embedded channels.
- Internal resonators.
- Periodic metamaterial structures.
- Hierarchical architectures.
- Architected composites.

Material architecture must be represented in three dimensions and must remain compatible with the existing geometry engine.

## 4.4 Functionally graded materials

Support gradual transitions between compositions and properties.

For example:

- Metal-to-ceramic transitions.
- Stiffness gradients.
- Thermal conductivity gradients.
- Electrical conductivity gradients.
- Magnetic property gradients.
- Density gradients.

The architecture must distinguish physically realizable graded materials from idealized mathematical distributions.

---

# 5. MULTISCALE PROPERTY ENGINE

Integrate Functionally Architected Matter with the existing Multiscale Material Modeling initiative.

Establish a hierarchical model of how material structure determines effective physical behavior.

The architecture should support connections between:

Atomic and electronic scale:
- Electronic structure.
- Bonding.
- Charge distribution.
- Polarization.
- Magnetic moments.
- Atomic-scale interactions.

Molecular scale:
- Molecular structure.
- Molecular orientation.
- Molecular dynamics.
- Chemical interactions.

Crystalline scale:
- Crystal structure.
- Lattice behavior.
- Defects.
- Domains.
- Orientation.

Microstructural scale:
- Grain structure.
- Porosity.
- Inclusions.
- Phase distribution.
- Grain boundaries.

Mesoscale:
- Layers.
- Fibers.
- Lattices.
- Resonators.
- Interfaces.
- Periodic arrangements.

Continuum scale:
- Effective constitutive behavior.
- Mechanical response.
- Electrical response.
- Thermal response.
- Magnetic response.
- Optical response.

Device scale:
- Complete structural and functional behavior.
- Energy transformation.
- Resonance.
- Operating performance.
- Reliability.

## 5.1 Scale-to-scale property transfer

Create an extensible architecture for transferring information between models at different scales.

Examples:

- Atomic-scale calculations produce material parameters.
- Microstructural models determine effective stiffness or conductivity.
- Effective properties feed continuum simulations.
- Device simulations determine system performance.
- Experimental results update material and interface parameters.

The transfer must preserve units, coordinate systems, assumptions, uncertainty, and provenance.

## 5.2 Homogenization and effective properties

Support appropriate methods for deriving effective material properties from heterogeneous architectures.

Potential methods include:

- Analytical homogenization.
- Numerical homogenization.
- Representative volume elements.
- Periodic boundary conditions.
- Multiscale finite-element methods.
- Reduced-order constitutive models.

Do not treat homogenized properties as universally valid.

Record the length scales, frequency ranges, loading conditions, and assumptions under which they are applicable.

## 5.3 Specialized atomic and molecular methods

Establish interfaces for future or existing specialized methods such as:

- Density Functional Theory.
- Molecular Dynamics.
- Tight-binding methods.
- Atomistic simulations.
- Phase-field methods.
- Crystal plasticity.
- Mesoscale modeling.

Do not implement placeholder solvers that claim to provide these capabilities without actually performing the required physics.

The architecture should permit external computational backends and specialized solver integrations.

---

# 6. EMERGENT PHYSICAL FUNCTION

Establish the ability to identify and optimize collective behavior arising from material organization.

The system must distinguish between:

1. Properties of individual constituents.
2. Properties of interfaces.
3. Effective properties of the combined structure.
4. Emergent behavior arising from the architecture.

Investigate behaviors including:

- Negative effective Poisson's ratio.
- Tunable stiffness.
- Mechanical wave localization.
- Phononic band gaps.
- Photonic band gaps.
- Effective anisotropy.
- Nonlinear response.
- Coupled resonances.
- Energy localization.
- Enhanced electromechanical coupling.
- Tunable thermal transport.
- Magnetic field concentration.
- Unusual dispersion behavior.
- Controlled damping.
- Adaptive material response.

These should be treated as possible physical phenomena, not guaranteed outcomes.

The system must identify the physical conditions under which an emergent property occurs.

It must also determine whether the property remains valid under realistic boundaries, losses, manufacturing tolerances, and operating conditions.

## 6.1 Discovering useful combinations

Create a framework for exploring combinations of materials and architectures that may produce useful behavior.

For example:

A structure may combine:

- A high-strain piezoelectric region.
- A compliant mechanical region.
- A conductive collection region.
- A dielectric isolation region.
- A resonant mechanical architecture.

The purpose is not simply to stack materials.

It is to engineer the interactions between them.

RESONANT should be capable of evaluating alternative arrangements and discovering which combinations produce the desired physical response.

---

# 7. FUNCTION-DRIVEN MATTER DESIGN AND INVERSE DESIGN

Integrate this initiative with the existing Function-Driven Inverse Design architecture.

Extend inverse design beyond geometry and conventional material selection.

The engineer must be able to specify a desired physical function and allow RESONANT to investigate the complete material architecture.

## 7.1 Design inputs

Support requirements such as:

- Desired physical transformation.
- Input energy domain.
- Output energy domain.
- Operating frequency or frequency range.
- Operating temperature.
- Mechanical loading.
- Electrical loading.
- Magnetic field conditions.
- Optical wavelength.
- Pressure and environmental conditions.
- Physical dimensions.
- Weight limits.
- Strength requirements.
- Efficiency targets.
- Durability requirements.
- Manufacturing constraints.
- Cost constraints.
- Material availability.
- Safety requirements.

Not every problem requires every input.

## 7.2 Design variables

Allow inverse design to investigate:

- Material composition.
- Material selection.
- Phase selection.
- Crystal orientation.
- Microstructure.
- Grain size.
- Defect distributions.
- Layer thickness.
- Layer ordering.
- Interface properties.
- Spatial material distribution.
- Geometric topology.
- Resonator dimensions.
- Functional-region placement.
- Electrode arrangement.
- Operating conditions.

## 7.3 Optimization objectives

Support multiple simultaneous objectives.

Examples:

- Maximize useful energy conversion.
- Minimize dissipated energy.
- Maximize mechanical strength.
- Minimize mass.
- Maximize bandwidth.
- Optimize resonance.
- Maximize field localization.
- Minimize temperature rise.
- Maximize service life.
- Minimize manufacturing complexity.
- Maximize environmental stability.
- Minimize material cost.

Do not silently collapse conflicting objectives into a single score.

Provide Pareto-optimal candidate designs and expose trade-offs.

## 7.4 Design exploration

RESONANT should be able to:

1. Identify applicable physical mechanisms.
2. Identify candidate materials.
3. Propose material architectures.
4. Generate candidate geometries.
5. Evaluate interface configurations.
6. Determine required solver capabilities.
7. Simulate candidate designs.
8. Compare predicted behavior.
9. Identify uncertainties.
10. Rank candidates against engineering objectives.
11. Recommend experiments to distinguish competing candidates.

The system must distinguish between an optimized design and a validated design.

---

# 8. MULTIDOMAIN ENERGY TRANSFORMATION

Explicitly integrate Functionally Architected Matter with the Coupled Energy–Matter Engine.

The architecture must support structures that perform multiple physical transformations within a single engineered object.

Examples:

Mechanical vibration:
→ resonant mechanical structure
→ piezoelectric material
→ electrical energy.

Acoustic energy:
→ acoustic metamaterial
→ localized mechanical deformation
→ piezoelectric conversion
→ electrical storage.

Thermal gradient:
→ engineered thermal architecture
→ thermoelectric material
→ electrical energy.

Electrical energy:
→ electrode architecture
→ piezoelectric material
→ mechanical deformation.

Magnetic field:
→ magnetostrictive material
→ mechanical motion.

Optical energy:
→ photonic architecture
→ photovoltaic conversion
→ electrical energy.

Support cascaded transformations and multiple simultaneous conversion mechanisms.

For every transformation stage, account for:

- Input energy.
- Useful output energy.
- Stored energy.
- Dissipated energy.
- Conversion efficiency.
- Coupling efficiency.
- Uncertainty.
- Operating conditions.

The complete system must obey conservation laws.

No predicted enhancement may imply creation of energy.

## 8.1 Coupled and competing functions

Allow a structure to perform several functions simultaneously.

For example, one component might:

- Carry mechanical load.
- Suppress unwanted vibration.
- Harvest mechanical energy.
- Sense deformation.
- Monitor damage.
- Control thermal transport.

The architecture must support both beneficial and detrimental interactions between these functions.

An improvement in one objective must not be assumed to improve every other objective.

---

# 9. DYNAMIC AND ADAPTIVE MATERIAL ARCHITECTURES

Integrate with the existing Dynamic Material State and Nonlinear Matter architecture.

Functionally Architected Matter must support structures whose behavior changes during operation.

Investigate:

- Temperature-dependent material properties.
- Stress-dependent material properties.
- Field-dependent polarization.
- Magnetic state changes.
- Phase transitions.
- Ferroelectric domain switching.
- Shape-memory behavior.
- Electroactive materials.
- Magnetorheological behavior.
- Electrorheological behavior.
- Nonlinear stiffness.
- Tunable damping.
- Reconfigurable resonant structures.
- Hysteresis.
- Fatigue and degradation.

Distinguish between:

- Static functionally engineered matter.
- Passive adaptive matter.
- Externally controlled adaptive matter.
- Actively reconfigurable physical systems.

The architecture must permit future designs in which the physical structure changes its response to different operating conditions.

A material architecture should be able to perform different functions under different states without assuming that all transformations are reversible or instantaneous.

---

# 10. MANUFACTURING-AWARE MATERIAL ARCHITECTURE

Integrate with existing geometry, CAD, and manufacturing workflows.

A proposed material architecture must be evaluated against plausible manufacturing processes.

Support manufacturing descriptions including:

- Metal additive manufacturing.
- Polymer additive manufacturing.
- Ceramic additive manufacturing.
- Multi-material printing.
- Directed-energy deposition.
- Powder-bed fusion.
- Sputter deposition.
- Atomic layer deposition.
- Chemical vapor deposition.
- Physical vapor deposition.
- Thin-film processing.
- Lithography.
- Patterned electrode fabrication.
- Lamination.
- Diffusion bonding.
- Sintering.
- Heat treatment.
- Machining and finishing.
- Composite fabrication.

## 10.1 Manufacturing constraints

Represent:

- Minimum feature size.
- Minimum layer thickness.
- Material compatibility.
- Deposition limitations.
- Thermal processing limitations.
- Surface roughness.
- Residual stress.
- Porosity.
- Anisotropy.
- Material availability.
- Process repeatability.
- Post-processing requirements.
- Geometric accessibility.
- Interface reliability.

## 10.2 Manufacturing-aware optimization

Inverse design must be capable of rejecting or penalizing architectures that cannot reasonably be manufactured.

Where appropriate, propose alternative architectures that retain the desired physical behavior while simplifying fabrication.

Do not confuse theoretical material arrangements with manufacturable designs.

## 10.3 Manufacturing provenance

The material architecture must preserve a record of:

- Material batch.
- Composition.
- Fabrication process.
- Processing parameters.
- Deposition conditions.
- Heat-treatment history.
- Layer sequence.
- Interface preparation.
- Post-processing.
- Inspection results.
- Measured material properties.

Manufacturing history can materially influence physical behavior and must be available to simulation and validation workflows.

---

# 11. MATERIAL ARCHITECTURE DESIGN ENVIRONMENT

Develop the conceptual and technical foundation for a dedicated Functionally Architected Matter design environment.

It must integrate with the existing RESONANT interface rather than create an isolated application.

The engineer should be able to:

1. Create or import a geometry.
2. Divide the object into functional regions.
3. Assign material compositions and properties.
4. Define layered and graded regions.
5. Define interfaces.
6. Specify orientations and anisotropy.
7. Define internal structures.
8. Associate physical functions with regions.
9. Inspect material architecture at different scales.
10. Run applicable simulations.
11. Explore design alternatives.
12. Compare candidate architectures.
13. Export manufacturing and analysis data.

## 11.1 Visualization

Provide appropriate visualization for:

- Material composition maps.
- Functional-region maps.
- Layer stacks.
- Interface boundaries.
- Crystal orientation fields.
- Microstructure.
- Spatial property distributions.
- Strain and stress fields.
- Electric and magnetic fields.
- Thermal fields.
- Wave propagation.
- Resonant modes.
- Energy-flow paths.
- Material-state changes.

Support scale-aware visualization.

An engineer should be able to move conceptually between the overall device and its internal material architecture without losing the relationship between scales.

Do not present illustrative microstructures or atomic structures as experimentally validated physical representations.

Every visualization must identify whether it is:

- Actual material data.
- A computed result.
- An idealized model.
- A schematic representation.
- An illustrative visualization.

---

# 12. MATERIAL KNOWLEDGE AND PROPERTY PROVENANCE

Create an extensible material-property infrastructure.

Material data must support:

- Source and provenance.
- Units.
- Temperature dependence.
- Frequency dependence.
- Anisotropy.
- Nonlinearity.
- Validity ranges.
- Measurement conditions.
- Uncertainty.
- Experimental versus theoretical origin.
- Material batch and processing history where available.

Distinguish clearly between:

1. Experimentally measured properties.
2. Manufacturer-published properties.
3. Literature-derived properties.
4. First-principles predictions.
5. Molecular or microstructural simulation results.
6. Estimated properties.
7. User-defined assumptions.

Do not silently substitute missing properties with generic values.

If a simulation requires unavailable material parameters, identify them and explain the consequences of using estimates.

Allow material-property datasets to be updated through experimental characterization.

Integrate with the Experimental Energy–Matter Laboratory.

Experimental measurements should be capable of improving material models, interface models, and effective-property predictions.

---

# 13. SCIENTIFIC VALIDATION AND UNCERTAINTY

Scientific integrity is mandatory.

Functionally Architected Matter must not become a mechanism for producing attractive but physically unsupported designs.

Implement explicit validation requirements.

## 13.1 Model validity

Every physical model must identify:

- Governing equations.
- Material assumptions.
- Boundary conditions.
- Validity range.
- Relevant length and time scales.
- Numerical limitations.
- Coupling assumptions.
- Known omissions.

## 13.2 Uncertainty

Represent uncertainty in:

- Material composition.
- Material properties.
- Interface properties.
- Geometry.
- Manufacturing tolerances.
- Microstructure.
- Environmental conditions.
- Solver parameters.
- Experimental measurements.

Propagate uncertainty where technically feasible.

## 13.3 Independent validation

Distinguish:

- Calibration data.
- Model development data.
- Independent validation data.
- Experimental verification.

Do not treat a model as validated simply because it reproduces the data used to calibrate it.

## 13.4 Physical consistency

Enforce appropriate:

- Conservation of mass.
- Conservation of momentum.
- Conservation of energy.
- Charge conservation.
- Thermodynamic consistency.
- Constitutive stability.
- Boundary-condition consistency.

Reject or flag physically inconsistent designs and simulations.

---

# 14. SOFTWARE ARCHITECTURE AND INTEGRATION

This initiative must extend RESONANT's existing architecture, not create a parallel application.

Before implementing anything, perform a comprehensive repository audit.

Examine:

- Existing Shape–Function–Energy architecture.
- Coupled Energy–Matter Engine.
- Geometry engine.
- Material database.
- Multiscale Material Modeling.
- Metamaterials and engineered effective properties.
- Dynamic Material State and Nonlinear Matter.
- Conservation and energy accounting.
- Function-Driven Inverse Design.
- Experimental Energy–Matter Laboratory.
- Existing simulation backends.
- CAD and manufacturing integration.
- Serialization and project persistence.
- Visualization infrastructure.
- AI and orchestration capabilities.

Read all relevant architecture documents, project guidance, implementation plans, and existing tests.

Identify existing capabilities that should be extended.

Do not duplicate existing functionality.

## 14.1 Core architectural components

Establish appropriate modular services or domain components for:

- MaterialCompositionModel.
- MaterialPhaseModel.
- MicrostructureModel.
- FunctionalRegionModel.
- MaterialArchitectureModel.
- InterfaceModel.
- SpatialPropertyField.
- MultiscalePropertyTransfer.
- EffectivePropertyModel.
- MaterialFunctionModel.
- EnergyTransformationPathway.
- ManufacturingConstraintModel.
- MaterialArchitectureOptimizer.
- ArchitectureValidationService.
- MaterialProvenanceService.

These are conceptual responsibilities, not mandatory class names.

Follow existing naming, layering, dependency-injection, and architectural conventions.

Avoid building an unnecessarily fragmented service architecture.

## 14.2 Shared data representation

Establish a canonical representation of functionally architected matter that can be consumed by:

- Geometry and CAD systems.
- Material-property systems.
- CEME.
- Structural solvers.
- Acoustic solvers.
- Electromagnetic solvers.
- Thermal solvers.
- Multiscale models.
- Inverse-design systems.
- Manufacturing systems.
- Experimental validation systems.

Preserve consistent identities, references, units, coordinate systems, and provenance.

## 14.3 Plugin and solver extensibility

Do not hardwire the architecture to a single simulation backend.

Provide extensible interfaces for specialized physical models.

A solver must explicitly declare:

- Supported physics.
- Supported material models.
- Supported interface models.
- Spatial dimensionality.
- Relevant scales.
- Coupling capabilities.
- Supported nonlinearities.
- Numerical limitations.
- Input requirements.
- Output quantities.

The architecture must not imply that a solver supports a coupling simply because its input data structures can represent the relevant fields.

---

# 15. AI-ASSISTED MATERIAL ARCHITECTURE DISCOVERY

Establish the foundation for AI-assisted exploration and design.

AI should help engineers investigate candidate physical systems, not replace physical modeling.

Potential capabilities:

- Suggest candidate material combinations.
- Identify relevant conversion mechanisms.
- Recommend promising interface architectures.
- Propose spatial material arrangements.
- Identify potentially useful resonant configurations.
- Explain competing physical effects.
- Generate candidate optimization objectives.
- Recommend simulation sequences.
- Identify missing material parameters.
- Recommend experiments that discriminate between competing hypotheses.
- Summarize physical trade-offs.

AI-generated designs must be treated as hypotheses until evaluated through appropriate physical models and experiments.

The system must explain why a proposed combination might work.

It must also be capable of explaining why a seemingly attractive combination may fail.

Examples include:

- Poor mechanical coupling.
- Electrical leakage.
- Excessive dielectric loss.
- Thermal incompatibility.
- Unwanted damping.
- Opposing polarization.
- Delamination.
- Unstable material states.
- Manufacturing incompatibility.

Preserve the distinction between established science, plausible hypotheses, computational predictions, and experimental findings.

---

# 16. SIGNATURE APPLICATION: THE FUNCTIONALLY ARCHITECTED ENERGY CONVERTER

Use the integrated piezoelectric bell concept as an architectural reference example, not as the sole purpose of this initiative.

The example should demonstrate how the new architecture can represent a complete physical object whose function emerges from its composition, layers, interfaces, and geometry.

Example:

A metal structure is additively manufactured with geometry optimized for a desired vibration spectrum.

Selected regions receive functional material layers.

These may include:

- An adhesion or transition layer.
- A piezoelectric film.
- Patterned electrodes.
- Dielectric isolation.
- Protective encapsulation.

The complete assembly is optimized for:

- Structural behavior.
- Resonance.
- Acoustic radiation.
- Surface strain.
- Piezoelectric conversion.
- Electrical collection.
- Mechanical durability.
- Manufacturing feasibility.

The model must account for the effect of the deposited layers on the structural behavior.

It must not calculate the bare metal structure and assume that the addition of functional layers leaves its behavior unchanged.

The same architecture should subsequently be reusable for other examples, including:

- Vibration-harvesting structures.
- Thermoelectric architectures.
- Photonic energy-conversion systems.
- Magnetostrictive structures.
- Adaptive mechanical systems.
- Multifunctional load-bearing materials.

The bell is a validation example for the architecture, not a hardcoded special case.

---

# 17. USER EXPERIENCE AND DESIGN WORKFLOW

The new capability must feel like a natural extension of RESONANT.

The engineer should be able to approach the system from two directions.

### Direction A: Design from function

"I want a structure that converts mechanical vibration into electricity while retaining a specified load-bearing capability."

RESONANT helps discover the mechanisms, materials, architecture, geometry, and operating conditions.

### Direction B: Explore an existing structure

"I have this material architecture. What can it do?"

RESONANT analyzes its composition, internal structure, interfaces, geometry, and physical properties to identify possible functions and limitations.

Support iterative exploration.

The engineer must remain in control of assumptions, design variables, material selection, and optimization objectives.

The system should expose physical reasoning rather than hiding decisions behind a black-box recommendation.

---

# 18. IMPLEMENTATION STRATEGY

This prompt establishes a foundational architecture. It does not authorize a superficial implementation of every advanced physical capability simultaneously.

Proceed in deliberate stages.

## Phase 0 — Repository and architecture audit

- Inspect the complete existing implementation.
- Identify reusable components.
- Map existing material, geometry, and physics representations.
- Identify architectural gaps.
- Review the previous engineering prompts and their implementation status.
- Document integration requirements.
- Establish a dependency map.

Do not begin by replacing existing systems.

## Phase 1 — Canonical Functionally Architected Matter model

Implement the foundational data representation.

Establish:

- Hierarchical material composition.
- Functional regions.
- Layered structures.
- Material assignments.
- Spatially varying properties.
- Interfaces.
- Anisotropy and orientation.
- Material provenance.
- Serialization and persistence.

Provide tests for creation, editing, serialization, loading, and round-trip preservation.

## Phase 2 — Interface and material architecture infrastructure

Implement the interface representation and initial supported interface models.

Provide:

- Layered-material support.
- Region adjacency.
- Interface properties.
- Material orientation.
- Spatial property fields.
- Basic validation.
- Geometry association.

Ensure the model can represent a multimaterial physical structure without requiring a specific solver.

## Phase 3 — Multiscale integration

Integrate with existing material and multiscale systems.

Establish:

- Property transfer interfaces.
- Effective-property records.
- Scale and validity metadata.
- Uncertainty and provenance.
- Backend extensibility.

Implement only scale-transfer capabilities supported by existing models or validated new work.

## Phase 4 — CEME and multiphysics integration

Integrate material architecture with the coupled physical engine.

Ensure that functional regions and interfaces can participate in appropriate simulations.

Implement explicit coupling declarations and validation.

Do not claim complete multiphysics coupling until it exists and has been tested.

## Phase 5 — Inverse-design integration

Extend existing inverse design to include material architecture variables.

Start with manageable optimization problems involving:

- Layer thickness.
- Material selection.
- Region placement.
- Geometry.
- Interface parameters.
- Resonance.

Demonstrate multi-objective optimization and trade-off reporting.

## Phase 6 — Visualization and design environment

Implement the user-facing material architecture environment.

Support editing, inspection, visualization, simulation preparation, and persistence.

## Phase 7 — Experimental and manufacturing integration

Connect the architecture to experimental characterization and manufacturing constraints.

Ensure that measured properties and actual manufacturing history can inform future simulations.

## Phase 8 — Advanced discovery

Establish extension points for:

- Advanced atomistic modeling.
- Microstructure generation.
- Advanced interface physics.
- AI-assisted architecture discovery.
- Adaptive and reconfigurable matter.
- More sophisticated multiscale optimization.

Do not simulate capabilities that have not been implemented.

---

# 19. TESTING AND ACCEPTANCE CRITERIA

The initiative is not complete simply because a material architecture can be displayed in the GUI.

It must function as an engineering representation.

## Data model tests

- Represent a conventional homogeneous material.
- Represent a multilayer material.
- Represent a functionally graded material.
- Represent a multimaterial 3D structure.
- Represent spatially varying properties.
- Represent material orientation.
- Represent interfaces.
- Preserve property provenance.
- Serialize and reload complete architectures without information loss.

## Integration tests

- Associate material regions with existing geometry.
- Pass appropriate properties to existing solvers.
- Preserve units and coordinate systems.
- Validate unsupported solver configurations.
- Integrate with CEME.
- Integrate with inverse design.
- Integrate with experimental records.

## Physical modeling tests

Create at least one supported benchmark demonstrating that a change in material architecture changes predicted physical behavior.

Examples:

- A layered composite with different effective stiffness.
- A piezoelectric structure with altered electrode or material orientation.
- A multilayer thermal structure with different effective thermal transport.
- A periodic mechanical structure with a measurable frequency-dependent response.

Choose benchmarks that existing or newly implemented solvers can actually support.

Compare results against analytical solutions, established benchmarks, or experimental data where available.

## Optimization tests

Demonstrate that an optimization variable changes a physical result in the expected direction.

Demonstrate that competing objectives are represented honestly.

Demonstrate that infeasible or unsupported architectures are identified.

## User acceptance

An engineer must be able to:

1. Create a multimaterial physical structure.
2. Assign multiple functional regions.
3. Define layers and interfaces.
4. Inspect spatial material properties.
5. Prepare an appropriate simulation.
6. Evaluate physical behavior.
7. Compare architectural alternatives.
8. Preserve the complete design.
9. Reopen and continue the project without loss of information.

---

# 20. NON-NEGOTIABLE SCIENTIFIC AND ARCHITECTURAL RULES

1. Do not treat materials as interchangeable scalar property records.
2. Do not assume that constituent properties combine through simple averaging.
3. Do not treat interfaces as automatically perfect.
4. Do not confuse a theoretical architecture with a manufacturable design.
5. Do not claim atomistic accuracy from continuum approximations.
6. Do not invent missing material properties.
7. Do not hide uncertainty.
8. Do not confuse optimization with experimental validation.
9. Do not claim energy creation or violate conservation.
10. Do not assume resonance improves every physical system.
11. Do not assume a functional layer has negligible influence on its substrate.
12. Do not introduce unsupported solver capabilities.
13. Do not create a parallel architecture that duplicates existing RESONANT systems.
14. Do not sacrifice established functionality for the new initiative.
15. Preserve backward compatibility and existing project data.

Every new capability must be traceable to a physical model, material property, engineering assumption, or experimental observation.

---

# 21. REQUIRED FINAL DELIVERABLES

At completion, provide:

1. Architecture audit and existing-capability assessment.
2. Functionally Architected Matter architectural specification.
3. Canonical data model and relationship diagrams.
4. Interface Engineering design.
5. Multiscale integration specification.
6. CEME integration documentation.
7. Inverse-design integration documentation.
8. Manufacturing-awareness specification.
9. User-interface and workflow implementation.
10. Solver capability and limitation matrix.
11. Test suite and benchmark results.
12. Performance and scalability assessment.
13. Known limitations and outstanding work.
14. A phased roadmap for future advanced capabilities.

Identify every new module, service, model, interface, and major file.

Document all changes and provide evidence that existing functionality remains operational.

Do not report incomplete capabilities as complete.

---

# 22. THE LONG-TERM VISION

This initiative establishes a new level of physical engineering within RESONANT.

The ultimate objective is to allow engineers to design physical systems by specifying what matter should accomplish, rather than being restricted to choosing from conventional materials and shaping them into conventional components.

A structure may be designed to:

- Carry loads.
- Control waves.
- Concentrate energy.
- Transform energy.
- Generate electricity.
- Manage heat.
- Manipulate magnetic fields.
- Control light.
- Sense its environment.
- Adapt to changing conditions.
- Perform multiple functions simultaneously.

These functions can emerge from the coordinated behavior of its constituents, interfaces, internal architecture, and overall geometry.

The physical form is not merely a container for the function.

**The physical form, material organization, and function are inseparable parts of the same engineered system.**

RESONANT should ultimately become a platform capable of designing, predicting, optimizing, and experimentally validating these systems.

Establish the architecture accordingly.

# FINAL DIRECTIVE

Think beyond the immediate examples.

Do not design this initiative around the piezoelectric bell, energy harvesting, or any single material family.

The bell is merely an accessible demonstration of a much larger principle.

Build the foundational architecture for engineering matter itself.

Be ambitious in the architecture, rigorous in the physics, conservative in scientific claims, and disciplined in implementation.

The objective is not simply to make RESONANT better at designing objects.

**The objective is to make RESONANT capable of designing new physical systems whose functions are inherent in the way their matter is organized.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: - (heuristic)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: capability, evolve, must provide, must support
  - evidence: 'capability' [head]; 'evolve' [head]; 'must support' [head]; 'must provide' [head]; 'objective' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: User: 'add this and perform in order' (queued after REQ-CHAT-20261126-023, Prompt 26). An unnumbered foundational architecture prompt (Functionally Architected Matter); it builds on Prompts 17-26, including the missing Prompt 25 (inverse design).

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

User: 'add this and perform in order' (queued after REQ-CHAT-20261126-023, Prompt 26). An unnumbered foundational architecture prompt (Functionally Architected Matter); it builds on Prompts 17-26, including the missing Prompt 25 (inverse design).

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000025
- Evidence: pending
