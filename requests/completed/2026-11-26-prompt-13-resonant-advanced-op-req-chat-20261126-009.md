# Request: # PROMPT 13 — RESONANT: ADVANCED OPTIMIZATION, INVERSE DESIGN, GEOMETRY OPTIMIZA

**Request ID:** REQ-CHAT-20261126-009
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T15:48:53-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 13 — RESONANT: ADVANCED OPTIMIZATION, INVERSE DESIGN, GEOMETRY OPTIMIZATION AND AUTOMATED ENGINEERING

You are continuing development of **RESONANT — Wave Field Engineering Studio**.

This milestone introduces a unified optimization and inverse-design framework across all supported physics domains:

1. Acoustic wave engineering.
2. Mechanical wave engineering.
3. Electromagnetic wave engineering.

The objective is to allow users to specify a desired physical result and have Resonant determine configurations that can achieve it.

Rather than requiring users to manually adjust every source, emitter, material, geometric feature, or excitation parameter, Resonant should provide tools to explore the design space and identify promising solutions.

Examples:

- Focus acoustic energy at a specified point or across a three-dimensional region.
- Design a reflector that produces a desired focal distribution.
- Optimize an ultrasonic transducer array.
- Shape vibration modes in a plate or mechanical structure.
- Minimize vibration at selected locations.
- Optimize an antenna array for a particular radiation pattern.
- Steer multiple electromagnetic beams.
- Suppress radiation in unwanted directions.
- Optimize a waveguide or resonant cavity.
- Find geometry and material configurations that produce a specified response.

This must become a reusable engineering capability, not a collection of unrelated optimization buttons.

Build on the existing physics, geometry, material, simulation, measurement, and experiment architectures.

Do not replace or weaken existing validated simulation capabilities.

---

## 1. Mission and objectives

Implement a unified optimization framework supporting:

- Inverse design.
- Source optimization.
- Array excitation optimization.
- Geometry optimization.
- Material optimization.
- Topology optimization.
- Multi-objective optimization.
- Constrained optimization.
- Parameter sweeps.
- Design-space exploration.
- Sensitivity analysis.
- Robust optimization.
- Uncertainty-aware optimization.
- Automated candidate generation.
- Candidate comparison.
- Optimization history.
- Design validation.
- CAD and geometry export.

Users must be able to define:

1. What they want to achieve.
2. What variables Resonant is allowed to change.
3. What constraints must be respected.
4. Which physical model evaluates candidates.
5. How candidate quality is measured.

The optimization engine should produce a set of candidate designs, not merely one opaque answer.

Every result must be traceable to its inputs, solver configuration, objective functions, constraints, and optimization history.

## 2. Unified optimization architecture

Introduce a physics-independent optimization subsystem.

Create core abstractions for:

- OptimizationProblem.
- DesignVariable.
- DesignParameter.
- DesignBounds.
- DesignConstraint.
- ObjectiveFunction.
- ObjectiveEvaluation.
- OptimizationAlgorithm.
- OptimizationRun.
- CandidateDesign.
- CandidateEvaluation.
- OptimizationResult.
- OptimizationHistory.
- DesignComparison.
- ValidationRun.

The optimization framework must not contain acoustic, mechanical, or electromagnetic equations directly.

Instead, it should communicate with domain-specific physics adapters.

Each physics domain must expose supported optimization capabilities through explicit interfaces.

For example:

- Acoustic adapter.
- Mechanical adapter.
- Electromagnetic adapter.

Each adapter must identify which variables, objectives, constraints, and solver types it supports.

Do not allow an optimization algorithm to silently assume that all physics domains expose the same quantities.

Use typed physical quantities and appropriate unit handling.

## 3. Design-variable framework

Implement a flexible design-variable system.

Support variable categories:

**Continuous**
- Position.
- Dimensions.
- Radius.
- Thickness.
- Spacing.
- Material parameters.
- Frequency.
- Amplitude.
- Phase.
- Delay.
- Orientation.

**Discrete**
- Material selection.
- Component selection.
- Number of emitters.
- Number of array elements.
- Structural configuration.
- Geometry type.

**Categorical**
- Antenna type.
- Reflector type.
- Solver choice.
- Material family.
- Array layout.

**Derived**
- Wavelength.
- Element spacing as a fraction of wavelength.
- Resonant dimensions.
- Geometric relationships.
- Dependent dimensions.

