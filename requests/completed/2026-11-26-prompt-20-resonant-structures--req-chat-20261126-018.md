# Request: # PROMPT 20 — RESONANT STRUCTURES, ENERGY STORAGE AND COUPLED OSCILLATORY SYSTEM

**Request ID:** REQ-CHAT-20261126-018
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T23:35:33-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 20 — RESONANT STRUCTURES, ENERGY STORAGE AND COUPLED OSCILLATORY SYSTEMS

## Project: RESONANT — Wave Field Engineering Studio

## Mission

Implement the next major scientific and engineering capability in RESONANT: **Resonant Structures and Energy Storage**.

This capability establishes resonance as a fundamental, cross-domain physical engineering discipline.

RESONANT must be able to investigate how physical structures and systems respond to excitation, develop oscillatory behavior, support natural modes, exchange energy between modes, store energy, dissipate energy, and interact with their environment.

The objective is to move beyond identifying resonant frequencies and plotting frequency-response curves.

RESONANT should progressively become an environment in which users can:

- Discover natural modes of physical structures.
- Predict resonant behavior from geometry and material properties.
- Understand where and how energy is stored.
- Investigate energy exchange between resonant modes.
- Study coupling between multiple resonators.
- Control resonance through geometric and material modifications.
- Investigate damping, leakage, and quality factor.
- Understand transient buildup and decay.
- Study nonlinear resonance and frequency-dependent behavior where supported.
- Engineer structures for particular resonant characteristics.
- Investigate how resonance changes when physical conditions evolve.

The governing engineering concept is:

**Structure + Material Properties + Excitation + Environment + Boundary Conditions**

↓

**Natural Modes + Resonant Response + Coupling + Energy Exchange**

↓

**Energy Storage + Dissipation + Transmission + Physical Response**

↓

**Measurable Functional Behavior**

Resonance must emerge from the underlying physical model. It must never be assigned to a structure merely because a user identifies it as resonant.

This work extends:

- Prompt 17 — Shape–Function–Energy Architecture.
- Prompt 18 — Coupled Energy–Matter Engine (CEME).
- Prompt 19 — Geometry-Driven Energy Transformation and Functional Structures.

Do not duplicate these systems. Extend their capabilities through well-defined interfaces.

---

# 1. AUDIT FIRST

Before implementation, inspect the existing repository.

Read AGENTS.md and all applicable development instructions.

Establish the actual state of:

- Existing resonance and modal analysis.
- Electromagnetic solvers.
- Acoustic solvers.
- Optical modeling.
- Mechanical and elastic solvers.
- Frequency-domain analysis.
- Time-domain analysis.
- Eigenvalue and eigenmode solvers.
- Frequency sweeps.
- Transient excitation and response.
- Geometry parameterization.
- Material constitutive models.
- Damping and loss models.
- Energy accounting.
- CEME coupling interfaces.
- Functional structures from Prompt 19.
- Field visualization.
- Solver orchestration.
- Optimization infrastructure.
- Project serialization.
- GUI workspaces.
- Automation and gRPC interfaces.
- Existing validation benchmarks.

Identify:

1. What is already implemented.
2. What is partially implemented.
3. What can be reused.
4. What is missing.
5. What existing functionality needs extension.
6. What external scientific dependencies might be appropriate.

Produce an implementation plan based on actual repository evidence.

Do not invent existing capabilities.

Do not create parallel modal, resonance, energy-accounting, or solver infrastructure.

Preserve existing project compatibility.

---

# 2. UNIFIED RESONANCE ARCHITECTURE

Establish a reusable resonance-analysis architecture that can coordinate different physical domains.

Resonance must not be implemented as one universal physical equation.

Instead, introduce a common analysis and data architecture with domain-specific physics.

The architecture should distinguish:

## 2.1 Natural modes

Natural modes describe the characteristic spatial and temporal behavior of a physical system under its defined boundary conditions.

Support, where the underlying solver permits:

- Eigenfrequencies.
- Complex eigenfrequencies.
- Eigenvalues.
- Eigenvectors.
- Mode shapes.
- Modal field distributions.
- Modal energy.
- Modal participation.
- Degenerate modes.
- Mode symmetry.
- Mode localization.
- Mode coupling.
- Mode sensitivity.

Distinguish natural frequencies from driven-response peaks.

A driven response may peak away from a natural frequency depending on damping, coupling, excitation, and system characteristics.

## 2.2 Resonant response

Support analysis of the response of a system to external excitation.

Expose relevant quantities:

- Excitation frequency.
- Response amplitude.
- Phase response.
- Impedance.
- Admittance.
- Transmission.
- Reflection.
- Absorption.
- Resonance bandwidth.
- Peak response.
- Damping.
- Quality factor.
- Stored energy.
- Dissipated power.

Use the appropriate domain-specific quantities and units.

## 2.3 Resonant energy storage

A resonator can temporarily store energy while continuously exchanging energy with its surroundings.

The system must distinguish:

