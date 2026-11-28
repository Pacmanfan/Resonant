# Request: # PROMPT 24 — UNIFIED ENERGY, MOMENTUM, FORCE AND CONSERVATION FRAMEWORK

## Pro

**Request ID:** REQ-CHAT-20261126-022
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T23:49:11-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 24 — UNIFIED ENERGY, MOMENTUM, FORCE AND CONSERVATION FRAMEWORK

## Project: RESONANT — Wave Field Engineering Studio

## Mission

Implement the next major scientific foundation in RESONANT: **Unified Energy, Momentum, Force and Conservation Accounting**.

RESONANT now has an architectural foundation for geometry-driven energy transformation, resonance, metamaterials, multiscale material modeling, and dynamic material evolution.

The next requirement is to establish a rigorous physical accounting framework connecting these capabilities.

Every physical simulation involves some combination of:

- Energy entering a system.
- Energy leaving a system.
- Energy stored within a system.
- Energy transferred between physical domains.
- Energy converted from one form to another.
- Energy dissipated or irreversibly transformed.
- Momentum transported by matter and fields.
- Forces acting between objects and physical domains.
- Changes in motion and angular momentum.
- Exchanges across boundaries.
- Conservation relationships and numerical residuals.

RESONANT must be able to account for these processes consistently.

The goal is to make physical accounting a first-class capability of the architecture rather than leaving energy, force, momentum, and power calculations scattered across individual solvers.

The governing concept is:

**Physical Sources + Boundary Exchanges + Initial State → Physical Evolution → Stored Energy and Momentum + Transfers + Dissipation + Final State**

The system must be able to determine whether its calculations are physically and numerically consistent, identify missing or duplicated contributions, and explain where quantities flow throughout a simulation.

This is not an attempt to replace domain-specific physical equations with one universal equation.

It is an attempt to create a common accounting and verification architecture that allows different physical domains to exchange information while retaining their correct governing physics.

---

# 1. Governing Physical Principles

Establish the following distinctions throughout RESONANT.

## 1.1 Energy

Energy is a conserved physical quantity for an appropriately defined isolated system.

Energy may exist in different forms, including:

- Electromagnetic field energy.
- Electric and magnetic potential energy.
- Optical energy.
- Acoustic energy.
- Elastic strain energy.
- Kinetic energy.
- Gravitational potential energy where modeled.
- Thermal internal energy.
- Chemical energy.
- Phase-transition energy.
- Molecular and atomic energy.
- Stored energy in material polarization or magnetization where applicable.
- Other domain-specific forms.

Energy can move between these forms and between subsystems.

Energy accounting must identify those transfers without double-counting them.

## 1.2 Power

Power is the rate of energy transfer.

Support power as a first-class physical quantity.

Examples:

- Electromagnetic power flow.
- Mechanical power.
- Thermal power transfer.
- Acoustic power.
- Electrical power.
- Power dissipated through material losses.
- Power transferred between coupled domains.

Power must be capable of being represented as a function of time and, where appropriate, position.

## 1.3 Momentum

Momentum is a conserved quantity in appropriately defined isolated systems.

It can be carried by:

- Moving matter.
- Mechanical waves.
- Electromagnetic fields.
- Fluids.
- Other physical systems supported by the model.

Momentum may transfer between fields, matter, structures, and external environments.

The accounting framework must recognize that momentum can be transported through boundaries and that forces can change the momentum of an individual subsystem even when total system momentum is conserved.

## 1.4 Force

Force is not itself a conserved quantity.

It is an interaction that changes momentum.

The system must distinguish:

- Applied forces.
- Reaction forces.
- Internal forces.
- External forces.
- Contact forces.
- Field-mediated forces.
- Distributed forces.
- Body forces.
- Surface tractions.
- Constraint forces.

Force calculations must retain their physical origin and direction.

## 1.5 Angular Momentum and Torque

Angular momentum must be treated as a first-class physical quantity where the modeled physics supports it.

Support:

- Angular momentum.
- Torque.
- Rotational inertia.
- Angular velocity.
- Angular acceleration.
- Angular momentum transfer.
- External torque.
- Angular momentum flux.

Do not assume that linear momentum accounting alone is sufficient for systems involving rotation, torque, or field-mediated angular momentum transfer.

---

# 2. Audit the Existing Architecture Before Implementation

Perform a comprehensive audit of the existing repository.

Inspect:

- CEME energy and state accounting.
- Electromagnetic field and power calculations.
- Optical energy and momentum representations.
- Acoustic energy and intensity calculations.
- Mechanical force, stress, and momentum calculations.
- Thermal energy and heat-transfer models.
- Fluid and pressure calculations where supported.
- Material constitutive models.
- Dynamic material state and internal energy.
- Resonance and stored-energy calculations.
- Metamaterial effective-property calculations.
- Multiscale property derivation.
- Geometry and boundary representations.
- Simulation time and spatial field infrastructure.
- Numerical solvers and integration methods.
- Existing energy-conservation checks.
- Existing force and momentum calculations.
- Serialization and result reporting.
- Automation and GUI infrastructure.

Determine:

1. Which physical quantities are already calculated.
2. Which quantities are calculated independently in different subsystems.
3. Which quantities are currently missing.
4. Whether existing energy terms are duplicated.
5. Whether energy transfers between domains are explicitly represented.
6. Whether boundary fluxes are accounted for.
7. Whether numerical conservation errors are measured.
8. Whether forces and momentum transfers are traceable to their physical origins.

Produce an architecture map.

Reuse existing systems where possible.