Support:

- Minimum and maximum values.
- Fixed values.
- Initial values.
- Units.
- Step sizes.
- Parameter dependencies.
- Design groups.
- Variable activation and deactivation.

Allow users to lock parameters.

Allow multiple variables to be grouped into meaningful design parameters.

For example, a reflector can expose focal length, aperture diameter, depth, and material while keeping its parametric definition valid.

Do not permit an optimization algorithm to create invalid geometry without detecting it.

## 4. Objective-function framework

Implement reusable objective definitions.

Support:

- Minimize.
- Maximize.
- Match a target value.
- Match a target distribution.
- Satisfy a threshold.
- Minimize deviation.
- Maximize efficiency.
- Minimize energy outside a desired region.

Objectives must be defined using domain-specific quantities.

Examples:

**Acoustic**
- Maximize pressure at a target point.
- Minimize pressure outside a target region.
- Maximize focal-to-background ratio.
- Minimize focal width.
- Minimize sidelobes.
- Match a target pressure distribution.
- Minimize phase error.
- Improve acoustic energy concentration.

**Mechanical**
- Maximize displacement at a selected location.
- Minimize vibration at selected locations.
- Match a desired displacement field.
- Minimize stress concentration.
- Minimize unwanted resonances.
- Shift a structural resonance.
- Minimize response at a selected frequency.
- Maximize a desired mode's participation.

**Electromagnetic**
- Maximize gain in a specified direction.
- Minimize sidelobe levels.
- Place radiation nulls.
- Match a desired radiation pattern.
- Improve impedance matching.
- Maximize efficiency.
- Shape near-field distributions.
- Optimize beamwidth.
- Minimize unwanted radiation.

Do not assume that the same numerical metric has the same physical meaning across different domains.

Each objective must document its mathematical definition, units, normalization, and applicable physics models.

## 5. Multiple objectives and trade-offs

Support multi-objective optimization.

Many engineering problems have competing goals.

Examples:

- Maximize acoustic focal intensity while minimizing sidelobes.
- Maximize antenna gain while minimizing sidelobes and improving impedance matching.
- Maximize mechanical response while reducing stress.
- Improve focal concentration while minimizing actuator power.
- Minimize weight while maintaining structural performance.

Implement Pareto-based optimization.

Support displaying Pareto fronts for two or more objectives where appropriate.

Allow users to inspect trade-offs between candidate designs.

Provide objective weighting for scalarized optimization where appropriate, but do not make weighted sums the only multi-objective method.

Allow users to choose priorities after reviewing the trade-off space.

Do not present one candidate as universally optimal when the problem contains meaningful competing objectives.

## 6. Constraint framework

Implement hard and soft constraints.

Support:

**Geometric constraints**
- Minimum wall thickness.
- Maximum dimensions.
- Minimum spacing.
- Non-overlapping geometry.
- Symmetry.
- Manufacturing clearances.
- Structural continuity.

**Physical constraints**
- Material validity ranges.
- Frequency limits.
- Amplitude limits.
- Phase limits.
- Power limits.
- Stress limits.
- Temperature limits.
- Field-strength limits.

**Manufacturing constraints**
- Minimum feature size.
- Available material.
- Fabrication method.
- Tool access.
- Layer or print direction where relevant.
- Machining or assembly limitations.

**Computational constraints**
- Maximum simulation time.
- Memory budget.
- Mesh limits.
- Solver availability.
- Accuracy requirements.

Support penalty functions and explicit feasibility checks.

Hard constraints must not be silently violated.

A candidate that fails a hard constraint must be marked infeasible.

Show which constraints failed and by how much.

## 7. Optimization algorithms

Build a modular algorithm framework.

Investigate and implement appropriate methods.

**Gradient-based methods**
- Gradient descent.
- L-BFGS.
- Sequential quadratic programming where suitable.
- Adjoint-based optimization where supported.

**Derivative-free methods**
- Nelder-Mead.
- Powell.
- CMA-ES.
- Differential evolution.
- Bayesian optimization.

**Population-based methods**
- Genetic algorithms.
- NSGA-II.
- Other suitable Pareto optimizers.

**Sampling and exploration**
- Grid search.
- Random sampling.
- Latin hypercube sampling.
- Design of experiments.
- Parameter sweeps.