- Energy supplied by an excitation source.
- Energy stored within the resonator.
- Energy transferred between modes.
- Energy dissipated internally.
- Energy radiated or transmitted externally.
- Energy returned to the source.
- Energy remaining after excitation stops.

Support time-dependent energy accounting where the underlying solver allows it.

Do not equate high field amplitude with high total stored energy.

Do not confuse energy density with total stored energy.

## 2.4 Damping and losses

Model the physical mechanisms responsible for energy loss.

Examples include:

- Electrical resistance.
- Conductive losses.
- Dielectric losses.
- Magnetic losses.
- Acoustic absorption.
- Viscous losses.
- Mechanical damping.
- Radiation losses.
- Scattering losses.
- Material hysteresis where supported.
- Thermoelastic losses where supported.

Different loss mechanisms must remain identifiable.

Do not use a single arbitrary damping parameter to conceal distinct physical mechanisms.

---

# 3. CROSS-DOMAIN RESONANCE

Establish a consistent conceptual framework for resonance across physical domains.

The following domains should be supported according to available solver capabilities.

## 3.1 Electromagnetic resonance

Support:

- Microwave cavity modes.
- Waveguide resonances.
- Transmission-line resonances.
- Lumped and distributed resonant structures.
- Coupled electromagnetic cavities.
- Dielectric resonators.
- Conductive resonators.
- Antenna resonance.
- Surface and localized electromagnetic resonances where supported.

Expose:

- Electric and magnetic field distributions.
- Resonant frequencies.
- Modal energy.
- Surface currents.
- Impedance.
- Reflection and transmission.
- Radiation losses.
- Quality factor.
- Coupling strength.

Distinguish lumped-element approximations from distributed electromagnetic models.

## 3.2 Optical resonance

Support:

- Fabry–Pérot resonators.
- Two-mirror optical cavities.
- Ring resonators.
- Whispering-gallery resonators.
- Distributed optical resonators.
- Coupled optical cavities.
- Photonic resonances.

Expose:

- Longitudinal modes.
- Transverse modes.
- Free spectral range.
- Finesse.
- Linewidth.
- Quality factor.
- Beam waist.
- Mode overlap.
- Coupling efficiency.
- Optical losses.

Identify whether each calculation uses geometric optics, paraxial optics, coupled-mode theory, or a full-wave electromagnetic model.

Do not present approximations as full-wave solutions.

## 3.3 Acoustic resonance

Support:

- Enclosed acoustic cavities.
- Helmholtz resonators.
- Quarter-wave resonators.
- Half-wave resonators.
- Acoustic ducts.
- Horn structures.
- Coupled acoustic cavities.
- Structural-acoustic resonances.
- Distributed acoustic resonators.

Expose:

- Pressure modes.
- Particle velocity.
- Acoustic energy density.
- Acoustic intensity.
- Resonant frequencies.
- Damping.
- Acoustic impedance.
- Transmission.
- Radiation behavior.

## 3.4 Mechanical resonance

Support:

- Mass-spring systems.
- Cantilevers.
- Beams.
- Plates.
- Membranes.
- Shells.
- Mechanical cavities and structures.
- Elastic wave resonators.
- Coupled mechanical oscillators.
- Flexible structures.

Expose:

- Natural frequencies.
- Displacement.
- Velocity.
- Acceleration.
- Stress.
- Strain.
- Modal mass.
- Modal stiffness.
- Modal damping.
- Stored elastic energy.
- Kinetic energy.
- Frequency response.

## 3.5 Fluid and hydrodynamic resonance

Where appropriate solvers exist, support:

- Fluid-column resonance.
- Acoustic-fluid interaction.
- Helmholtz-type fluid systems.
- Fluid-structure interaction.
- Hydrodynamic oscillations.
- Sloshing modes.
- Pressure-wave resonance.

Clearly distinguish acoustic pressure modes from hydrodynamic instability and other fluid phenomena.

Do not introduce unsupported fluid-resonance models simply to claim cross-domain coverage.

---

# 4. ENERGY STORAGE AND EXCHANGE

Make energy storage a first-class engineering quantity.

The system must identify where energy exists, how it moves, and how it changes over time.

## 4.1 Energy storage mechanisms

Support domain-appropriate storage calculations.

Examples:

Electromagnetic:
- Electric-field energy.
- Magnetic-field energy.
- Stored energy in resonant cavities.
- Energy associated with reactive fields.

Mechanical:
- Elastic potential energy.
- Kinetic energy.
- Strain energy.
- Energy in oscillating structures.

Acoustic:
- Potential energy associated with compressibility.
- Kinetic energy associated with particle motion.
- Total acoustic energy.

Optical:
- Stored electromagnetic energy.
- Cavity energy.
- Energy associated with circulating optical fields.

Thermal:
- Internal energy.
- Thermal storage.
- Temperature-dependent energy changes.

Use the correct physical equations for each domain.

Clearly document assumptions, including dispersive materials, lossy materials, nonlinear materials, and frequency-dependent constitutive properties.