Do not create a second energy engine, a competing physical state framework, or a parallel set of force calculations.

The new framework should coordinate and verify existing physical calculations.

---

# 3. Unified Physical Quantity Architecture

Implement a common physical quantity representation.

The purpose is to give the accounting framework a consistent way to identify, describe, compare, and aggregate physical quantities.

Each quantity must retain its physical meaning.

A quantity record should support:

- Quantity identity.
- Quantity type.
- Physical domain.
- Numerical value or field reference.
- SI units.
- Spatial region.
- Coordinate system.
- Simulation time or time interval.
- Source subsystem.
- Associated material or object.
- Direction where applicable.
- Provenance.
- Numerical uncertainty.
- Validity information.

Support scalar, vector, tensor, field, and time-dependent quantities as appropriate.

## 3.1 Units and Dimensional Consistency

Implement or extend a robust dimensional analysis framework.

All physical accounting operations must respect units.

At minimum, support the dimensions needed for:

- Energy.
- Power.
- Force.
- Linear momentum.
- Angular momentum.
- Torque.
- Mass.
- Velocity.
- Acceleration.
- Pressure.
- Stress.
- Energy density.
- Power density.
- Momentum flux.
- Energy flux.
- Temperature.
- Heat capacity.
- Other quantities already used by supported physical domains.

Use SI as the canonical internal system unless the existing architecture establishes a better compatible convention.

Support display conversion without changing the underlying physical value.

Reject or flag invalid dimensional operations.

For example:

- Energy cannot be added directly to power.
- Force cannot be added to momentum.
- Torque and energy share dimensional units but are different physical quantities and must not be treated as interchangeable.
- Angular momentum and energy must retain distinct identities.

Dimensional correctness is necessary but not sufficient. Physical quantity types must also be preserved.

## 3.2 Typed Physical Quantities

A quantity must carry semantic information beyond its numerical units.

For example, distinguish:

- Stored energy.
- Transferred energy.
- Dissipated energy.
- Radiated energy.
- Kinetic energy.
- Thermal energy.
- Elastic strain energy.

Likewise distinguish:

- Applied force.
- Reaction force.
- Contact force.
- Field-mediated force.
- Internal force.
- External force.

Do not allow unrelated quantities to be combined simply because their dimensions happen to match.

---

# 4. Physical System Boundaries and Control Volumes

Establish a common architecture for defining the system being analyzed.

Energy and momentum accounting depend on what is included in the system.

A simulation may represent:

- A closed system.
- An open system.
- A control volume.
- A single object.
- Multiple interacting objects.
- A physical field region.
- A coupled multiphysics assembly.
- A complete engineered device.
- A subsystem embedded in a larger environment.

Implement explicit system and control-volume definitions.

Each accounting region must identify:

- Included physical objects.
- Included fields.
- Included material regions.
- Boundary geometry.
- Boundary conditions.
- External interactions.
- Allowed boundary fluxes.
- Relevant reference frames.
- Accounting time interval.

## 4.1 Internal and External Interactions

Distinguish interactions internal to the selected system from interactions crossing its boundary.

For example:

Two coupled resonators may exchange energy internally.

If both resonators are inside the same accounting system, that exchange is internal and must not be counted as energy entering or leaving the combined system.

If only one resonator is included, the same exchange becomes a boundary transfer.

The accounting architecture must support both perspectives without changing the underlying physical event.

## 4.2 Boundary Fluxes

Support boundary accounting for:

- Electromagnetic energy flux.
- Acoustic energy flux.
- Heat flux.
- Mechanical power flux.
- Fluid energy transport.
- Momentum flux.
- Mass transport where the underlying model supports it.

Boundary fluxes must be calculated using the appropriate domain-specific physics.

Do not assume every solver provides all necessary flux information.

Identify missing information and expose it explicitly.

---

# 5. Unified Energy Accounting

Implement an energy accounting framework that tracks physical energy through the system.

For a suitably defined system, establish a general accounting relationship:

\[
\Delta E_{\mathrm{stored}}
=
E_{\mathrm{in}}
-
E_{\mathrm{out}}
+
E_{\mathrm{generated}}
-
E_{\mathrm{dissipated}}
\]

This is an accounting structure, not a universal constitutive equation.

The exact terms and their interpretation must be determined by the physical model and system boundary.

In particular, internal conversion between energy forms must not be counted as external energy generation or loss.

## 5.1 Energy Categories

Implement explicit categories for:

### Input Energy

- External excitation.
- Applied mechanical work.
- Electrical input.
- Electromagnetic radiation entering the system.
- Acoustic input.
- Thermal input.
- Chemical energy input.
- Other supported sources.

### Stored Energy

- Field energy.
- Kinetic energy.
- Elastic energy.
- Thermal internal energy.
- Chemical energy.
- Magnetic energy.
- Dielectric energy.
- Resonant stored energy.
- Other physically supported storage mechanisms.

### Transferred Energy

- Energy exchanged between coupled physical domains.
- Energy transferred between objects.
- Energy transferred across interfaces.
- Energy transferred between material regions.

### Dissipated Energy

- Resistive losses.
- Dielectric losses.
- Magnetic losses.
- Viscous losses.
- Mechanical damping.
- Plastic dissipation.
- Fracture energy.
- Acoustic absorption.
- Radiation losses where they leave the defined system.
- Other supported irreversible processes.

### Output Energy

- Radiated energy.
- Transmitted energy.
- Reflected energy.
- Extracted electrical energy.
- Mechanical work delivered to an external load.
- Heat leaving the system.
- Other boundary outputs.