Do not implement every algorithm unnecessarily.

Prioritize methods that are robust and useful for expensive engineering simulations.

Allow algorithms to be selected based on:

- Number of variables.
- Continuous versus discrete variables.
- Objective smoothness.
- Simulation cost.
- Availability of gradients.
- Number of objectives.
- Constraint complexity.

Use established libraries where appropriate.

Investigate dependencies, licensing, and platform compatibility before adoption.

Do not tie the optimization framework to a single third-party library.

## 8. Gradient and sensitivity infrastructure

Prepare for gradient-based optimization.

Support numerical finite-difference gradients initially where appropriate.

Design interfaces for future:

- Automatic differentiation.
- Adjoint methods.
- Solver-specific gradients.
- Sensitivity equations.

Implement sensitivity analysis.

Determine how changes in design parameters affect objective functions.

Provide:

- Parameter sensitivity rankings.
- Local derivative estimates.
- Response curves.
- Parameter correlation.
- Sensitivity heatmaps where meaningful.

Identify variables that have negligible effect on the objective.

Warn when numerical gradients are unreliable because of discontinuities, solver noise, or mesh changes.

Do not claim exact gradients when using finite-difference approximations.

## 9. Inverse design of source configurations

Implement source-focused inverse design.

This is an essential capability for Resonant.

Allow users to specify a target wave field and solve for source configurations.

Support optimization of:

- Source positions.
- Source amplitudes.
- Source phases.
- Source delays.
- Source orientations.
- Source frequencies where supported.
- Element activation.
- Array geometry.

Target definitions should support:

- Single points.
- Multiple points.
- Lines.
- Planes.
- Surfaces.
- Three-dimensional volumes.
- Spatial amplitude distributions.
- Spatial phase distributions.
- Suppression regions.

Support complex-valued target fields where the physics domain requires them.

Implement linear and regularized least-squares methods where applicable.

For example:

\[
Aw\approx b
\]

where:

- \(A\) represents the relevant source-to-target response.
- \(w\) represents the source excitation weights.
- \(b\) represents the desired field.

Support:

- Tikhonov regularization.
- Weight bounds.
- Power penalties.
- Sparse excitation where appropriate.
- Minimum-norm solutions.

Report target error, residual error, feasibility, and limitations.

Distinguish exact mathematical solutions from approximate engineering solutions.

## 10. Geometry optimization

Implement parametric geometry optimization.

Allow optimization of geometry objects already supported by the scene and geometry systems.

Support:

- Reflectors.
- Lenses.
- Waveguides.
- Cavities.
- Antennas.
- Antenna arrays.
- Structural components.
- Plates.
- Membranes.
- Mechanical resonators.
- Engineered periodic structures.

Geometry parameters must retain their parametric relationships.

Examples:

**Parabolic reflector**
- Focal length.
- Aperture diameter.
- Depth.
- Rim geometry.
- Thickness.

**Acoustic lens**
- Surface curvature.
- Thickness.
- Material.
- Profile.
- Aperture.

**Antenna**
- Element length.
- Element spacing.
- Feed position.
- Substrate dimensions.
- Conductor geometry.

**Mechanical resonator**
- Dimensions.
- Thickness.
- Material.
- Support position.
- Resonant geometry.

After each candidate geometry change:

1. Regenerate the geometry.
2. Validate geometric integrity.
3. Recalculate or update the mesh.
4. Run the selected solver.
5. Evaluate objectives and constraints.
6. Store results.

Do not reuse stale simulation results after a geometry change.

Use caching only when the full dependency state is known to be identical.

## 11. Topology optimization

Establish an architecture for topology optimization.

Topology optimization should allow Resonant to explore the distribution of material or geometry within a defined design region.

Potential applications:

- Acoustic metamaterials.
- Phononic structures.
- Electromagnetic metamaterials.
- Structural vibration control.
- Mechanical resonators.
- Waveguide structures.
- Acoustic scattering and diffraction structures.

Support design-region definitions and material-selection rules.

Investigate established approaches such as:

- SIMP.
- Level-set methods.
- Density-based optimization.
- Evolutionary structural optimization.
- Shape optimization.

Do not attempt to implement every topology method in the initial phase.

Prioritize a tractable proof of concept using an appropriate solver and a well-defined benchmark.