## 4.2 Energy exchange

Support the investigation of energy transfer between:

- Different resonant modes.
- Coupled cavities.
- Different physical components.
- Different physical domains.
- A resonator and an external source.
- A resonator and its environment.

Represent energy transfer through existing CEME mechanisms.

Do not count the same energy transfer twice.

## 4.3 Energy buildup

Investigate how resonant energy accumulates under continuous excitation.

Support:

- Transient buildup.
- Steady-state response.
- Time to reach steady state.
- Energy accumulation.
- Source coupling.
- Internal losses.
- Radiation and leakage.

## 4.4 Energy decay

Investigate what happens after excitation is removed.

Support:

- Free decay.
- Ring-down.
- Decay constants.
- Amplitude decay.
- Energy decay.
- Frequency-domain linewidth.
- Damping estimation.

Distinguish amplitude decay from energy decay.

Where a simple exponential decay model is appropriate, expose the fitted decay constant and the assumptions behind it.

## 4.5 Energy extraction

Investigate how energy can be transferred out of a resonant system.

Examples:

- Output coupling from an electromagnetic cavity.
- Acoustic energy extraction.
- Mechanical energy harvesting models.
- Optical cavity output coupling.
- Coupled-resonator energy transfer.

Expose the tradeoff between internal energy storage and useful external coupling.

Do not imply that a resonator generates energy.

All energy supplied, stored, transferred, dissipated, and extracted must remain consistent with physical conservation.

---

# 5. QUALITY FACTOR, BANDWIDTH AND RESONATOR CHARACTERIZATION

Implement a consistent analysis interface for resonator characterization.

Support the appropriate definitions of quality factor for each physical model.

Where applicable:

Q = 2π × (energy stored / energy dissipated per cycle)

For a suitably characterized resonance, support the relationship between resonant frequency and bandwidth:

Q ≈ f₀ / Δf

Clearly identify the conditions under which these relationships apply.

Expose:

- Unloaded Q.
- Loaded Q.
- External Q.
- Internal Q.
- Radiation Q where applicable.
- Material-loss Q where applicable.
- Resonance bandwidth.
- Resonance linewidth.
- Finesse where appropriate.
- Coupling coefficient.
- Critical coupling.
- Overcoupling.
- Undercoupling.

For systems with multiple loss mechanisms, calculate the combined loss behavior using the appropriate physical relationship.

Do not assume that every resonance has a simple Lorentzian response.

Identify overlapping modes, asymmetric line shapes, nonlinear behavior, and other cases where simple resonance fitting is insufficient.

Provide fitting quality and uncertainty where possible.

---

# 6. COUPLED RESONATORS

This is a particularly important capability.

Implement a reusable architecture for investigating multiple interacting resonant structures.

A coupled system may contain two or more resonators with individual natural modes and a physical coupling mechanism.

Support:

- Two-resonator systems.
- Multiple coupled resonators.
- Identical resonators.
- Detuned resonators.
- Weak coupling.
- Strong coupling.
- Near-field coupling.
- Distributed coupling.
- Shared structural coupling.
- Shared acoustic volumes.
- Electromagnetic coupling.
- Mechanical coupling.
- Optical coupling.

Analyze:

- Individual resonant frequencies.
- Coupled-system eigenfrequencies.
- Mode splitting.
- Hybridized modes.
- Energy exchange.
- Coupling strength.
- Mode localization.
- Frequency detuning.
- Transfer efficiency.
- Damping.
- Transient response.

## 6.1 Coupled-mode analysis

Introduce an extensible coupled-mode analysis interface.

Use appropriate domain-specific coupled-mode formulations.

For models where a coupled-mode approximation is appropriate, support:

- Resonator amplitudes.
- Resonator frequencies.
- Coupling coefficients.
- Intrinsic losses.
- External coupling.
- Detuning.
- Time-dependent energy exchange.

Clearly document normalization and units.

Distinguish approximate coupled-mode calculations from solutions of the full underlying physical system.

## 6.2 Mode splitting

Investigate how coupled resonators produce distinct eigenmodes.

Allow users to visualize:

- Symmetric modes.
- Antisymmetric modes.
- Hybridized modes.
- Frequency splitting.
- Relative modal participation.

Do not hardcode expected mode splitting.

Calculate it from the model.

## 6.3 Energy transfer

Allow users to investigate how energy moves between coupled resonators.

Where supported, calculate:

- Energy transfer rate.
- Transfer efficiency.
- Maximum transferred energy.
- Energy remaining in each resonator.
- Effects of detuning.
- Effects of unequal damping.
- Effects of coupling strength.

Support time-domain visualization.

---

# 7. RESONANCE CONTROL AND TUNING

Introduce a resonance-tuning architecture that integrates with the geometry parameterization established in Prompt 19.

Support tuning through:

## 7.1 Geometric modification

Examples:

- Cavity dimensions.
- Resonator spacing.
- Mirror separation.
- Mirror curvature.
- Mechanical dimensions.
- Acoustic cavity volume.
- Neck dimensions.
- Wall thickness.
- Resonator placement.
- Structural supports.

## 7.2 Material modification

Examples:

- Permittivity.
- Permeability.
- Conductivity.
- Elastic modulus.
- Density.
- Acoustic impedance.
- Dielectric loss.
- Mechanical damping.
- Refractive index.

Support temperature-dependent and state-dependent properties where available.

## 7.3 Environmental modification

Examples:

- Temperature.
- Pressure.
- External loading.
- Ambient medium.
- Fluid density.
- Boundary conditions.

## 7.4 Active tuning

Establish an architecture for investigating tunable resonators.

Potential mechanisms include:

- Variable cavity dimensions.
- Adjustable mirror positions.
- Mechanical actuators.
- Variable capacitance.
- Electrically controlled material properties.
- Thermally controlled resonant structures.
- Pressure-controlled resonators.
- Tunable dielectric or mechanical components.

Distinguish simulated tunability from actual physical actuator implementation.

Do not implement arbitrary tuning effects without a physical model.

## 7.5 Tuning sensitivity

Calculate:

- Frequency shift per parameter change.
- Sensitivity to geometry.
- Sensitivity to material properties.
- Sensitivity to environmental changes.
- Mode stability.
- Frequency drift.

Use actual solver results or valid analytical models.

---

# 8. NONLINEAR RESONANCE AND DYNAMIC RESPONSE

Establish the architecture for investigating nonlinear resonant systems.

This is an important extension beyond conventional linear modal analysis.

Support where physically modeled:

- Amplitude-dependent resonance.
- Nonlinear stiffness.
- Nonlinear elasticity.
- Nonlinear acoustic response.
- Nonlinear electromagnetic materials.
- Duffing-type oscillators.
- Parametric excitation.
- Harmonic generation.
- Subharmonic response.
- Frequency mixing.
- Bistability.
- Hysteresis.
- Mode coupling.
- Nonlinear damping.
- Frequency pulling.

Distinguish:

1. Linear eigenmode analysis.
2. Nonlinear steady-state response.
3. Nonlinear transient simulation.
4. Parametric stability analysis.

Do not infer nonlinear behavior from a linear frequency sweep.

Do not attempt to implement a universal nonlinear solver.

Use the existing domain solvers and CEME infrastructure.

Where a nonlinear capability is missing, define the appropriate interface and document the limitation.

## 8.1 Parametric resonance

Where supported, investigate systems whose parameters vary periodically in time.

Examples:

- Time-varying stiffness.
- Modulated electromagnetic properties.
- Modulated boundary conditions.
- Periodically driven mechanical systems.

Analyze stability regions and excitation conditions using appropriate mathematical models.

Clearly distinguish parametric resonance from ordinary externally driven resonance.

---

# 9. RESONANCE SPECTROSCOPY AND MODE DISCOVERY

Introduce tools for identifying resonant behavior from simulated or measured responses.

Support:

- Frequency sweeps.
- Impulse responses.
- Ring-down measurements.
- Transfer functions.
- Impedance spectra.
- Reflection spectra.
- Transmission spectra.
- Modal decomposition.
- Peak detection.
- Resonance fitting.
- Mode identification.

Where appropriate, support:

- FFT analysis.
- Spectral estimation.
- Eigenmode extraction.
- Complex pole analysis.
- System identification.

For every identified resonance, expose:

- Estimated frequency.
- Amplitude.
- Phase.
- Bandwidth.
- Quality factor.
- Confidence or uncertainty.
- Identification method.
- Data source.
- Physical model.
- Geometry and material revisions.

Do not mistake every spectral peak for a physical natural mode.

Distinguish true resonances from:

- Source-spectrum features.
- Measurement artifacts.
- Numerical artifacts.
- Interference effects.
- Overlapping modes.
- Broadband responses.

---

# 10. MODE TRACKING AND MODE EVOLUTION

Extend the mode-tracking capabilities introduced in Prompt 19.

The system must be able to follow physical modes as geometry, materials, or operating conditions change.

Support:

- Mode correspondence.
- Modal overlap.
- Mode shape correlation.
- Frequency continuity.
- Symmetry classification.
- Mode splitting.
- Mode crossing.
- Avoided crossings.
- Mode hybridization.
- Mode localization.

Do not identify a mode solely by its position in a sorted frequency list.

Use appropriate modal correlation methods.

When mode identity is ambiguous, report the ambiguity rather than silently assigning an incorrect correspondence.

Provide visualizations showing how modes evolve across parameter sweeps.

---

# 11. REFERENCE ENGINEERING EXAMPLES

Implement these as actual engineering models using the underlying physical equations.

They must be functional, parameterized, and scientifically validated.

Prioritize the following examples.

## 11.1 Microwave cavity resonator

Extend the reference microwave cavity from Prompt 19.

Support:

- Multiple cavity geometries.
- Configurable dimensions.
- Conducting materials.
- Dielectric inserts.
- Excitation ports.
- Coupling arrangements.
- Frequency sweeps.
- Eigenmode analysis.
- Transient excitation where supported.

Analyze:

- Resonant frequencies.
- Electric-field modes.
- Magnetic-field modes.
- Stored energy.
- Surface losses.
- Volume losses.
- Loaded and unloaded Q.
- Bandwidth.
- Coupling.
- Mode splitting.
- Ring-down.
- Frequency sensitivity.

Include analytical benchmark comparisons for geometries with known solutions.

## 11.2 Fabry–Pérot optical resonator

Implement a functional two-mirror optical cavity.

Support:

- Mirror separation.
- Mirror curvature.
- Mirror reflectivity.
- Optical wavelength.
- Aperture.
- Alignment.
- Cavity stability.
- Longitudinal modes.
- Transverse modes where supported.

Analyze:

- Free spectral range.
- Finesse.
- Linewidth.
- Beam waist.
- Mode spacing.
- Mode overlap.
- Cavity losses.
- Coupling.
- Energy buildup.

Compare against analytical resonator theory.

## 11.3 Helmholtz resonator

Implement a parameterized acoustic Helmholtz resonator.

Support:

- Cavity volume.
- Neck area.
- Neck length.
- End corrections.
- Material properties.
- Excitation.
- Environmental conditions.

Analyze:

- Resonant frequency.
- Pressure distribution.
- Particle velocity.
- Acoustic impedance.
- Energy storage.
- Acoustic damping.
- Bandwidth.
- Radiation losses.

Compare against the appropriate analytical approximation.

Identify the approximation's validity range.

## 11.4 Mechanical cantilever resonator

Implement a parameterized mechanical cantilever.

Support:

- Length.
- Width.
- Thickness.
- Material.
- Density.
- Elastic modulus.
- Boundary conditions.
- Excitation.
- Damping.

Analyze:

- Natural frequencies.
- Mode shapes.
- Displacement.
- Velocity.
- Stress.
- Strain.
- Stored elastic energy.
- Kinetic energy.
- Frequency response.
- Ring-down.

Compare against analytical beam theory.

Include mesh convergence testing where applicable.

## 11.5 Coupled resonator system

Implement at least one physically meaningful two-resonator example.

Prefer a configuration that can be investigated in more than one domain.

Support:

- Individual resonator tuning.
- Coupling adjustment.
- Detuning.
- Damping.
- Transient excitation.

Analyze:

- Coupled modes.
- Mode splitting.
- Energy exchange.
- Transfer efficiency.
- Frequency response.
- Ring-down.

Validate against an appropriate analytical coupled-oscillator model.

## 11.6 Coupled multiphysics resonator

Establish at least one reference workflow demonstrating resonance interacting with another physical domain through CEME.

A preferred example is an electromagnetic cavity whose absorbed energy produces heating, changing material properties or geometry and consequently shifting its resonance.

Where supported, demonstrate:

1. Initial resonant response.
2. Energy absorption.
3. Thermal response.
4. Material or dimensional change.
5. Updated resonant behavior.
6. Feedback into the electromagnetic response.

A simpler validated one-way coupling is acceptable if full feedback is not supported.

Clearly identify which coupling steps are implemented and which remain future work.

---

# 12. RESONANCE EXPLORATION AND DESIGN OBJECTIVES

Extend the function-driven design architecture established in Prompt 19.

Support measurable resonance-related objectives.

Examples:

- Target a resonant frequency.
- Maximize quality factor.
- Minimize bandwidth.
- Maximize energy storage.
- Maximize useful external coupling.
- Minimize internal losses.
- Control mode spacing.
- Maximize separation between unwanted modes.
- Produce a particular mode shape.
- Maximize energy transfer between resonators.
- Minimize sensitivity to manufacturing tolerances.
- Maintain resonance over a temperature range.

Every objective must identify:

- Physical quantity.
- Units.
- Target.
- Operating conditions.
- Solver.
- Geometry dependencies.
- Material dependencies.
- Constraints.
- Validity conditions.

Support multi-objective tradeoffs.

For example, maximizing unloaded Q may conflict with maximizing useful energy extraction.

The software must expose this tradeoff rather than presenting one as universally optimal.

Use the existing optimization infrastructure.

Do not implement a second optimization engine.

---

# 13. TRANSIENT AND FREQUENCY-DOMAIN ANALYSIS

Provide a coherent workflow for comparing frequency-domain and time-domain behavior.

Support, where appropriate:

Frequency domain:
- Eigenmode analysis.
- Harmonic response.
- Frequency sweeps.
- Impedance response.
- Transfer functions.
- Modal response.

Time domain:
- Transient excitation.
- Energy buildup.
- Free oscillation.
- Ring-down.
- Mode beating.
- Energy exchange.
- Nonlinear response.

Allow the user to investigate the same resonant structure using different analysis methods when supported.

Explain differences between the results.

Do not silently substitute one analysis type for another.

