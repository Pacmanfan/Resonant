# Request: # PROMPT 21 — METAMATERIALS AND ENGINEERED EFFECTIVE PROPERTIES

## Project: RES

**Request ID:** REQ-CHAT-20261126-019
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T23:38:19-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 21 — METAMATERIALS AND ENGINEERED EFFECTIVE PROPERTIES

## Project: RESONANT — Wave Field Engineering Studio

## Mission

Implement the next major scientific and engineering capability in RESONANT: **Metamaterials and Engineered Effective Properties**.

This capability establishes the ability to engineer macroscopic physical behavior through deliberately designed internal material structures.

RESONANT must move beyond treating materials as homogeneous substances with fixed properties.

It must support the investigation and engineering of materials whose effective physical behavior emerges from:

- Internal geometry.
- Constituent materials.
- Periodic arrangements.
- Resonant inclusions.
- Spatial organization.
- Layering.
- Interfaces.
- Anisotropy.
- Scale-dependent interactions.
- Frequency-dependent response.
- Coupling between internal structures.

The governing concept is:

**Constituent Materials + Internal Geometry + Spatial Organization + Excitation**

↓

**Local Physical Interactions + Resonance + Scattering + Coupling**

↓

**Emergent Effective Properties**

↓

**Macroscopic Wave, Energy and Matter Behavior**

The objective is to progressively allow users to design materials with deliberately engineered physical characteristics rather than simply selecting from predefined material-property databases.

RESONANT should support metamaterial engineering across electromagnetic, optical, acoustic, mechanical, elastic, thermal, and other supported physical domains.

The system must distinguish actual calculated effective properties from assumed, fitted, homogenized, or approximate values.

A negative effective property, unusual dispersion curve, or apparent exotic material response must emerge from a valid physical model. It must never be assigned artificially to make a structure appear to exhibit metamaterial behavior.

This work extends:

- Prompt 17 — Shape–Function–Energy Architecture.
- Prompt 18 — Coupled Energy–Matter Engine.
- Prompt 19 — Geometry-Driven Energy Transformation.
- Prompt 20 — Resonant Structures and Energy Storage.

Do not duplicate these systems. Extend their capabilities through established interfaces.

---

# 1. AUDIT FIRST

Before implementation, inspect the existing repository.

Read AGENTS.md and all applicable development instructions.

Establish the actual state of:

- Material models and constitutive properties.
- Geometry representation.
- CAD and OpenCASCADE integration.
- Geometry parameterization.
- Functional structures.
- Periodic geometry support.
- Unit-cell representation.
- Electromagnetic solvers.
- Optical solvers.
- Acoustic solvers.
- Mechanical and elastic solvers.
- Thermal solvers.
- Eigenmode analysis.
- Frequency-domain analysis.
- Time-domain analysis.
- Coupled-mode analysis.
- Resonance characterization.
- Energy and momentum accounting.
- CEME material-state management.
- Boundary-condition management.
- Periodic boundary conditions.
- Bloch-Floquet boundary conditions.
- Mesh generation.
- Parameter sweeps.
- Optimization infrastructure.
- Project serialization.
- Visualization.
- Automation and gRPC services.

Identify:

1. Existing capabilities.
2. Partially implemented capabilities.
3. Missing capabilities.
4. Reusable solvers and scientific libraries.
5. Architectural changes required.
6. External dependencies and licensing implications.

Produce an implementation plan based on repository evidence.

Do not invent existing capabilities.

Do not create parallel material, geometry, resonance, or solver architectures.

Preserve existing project compatibility.

---

# 2. METAMATERIAL ARCHITECTURE

Establish a reusable metamaterial architecture.

A metamaterial must be represented as a physical structure with explicitly defined constituent geometry, materials, interfaces, and spatial organization.

Introduce a metamaterial resource that references:

- Metamaterial identity.
- Constituent material definitions.
- Unit-cell geometry.
- Internal inclusions.
- Resonant elements.
- Periodicity.
- Lattice configuration.
- Spatial arrangement.
- Layering.
- Interfaces.
- Physical domains.
- Excitation conditions.
- Boundary conditions.
- Operating frequency or frequency range.
- Analysis configuration.
- Homogenization configuration.
- Effective-property results.
- Manufacturing constraints.
- Validation information.

Do not make metamaterial a special-purpose replacement for the existing material system.

A metamaterial is a structured physical assembly whose behavior is calculated from its constituents.

The resulting effective material model should integrate with CEME through existing material and constitutive-model interfaces.

## 2.1 Distinguish material representations

Keep these concepts distinct:

1. Constituent materials: actual physical materials used to build the structure.
2. Microstructure: the geometry and arrangement of constituent materials.
3. Unit cell: a representative repeating physical region.
4. Periodic structure: an assembly of repeating unit cells.
5. Effective material: a homogenized representation derived from the underlying structure.
6. Equivalent model: a reduced-order representation used within a defined validity range.

Maintain traceability between these representations.