Topology optimization must account for:

- Mesh dependence.
- Minimum feature size.
- Filtering.
- Numerical stability.
- Manufacturing constraints.
- Material interpolation.
- Physical validity.

Do not present an unmanufacturable numerical material distribution as a finished physical design.

Provide geometry cleanup and export preparation.

## 12. Shape-based passive focusing optimization

This capability is especially important.

Resonant must be able to optimize physical shapes that manipulate waves through geometry and material behavior.

Support optimization of:

- Parabolic reflectors.
- Elliptical reflectors.
- Spherical reflectors.
- Acoustic lenses.
- Electromagnetic reflectors.
- Waveguides.
- Resonant cavities.
- Diffraction structures.
- Phononic structures.
- Metamaterial-inspired structures.
- Arbitrary parametric surfaces.

Allow users to specify:

- Desired focal location.
- Desired focal region.
- Desired focal width.
- Desired field strength.
- Desired field distribution.
- Frequency range.
- Material constraints.
- Physical dimensions.
- Manufacturing limitations.

The optimization engine must evaluate actual geometry using an appropriate physical model.

Do not assume that a shape labeled parabolic, elliptical, or spherical automatically produces the desired focus under all physical conditions.

Account for wavelength, source location, aperture, material behavior, and the solver's assumptions.

Support passive, active, and hybrid optimization.

For hybrid designs, allow optimization of both geometry and source excitation.

## 13. Material optimization

Allow material properties to participate in optimization.

Support:

- Material selection.
- Material parameter sweeps.
- Layer thickness.
- Layer ordering.
- Composite construction.
- Frequency-dependent properties where supported.

Examples:

- Optimize acoustic lens material.
- Optimize mechanical damping layers.
- Optimize electromagnetic dielectric substrate.
- Optimize multilayer reflection and transmission behavior.
- Explore metamaterial unit-cell parameters.

Respect material validity ranges and real-world availability.

Distinguish selecting an existing material from inventing a hypothetical material.

Hypothetical material properties must be clearly labeled and must not be mistaken for commercially available materials.

## 14. Multi-fidelity optimization

Introduce multi-fidelity optimization.

Engineering simulations can be expensive.

Support a workflow in which inexpensive models screen candidate designs before expensive numerical solvers evaluate the most promising candidates.

Possible model levels:

**Level 1 — Analytical**
- Closed-form approximations.
- Idealized source models.
- Basic array factors.

**Level 2 — Reduced numerical**
- Simplified geometry.
- Coarser meshes.
- Reduced-order models.
- Approximate solvers.

**Level 3 — Higher-fidelity numerical**
- Refined meshes.
- More complete material models.
- Full-wave simulation.
- Detailed boundary conditions.

**Level 4 — Experimental**
- Calibrated measurement.
- Physical validation.
- Measured performance.

Allow users to define which model levels are available.

Every candidate must record which model evaluated it.

Do not combine results from incompatible models without documenting the differences.

A design optimized using a simplified model must be re-evaluated with an appropriate higher-fidelity solver before being treated as validated.

## 15. Surrogate models and response surfaces

Prepare for surrogate modeling.

Support:

- Response surfaces.
- Polynomial approximations.
- Gaussian-process models.
- Radial-basis approximations.
- Other suitable regression models.

Use surrogate models to reduce the number of expensive simulation runs.

Track:

- Training data.
- Input variables.
- Output objectives.
- Prediction uncertainty.
- Training error.
- Validation error.
- Model version.

Do not treat a surrogate prediction as a physical simulation result.

Support selecting promising candidates for real solver evaluation.

Where appropriate, use active learning to identify the next most informative simulation.

## 16. Robust optimization and uncertainty

Engineering designs must work under realistic variation.

Introduce robust optimization.

Support uncertain parameters such as:

- Material properties.
- Manufacturing tolerances.
- Source positioning.
- Emitter phase errors.
- Amplitude mismatch.
- Timing errors.
- Environmental temperature.
- Mechanical support conditions.
- Antenna element variation.

Allow users to define parameter distributions or tolerance ranges where justified.

Evaluate candidate designs under multiple sampled conditions.

Support objectives such as:

- Expected performance.
- Worst-case performance.
- Performance variance.
- Probability of meeting constraints.

Do not assume arbitrary probability distributions without user input or defensible source data.

