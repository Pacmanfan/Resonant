# Request: # PROMPT 8 — RESONANT: PASSIVE WAVE MANIPULATION, ACOUSTIC REFLECTORS, LENSES AN

**Request ID:** REQ-CHAT-20261126-004
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T15:14:43-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 8 — RESONANT: PASSIVE WAVE MANIPULATION, ACOUSTIC REFLECTORS, LENSES AND GEOMETRY-BASED FOCUSING

You are continuing development of **RESONANT — Wave Field Engineering Studio**.

This milestone introduces **passive physical wave manipulation**: using geometry, materials, and structural design to control how acoustic energy propagates through space.

The objective is to let users design physical structures that focus, redirect, concentrate, distribute, guide, or suppress acoustic energy.

This capability must integrate with the existing geometry system, material library, acoustic physics engine, simulation infrastructure, and 3D visualization system.

Do not replace the existing acoustic engine. Extend it through well-defined interfaces and additional propagation models.

This is a scientific engineering capability, not a collection of decorative shapes with assumed focal properties.

---

## 1. Mission

Implement a modular system called **Wave Manipulators**.

Wave Manipulators are physical structures designed to influence wave propagation through geometry, material properties, boundary conditions, or combinations of these mechanisms.

The initial focus is acoustic manipulation.

The architecture must eventually accommodate mechanical and electromagnetic wave manipulation without forcing all physical domains into the same equations or solver implementations.

Support three distinct manipulation strategies:

**Passive manipulation**
- Wave control through physical geometry and material properties.
- No electronically controlled emitter phase or amplitude adjustments required.

**Active manipulation**
- Wave control through independently controlled emitters.
- Uses amplitude, phase, timing, and waveform adjustments.

**Hybrid manipulation**
- Combines active emitter control with passive physical structures.

The user must be able to compare these approaches using the same target field and evaluation criteria.

## 2. Wave Manipulator architecture

Create a dedicated Wave Manipulator subsystem.

Each manipulator should have:

- Stable ID.
- Name.
- Manipulator category.
- Physical geometry reference.
- Material assignments.
- Transform.
- Domain applicability.
- Configuration parameters.
- Boundary-condition information where relevant.
- Optional simulation-specific properties.
- Design and provenance metadata.

Separate the conceptual design object from its actual geometry and simulation representation.

A manipulator may consist of:
- One continuous surface.
- A solid volume.
- Multiple geometric components.
- Multiple materials.
- Layered or composite structures.
- Periodic or repeating structures.

The architecture must support parametric designs and arbitrary custom geometry.

Avoid creating one enormous class with every possible physical parameter.

Use domain-specific extensions and solver-specific representations.

## 3. Initial manipulator categories

Implement the following conceptual categories:

### A. Reflectors

- Parabolic reflectors.
- Elliptical reflectors.
- Spherical reflectors.
- Cylindrical reflectors.
- Flat reflectors.
- Custom reflector surfaces.

### B. Acoustic lenses

- Convex lenses.
- Concave lenses.
- Plano-convex lenses.
- Plano-concave lenses.
- Biconvex lenses.
- Biconcave lenses.
- Custom freeform lenses.

### C. Waveguides

- Straight waveguides.
- Curved waveguides.
- Tapered waveguides.
- Branching waveguides.
- Custom cross-section waveguides.

### D. Resonant structures

- Resonant cavities.
- Helmholtz resonators.
- Quarter-wave resonators.
- Layered resonant structures.
- Tunable cavity geometries.

### E. Diffraction and engineered structures

- Apertures.
- Slits.
- Perforated structures.
- Periodic structures.
- Phased geometric surfaces.
- Metamaterial-inspired structures.
- Phononic-crystal-inspired structures.

The initial implementation does not need to provide a complete physical solver for every category.

However, the data model, geometry representation, and user interface should be designed to grow into these capabilities.

Clearly distinguish supported physical simulation from geometry-only design and future capabilities.

## 4. Geometry-driven focal design