An effective material must identify the microstructure and constituent revisions from which it was derived.

---

# 3. UNIT-CELL ENGINEERING

Establish the unit cell as a first-class engineering object.

The unit cell is the fundamental building block for periodic metamaterial design.

Support:

## 3.1 Unit-cell geometry

- Three-dimensional unit cells.
- Two-dimensional unit cells.
- One-dimensional periodic structures.
- Rectangular lattices.
- Square lattices.
- Hexagonal lattices.
- Triangular lattices.
- Custom lattice vectors.
- Layered unit cells.
- Embedded inclusions.
- Resonant elements.
- Cavities.
- Channels.
- Voids.
- Conductive patterns.
- Dielectric structures.
- Elastic inclusions.
- Composite arrangements.

Support arbitrary parameterized geometry using the existing CAD architecture.

## 3.2 Unit-cell parameters

Support named parameters including:

- Cell dimensions.
- Lattice constants.
- Inclusion dimensions.
- Inclusion spacing.
- Inclusion orientation.
- Wall thickness.
- Layer thickness.
- Void dimensions.
- Resonator dimensions.
- Resonator placement.
- Material proportions.
- Symmetry parameters.

Parameters must have:

- Units.
- Valid ranges.
- Dependencies.
- Constraints.
- Revision tracking.
- Derived values.

## 3.3 Unit-cell boundaries

Support appropriate boundary conditions:

- Periodic boundaries.
- Bloch-Floquet boundaries.
- Symmetry boundaries.
- Perfect electric conductor.
- Perfect magnetic conductor.
- Domain-specific physical boundaries.

Do not assume that ordinary periodic boundary conditions are interchangeable with Bloch-Floquet conditions.

The phase relationship across periodic boundaries must be represented correctly.

## 3.4 Unit-cell validation

Validate:

- Geometric closure.
- Periodic face correspondence.
- Material-region consistency.
- Interface continuity.
- Mesh compatibility.
- Boundary-condition compatibility.
- Lattice-vector validity.

Reject invalid configurations with useful engineering diagnostics.

---

# 4. PERIODIC STRUCTURES AND BLOCH-FLOQUET ANALYSIS

Implement a reusable periodic-physics analysis interface.

Support Bloch-Floquet analysis where the underlying solver permits it.

For a periodic structure, represent the appropriate phase relationship between corresponding fields in adjacent cells.

Use the relevant Bloch condition:

\[
\psi(\mathbf r+\mathbf R)
=
\psi(\mathbf r)e^{i\mathbf k\cdot\mathbf R}
\]

where:

- \(\mathbf R\) is a lattice translation.
- \(\mathbf k\) is the Bloch wavevector.
- \(\psi\) represents the relevant physical field or state.

The actual field formulation must remain domain-specific.

Support:

- One-dimensional dispersion.
- Two-dimensional dispersion.
- Three-dimensional dispersion where supported.
- Wavevector sweeps.
- Frequency sweeps.
- Eigenmode analysis.
- Band structure.
- Band-gap identification.
- Mode classification.
- Group velocity.
- Mode localization.

## 4.1 Brillouin zone

Support:

- Reciprocal lattice construction.
- First Brillouin zone.
- High-symmetry points.
- High-symmetry paths.
- Wavevector sampling.
- Band-structure plotting.

Do not hardcode a single lattice configuration.

Generate reciprocal-space geometry from the actual lattice definition.

## 4.2 Band structures

Calculate dispersion relations from the underlying physical model.

Display:

- Frequency versus wavevector.
- Band branches.
- Mode crossings.
- Avoided crossings.
- Degeneracies.
- Band gaps.
- Flat bands.
- Group velocity.
- Mode polarization where supported.

Do not fabricate band structures from analytical sketches.

## 4.3 Band-gap analysis

Identify:

- Complete band gaps.
- Directional band gaps.
- Polarization-dependent gaps.
- Partial gaps.
- Local resonant gaps.
- Bragg scattering gaps.

Distinguish these mechanisms.

Report the frequency range, propagation directions, and polarizations for which a gap exists.

A gap in one propagation direction must not be presented as a complete three-dimensional band gap.

---

# 5. ENGINEERED EFFECTIVE MATERIAL PROPERTIES

This is the central scientific capability of Prompt 21.

Develop an architecture for calculating effective constitutive behavior from the underlying structured material.

Do not create a universal effective-property calculator.

Use domain-specific homogenization methods.

## 5.1 Electromagnetic effective properties

Where supported, investigate:

- Effective permittivity.
- Effective permeability.
- Effective impedance.
- Effective refractive index.
- Effective conductivity.
- Effective anisotropy.
- Frequency-dependent constitutive response.

Support tensor-valued properties where appropriate.

Distinguish:

- Constituent properties.
- Effective bulk properties.
- Retrieved effective parameters.
- Local field-derived response.
- Nonlocal or spatially dispersive behavior.

Investigate:

- Negative effective permittivity.
- Negative effective permeability.
- Negative refractive index.
- Double-negative behavior.
- Near-zero permittivity.
- Near-zero permeability.
- Anisotropic response.
- Hyperbolic dispersion.

These behaviors must be derived from valid physical calculations.

Do not assign negative parameters manually.

## 5.2 Acoustic effective properties

Where supported, investigate:

- Effective density.
- Effective bulk modulus.
- Effective compressibility.
- Effective acoustic impedance.
- Effective sound speed.
- Anisotropic acoustic response.
- Frequency-dependent acoustic behavior.

Support investigation of:

- Negative effective density.
- Negative effective bulk modulus.
- Near-zero effective parameters.
- Acoustic band gaps.
- Local resonant behavior.
- Unusual acoustic dispersion.

## 5.3 Mechanical and elastic effective properties

Where supported, investigate:

- Effective stiffness tensors.
- Effective compliance.
- Effective density.
- Effective elastic modulus.
- Effective Poisson response.
- Effective anisotropy.
- Effective wave speed.
- Frequency-dependent dynamic stiffness.

Support investigation of:

- Negative effective dynamic mass density.
- Negative effective stiffness.
- Auxetic response.
- Anisotropic elasticity.
- Local resonant elastic behavior.
- Elastic band gaps.

Clearly distinguish dynamic effective parameters from static material properties.

## 5.4 Thermal effective properties

Where supported, investigate:

- Effective thermal conductivity.
- Anisotropic thermal conductivity.
- Effective thermal diffusivity.
- Thermal interface effects.
- Directional heat transport.
- Engineered thermal pathways.

Support investigation of:

- Thermal concentration.
- Thermal insulation.
- Directional heat transport.
- Effective thermal anisotropy.

Do not claim thermal metamaterial functionality without a validated heat-transfer model.

## 5.5 Optical effective properties

Where supported, derive optical effective behavior from appropriate electromagnetic or optical models.

Support:

- Effective refractive index.
- Effective anisotropy.
- Effective optical dispersion.
- Photonic band gaps.
- Engineered optical propagation.
- Frequency-dependent optical response.

Distinguish effective optical models from full-wave electromagnetic calculations.

---

# 6. HOMOGENIZATION AND VALIDITY

Implement homogenization as a first-class scientific operation.

Effective properties are meaningful only under appropriate assumptions.

Support applicable methods including:

- Volume averaging.
- Field averaging.
- Effective constitutive retrieval.
- Asymptotic homogenization.
- Dynamic homogenization.
- Domain-specific equivalent-medium methods.

Use existing scientific implementations where appropriate.

Do not implement every homogenization method in this prompt.

## 6.1 Scale separation

Evaluate whether the unit-cell dimensions are sufficiently small relative to the relevant wavelength or characteristic physical length.

Do not automatically assume homogenization is valid.

Report:

- Unit-cell dimensions.
- Operating wavelength.
- Wavelength-to-cell-size ratio.
- Relevant spatial scales.
- Frequency range.
- Homogenization assumptions.
- Expected validity limitations.

## 6.2 Frequency dependence

Effective properties may vary significantly with frequency.

Represent effective material properties as frequency-dependent quantities when required.

Do not reduce a dispersive material to a single constant value unless that approximation is explicitly justified.

## 6.3 Spatial dispersion

Where appropriate, identify behavior that depends on wavevector as well as frequency.

Distinguish local effective constitutive models from nonlocal or spatially dispersive models.

Do not force a local effective material representation onto a structure when it is physically inadequate.

## 6.4 Retrieval stability

For retrieval-based methods, investigate:

- Branch ambiguity.
- Phase ambiguity.
- Resonance singularities.
- Numerical instability.
- Sensitivity to sample thickness.
- Boundary effects.
- Parameter non-uniqueness.

Report uncertain or nonphysical retrieval results rather than silently accepting them.

## 6.5 Full-structure comparison

Where practical, compare effective-medium predictions against a finite structure containing multiple unit cells.

Measure:

- Reflection.
- Transmission.
- Phase.
- Attenuation.
- Dispersion.
- Field distribution.

Quantify the difference between the homogenized model and the explicit structure.

---

# 7. RESONANT METAMATERIALS

Integrate the resonance architecture from Prompt 20.

Support the engineering of metamaterials whose response emerges from resonant internal structures.

Examples include:

- Split-ring resonators.
- Dielectric resonators.
- Electric resonators.
- Magnetic resonators.
- Acoustic Helmholtz resonators.
- Membrane-type acoustic resonators.
- Elastic local resonators.
- Mechanical resonator arrays.
- Coupled resonator lattices.
- Optical resonant inclusions.

Support analysis of:

- Individual resonator modes.
- Unit-cell modes.
- Coupled resonances.
- Mode splitting.
- Resonance bandwidth.
- Q factor.
- Effective-property response.
- Energy localization.
- Field concentration.
- Damping.
- Band formation.

Investigate the transition from individual resonant elements to collective periodic behavior.

Distinguish:

- Isolated resonator behavior.
- Coupled unit-cell behavior.
- Collective lattice modes.
- Effective macroscopic response.

Do not assume that a resonant inclusion automatically creates a useful metamaterial response.

Calculate the resulting physical behavior.

---

# 8. SPATIAL ORGANIZATION AND STRUCTURED MATERIALS

Support metamaterials beyond perfectly periodic arrangements.

Introduce architecture for:

- Periodic materials.
- Quasiperiodic materials.
- Graded materials.
- Layered metamaterials.
- Composite metamaterials.
- Spatially varying lattices.
- Defect-engineered structures.
- Disordered structures.
- Hierarchical structures.

## 8.1 Graded metamaterials

Allow unit-cell parameters to vary spatially.

Examples:

- Gradually changing lattice constant.
- Gradually changing inclusion dimensions.
- Spatially varying resonator frequency.
- Varying material composition.
- Varying effective anisotropy.

Analyze the resulting spatially dependent physical response.

## 8.2 Defect engineering

Support controlled deviations from periodicity.

Examples:

- Missing inclusions.
- Modified unit cells.
- Embedded resonators.
- Localized cavities.
- Defect channels.
- Localized modes.

Analyze defect states and localized energy where supported.

## 8.3 Layered structures

Support stacked layers with different:

- Unit cells.
- Materials.
- Orientations.
- Thicknesses.
- Effective properties.

Analyze interfaces and wave propagation through layered assemblies.

## 8.4 Quasiperiodic and disordered structures

Establish extensible representations for non-periodic structured materials.

Use appropriate finite-structure analysis.

Do not apply Bloch-Floquet periodicity to structures that do not possess the required translational symmetry.

---

# 9. WAVE PROPAGATION AND ENERGY TRANSPORT

Investigate how engineered material structures influence energy propagation.

Support:

- Transmission.
- Reflection.
- Absorption.
- Scattering.
- Refraction.
- Diffraction.
- Attenuation.
- Evanescent fields.
- Slow-wave behavior.
- Group velocity.
- Phase velocity.
- Energy velocity where supported.

Analyze:

- Energy flux.
- Power transmission.
- Field concentration.
- Mode localization.
- Surface and interface modes.
- Directional propagation.
- Energy confinement.

Distinguish phase velocity, group velocity, and energy transport.

Do not assume they are equivalent.

Where negative phase velocity or unusual group velocity occurs, verify the actual energy-flow direction and physical interpretation.

## 9.1 Energy accounting

Use CEME to account for:

- Incident energy.
- Reflected energy.
- Transmitted energy.
- Absorbed energy.
- Stored energy.
- Scattered energy.
- Dissipated energy.

Avoid double-counting energy in periodic cells and finite structures.

---

# 10. METAMATERIAL REFERENCE ENGINEERING EXAMPLES

Implement these as genuine engineering models.

They must use real geometry, material definitions, appropriate solvers, and scientifically valid analysis.

Prioritize:

## 10.1 Electromagnetic split-ring resonator array

Implement a parameterized split-ring resonator unit cell.

Support:

- Ring dimensions.
- Ring thickness.
- Gap size.
- Conductivity.
- Substrate properties.
- Lattice spacing.
- Ring orientation.
- Excitation polarization.
- Frequency sweep.

Analyze:

- Resonant frequency.
- Electric-field concentration.
- Magnetic-field response.
- Surface currents.
- Reflection.
- Transmission.
- Absorption.
- Effective response where valid.
- Coupling between neighboring resonators.

Investigate the influence of geometric changes on resonance and effective properties.

Do not assume a negative permeability response. Calculate and validate it where the selected model supports meaningful retrieval.

## 10.2 Acoustic locally resonant metamaterial

Implement a parameterized acoustic unit cell with a local resonator.

Support:

- Cell dimensions.
- Cavity geometry.
- Neck geometry.
- Resonator mass and compliance where appropriate.
- Material properties.
- Array spacing.
- Acoustic excitation.

Analyze:

- Local resonance.
- Dispersion.
- Band gaps.
- Effective density or bulk modulus where valid.
- Pressure fields.
- Particle velocity.
- Transmission and reflection.

Compare with an appropriate analytical or established model.

## 10.3 Mechanical locally resonant lattice

Implement a parameterized elastic or mechanical unit cell.

Support:

- Host material.
- Internal resonator.
- Resonator mass.
- Spring or elastic connection.
- Lattice spacing.
- Boundary conditions.
- Excitation.

Analyze:

- Dispersion.
- Local resonance.
- Band gaps.
- Effective dynamic properties.
- Mode shapes.
- Energy localization.
- Transmission.

Compare against an analytical coupled-oscillator or established elastic model.

## 10.4 Photonic periodic structure

Implement a photonic crystal or equivalent periodic optical structure using an appropriate existing optical or electromagnetic solver.

Support:

- Lattice geometry.
- Dielectric inclusions.
- Refractive indices.
- Periodicity.
- Polarization.
- Wavevector.
- Frequency range.