Do not treat every loss term as energy destroyed.

Energy dissipated from an organized physical mode may become thermal energy within the system.

The accounting must follow that conversion.

## 5.2 Energy Ledger

Implement a persistent, traceable energy ledger.

Every accounting contribution should record:

- Event identity.
- Source domain.
- Destination domain.
- Energy category.
- Amount.
- Units.
- Time interval.
- Spatial region.
- Associated object or material.
- Calculation method.
- Provenance.
- Uncertainty.
- Whether the contribution is internal or crosses the system boundary.

The ledger must make it possible to trace an energy contribution back to the physical calculation that produced it.

Avoid storing only final totals.

## 5.3 Stored Energy Versus Energy Flow

Maintain a strict distinction between:

- Energy stored at a particular time.
- Energy transferred during an interval.
- Instantaneous power.
- Cumulative energy transfer.

Do not confuse a high instantaneous power with a large accumulated energy.

Do not confuse a resonator's stored energy with the power being supplied to it.

This distinction is especially important for resonant and pulsed systems.

---

# 6. Energy Transfer Between Physical Domains

This is a central requirement.

RESONANT must be capable of tracking energy as it moves between different physical forms.

Establish explicit transfer interfaces between supported domains.

Examples:

### Electromagnetic to Thermal

Electromagnetic absorption produces heat.

Track:

1. Electromagnetic energy entering a material.
2. Electromagnetic energy absorbed.
3. Energy transferred into thermal internal energy.
4. Remaining electromagnetic energy.
5. Subsequent thermal evolution.

### Electromagnetic to Mechanical

Electromagnetic fields exert forces on matter.

Track field-to-matter momentum and energy transfer using the appropriate physical formulation.

### Mechanical to Thermal

Mechanical deformation and friction may produce heat.

Track elastic storage, irreversible dissipation, and thermal energy generation.

### Acoustic to Mechanical

Acoustic waves transfer energy and momentum to structures.

Track incident, reflected, transmitted, and absorbed acoustic energy where supported.

### Thermal to Mechanical

Temperature changes can cause expansion and mechanical work.

Track thermal energy, resulting deformation, and associated mechanical energy changes.

### Chemical to Thermal or Mechanical

Chemical reactions may release or absorb energy and alter material state.

Only implement where the relevant physical models exist.

## 6.1 Transfer Contracts

Every inter-domain transfer must identify:

- Source domain.
- Destination domain.
- Physical mechanism.
- Transferred quantity.
- Transfer rate or time-integrated amount.
- Spatial location.
- Relevant state variables.
- Sign convention.
- Associated uncertainty.
- Whether the source calculation already includes the destination contribution.

The last item is essential to prevent double-counting.

## 6.2 Internal Conversion

An internal transfer must appear consistently in both source and destination accounting.

For example:

Electromagnetic energy absorbed by a material and converted into heat must reduce the appropriate electromagnetic energy contribution and increase thermal energy.

It must not be counted as both an external energy input and a new energy source.

---

# 7. Momentum Accounting

Establish a first-class momentum accounting framework.

Momentum must be tracked across physical objects, material regions, and fields where supported.

The accounting must distinguish between:

- Momentum stored in moving matter.
- Momentum carried by fields.
- Momentum transferred through boundaries.
- Momentum exchanged between subsystems.
- Momentum supplied or removed by external interactions.

For a system with appropriately defined boundaries, implement a general momentum balance:

\[
\Delta \mathbf P
=
\mathbf P_{\mathrm{in}}
-
\mathbf P_{\mathrm{out}}
+
\mathbf I_{\mathrm{external}}
\]

where the external impulse is the time integral of the appropriate external force.

This is a bookkeeping structure. Actual momentum density, flux, and force must be calculated using the relevant domain-specific physics.

## 7.1 Mechanical Momentum

Support:

- Linear momentum.
- Angular momentum.
- Momentum of rigid bodies.
- Momentum of deformable bodies.
- Momentum of fluid regions where supported.
- Momentum transfer through contact and collision.
- Momentum transfer through applied forces.

## 7.2 Electromagnetic Momentum

Where supported, account for electromagnetic field momentum.

Investigate the appropriate formulation for the modeled physical system.

Potential concepts include:

- Electromagnetic momentum density.
- Poynting vector.
- Maxwell stress tensor.
- Lorentz force density.
- Electromagnetic momentum transfer to matter.

Do not assume that electromagnetic momentum can always be represented by a simple mechanical particle-momentum expression.

The treatment of momentum in material media can depend on the physical formulation and assumptions.

Document the formulation selected by each solver.

Do not silently mix incompatible conventions.

## 7.3 Acoustic and Fluid Momentum

Where supported, account for momentum transfer associated with:

- Acoustic radiation pressure.
- Fluid pressure.
- Fluid momentum transport.
- Acoustic streaming.
- Wave-mediated forces.
- Boundary tractions.

Use domain-specific equations.

## 7.4 Momentum Flux

Implement boundary momentum flux accounting where supported.

A system may experience momentum transfer even when no material crosses its boundary.

For example, radiation can transfer momentum to an absorbing surface.

The framework must support this possibility.

---

# 8. Force and Torque Architecture

Establish a unified architecture for collecting, identifying, and accounting for forces and torques.

Do not calculate every force through one universal mechanism.

Each force must originate from an appropriate physical model.

## 8.1 Force Categories

Support:

- Applied mechanical forces.
- Contact forces.
- Constraint forces.
- Elastic restoring forces.
- Viscous forces.
- Pressure forces.
- Gravity where modeled.
- Electromagnetic forces.
- Magnetic forces.
- Acoustic radiation forces.
- Radiation pressure.
- Other domain-specific forces.

## 8.2 Force Provenance

Every force contribution should record:

- Force identity.
- Originating model.
- Source object or field.
- Target object or region.
- Magnitude and direction.
- Coordinate system.
- Application location.
- Time dependence.
- Associated physical mechanism.
- Numerical uncertainty.

## 8.3 Distributed Forces and Stress

Support force distributions over surfaces and volumes.

Represent relevant quantities including:

- Body force density.
- Surface traction.
- Stress tensors.
- Pressure distributions.
- Field force density.

Where possible, provide a way to integrate distributed force contributions into resultant force and torque.

Do not discard spatial distribution information when calculating a total resultant.

## 8.4 Newton's Laws and Momentum Balance

Where the underlying mechanical model supports it, verify the relationship between force, momentum, and acceleration.

For a simple constant-mass system:

\[
\mathbf F_{\mathrm{net}} =
\frac{d\mathbf p}{dt}
\]

The system must not assume constant mass for every physical system.

For variable-mass systems, fluids, and field-coupled systems, use the appropriate momentum balance and boundary terms.

## 8.5 Action and Reaction

Support the identification of interacting force pairs where the underlying physical formulation provides them.

Do not assume every force contribution can be paired locally without accounting for fields, boundaries, and momentum transport.

For field-mediated interactions, momentum carried by the field may be essential to a complete balance.

---

# 9. Angular Momentum and Rotational Energy

Extend accounting to rotational systems.

Support:

- Angular momentum.
- Torque.
- Rotational kinetic energy.
- Rotational inertia.
- Angular velocity.
- Angular acceleration.
- Angular momentum flux.
- External torque.
- Rotational energy transfer.

Distinguish between torque and energy even though they have the same SI dimensions.

Support angular momentum exchange between:

- Rotating structures.
- Moving fluids.
- Mechanical resonators.
- Electromagnetic fields where supported.
- Other modeled physical systems.

Include rotational examples in the validation suite.

---

# 10. Energy Storage and Resonant Systems

Integrate directly with Prompt 20.

Resonant systems require a particularly careful distinction between:

- Energy supplied.
- Energy stored.
- Energy exchanged between modes or subsystems.
- Energy dissipated.
- Energy radiated.
- Energy extracted.

The accounting framework must support resonant energy buildup and decay.

## 10.1 Stored Energy

Where physically defined, track:

- Electric field energy.
- Magnetic field energy.
- Mechanical strain energy.
- Mechanical kinetic energy.
- Acoustic energy.
- Other domain-specific stored energy.

Do not assume all resonators store energy in the same way.

## 10.2 Resonant Energy Exchange

For coupled resonators, track energy exchange between the individual resonators.

If the combined system is isolated and lossless under the modeled assumptions, internal energy exchange must not appear as a net external energy loss.

## 10.3 Q Factor and Loss Accounting

Connect the ledger to resonance analysis.

Where applicable, relate stored energy and dissipated power to the appropriate quality-factor definitions.

For a suitable resonator under the relevant steady-state assumptions:

\[
Q = \omega_0
\frac{E_{\mathrm{stored}}}{P_{\mathrm{loss}}}
\]

Ensure that the stored energy and power loss use compatible definitions.

Distinguish internal, external, radiation, and material losses.

Do not force this expression onto systems for which the required assumptions do not hold.

## 10.4 Transient Energy

Support time-domain energy accounting for:

- Resonator startup.
- Energy buildup.
- Steady-state operation.
- Excitation removal.
- Energy decay.
- Coupled energy transfer.
- Energy extraction.

---

# 11. Dynamic Material State and Energy Dissipation

Integrate with Prompt 23.

Material state changes may store, transfer, or dissipate energy.

Examples:

- Elastic deformation stores recoverable energy.
- Plastic deformation dissipates energy.
- Viscous behavior dissipates mechanical energy.
- Damage may consume fracture energy.
- Phase transitions may absorb or release latent heat.
- Thermal expansion may perform mechanical work.
- Magnetic hysteresis dissipates energy.
- Dielectric hysteresis may dissipate energy.

Every implemented dynamic material model must expose its relevant energy contributions to the accounting framework.

## 11.1 Internal Energy Variables

Preserve the distinction between:

- Recoverable energy.
- Irrecoverable energy.
- Thermal internal energy.
- Stored field energy.
- Energy transferred to other domains.

Do not assume all work performed on a material is stored as elastic energy.

## 11.2 Hysteresis

Hysteretic models must expose their cycle energy and relevant dissipation.

The calculated hysteresis loss must be consistent with the constitutive response.

Do not calculate hysteresis energy independently from a graphical plot.

## 11.3 Material State Changes

When a material changes phase, accumulates damage, or otherwise evolves, the energy accounting system must receive the relevant contributions from the physical model.

Do not invent energy terms based on arbitrary property changes.

---

# 12. Thermodynamic Consistency and Entropy

Introduce an appropriate architecture for thermodynamic accounting.

Energy conservation alone is not sufficient to establish that every physical process is thermodynamically valid.

Where the underlying material and thermal models support it, track:

- Internal energy.
- Heat transfer.
- Work.
- Entropy.
- Entropy production.
- Temperature.
- Relevant thermodynamic potentials.
- Phase-transition contributions.

## 12.1 First Law

Support appropriate first-law accounting for the selected system.

The system must distinguish heat transfer, work, and internal energy changes using consistent sign conventions.