Distinguish nominal optimization from robust optimization.

## 17. Automated design-space exploration

Implement a design exploration interface.

Allow users to define parameter ranges and explore their effects.

Support:

- One-variable sweeps.
- Multi-variable sweeps.
- Parameter grids.
- Random sampling.
- Design of experiments.
- Candidate filtering.
- Candidate ranking.
- Pareto visualization.

Provide plots and tables that allow users to understand how parameters affect the result.

Support selecting a candidate and promoting it into the main project scene.

Do not overwrite the original design during exploration.

## 18. Optimization job management

Optimization runs may require hundreds or thousands of simulations.

Implement a dedicated optimization job manager.

Support:

- Background execution.
- Parallel candidate evaluation.
- CPU resource limits.
- Memory limits.
- Cancellation.
- Checkpointing.
- Restart.
- Progress reporting.
- Failure recovery.
- Candidate caching.
- Run history.

Allow a user to inspect completed candidates while an optimization is still running.

Do not allow one optimization job to monopolize the entire application.

Respect solver concurrency limitations.

Avoid running multiple incompatible jobs against shared mutable scene state.

Each candidate must operate on an immutable or independently reconstructed design snapshot.

## 19. Candidate management and comparison

Implement a candidate-design browser.

Each candidate should include:

- Candidate ID.
- Parent design.
- Parameter values.
- Geometry reference.
- Material configuration.
- Solver configuration.
- Objective values.
- Constraint results.
- Simulation result references.
- Execution status.
- Model fidelity.
- Uncertainty metadata.

Support:

- Side-by-side comparison.
- Objective comparison.
- Constraint comparison.
- Geometry comparison.
- Field visualization comparison.
- Performance ranking.
- Pareto-front exploration.

Allow users to select a candidate and promote it to the main scene as a new design revision.

Never silently replace the original project geometry.

Maintain candidate lineage and optimization history.

## 20. Domain-specific optimization adapters

Implement optimization adapters for each physics domain.

### Acoustic adapter

Support optimization of:

- Emitter positions.
- Array spacing.
- Amplitude.
- Phase.
- Delay.
- Frequency where supported.
- Reflector geometry.
- Lens geometry.
- Material properties.
- Waveguide geometry.
- Target-field distributions.

Initial demonstrations:

1. Optimize a linear array to focus at a specified point.
2. Optimize source phases for multiple focal points.
3. Optimize a reflector profile for a specified focal region.
4. Reduce sidelobes around a target focus.
5. Compare active and passive focusing.

### Mechanical adapter

Support optimization of:

- Structural dimensions.
- Material selection.
- Damping.
- Actuator locations.
- Actuator excitation.
- Support positions.
- Resonator geometry.
- Plate and membrane parameters.

Initial demonstrations:

1. Tune a resonant structure to a target frequency.
2. Minimize vibration at a selected point.
3. Optimize an actuator configuration for a desired displacement field.
4. Reduce stress concentration subject to response constraints.
5. Compare alternative damping configurations.

### Electromagnetic adapter

Support optimization of:

- Antenna dimensions.
- Array element spacing.
- Array excitation.
- Phase and amplitude.
- Feed positions.
- Reflector geometry.
- Substrate properties.
- Waveguide geometry.
- Radiation patterns.
- Impedance matching.

Initial demonstrations:

1. Optimize an array for a specified steering angle.
2. Reduce sidelobes.
3. Place directional radiation nulls.
4. Optimize a dipole or patch antenna parameter.
5. Compare array-factor and higher-fidelity results.

Each adapter must clearly expose its actual capabilities and limitations.

## 21. Python integration

Prepare the optimization framework for Python-based scientific workflows.

Use the existing Python integration architecture.

Expose appropriate APIs for:

- Defining optimization problems.
- Reading design parameters.
- Setting bounds.
- Defining objectives.
- Running parameter sweeps.
- Accessing simulation results.
- Exporting candidates.
- Plotting results.

Use NumPy and SciPy where appropriate.

Investigate suitable optimization packages.

Do not require Python for the basic optimization framework to function.

Keep the C++ core independently usable.

If Python-based optimization runs are isolated in a worker process, ensure that project and simulation data are transferred through explicit, versioned interfaces.

## 22. Geometry and manufacturing export

Allow selected optimized geometry to be promoted into the main scene.