Analyze:

- Photonic bands.
- Band gaps.
- Mode localization.
- Transmission.
- Reflection.
- Polarization dependence.

Clearly identify the solver and approximation used.

## 10.5 Layered effective medium

Implement a layered structure as a reference for engineered anisotropic response.

Support:

- Layer thickness.
- Constituent materials.
- Layer orientation.
- Number of layers.
- Operating frequency.

Analyze:

- Effective anisotropy.
- Reflection.
- Transmission.
- Dispersion.
- Field distribution.

Compare the homogenized representation with an explicit multilayer model.

---

# 11. FUNCTION-DRIVEN METAMATERIAL DESIGN

Extend the function-driven design architecture established in Prompt 19.

Allow the user to define a desired macroscopic physical response and investigate candidate microstructures.

Example objectives:

- Create a band gap around a target frequency.
- Produce a specified effective refractive index.
- Achieve a target effective density.
- Engineer directional propagation.
- Minimize reflection.
- Maximize transmission.
- Create localized modes.
- Maximize field concentration.
- Control dispersion.
- Create an anisotropic response.
- Minimize material losses.
- Maximize a specified bandwidth.
- Produce a desired effective stiffness tensor.
- Engineer directional thermal conductivity.

Each objective must specify:

- Physical quantity.
- Units.
- Target.
- Frequency or operating range.
- Direction or polarization where applicable.
- Relevant spatial region.
- Solver.
- Homogenization method.
- Validity conditions.
- Constraints.

Support multiple objectives and tradeoffs.

Do not optimize a homogenized result without checking that the underlying microstructure supports it.

Candidate designs must be validated against the relevant physical model.

---

# 12. PARAMETER EXPLORATION AND OPTIMIZATION

Integrate with the existing parameterization and optimization systems.

Support:

- Unit-cell parameter sweeps.
- Lattice parameter sweeps.
- Inclusion geometry sweeps.
- Resonator tuning.
- Material parameter sweeps.
- Orientation sweeps.
- Frequency sweeps.
- Wavevector sweeps.
- Sensitivity analysis.
- Tolerance analysis.
- Multi-objective comparisons.

Where supported, support:

- Gradient-based optimization.
- Evolutionary optimization.
- Surrogate models.
- Topology optimization.
- Adjoint methods.

Do not implement a second optimization framework.

Track parameter dependencies and geometry revisions.

Ensure that changes invalidate only the affected meshes, effective-property calculations, and results.

Every optimization candidate must identify:

- Constituent geometry.
- Material definitions.
- Parameter values.
- Solver.
- Boundary conditions.
- Excitation.
- Effective-property method.
- Validity range.
- Objective values.
- Constraint violations.
- Numerical convergence.

---

# 13. MULTISCALE AND HIERARCHICAL STRUCTURES

Establish an extensible architecture for hierarchical metamaterials.

A structure may contain:

- Multiple unit-cell types.
- Unit cells assembled into supercells.
- Supercells assembled into larger structures.
- Multiple periodic scales.
- Nested resonant structures.
- Graded internal architectures.

Support supercell representations and corresponding analysis configurations.

Distinguish the scale of:

- Constituent features.
- Unit cells.
- Supercells.
- Finite assemblies.
- Macroscopic models.

Do not attempt to implement complete molecular, atomic, or quantum modeling here. That is the subject of Prompt 22.

However, preserve interfaces that will allow lower-scale material models to inform constituent material properties later.

---

# 14. MANUFACTURABILITY AND PHYSICAL REALIZABILITY

Extend the manufacturing constraints from Prompt 19.

Support:

- Minimum feature dimensions.
- Minimum gap size.
- Minimum wall thickness.
- Material availability.
- Fabrication tolerances.
- Surface roughness constraints.
- Layer alignment.
- Assembly tolerances.
- Maximum aspect ratio.
- Minimum spacing.
- Structural stability constraints.

Support manufacturing-process profiles for:

- CNC machining.
- PCB fabrication.
- Lithography.
- Additive manufacturing.
- Laser cutting.
- Sheet fabrication.
- Composite fabrication.
- Lamination.

Do not assume a theoretical unit cell can be manufactured.

Allow optimization to penalize or reject physically unrealizable designs.

Where relevant, investigate tolerance sensitivity and the effect of manufacturing deviations on effective properties.

---

# 15. VISUALIZATION AND ENGINEERING INSPECTION

Extend the existing RESONANT visualization framework.

Do not build a separate visualization application.

Support:

## 15.1 Microstructure visualization

Display:

- Unit cells.
- Lattice geometry.
- Internal inclusions.
- Resonators.
- Material regions.
- Interfaces.
- Periodic boundaries.
- Supercells.
- Layered structures.
- Defects.
- Graded arrangements.

## 15.2 Field visualization

Where available:

- Electric fields.
- Magnetic fields.
- Acoustic pressure.
- Particle velocity.
- Mechanical displacement.
- Stress and strain.
- Temperature.
- Energy density.
- Energy flux.
- Surface currents.