## 12.2 Second Law

Where thermodynamic models provide the necessary information, evaluate whether the modeled processes are consistent with the second law.

For irreversible processes, entropy production must satisfy the appropriate nonnegative constraint under the applicable formulation.

Do not claim that a general-purpose entropy calculation is available if the underlying solver does not provide sufficient thermodynamic information.

## 12.3 Open Systems

Where mass enters or leaves a control volume, account for relevant energy and entropy carried by the transported matter if the physical model supports it.

Do not apply closed-system thermodynamic equations to open systems without the required flow terms.

---

# 13. Conservation Laws Across Physical Domains

Create a common conservation-law registry.

The registry should identify which conservation laws apply to each simulation and which quantities are available to verify them.

Potential conservation relationships include:

### Energy
- Total energy.
- Internal energy.
- Mechanical energy.
- Field energy.
- Energy flux.
- Work and heat transfer.

### Linear Momentum
- Mechanical momentum.
- Field momentum.
- Momentum flux.
- External impulse.

### Angular Momentum
- Rotational momentum.
- Torque.
- Angular momentum flux.

### Mass
- Total mass.
- Mass flux.
- Sources and sinks where physically appropriate.

### Charge
- Charge density.
- Current.
- Charge flux.
- Charge conservation.

### Other Domain-Specific Quantities
- Species concentration.
- Chemical species conservation.
- Other conserved quantities required by supported solvers.

Do not assume that every quantity is conserved in every selected subsystem.

For example, charge or mass may cross a boundary.

The conservation framework must identify the relevant boundary terms.

## 13.1 Conservation Model Registry

Each conservation model should define:

- Conserved quantity.
- Relevant physical domain.
- System boundary.
- Source terms.
- Sink terms.
- Flux terms.
- Applicable equations.
- Required solver data.
- Numerical evaluation method.
- Validity assumptions.

The registry should make it possible to add future physical domains without rewriting the accounting architecture.

---

# 14. Numerical Conservation Monitoring

This is one of the most important implementation requirements.

A simulation can execute successfully while violating conservation due to numerical error, incorrect coupling, missing terms, or implementation bugs.

RESONANT must measure and report conservation residuals.

For an appropriately defined energy balance, calculate a residual such as:

\[
R_E =
\Delta E_{\mathrm{stored}}
-
\left(
E_{\mathrm{in}}
-
E_{\mathrm{out}}
+
E_{\mathrm{generated}}
-
E_{\mathrm{dissipated}}
\right)
\]

Use the sign convention consistently throughout the implementation.

For momentum, calculate the corresponding vector residual using the applicable momentum balance.

## 14.1 Residual Reporting

For each supported conservation check, report:

- Initial quantity.
- Final quantity.
- Integrated input.
- Integrated output.
- Internal transfers.
- Sources and sinks.
- Absolute residual.
- Relative residual where meaningful.
- Numerical tolerance.
- Time-step sensitivity.
- Spatial discretization sensitivity where relevant.
- Warning or failure status.

A residual must be interpretable.

Do not present a percentage error when the reference denominator is zero or too small to be meaningful.

## 14.2 Conservation Status

Distinguish between:

- Verified within tolerance.
- Within warning threshold.
- Exceeds tolerance.
- Not evaluated.
- Insufficient data.
- Not applicable.

Do not display a green success state for an accounting check that was never performed.

## 14.3 Local and Global Conservation

Support global system accounting.

Where the underlying solver permits, support local or spatial conservation diagnostics.

A globally balanced result can conceal local numerical errors.

The system should not claim local conservation when only a global total has been checked.

## 14.4 Numerical Error Versus Physical Loss

Distinguish between:

- Physical dissipation.
- Energy transported out of the system.
- Numerical dissipation.
- Numerical instability.
- Unaccounted residual.

Numerical error must never be silently labeled as material loss or physical energy conversion.

---

# 15. Multiscale Conservation and Homogenization

Integrate with Prompt 22.

Multiscale material models introduce additional accounting challenges.

A microscopic model may calculate energy or momentum transfers that must be reflected in an effective material description.

Establish interfaces for preserving appropriate accounting information across scales.

Examples:

- Atomistic potential energy contributing to effective material behavior.
- Microstructural deformation contributing to macroscopic strain energy.
- Defect evolution contributing to stored or dissipated energy.
- Microscale thermal transport contributing to effective conductivity.
- Effective constitutive response contributing to macroscopic mechanical work.

Avoid double-counting microscopic and macroscopic energy contributions.

When an effective material replaces an explicitly modeled microstructure, the accounting framework must know which representation is active.

It must not simultaneously count the energy of both representations as independent physical systems unless the model explicitly represents both.

## 15.1 Scale Transfer Contracts

Each cross-scale accounting transfer must identify:

- Source representation.
- Target representation.
- Physical quantity.
- Transfer method.
- Averaging or homogenization method.
- Spatial and temporal resolution.
- Conservation assumptions.
- Uncertainty.
- Whether the source-scale energy remains explicitly represented.

## 15.2 Effective Material Accounting

An effective material may store and transfer energy differently from its individual constituents.

The framework must support the physical model's effective constitutive energy rather than assuming that all effective properties can be decomposed into simple constituent scalar quantities.

---

# 16. Geometry, Boundaries and Physical Interaction

Integrate with Prompt 19.

Geometry determines where physical interactions occur and how energy, momentum, and forces are distributed.

Support accounting across:

- Object boundaries.
- Material interfaces.
- Internal surfaces.
- Contact regions.
- Openings.
- Waveguide boundaries.
- Resonator boundaries.
- Control-volume surfaces.
- Other relevant physical interfaces.

Where appropriate, associate force, traction, energy flux, and momentum flux with the actual geometric surface.

Geometry changes must update the corresponding accounting regions and boundary definitions.

For moving or deforming boundaries, use appropriate physical formulations.

Do not assume a fixed control volume when the actual system boundary moves.

---

# 17. Metamaterial and Engineered Energy Transformation

Integrate with Prompt 21.

Metamaterials and structured materials may redirect, confine, amplify local field intensity, alter momentum transfer, or change the distribution of energy.

RESONANT must distinguish between:

- Energy entering a unit cell.
- Energy reflected.
- Energy transmitted.
- Energy absorbed.
- Energy stored temporarily.
- Energy transferred to other modes.
- Energy leaving through other channels.

A local field enhancement is not automatically an increase in total energy.

The system must distinguish between energy density, field amplitude, total stored energy, and power flow.

For passive systems, any apparent energy amplification must be explained through energy redistribution, resonance, or an explicitly modeled energy source.

For active or externally powered systems, include the supplied energy in the accounting.

This distinction is essential for scientifically credible engineered energy transformation.

---

# 18. Visualization and User Experience

Create a coherent physical accounting interface integrated with existing RESONANT workflows.

The accounting system should help users understand the physical behavior of their engineering system.

## 18.1 Energy Flow Explorer

Provide a visualization of energy movement through the modeled system.

Display relevant nodes for:

- Physical objects.
- Material regions.
- Physical domains.
- Energy storage.
- Energy transfer.
- Dissipation.
- External sources.
- System outputs.

Connect them with directional flows.

Use actual calculated quantities.

Do not use arbitrary decorative flow widths or percentages.

## 18.2 Energy Breakdown

Provide a breakdown of:

- Input energy.
- Stored energy.
- Internal transfers.
- Dissipated energy.
- Output energy.
- Unaccounted residual.

Allow the user to drill into individual contributions.

## 18.3 Momentum and Force Explorer

Provide a way to inspect:

- Forces acting on objects.
- Force origins.
- Force directions.
- Resultant force.
- Torque.
- Linear momentum.
- Angular momentum.
- Momentum transfer.
- Boundary momentum flux where available.

Use existing vector and field visualization systems.

## 18.4 Conservation Dashboard

Provide a clear view of conservation status.

Display:

- Energy residual.
- Momentum residual.
- Other supported conservation residuals.
- Tolerance.
- Time evolution.
- Numerical warnings.
- Missing accounting contributions.

Users should be able to inspect why a balance is not satisfied.

## 18.5 Time-Resolved Accounting

Support time-dependent accounting.

Users should be able to inspect:

- Instantaneous power.
- Cumulative energy.
- Stored energy over time.
- Energy exchange between domains.
- Force over time.
- Momentum evolution.
- Conservation residual over time.

## 18.6 Spatial Accounting

Where supported, visualize spatial distributions of:

- Energy density.
- Power density.
- Force density.
- Stress and traction.
- Momentum density.
- Energy flux.
- Momentum flux.
- Dissipation density.

Use the appropriate physical definitions for each domain.

## 18.7 Object and Subsystem Accounting

Allow the user to select a subsystem and inspect its accounting separately from the larger system.

The same physical event must be correctly classified as internal or external depending on the selected boundary.

This is a particularly important usability and correctness requirement.

---

# 19. Automation and Programmatic Access

Extend the existing automation architecture.

Do not create a separate accounting automation subsystem.

Expose supported operations including:

- Define accounting system.
- Define control volume.
- Inspect physical quantities.
- Query energy ledger.
- Query power flow.
- Query stored energy.
- Query dissipated energy.
- Query energy transfers.
- Query force contributions.
- Query momentum.
- Query angular momentum.
- Query torque.
- Evaluate conservation.
- Inspect conservation residuals.
- Compare subsystem accounting.
- Export accounting reports.
- Identify missing accounting data.

Automation responses must include:

- Quantity identity.
- Value.
- Units.
- Physical domain.
- Spatial and temporal context.
- Provenance.
- Accounting status.
- Uncertainty where available.

Do not return a bare numerical total without its physical meaning and context.

---

# 20. Persistence and Reproducibility

Persist the accounting information necessary to reproduce and inspect a simulation.

Store:

- Accounting system definitions.
- Control volumes.
- Energy ledger entries.
- Momentum ledger entries.
- Force contributions.
- Boundary fluxes.
- Transfer records.
- Conservation models.
- Conservation residuals.
- Numerical tolerances.
- Solver provenance.
- Time history.
- Relevant spatial field references.

Support selective retention for large simulations.

Do not require the full duplication of every underlying simulation field if the ledger can reference existing stored results.

A saved project must preserve enough information to reconstruct the accounting interpretation.

Maintain project compatibility and version migrations.

---

# 21. Performance and Numerical Robustness

Accounting must not make ordinary simulations unnecessarily expensive.

Use appropriate approaches for:

- Incremental ledger updates.
- Aggregation.
- Spatial integration.
- Time integration.
- Boundary flux calculations.
- Vector and tensor operations.
- Conservation checks.
- Result caching.
- Selective history retention.

Avoid repeatedly recalculating physical quantities already available from the solvers.

Accounting must consume authoritative solver outputs where possible.

Do not introduce a second, slightly different implementation of an existing physical calculation merely to populate the ledger.

For large simulations, allow users to select:

- Global accounting.
- Subsystem accounting.
- Spatial accounting.
- Time-resolved accounting.
- Detailed provenance.

Where possible, provide configurable monitoring frequency.

Do not sacrifice accounting correctness for visualization performance.

---

# 22. Scientific Validation and Benchmarking

Validation is mandatory.

Use simple analytical systems before attempting complicated coupled simulations.

## 22.1 Closed Mechanical System

Create a simple isolated mechanical system.

Verify that:

- Initial and final momentum are consistent.
- Energy exchanges between kinetic and potential forms are tracked.
- Numerical residuals are reported.
- External contributions are absent or explicitly accounted for.

## 22.2 Mechanical Work and Dissipation

Create a mechanical system with a supported damping mechanism.

Verify:

- Applied work.
- Stored mechanical energy.
- Dissipated energy.
- Resulting thermal contribution where modeled.
- Total accounting consistency.

## 22.3 Resonant Energy Buildup and Decay

Use a supported resonator.

Verify:

- Input power.
- Stored energy.
- Internal loss.
- External or radiation loss.
- Energy buildup.
- Energy decay after excitation removal.

Compare the results against the expected analytical behavior under the chosen assumptions.

## 22.4 Coupled Resonators

Create two coupled resonators.

Verify:

- Energy transfer between resonators.
- Internal transfer classification.
- Combined-system accounting.
- Individual subsystem accounting.
- Energy conservation under appropriate lossless conditions.

## 22.5 Electromagnetic Absorption

Use a supported electromagnetic material model.

Verify:

- Incident energy.
- Reflected energy.
- Transmitted energy.
- Absorbed energy.
- Thermal transfer where coupled.
- Accounting residual.

## 22.6 Momentum Transfer

Where the relevant solver supports it, implement a benchmark involving field-mediated momentum transfer.

For example, electromagnetic radiation acting on an absorbing surface.

Verify the selected momentum formulation and boundary flux accounting.

## 22.7 Dynamic Material Evolution

Use a Prompt 23 material model.

Verify that:

- Reversible energy storage is represented.
- Irreversible dissipation is accounted for.
- Internal transfers are not counted twice.
- State evolution remains physically consistent.

## 22.8 Numerical Convergence

Repeat selected tests with different:

- Time steps.
- Mesh resolutions.
- Solver tolerances.
- Spatial integration resolutions.

Demonstrate how accounting residuals change with numerical refinement.

Do not assume a small residual automatically proves that the physical model is correct.

---

# 23. Implementation Strategy

Implement the capability in deliberate phases.

## Phase 1 — Architecture Audit and Quantity Model

- Audit existing energy, force, and momentum systems.
- Establish physical quantity types.
- Establish units and dimensional validation.
- Establish system boundaries and control volumes.
- Define accounting contracts.
- Identify existing authoritative calculations.

## Phase 2 — Energy Ledger

- Implement energy categories.
- Implement transfer records.
- Implement boundary input and output.
- Implement stored-energy tracking.
- Implement dissipation tracking.
- Implement provenance.
- Add a simple validated energy balance.

This is the first major deliverable.

## Phase 3 — Force and Momentum

- Implement force provenance.
- Implement force aggregation.
- Implement linear momentum accounting.
- Implement angular momentum and torque interfaces.
- Integrate supported boundary momentum fluxes.
- Add mechanical benchmark tests.

## Phase 4 — Cross-Domain Transfers

- Implement transfer contracts.
- Integrate electromagnetic and thermal accounting where supported.
- Integrate mechanical and thermal accounting where supported.
- Integrate acoustic or other supported domains.
- Verify internal transfer consistency.

## Phase 5 — Conservation Monitoring

- Implement residual calculations.
- Implement numerical tolerances.
- Implement global and supported local checks.
- Implement warning and failure statuses.
- Add time-resolved monitoring.
- Validate numerical convergence.

## Phase 6 — Resonance and Dynamic Material Integration

- Connect stored-energy accounting to Prompt 20.
- Connect dynamic material energy to Prompt 23.
- Track energy exchange and dissipation.
- Integrate state-dependent accounting.
- Validate coupled physical examples.

## Phase 7 — Multiscale and Metamaterial Integration

- Connect accounting to Prompt 22.
- Connect effective material representations to Prompt 21.
- Implement appropriate scale-transfer contracts.
- Prevent double-counting.
- Validate structured-material examples.

## Phase 8 — User Experience and Automation

- Implement energy flow exploration.
- Implement force and momentum inspection.
- Implement conservation dashboards.
- Add subsystem accounting.
- Expose accounting APIs.
- Add report export.

## Phase 9 — Validation and Documentation

- Run all physical benchmarks.
- Run numerical convergence tests.
- Run integration tests.
- Evaluate performance.
- Document supported accounting methods.
- Document limitations.
- Produce the final engineering report.

Prioritize a small number of complete, validated accounting pathways over a large number of empty domain integrations.

---

# 24. Explicit Scientific Guardrails

These requirements are mandatory.