A primary requirement is the ability to create a physical shape intended to focus acoustic energy at a specified point or region.

The user should be able to:

1. Select a wave manipulator category.
2. Specify the intended focal point.
3. Specify the source position or source arrangement.
4. Define the physical dimensions and constraints.
5. Select material properties.
6. Generate or modify the geometry.
7. Run an appropriate simulation.
8. Evaluate the actual resulting acoustic field.
9. Compare the achieved focus against the intended target.

Support geometric parameters including:

- Aperture diameter or width.
- Aperture height.
- Focal distance.
- Curvature.
- Surface profile.
- Thickness.
- Offset.
- Orientation.
- Material.
- Layer count where applicable.

The geometry generator must use real mathematical definitions.

For example, implement a parabolic surface using an appropriate parametric representation.

Do not simply create a generic bowl and label it parabolic.

For an ideal parabolic reflector, document the coordinate convention, focal location, and relationship between curvature and focal length.

The system must make clear which geometric design assumptions are idealized.

## 5. Elliptical reflector design

Implement an elliptical reflector generator.

An ellipse has two focal points.

Allow the user to specify:

- First focal point.
- Second focal point.
- Major axis.
- Minor axis.
- Aperture limits.
- Reflector thickness.
- Material.
- Orientation.

For an ideal ellipse, a wave originating at one focus can be reflected toward the other under the appropriate reflection assumptions.

Generate the actual elliptical geometry from the selected parameters.

Allow both two-dimensional elliptical cross-sections and three-dimensional revolved or extruded implementations where appropriate.

Provide a useful visualization of the two focal locations.

The user should be able to place a source at one focus and an evaluation target at the other.

Do not guarantee perfect focusing in a real physical simulation. The actual result depends on frequency, aperture, wavelength, source directivity, boundary conditions, and material behavior.

## 6. Acoustic lens design

Implement parametric acoustic lens geometry.

The lens model must distinguish between:

- Physical shape.
- Lens material.
- Surrounding propagation medium.
- Acoustic properties.
- Frequency-dependent behavior.

Provide controls for:

- Lens thickness.
- Curvature.
- Diameter or aperture.
- Material.
- Source location.
- Intended focal distance.
- Orientation.

Allow the user to design a lens to focus or redirect an incident acoustic wave.

Where practical, provide an analytical first-order design estimate based on sound-speed contrast and geometry.

Clearly distinguish this approximate estimate from a full-wave solution.

Do not assume optical lens equations are automatically valid for all acoustic lens configurations.

The physical model must account for the relevant acoustic impedance and sound-speed relationships when implementing transmission and refraction.

## 7. Waveguide design

Introduce a parametric waveguide model.

Support:

- Circular cross-section.
- Rectangular cross-section.
- Elliptical cross-section.
- Custom cross-section.
- Straight and curved paths.
- Tapered geometry.

Provide geometry controls for:

- Length.
- Width.
- Height.
- Radius.
- Bend radius.
- Wall thickness.
- Cross-sectional profile.
- Material.
- Boundary properties.

The initial release may focus on geometry creation and basic analytical modal estimates where appropriate.

Prepare for later numerical waveguide simulation.

Do not claim arbitrary waveguides are physically modeled merely because their geometry is visible.

## 8. Resonant cavities

Introduce resonant cavity design objects.

Support initial geometry for:

- Rectangular cavities.
- Cylindrical cavities.
- Spherical cavities.
- Custom enclosed volumes.

Allow users to configure dimensions, material properties, and boundary characteristics.

Where applicable, calculate analytical resonant-frequency estimates for idealized geometries.

Document the assumptions behind these estimates.

Prepare for later eigenmode and full-wave numerical analysis.

The system should eventually support mode visualization, nodal regions, antinodes, and frequency-response curves.

## 9. Geometry and material interaction

Extend the existing acoustic material architecture.

The simulation must distinguish between:

- Propagation medium.
- Reflector material.
- Lens material.
- Waveguide wall material.
- Cavity boundary material.
- Absorptive or damping material.