## 15.3 Reciprocal-space visualization

Provide:

- Reciprocal lattice.
- Brillouin zone.
- High-symmetry points.
- Wavevector paths.
- Dispersion surfaces where supported.

## 15.4 Band-structure visualization

Display:

- Frequency versus wavevector.
- Band branches.
- Band gaps.
- Mode crossings.
- Avoided crossings.
- Flat bands.
- Polarization.
- Group velocity.

## 15.5 Effective-property visualization

Provide frequency-dependent plots of:

- Effective permittivity.
- Effective permeability.
- Effective refractive index.
- Effective density.
- Effective bulk modulus.
- Effective stiffness.
- Effective conductivity.
- Effective thermal conductivity.

Display uncertainty and validity regions.

Clearly distinguish:

- Measured constituent properties.
- Calculated effective properties.
- Retrieved properties.
- Approximate properties.
- Assumed properties.

## 15.6 Comparison views

Support:

- Individual unit-cell comparison.
- Explicit versus homogenized structure.
- Before and after optimization.
- Different lattice configurations.
- Different material configurations.
- Different frequency bands.

Every visualization must identify its physical quantity, units, solver, model, geometry revision, material revision, and validity range.

---

# 16. USER WORKFLOW

Integrate metamaterial engineering into the existing RESONANT workspace.

Provide a coherent workflow:

1. Create or import a unit cell.
2. Define lattice geometry.
3. Parameterize constituent features.
4. Assign constituent materials.
5. Define physical domains.
6. Configure periodic or Bloch-Floquet boundaries.
7. Define excitation and polarization.
8. Select physical analysis.
9. Validate unit-cell geometry and boundaries.
10. Generate or update the mesh.
11. Run eigenmode or frequency-domain analysis.
12. Calculate dispersion.
13. Identify band gaps.
14. Calculate effective properties where valid.
15. Evaluate homogenization assumptions.
16. Compare explicit and effective structures.
17. Investigate resonant behavior.
18. Sweep geometric and material parameters.
19. Define functional objectives.
20. Evaluate candidate designs.
21. Validate optimized structures.
22. Save and document the resulting material design.

Allow users to move between unit-cell analysis, finite-structure analysis, and effective-material analysis without rebuilding the project.

---

# 17. CEME INTEGRATION

Use CEME as the authoritative framework for material state, constitutive properties, energy transfer, and multiphysics coupling.

Do not duplicate:

- Material registry.
- Material state management.
- Physical quantity registry.
- Geometry management.
- Energy transfer records.
- Conservation auditing.
- Solver orchestration.
- Provenance.

Metamaterial resources must provide derived effective-property models through appropriate CEME interfaces.

Support:

- Frequency-dependent effective constitutive properties.
- Anisotropic material tensors.
- Spatially varying effective properties.
- Dynamic effective properties where appropriate.
- Temperature-dependent effective response.
- Material-state-dependent behavior.

Ensure that the effective material model retains a link to its constituent structure.

Where material state changes, invalidate affected effective-property results.

Examples:

- Heating changes constituent conductivity.
- Temperature changes resonator dimensions.
- Mechanical deformation changes lattice spacing.
- Fluid loading changes acoustic resonator behavior.
- Constituent damage changes effective stiffness.
- Material phase changes alter effective electromagnetic properties.

Do not automatically treat an effective material as independent of the physical structure that produced it.

---

# 18. AUTOMATION AND PROGRAMMATIC ACCESS

Expose meaningful metamaterial engineering capabilities through the existing application services.

The existing Protobuf contract remains authoritative.

Potential operations include:

- Create metamaterial.
- Create unit cell.
- Configure lattice.
- Configure periodic boundaries.
- Assign constituent materials.
- Configure internal resonators.
- Update geometric parameters.
- Validate unit cell.
- Generate mesh.
- Run Bloch-Floquet analysis.
- Calculate band structure.
- Identify band gaps.
- Calculate effective properties.
- Configure homogenization.
- Validate homogenization.
- Compare explicit and effective structures.
- Run parameter sweeps.
- Define metamaterial objectives.
- Run optimization.
- Compare candidate designs.
- Retrieve dispersion results.
- Retrieve effective constitutive tensors.
- Export engineering results.

Follow established patterns for:

- Asynchronous jobs.
- Cancellation.
- Progress reporting.
- Idempotency.
- Optimistic concurrency.
- Revision tracking.
- Capability discovery.
- Error handling.
- Provenance.

Do not transfer large scientific field arrays through ordinary control-plane protobuf messages.

Use established scientific-data and bulk-transfer mechanisms.

GUI, Python, and automation must use the same application services.

---

# 19. DATA MODEL AND PROJECT SERIALIZATION

Persist metamaterial engineering resources using existing project serialization conventions.

Use stable UUIDs.

Persist:

- Metamaterial identity.
- Unit-cell definitions.
- Lattice vectors.
- Constituent material references.
- Constituent geometry.
- Parameter definitions.
- Parameter values.
- Periodic boundary definitions.
- Physical domains.
- Excitation configurations.
- Analysis configurations.
- Homogenization configurations.
- Effective-property results.
- Validity information.
- Manufacturing constraints.
- Optimization objectives.
- Candidate configurations.
- Solver provenance.
- Validation results.

Keep durable engineering definitions separate from transient solver state.

Maintain complete traceability between an effective material and its underlying constituent structure.

Ensure backward compatibility.

---

# 20. SCIENTIFIC VALIDATION

Scientific correctness is a release requirement.

Every implemented reference model must have automated validation.

## Electromagnetic split-ring resonator

- Resonant-frequency comparison.
- Mesh convergence.
- Surface-current verification.
- Reflection and transmission.
- Energy balance.
- Effective-property retrieval stability.

## Acoustic locally resonant metamaterial

- Analytical resonance comparison.
- Dispersion behavior.
- Band-gap verification.
- Effective-property consistency.
- Energy accounting.

## Mechanical locally resonant lattice

- Analytical coupled-oscillator comparison.
- Dispersion verification.
- Mode-shape consistency.
- Energy transfer.
- Mesh convergence.

## Photonic periodic structure

- Analytical or established band-structure comparison.
- Periodic boundary verification.
- Polarization consistency.
- Mesh convergence.
- Band-gap verification.

## Layered effective medium

- Analytical effective-medium comparison where applicable.
- Explicit versus homogenized structure comparison.
- Frequency-range validity.
- Reflection and transmission consistency.

For all analyses:

- Record solver versions.
- Record geometry revisions.
- Record material revisions.
- Record lattice parameters.
- Record boundary conditions.
- Record excitation.
- Record mesh and convergence information.
- Record homogenization assumptions.
- Record validity ranges.
- Report uncertainty.
- Identify numerical failures.

Do not claim a negative effective parameter or exotic physical behavior without a valid calculation and documented interpretation.

---

# 21. TESTING REQUIREMENTS

Add tests covering:

- Unit-cell creation.
- Lattice geometry.
- Reciprocal lattice generation.
- Periodic boundary validation.
- Bloch-Floquet phase relationships.
- Material-region assignment.
- Constituent geometry updates.
- Geometry revision tracking.
- Mesh invalidation.
- Band-structure analysis.
- Band-gap identification.
- Effective-property calculation.
- Tensor-valued properties.
- Homogenization validity.
- Retrieval stability.
- Resonant inclusion behavior.
- Parameter sweeps.
- Sensitivity analysis.
- Explicit versus homogenized comparison.
- CEME integration.
- Serialization.
- Automation operations.
- Project compatibility.

Add failure tests for:

- Invalid unit cells.
- Incompatible periodic faces.
- Invalid lattice vectors.
- Missing constituent materials.
- Invalid boundary conditions.
- Unsupported physical domains.
- Solver failure.
- Non-convergence.
- Invalid homogenization assumptions.
- Retrieval instability.
- Stale geometry.
- Stale material properties.
- Cancellation.
- Invalid optimization bounds.

Run existing regression suites.

Do not mark a capability complete merely because its data structures or GUI panels exist.

---

# 22. IMPLEMENTATION PHASES

Implement in carefully scoped phases.

## Phase A — Audit and architecture

- Inspect existing material and periodic-physics capabilities.
- Define metamaterial resource contracts.
- Define unit-cell and lattice representations.
- Define homogenization interfaces.
- Identify solver support.

## Phase B — Unit-cell engineering

- Unit-cell representation.
- Lattice definitions.
- Periodic boundaries.
- Bloch-Floquet conditions.
- Parameterization.
- Validation.

## Phase C — Periodic analysis

- Reciprocal lattice.
- Brillouin zones.
- Wavevector sweeps.
- Eigenmode analysis.
- Dispersion relations.
- Band-gap detection.

## Phase D — Effective-property modeling

- Domain-specific homogenization.
- Effective tensors.
- Frequency dependence.
- Retrieval stability.
- Validity reporting.
- Explicit versus effective comparisons.

## Phase E — Reference metamaterials

Prioritize:

1. Electromagnetic split-ring resonator array.
2. Acoustic locally resonant metamaterial.
3. Mechanical locally resonant lattice.
4. Photonic periodic structure.
5. Layered effective medium.

## Phase F — Structured and hierarchical materials

- Graded materials.
- Defect structures.
- Layered structures.
- Supercells.
- Non-periodic configurations.
- Hierarchical representations.

## Phase G — Function-driven design

- Effective-property objectives.
- Band-gap objectives.
- Candidate generation.
- Parameter exploration.
- Optimization.
- Validation.

## Phase H — GUI and automation

- Unit-cell workspace.
- Reciprocal-space inspection.
- Band-structure visualization.
- Effective-property inspection.
- Application service integration.

## Phase I — Validation and release readiness