Ensure that simulation duration, frequency resolution, time step, and numerical convergence are appropriate for the phenomenon being investigated.

---

# 14. VISUALIZATION AND ENGINEERING INSPECTION

Extend the existing RESONANT viewport and dockable panel architecture.

Do not create a separate application or duplicate the visualization framework.

Support synchronized inspection of geometry, modes, fields, and energy.

## 14.1 Mode visualization

Display:

- Mode shapes.
- Electric-field modes.
- Magnetic-field modes.
- Acoustic pressure modes.
- Mechanical displacement modes.
- Optical intensity distributions.
- Coupled mode shapes.

Support animation of oscillatory modes.

Clearly distinguish visualization scaling from actual physical displacement or field amplitude.

## 14.2 Energy visualization

Where supported, visualize:

- Stored energy.
- Energy density.
- Energy flux.
- Dissipation.
- Leakage.
- Inter-resonator energy transfer.
- Time-dependent energy distribution.

Use physically appropriate representations.

## 14.3 Resonance plots

Provide:

- Frequency response.
- Phase response.
- Impedance plots.
- Reflection and transmission.
- Resonance linewidth.
- Q-factor comparison.
- Mode splitting.
- Mode evolution.
- Ring-down plots.
- Energy buildup and decay.

## 14.4 Coupled system visualization

Allow users to inspect multiple resonators simultaneously.

Show:

- Individual resonator properties.
- Coupling connections.
- Mode participation.
- Energy exchange.
- Detuning.
- Coupled-system behavior.

## 14.5 Measurement and probing

Support:

- Probe points.
- Probe regions.
- Frequency-dependent probes.
- Time-dependent probes.
- Modal participation measurements.
- Energy measurements.

Every visualization must identify:

- Physical quantity.
- Units.
- Normalization.
- Analysis method.
- Solver.
- Geometry revision.
- Material revision.
- Simulation time or frequency.

Changing visualization settings must not trigger unnecessary physics recalculation.

---

# 15. USER WORKFLOW

Integrate the functionality into the existing RESONANT workspace.

Provide a coherent resonant-engineering workflow:

1. Create or import a structure.
2. Define geometric parameters.
3. Assign materials.
4. Configure physical domains.
5. Define boundaries and interfaces.
6. Configure excitation.
7. Select resonance analysis.
8. Validate the physical configuration.
9. Run modal or frequency-domain analysis.
10. Identify resonant modes.
11. Inspect field and energy distributions.
12. Characterize damping and Q.
13. Investigate transient behavior.
14. Configure coupled resonators where applicable.
15. Investigate energy exchange.
16. Modify geometry, materials, or environment.
17. Compare resonance behavior.
18. Optimize against measurable objectives.
19. Save and document the engineering results.

Allow transitions between ordinary functional-structure analysis and specialized resonant analysis without rebuilding the physical system.

---

# 16. CEME INTEGRATION

Use CEME as the authoritative framework for multiphysics interactions.

Do not duplicate:

- Physical quantity registry.
- Material state management.
- Geometry state management.
- Coupling manager.
- Energy transfer records.
- Conservation auditing.
- Time coordination.
- Solver orchestration.
- Provenance.

Resonance analysis must integrate with these systems.

Examples:

- Electromagnetic resonance produces dielectric and conductive heating.
- Heating changes material properties.
- Material changes shift resonant frequencies.
- Mechanical deformation changes cavity geometry.
- Acoustic resonance excites mechanical structures.
- Mechanical vibration radiates acoustic energy.
- Optical absorption produces thermal effects.
- Temperature changes optical refractive index.
- Coupled resonators exchange energy across physical components.

Use appropriate coupling strategies.

Do not force multiphysics simulation where independent analysis is sufficient.

Where energy is exchanged between domains, record the transfer through CEME.

Preserve conservation and provenance.

---

# 17. AUTOMATION AND PROGRAMMATIC ACCESS

Expose meaningful resonance-engineering capabilities through existing application services.

The existing Protobuf contract remains authoritative.

Potential operations include:

- Create resonant analysis.
- Configure resonator.
- Run eigenmode analysis.
- Run frequency sweep.
- Run transient response.
- Identify resonances.
- Calculate Q factors.
- Inspect mode shapes.
- Inspect stored energy.
- Inspect energy decay.
- Configure coupled resonators.
- Calculate coupling.
- Compare resonator configurations.
- Run parameter sweeps.
- Track modes.
- Define resonance objectives.
- Run resonance optimization.
- Retrieve energy accounting.
- Export resonance results.

Follow established patterns for:

- Asynchronous jobs.
- Progress reporting.
- Cancellation.
- Idempotency.
- Optimistic concurrency.
- Project revision tracking.
- Capability discovery.
- Error handling.
- Provenance.

Do not transfer large field arrays through ordinary control-plane messages.

Use established scientific-data and bulk-transfer mechanisms.

GUI, Python, and automation must use the same underlying application services.

---

# 18. DATA MODEL AND SERIALIZATION