Support export through existing geometry and CAD infrastructure.

Prepare for:

- STEP export.
- STL export.
- OBJ export.
- Mesh export.
- Parametric design metadata.
- Manufacturing constraints and annotations.

For 3D-printable designs, support checks for:

- Minimum wall thickness.
- Minimum feature size.
- Overhang constraints where applicable.
- Closed geometry.
- Mesh integrity.
- Manufacturing volume.

Do not imply that simulation-ready geometry is automatically manufacturing-ready.

Preserve the original optimized parameter values and design lineage.

## 23. Optimization UI

Introduce an **Optimization Studio** workspace.

It should contain:

- Problem definition.
- Design-variable editor.
- Objective editor.
- Constraint editor.
- Algorithm selection.
- Solver fidelity selection.
- Resource limits.
- Optimization controls.
- Live progress.
- Candidate browser.
- Pareto visualization.
- Sensitivity analysis.
- Comparison tools.
- Optimization history.

Allow users to configure a problem visually.

Keep advanced controls available without overwhelming basic workflows.

Provide useful templates for common acoustic, mechanical, and electromagnetic optimization tasks.

Use clear terminology.

Show the selected physics model and solver fidelity prominently.

## 24. Example optimization projects

Create reproducible examples.

**Example A — Acoustic focal array**
- Linear array.
- Target point.
- Optimized phase and amplitude.
- Comparison with uniform excitation.

**Example B — Acoustic reflector**
- Parametric reflector.
- Target focal region.
- Geometry optimization.
- Candidate comparison.

**Example C — Mechanical vibration suppression**
- Structural model.
- Selected response points.
- Actuator variables.
- Optimization of vibration suppression.

**Example D — Mechanical resonator**
- Parametric geometry.
- Target resonant frequency.
- Material and dimension constraints.
- Candidate comparison.

**Example E — Electromagnetic beam steering**
- Planar antenna array.
- Target direction.
- Optimized phase and amplitude.
- Radiation-pattern comparison.

**Example F — Sidelobe reduction**
- Antenna array.
- Multiple objective functions.
- Pareto comparison between gain and sidelobe levels.

**Example G — Robust design**
- A selected acoustic, mechanical, or electromagnetic configuration.
- Defined tolerances.
- Multiple perturbed simulations.
- Nominal versus robust performance.

Every example must use actual implemented optimization and physics calculations.

## 25. Testing and validation

Implement tests for:

- Design-variable bounds.
- Constraint evaluation.
- Objective evaluation.
- Optimization convergence.
- Candidate reproducibility.
- Multi-objective behavior.
- Candidate caching.
- Job cancellation.
- Checkpoint and restart.
- Geometry regeneration.
- Invalid geometry detection.
- Domain adapter correctness.
- Result provenance.

Validate optimization results against known analytical problems.

For source optimization, compare against known linear least-squares solutions where applicable.

For geometry optimization, compare simple cases against known analytical geometry or independently computed results.

For robust optimization, test with known parameter distributions.

Ensure that optimization does not mutate the original scene or corrupt saved projects.

## 26. Documentation

Create or update:

- OPTIMIZATION_ARCHITECTURE.md
- INVERSE_DESIGN.md
- DESIGN_VARIABLES.md
- OBJECTIVE_FUNCTIONS.md
- CONSTRAINT_SYSTEM.md
- OPTIMIZATION_ALGORITHMS.md
- GEOMETRY_OPTIMIZATION.md
- TOPOLOGY_OPTIMIZATION.md
- MULTI_FIDELITY_OPTIMIZATION.md
- SURROGATE_MODELS.md
- ROBUST_OPTIMIZATION.md
- DOMAIN_OPTIMIZATION_ADAPTERS.md
- OPTIMIZATION_VALIDATION.md
- IMPLEMENTATION_STATUS.md
- DECISIONS.md

Document algorithm assumptions, supported problem types, numerical limitations, convergence criteria, and validation evidence.

## 27. Implementation sequence

**Phase A — Optimization architecture**
- Core optimization models.
- Variables.
- Objectives.
- Constraints.
- Run metadata.

**Phase B — Basic design exploration**
- Parameter sweeps.
- Candidate generation.
- Candidate storage.
- Candidate comparison.