Where supported, model:

- Acoustic impedance.
- Density.
- Sound speed.
- Absorption.
- Attenuation.
- Transmission.
- Reflection.
- Frequency dependence.
- Boundary impedance.

Do not invent physical constants.

Material properties must have provenance and validity information.

Support idealized boundary conditions where necessary, including:

- Rigid boundary.
- Pressure-release boundary.
- Impedance boundary.
- Absorbing boundary.

Clearly identify approximations and unsupported boundary behaviors.

## 10. Physical solver integration

The current analytical free-field acoustic solver is not sufficient for all passive structures.

Introduce solver selection based on the physical problem.

Investigate and integrate appropriate numerical approaches.

Potential methods include:

- Geometrical acoustics and ray tracing.
- Image-source approximations.
- Boundary element methods.
- Finite element methods.
- Finite-difference time-domain methods.
- Angular spectrum propagation.
- Modal analysis.

Do not attempt to force every method into one universal solver.

Choose the method according to:
- Frequency.
- Wavelength.
- Geometry.
- Material behavior.
- Boundary conditions.
- Required accuracy.
- Computational cost.

For the first implementation, prioritize a practical route to real reflector simulation.

A reasonable staged approach is:

1. Validate ideal reflector geometry analytically.
2. Implement a suitable ray-based approximation for reflector behavior.
3. Establish a full-wave solver integration strategy.
4. Implement or integrate a numerical solver for selected structures.
5. Validate numerical results against analytical and independent reference cases.

Do not present ray tracing as a full-wave solution.

Ray methods may be appropriate for some high-frequency, large-geometry scenarios but can fail when diffraction, interference, or wavelength-scale features matter.

The user interface must identify the selected solver and its assumptions.

## 11. Passive focusing

Implement passive focal evaluation.

For each passive manipulator, allow the user to specify:

- Source location.
- Source frequency.
- Source orientation.
- Source model.
- Target region.
- Propagation medium.

Calculate or simulate the resulting field using the appropriate available model.

Measure:

- Peak field location.
- Peak field magnitude.
- Focal-region dimensions where meaningful.
- Focal displacement from the intended target.
- Sidelobe behavior where applicable.
- Energy distribution.
- Efficiency metrics where physically meaningful.

Do not use a single universal definition of focusing efficiency across unrelated physical configurations.

Document the calculation and units for every reported metric.

## 12. Hybrid active/passive focusing

Integrate Wave Manipulators with the active acoustic focusing system from Prompt 7.

Allow users to combine:

- Parametric emitter arrays.
- Individually controlled emitters.
- Passive reflectors.
- Acoustic lenses.
- Waveguides.
- Other supported structures.

The workflow should be:

1. Create an emitter or array.
2. Add a physical wave manipulator.
3. Configure the geometry and material.
4. Define one or more target points or regions.
5. Select active control strategy.
6. Calculate emitter phase, amplitude, and timing.
7. Simulate the combined system.
8. Evaluate the resulting field.

The propagation solver must account for the supported physical interaction between emitters and manipulator geometry.

Do not simply calculate a free-field array and overlay a reflector mesh.

If the current solver cannot account for a particular interaction, state that limitation and prevent misleading hybrid results.

## 13. Geometry optimization

Introduce the architecture for geometry-based optimization.

The long-term objective is to allow users to specify a desired acoustic field and have Resonant search for physical geometry that produces it.

Support optimization variables such as:

- Reflector curvature.
- Focal distance.
- Aperture.
- Surface profile.
- Lens thickness.
- Lens curvature.
- Waveguide dimensions.
- Cavity dimensions.
- Material selection where appropriate.
- Layer thickness.
- Structural periodicity.

Define optimization objectives such as:

- Maximize pressure at a target.
- Minimize focal-region width.
- Reduce sidelobes.
- Improve field uniformity.
- Minimize unwanted field regions.
- Improve energy concentration.
- Minimize material volume.
- Respect manufacturing constraints.