1. Do not treat force as a conserved quantity.
2. Do not confuse energy with power.
3. Do not confuse torque with energy merely because they share dimensions.
4. Do not confuse momentum with force.
5. Do not assume that every subsystem conserves energy independently.
6. Do not classify internal transfers as external energy inputs or losses.
7. Do not double-count energy when converting between physical domains.
8. Do not label numerical error as physical dissipation.
9. Do not assume all energy leaving an organized mode leaves the complete system.
10. Do not assume field momentum can always be ignored.
11. Do not mix incompatible electromagnetic momentum formulations without explicitly identifying the assumptions.
12. Do not infer force from a visual field representation without a valid physical calculation.
13. Do not assume a global conservation check proves local conservation.
14. Do not claim conservation has been verified when required boundary fluxes or source terms are missing.
15. Do not use dimensionally valid but physically unrelated quantities interchangeably.
16. Do not treat a high local field amplitude as proof of increased total energy.
17. Do not create universal energy equations that replace domain-specific physics.
18. Do not silently omit energy or momentum contributions that are unavailable.
19. Do not claim thermodynamic consistency without the necessary physical state and entropy information.
20. Do not compromise the existing physical solvers merely to make accounting totals appear balanced.

The accounting framework must be capable of reporting that a physical balance is incomplete or cannot currently be evaluated.

That is preferable to manufacturing a false result.

---

# 25. Acceptance Criteria

Prompt 24 is complete when the following conditions are satisfied.

### Physical Quantity Architecture

- [ ] Physical quantities have consistent identities and units.
- [ ] Dimensional validation is operational.
- [ ] Energy, power, force, momentum, torque, and angular momentum remain distinct.
- [ ] Quantity provenance is preserved.
- [ ] Spatial and temporal context is supported.

### Energy Accounting

- [ ] An energy ledger exists.
- [ ] Stored, transferred, dissipated, and exported energy are distinguishable.
- [ ] Internal domain transfers are represented explicitly.
- [ ] Boundary energy flows can be accounted for where supported.
- [ ] At least one complete energy balance is validated.

### Force and Momentum

- [ ] Force contributions retain physical origin and direction.
- [ ] Linear momentum accounting is operational for at least one supported physical system.
- [ ] Angular momentum and torque interfaces are established.
- [ ] Momentum transfer and external impulse are distinguished.
- [ ] At least one relevant momentum benchmark is validated where supported.

### Conservation Monitoring

- [ ] Energy residuals can be calculated.
- [ ] Momentum residuals can be calculated for supported models.
- [ ] Numerical tolerances and warning states are implemented.
- [ ] Physical dissipation is distinguished from numerical error.
- [ ] Missing accounting contributions are reported honestly.
- [ ] Time-resolved monitoring is supported.

### Cross-Domain Integration

- [ ] At least one genuine cross-domain energy transfer is operational.
- [ ] Internal transfers are not double-counted.
- [ ] CEME can access accounting results.
- [ ] Prompt 20 resonance accounting is integrated where supported.
- [ ] Prompt 22 and 23 interfaces preserve relevant energy and state information.
- [ ] Prompt 21 effective material accounting is supported where applicable.

### User Experience and Automation

- [ ] Users can inspect energy flows.
- [ ] Users can inspect force and momentum contributions.
- [ ] Users can inspect conservation residuals.
- [ ] Users can select and inspect subsystem accounting.
- [ ] Supported accounting functions are available through existing automation.
- [ ] Results can be persisted and reproduced.

### Scientific Quality

- [ ] Analytical benchmark tests pass.
- [ ] Numerical convergence tests pass.
- [ ] Regression tests pass.
- [ ] Documentation identifies supported conservation laws and limitations.
- [ ] No unsupported conservation claims are presented.

**Completion does not require every physical domain to be fully integrated. It requires a common accounting architecture, validated energy accounting, working force and momentum interfaces, and at least one meaningful cross-domain transfer pathway.**

---

# 26. Final Engineering Report

When implementation is complete, provide a detailed report containing:

1. Existing architecture audit.
2. Unified physical quantity architecture.
3. Units and dimensional validation.
4. System boundary and control-volume implementation.
5. Energy ledger architecture.
6. Energy storage and transfer implementation.
7. Force and momentum architecture.
8. Angular momentum and torque support.
9. Cross-domain transfer integrations.
10. Conservation monitoring and residual calculations.
11. Numerical stability and convergence.
12. Thermodynamic accounting capabilities.
13. Resonance and dynamic material integration.
14. Multiscale and metamaterial integration.
15. GUI and automation changes.
16. Scientific benchmark results.
17. Test results.
18. Performance observations.
19. Known limitations.
20. Deferred capabilities.
21. Recommended next engineering steps.

Identify the relevant source files, modules, interfaces, and tests.

Clearly distinguish implemented accounting methods from planned interfaces.

---

# Final Directive

RESONANT must be able to explain the physical accounting of an engineered system.

When energy enters a structure, the system should be able to identify where it goes.

When a material absorbs energy, the system should be able to identify what changes.

When a resonator stores energy, the system should be able to quantify that storage and follow its subsequent transfer or dissipation.

When fields exert forces on matter, the system should be able to account for the resulting momentum transfer.

When a physical simulation produces an unexpected result, the system should be able to investigate whether the result comes from real physical behavior, an incomplete model, or numerical error.

This capability must connect all of the existing RESONANT architecture without erasing the distinctions between physical domains.

**We are establishing the physical accounting system that makes every other capability measurable, explainable, and scientifically accountable.**

Implement it as a cohesive extension of RESONANT, preserving existing solver physics and prioritizing genuine conservation verification over superficial numerical balance.

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: - (heuristic)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: FRAMEWORK, Implement, capabilities, capability
  - evidence: 'FRAMEWORK' [head]; 'Implement' [head]; 'framework' [head]; 'capabilities' [head]; 'must be able' [head]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: User: 'add and work in order' (queued after REQ-CHAT-20261126-021, Prompt 23)

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

User: 'add and work in order' (queued after REQ-CHAT-20261126-021, Prompt 23)

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000023
- Evidence: pending