**Phase C — Inverse source design**
- Linear inverse solutions.
- Regularization.
- Source weights.
- Target-field constraints.

**Phase D — General optimization algorithms**
- Derivative-free methods.
- Gradient-based methods where appropriate.
- Multi-objective methods.

**Phase E — Geometry optimization**
- Parametric geometry.
- Geometry validation.
- Mesh regeneration.
- Solver evaluation.

**Phase F — Domain adapters**
- Acoustic.
- Mechanical.
- Electromagnetic.

**Phase G — Multi-fidelity optimization**
- Analytical screening.
- Reduced-order models.
- Higher-fidelity validation.

**Phase H — Robust optimization**
- Tolerances.
- Parameter sampling.
- Statistical evaluation.

**Phase I — Advanced design**
- Topology optimization proof of concept.
- Surrogate modeling.
- Sensitivity analysis.

**Phase J — User interface and reporting**
- Optimization Studio.
- Candidate browser.
- Pareto analysis.
- Design promotion.
- Export.

**Phase K — Validation**
- Analytical benchmarks.
- Numerical verification.
- Domain-specific examples.
- Regression testing.

Implement this incrementally.

Do not attempt to build a general-purpose topology optimizer for every physics domain before delivering reliable source and geometry optimization.

## 28. Acceptance criteria

This milestone is complete only when:

1. A physics-independent optimization framework exists.
2. Design variables and constraints are configurable.
3. Multiple objective functions are supported.
4. Parameter sweeps work.
5. Candidate designs are independently stored.
6. At least one derivative-free optimization method works.
7. At least one appropriate gradient-based method works where supported.
8. Inverse source design works for a supported physics model.
9. Geometry optimization works for at least one real geometry type.
10. Candidates are evaluated using actual physics calculations.
11. Optimization results retain solver and model provenance.
12. Multi-objective comparison is available.
13. Optimization jobs run asynchronously.
14. Cancellation and checkpointing work.
15. The original project design remains unchanged until the user promotes a candidate.
16. At least one working optimization example exists for each physics domain.
17. Simplified models are distinguished from higher-fidelity models.
18. Candidate designs can be re-evaluated using a more accurate solver.
19. Automated tests and analytical benchmarks pass.
20. Documentation explains limitations and supported capabilities.

## 29. Critical development rules

- Optimization must be physics-aware.
- Do not put domain equations into the generic optimization core.
- Never silently violate hard constraints.
- Do not treat an optimization result as proof of physical feasibility.
- Do not reuse stale simulation results after design changes.
- Preserve original project designs.
- Preserve candidate history and lineage.
- Record the exact model used to evaluate every candidate.
- Distinguish surrogate predictions from real simulations.
- Distinguish nominal optimization from robust optimization.
- Do not invent material properties.
- Respect manufacturing constraints.
- Validate high-fidelity candidates before declaring them successful.
- Keep the optimization framework independent of a single algorithm library.
- Maintain reproducibility.

**The goal is to make Resonant an automated wave-engineering design environment: users describe the physical behavior they want, define the design freedom and constraints, and Resonant searches, simulates, compares, and validates candidate solutions across acoustic, mechanical, and electromagnetic systems.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: PERFORMANCE (confidence high)
  - validation: PASS
  - classifier_reason: override FEATURE accepted (text alone suggests PERFORMANCE: performance/resource problem in the request statement: OPTIMIZATION, Optimization, optimization)
  - evidence: 'milestone' [body]; 'introduces' [body]; 'framework' [body]; 'objective' [body]; 'allow' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: Ninth ticket, submitted by the user in chat while REQ-CHAT-20261126-002 was being processed, with the instruction to process it in order (after REQ-CHAT-20261126-008). Unified optimization framework across the acoustic (003-005), mechanical (006) and electromagnetic (007) domains, building on the experiment framework (008). Standing operator instructions for all tickets: keep qmake support in addition to CMake; Qt user interfaces use .ui files.

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

Ninth ticket, submitted by the user in chat while REQ-CHAT-20261126-002 was being processed, with the instruction to process it in order (after REQ-CHAT-20261126-008). Unified optimization framework across the acoustic (003-005), mechanical (006) and electromagnetic (007) domains, building on the experiment framework (008). Standing operator instructions for all tickets: keep qmake support in addition to CMake; Qt user interfaces use .ui files.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000012
- Evidence: pending