For this milestone, establish the optimizer interfaces and implement a limited demonstration using a tractable analytical or ray-based model.

Do not attempt expensive unrestricted topology optimization prematurely.

## 14. Manufacturing and geometry constraints

Prepare for manufacturable physical designs.

Support constraints including:

- Minimum wall thickness.
- Maximum dimensions.
- Minimum feature size.
- Minimum curvature radius.
- Layer thickness.
- Material availability.
- Symmetry.
- Periodicity.
- Mesh quality.

Keep manufacturing constraints separate from physical simulation parameters.

Eventually, users should be able to optimize a structure while enforcing fabrication constraints.

Do not silently modify geometry to satisfy constraints. Report violations and proposed corrections.

## 15. Visualization

Extend the OpenGL viewport to visualize physical wave manipulators and their acoustic behavior.

Implement:

- Reflector geometry.
- Lens geometry.
- Waveguide geometry.
- Cavity geometry.
- Material visualization.
- Focal-point markers.
- Source and target markers.
- Wave propagation paths where appropriate.
- Field slices.
- Pressure distribution.
- Phase distribution.
- Before/after comparison.

Support visual distinction between:

- Physical geometry.
- Generated simulation mesh.
- Numerical field data.
- Intended focal points.
- Calculated focal locations.

The user must be able to see where the actual simulated field differs from the intended result.

Do not use decorative wave graphics as substitutes for calculated data.

## 16. Comparison and analysis

Implement a comparison workflow for:

- Free-field propagation.
- Active focusing.
- Passive focusing.
- Hybrid focusing.

Allow the user to compare multiple design configurations.

Where supported, calculate and display:

- Peak pressure.
- Peak location.
- Focal width.
- Focal displacement.
- Sidelobe level.
- Field uniformity.
- Efficiency.
- Computational cost.

Use metrics only when physically meaningful for the selected solver and domain.

Retain the original simulation configuration for each comparison.

## 17. Example projects

Create reproducible demonstration projects.

**Example A — Parabolic reflector**
- Point source.
- Parametric parabolic reflector.
- Intended focal region.
- Simulated field.
- Focal-location analysis.

**Example B — Elliptical reflector**
- Source at first focus.
- Target at second focus.
- Reflector geometry.
- Field visualization.

**Example C — Acoustic lens**
- Source.
- Lens geometry.
- Surrounding medium.
- Intended focal point.
- Calculated or simulated result using an explicitly documented model.

**Example D — Hybrid focusing**
- Planar emitter array.
- Passive reflector.
- Target point.
- Active phase compensation.
- Combined field evaluation.

**Example E — Geometry optimization**
- Parametric reflector.
- Adjustable curvature or focal length.
- Target field objective.
- Optimization result.
- Comparison against the original design.

Every example must use the actual implemented physics model.

Clearly label analytical, approximate, and full-wave results.

## 18. Validation

Implement validation cases for:

- Parabolic geometry and focal properties.
- Ellipse geometry and focal relationships.
- Reflection direction under idealized assumptions.
- Lens geometry.
- Basic impedance relationships.
- Boundary-condition behavior where implemented.
- Ray-based propagation.
- Numerical solver behavior where integrated.
- Passive focusing.
- Hybrid focusing.

Use independent analytical references wherever possible.

For numerical solvers, include convergence and mesh-refinement studies.

Demonstrate that results improve or stabilize as numerical resolution increases.

Document known limitations.

## 19. Documentation

Create or update:

- WAVE_MANIPULATORS.md
- REFLECTOR_DESIGN.md
- ACOUSTIC_LENSES.md
- WAVEGUIDES.md
- RESONANT_CAVITIES.md
- PASSIVE_FOCUSING.md
- HYBRID_FOCUSING.md
- GEOMETRY_OPTIMIZATION.md
- PASSIVE_WAVE_VALIDATION.md
- IMPLEMENTATION_STATUS.md
- DECISIONS.md

Document equations, geometry conventions, physical assumptions, solver selection, and numerical limitations.