- Analytical benchmarks.
- Numerical convergence.
- Homogenization validation.
- Energy conservation.
- Regression testing.
- Documentation.

If a phase depends on missing solver functionality, implement clean interfaces and document the dependency.

Do not fabricate scientific results.

---

# 23. PERFORMANCE AND ARCHITECTURAL REQUIREMENTS

- Reuse existing geometry and meshing infrastructure.
- Reuse existing resonance and modal systems.
- Preserve domain-specific physical models.
- Keep homogenization independent of rendering.
- Avoid unnecessary remeshing.
- Track dependency revisions.
- Support asynchronous analysis.
- Support cancellation and progress reporting.
- Reuse valid frequency and wavevector results.
- Avoid unnecessary field-data copies.
- Preserve CPU reference implementations.
- Use existing CUDA acceleration where appropriate.
- Keep optional scientific dependencies modular.
- Document external dependency licenses.
- Avoid monolithic metamaterial classes.
- Keep physical calculations outside the GUI.

---

# 24. DOCUMENTATION

Create or update documentation covering:

- Metamaterial architecture.
- Unit-cell engineering.
- Lattice geometry.
- Bloch-Floquet analysis.
- Reciprocal-space analysis.
- Band structures.
- Band gaps.
- Resonant inclusions.
- Effective material properties.
- Homogenization.
- Retrieval limitations.
- Spatial dispersion.
- Graded and layered materials.
- Hierarchical structures.
- Manufacturing constraints.
- Function-driven metamaterial design.
- CEME integration.
- Automation API.
- Scientific assumptions.
- Validation methodology.

Update the documentation from Prompts 17–20.

Include architecture diagrams and representative engineering workflows.

Clearly distinguish implemented, partially implemented, and planned capabilities.

---

# 25. ACCEPTANCE CRITERIA

This work is complete only when:

1. Metamaterials are represented as structured physical assemblies rather than ordinary materials with manually assigned exotic properties.
2. Unit cells can be created and parameterized.
3. Lattice definitions and periodic boundaries are validated.
4. Bloch-Floquet analysis works for at least one validated physical domain.
5. Dispersion and band structures can be calculated.
6. Band gaps can be identified and characterized.
7. At least one effective-property calculation is derived from an actual constituent structure.
8. Effective properties retain traceability to constituent geometry and materials.
9. Homogenization assumptions and validity ranges are reported.
10. At least one resonant metamaterial reference model is implemented and validated.
11. Explicit and effective models can be compared where supported.
12. Parameter sweeps and sensitivity analysis work.
13. Metamaterial resources integrate with CEME.
14. GUI, Python, and automation use the same underlying application services.
15. Existing projects remain compatible.
16. Regression tests pass.
17. Scientific validation tests pass within documented tolerances.
18. Documentation reflects actual implementation and limitations.

---

# 26. FINAL ENGINEERING REPORT

At completion, provide a detailed report containing:

- Repository audit findings.
- Existing capabilities reused.
- New architecture introduced.
- Files and modules changed.
- Unit-cell implementation.
- Periodic solver integration.
- Band-structure implementation.
- Homogenization methods implemented.
- Effective-property calculations.
- Reference metamaterials implemented.
- Scientific equations and assumptions.
- Validation results and tolerances.
- Test results.
- Performance observations.
- GUI changes.
- Automation API additions.
- Serialization changes.
- Known limitations.
- Incomplete work and reasons.
- External dependencies and license implications.
- Recommended next development prompt.

Do not claim a feature is implemented unless it is present, integrated, and tested.

Do not conceal failed tests or unsupported physics.

---

# FINAL DIRECTIVE

The objective is to give RESONANT the ability to engineer materials whose physical properties emerge from their internal architecture.

A user should progressively be able to ask:

- What happens if I change the internal structure of this material?
- Can I engineer a material to block a particular frequency range?
- Can I create a material with a particular effective refractive index?
- Can I control the direction in which energy propagates?
- Can I create a structure that confines or localizes energy?
- Can I engineer effective density, stiffness, or acoustic impedance?
- Can I create unusual electromagnetic or acoustic dispersion?
- How do individual resonators interact to produce collective behavior?
- When is a homogenized model physically valid?
- How closely does the effective model reproduce the actual structure?
- What manufacturing tolerances will destroy the desired behavior?
- What constituent geometry produces the macroscopic response I want?

RESONANT must progressively become an environment where physical material behavior can be engineered from the inside out.

**Do not build a library of decorative metamaterial shapes or manually assigned exotic material properties. Build a rigorous computational foundation for designing and validating emergent physical behavior from structured matter.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: - (heuristic)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Implement, ability to, capability, must support
  - evidence: 'Implement' [head]; 'capability' [head]; 'capability' [head]; 'ability to' [head]; 'must support' [head]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: User: 'add this ticket and perform it in order' (queued after REQ-CHAT-20261126-018)

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

User: 'add this ticket and perform it in order' (queued after REQ-CHAT-20261126-018)

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000020
- Evidence: pending