Persist durable resonance-engineering definitions and results using existing project serialization conventions.

Use stable UUIDs.

Persist:

- Resonator identity.
- Geometry references.
- Geometry revisions.
- Material references and revisions.
- Physical domain.
- Boundary conditions.
- Excitation configuration.
- Analysis configuration.
- Modal definitions.
- Resonance characterization.
- Coupling definitions.
- Loss models.
- Tuning parameters.
- Functional objectives.
- Optimization configuration.
- Result references.
- Solver provenance.
- Validation information.

Distinguish durable engineering definitions from transient solver state.

Results must identify the exact geometry, material, boundary, and analysis revisions used.

Ensure backward compatibility with existing projects.

---

# 19. SCIENTIFIC VALIDATION

Scientific correctness is a release requirement.

Each reference model must include automated validation.

## Microwave cavity

- Analytical eigenfrequency comparison.
- Mesh convergence.
- Boundary-condition verification.
- Q-factor behavior.
- Energy balance.
- Frequency-response consistency.

## Optical resonator

- Free spectral range.
- Stability conditions.
- Finesse and linewidth.
- Beam parameter consistency.
- Comparison with analytical resonator theory.

## Helmholtz resonator

- Analytical resonance comparison.
- End-correction verification.
- Impedance behavior.
- Damping behavior.
- Mesh convergence.

## Mechanical cantilever

- Analytical beam-theory comparison.
- Mode-shape verification.
- Mesh convergence.
- Modal energy consistency.
- Boundary-condition verification.

## Coupled resonators

- Analytical eigenfrequency comparison.
- Mode splitting.
- Energy exchange.
- Detuning behavior.
- Damping behavior.
- Conservation.

## Multiphysics resonator

- Energy transfer verification.
- Material-state update.
- Geometry or property revision tracking.
- Resonance shift.
- Coupling consistency.
- Conservation audit.

For every analysis:

- Record solver and model versions.
- Record physical assumptions.
- Record geometry and material revisions.
- Record numerical resolution.
- Record convergence information.
- Record excitation and boundary conditions.
- Record uncertainty where available.
- Report unsupported effects.

Never silently substitute an approximate model for a more complete one.

Do not claim scientific validation solely because a result looks plausible.

---

# 20. TESTING REQUIREMENTS

Add automated tests covering:

- Natural-mode analysis.
- Resonant frequency extraction.
- Frequency response.
- Transient response.
- Energy storage.
- Energy decay.
- Q-factor calculation.
- Damping models.
- Multiple loss mechanisms.
- Mode tracking.
- Mode splitting.
- Coupled resonator analysis.
- Energy exchange.
- Detuning.
- Parameter sensitivity.
- Resonance tuning.
- Nonlinear model interfaces.
- CEME integration.
- Serialization.
- Project compatibility.
- Automation operations.
- Result provenance.

Add failure tests for:

- Invalid geometry.
- Missing material properties.
- Unsupported physical models.
- Invalid excitation.
- Invalid boundary conditions.
- Solver failure.
- Non-convergence.
- Insufficient frequency resolution.
- Insufficient simulation duration.
- Invalid damping configuration.
- Stale geometry during a job.
- Stale material properties.
- Cancellation.
- Invalid coupling configuration.

Test that energy accounting does not double-count transfers between resonators or physical domains.

Run existing geometry and physics regression suites.

Do not mark a capability complete merely because its GUI or data structures exist.

---

# 21. IMPLEMENTATION PHASES

Implement in carefully scoped phases.

## Phase A — Audit and architecture

- Audit existing resonance capabilities.
- Identify reusable solvers.
- Define common resonance-analysis contracts.
- Define energy-storage and coupling interfaces.
- Document domain-specific physical requirements.

## Phase B — Resonance foundation

- Natural-mode analysis integration.
- Resonance identification.
- Frequency response.
- Damping and loss characterization.
- Q-factor calculation.
- Energy storage and decay interfaces.

## Phase C — Reference resonators

Implement in priority order:

1. Microwave cavity.
2. Fabry–Pérot optical resonator.
3. Helmholtz acoustic resonator.
4. Mechanical cantilever.
5. Coupled resonator system.

## Phase D — Coupled resonator engineering

- Coupled-mode analysis.
- Mode splitting.
- Energy exchange.
- Detuning.
- Transient coupling behavior.

## Phase E — Dynamic and nonlinear resonance

- Transient analysis.
- Ring-down.
- Amplitude-dependent behavior.
- Parametric resonance interfaces.
- Nonlinear solver integration where available.

## Phase F — Resonance tuning and exploration

- Geometry-driven tuning.
- Material-driven tuning.
- Environmental sensitivity.
- Mode tracking.
- Parameter sweeps.
- Resonance objectives.

## Phase G — GUI and automation

- Integrate resonance workflows.
- Implement synchronized visualizations.
- Expose application services.
- Persist engineering definitions and results.

## Phase H — Validation and release readiness