## 20. Implementation sequence

**Phase A — Wave Manipulator architecture**
- Domain model.
- Manipulator identifiers.
- Geometry references.
- Material relationships.
- Solver interfaces.

**Phase B — Parametric reflector geometry**
- Parabolic.
- Elliptical.
- Spherical.
- Cylindrical.
- Custom surfaces.

**Phase C — Passive acoustic modeling**
- Boundary conditions.
- Reflection behavior.
- Ray-based approximation.
- Analytical validation.

**Phase D — Acoustic lens design**
- Parametric lens geometry.
- Material relationships.
- Analytical estimates.
- Initial physical modeling.

**Phase E — Waveguides and cavities**
- Geometry generators.
- Basic analytical estimates.
- Solver integration interfaces.

**Phase F — Hybrid focusing**
- Emitter arrays.
- Passive structures.
- Active compensation.
- Combined field calculation.

**Phase G — Geometry optimization**
- Design variables.
- Objectives.
- Constraints.
- Initial optimizer.
- Result comparison.

**Phase H — Validation and documentation**
- Tests.
- Numerical validation.
- Example projects.
- Performance assessment.
- Documentation.

Implement incrementally. Do not attempt to implement every solver and manipulator type in a single development pass.

## 21. Acceptance criteria

This milestone is complete only when:

1. Users can create Wave Manipulator objects.
2. Users can generate parametric parabolic reflectors.
3. Users can generate parametric elliptical reflectors.
4. Focal geometry is mathematically correct.
5. Users can create and configure acoustic lenses.
6. Material assignments are associated with the appropriate physical models.
7. The application distinguishes supported physical calculations from geometry-only features.
8. At least one passive reflector propagation model produces real calculated field results.
9. The calculated focal behavior can be validated.
10. Users can inspect the resulting field in the 3D viewport.
11. Passive focusing can be compared with free-field propagation.
12. The architecture supports hybrid active/passive focusing.
13. At least one hybrid example produces physically meaningful calculated results.
14. Geometry optimization can demonstrate an actual objective-driven improvement.
15. Numerical limitations and assumptions are documented.
16. Automated tests pass.
17. Existing active acoustic focusing remains functional.
18. All example projects are reproducible.

## 22. Critical development rules

- Geometry alone is not proof of physical focusing.
- Do not assume an ideal reflector remains ideal under every frequency and source condition.
- Do not treat ray tracing as full-wave simulation.
- Do not apply optical lens equations indiscriminately to acoustics.
- Do not pretend unsupported material interactions are simulated.
- Do not claim hybrid focusing works if the solver ignores the passive structure.
- Do not fabricate material data.
- Preserve the distinction between design intent and calculated physical behavior.
- Keep solver implementations modular.
- Preserve the existing acoustic engine.
- Validate numerical results independently.
- Record architectural decisions and limitations.

**The goal is to make Resonant capable of designing and evaluating physical structures that manipulate acoustic energy: reflectors, lenses, waveguides, cavities, and hybrid active/passive systems. The geometry must be real, the calculations must be grounded in physics, and the resulting field must be measurable and verifiable.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Implement, Support, must be able
  - evidence: 'milestone' [body]; 'introduces' [body]; 'objective' [body]; 'capability' [body]; 'integrate' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: Fourth ticket, submitted by the user in chat (submit-only; the user asked not to claim or process tickets yet). Depends on REQ-CHAT-20261126-003 (acoustic physics engine, 'Prompt 7'), which in turn depends on REQ-CHAT-20261126-002 (V0.1 application foundation). Product vision: REQ-CHAT-20261126-001.

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

Fourth ticket, submitted by the user in chat (submit-only; the user asked not to claim or process tickets yet). Depends on REQ-CHAT-20261126-003 (acoustic physics engine, 'Prompt 7'), which in turn depends on REQ-CHAT-20261126-002 (V0.1 application foundation). Product vision: REQ-CHAT-20261126-001.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000004
- Evidence: pending