- Execute analytical benchmarks.
- Run numerical convergence tests.
- Verify energy conservation.
- Test multiphysics coupling.
- Run regression tests.
- Update documentation.

If an implementation phase depends on missing capabilities, report the dependency explicitly.

Do not fabricate results to make the phase appear complete.

---

# 22. PERFORMANCE AND ARCHITECTURAL REQUIREMENTS

- Preserve modular solver architecture.
- Keep domain-specific physical equations separate.
- Keep physics independent of rendering.
- Reuse geometry and meshes when dependencies remain unchanged.
- Track result validity through revisions.
- Keep expensive operations asynchronous.
- Support cancellation and progress reporting.
- Avoid unnecessary data copying.
- Preserve CPU reference implementations.
- Use existing CUDA acceleration where appropriate.
- Support frequency and transient analyses without unnecessary recomputation.
- Keep external scientific dependencies optional where practical.
- Document dependency licenses.
- Avoid monolithic resonance-management classes.
- Do not put physics calculations in the GUI.

---

# 23. DOCUMENTATION

Create or update documentation covering:

- Resonance architecture.
- Natural modes and eigenmode analysis.
- Driven resonance.
- Energy storage and exchange.
- Damping and losses.
- Quality factor.
- Coupled resonators.
- Mode splitting.
- Resonance tuning.
- Nonlinear resonance.
- Transient response.
- Mode tracking.
- Reference engineering examples.
- CEME integration.
- Automation API.
- Scientific assumptions.
- Validation methodology.

Update the documentation established by Prompts 17, 18, and 19.

Include architecture diagrams and representative workflows.

Keep documentation synchronized with actual implementation.

Clearly distinguish implemented functionality, partial functionality, and future roadmap items.

---

# 24. ACCEPTANCE CRITERIA

This work is complete only when:

1. The resonance architecture integrates with existing geometry and physics systems.
2. The system can calculate natural modes for at least one validated physical model.
3. The system can calculate driven resonant response.
4. The system can identify and characterize resonances.
5. The system can calculate stored energy using appropriate physical models.
6. The system can distinguish stored, supplied, dissipated, transferred, and extracted energy.
7. The system can calculate and validate Q factor.
8. At least the microwave cavity, optical resonator, Helmholtz resonator, and mechanical cantilever have real model implementations or explicitly documented solver-backed limitations.
9. A two-resonator coupled system is implemented and validated.
10. Mode splitting and energy exchange are demonstrated.
11. Geometry-driven resonance tuning works.
12. Parameter sweeps and mode tracking work.
13. Transient buildup and decay are supported for at least one validated model.
14. CEME integration works where physical energy transfer requires it.
15. Results contain sufficient provenance to reproduce the analysis.
16. GUI, Python, and automation use the same underlying application services.
17. Existing projects remain compatible.
18. Existing regression tests pass.
19. Scientific validation tests pass within documented tolerances.
20. Documentation accurately describes actual capabilities and limitations.

---

# 25. FINAL ENGINEERING REPORT

At completion, provide a detailed engineering report containing:

- Repository audit findings.
- Existing capabilities reused.
- Architecture changes.
- Files and modules changed.
- Resonance models implemented.
- Physical equations and assumptions.
- Solvers and external packages integrated.
- Energy-storage and accounting implementation.
- Coupled resonator implementation.
- Validation results and tolerances.
- Test results.
- Performance observations.
- GUI workflow changes.
- Automation API additions.
- Serialization changes.
- Known limitations.
- Incomplete work and reasons.
- Dependency and licensing implications.
- Recommended next development prompt.

Do not claim a capability is implemented unless it is present, integrated, and tested.

Do not conceal failed tests or unsupported physics.

---

# FINAL DIRECTIVE

The objective is to make resonance a genuine engineering discipline within RESONANT.

The software should progressively allow a user to ask:

- What natural modes does this structure support?
- At which frequencies does it resonate?
- How much energy can it store?
- Where is that energy physically located?
- How does energy move between different modes?
- How quickly does energy accumulate?
- How quickly does it decay?
- What mechanisms cause energy loss?
- How does coupling change the behavior of multiple resonators?
- Can I control the resonance through geometry or material changes?
- How does temperature or mechanical deformation shift the resonance?
- What happens when the system is driven beyond its linear operating range?
- How can I extract useful energy without destroying the resonant behavior I want?
- What are the physical limitations and engineering tradeoffs?

RESONANT must treat resonance as an emergent physical property of structures and materials, governed by real physical laws.

**Build an engineering environment that understands resonant behavior, energy storage, and physical coupling—not simply a collection of frequency-response plots.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: - (heuristic)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Implement, capability, must be able, objective
  - evidence: 'Implement' [head]; 'capability' [head]; 'capability' [head]; 'must be able' [head]; 'objective' [head]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: User: 'add this ticket. process in order' (queued after REQ-CHAT-20261126-017)

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

User: 'add this ticket. process in order' (queued after REQ-CHAT-20261126-017)

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000019
- Evidence: pending
