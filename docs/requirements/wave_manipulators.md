# Passive Wave Manipulation and Hybrid Focusing

**Owner of:** `RES-MANIP-001..016`
**Source:** REQ-CHAT-20261126-004 ("Prompt 8")
**Refines:** `product.md` RES-PRODUCT-005, -006, -010
**Decisions:** ADR-000205, ADR-000207, ADR-000208
**Documents:** `docs/manipulators/`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-MANIP-001 — A separate module with a common manipulator abstraction

Wave manipulators are a core module (`src/core/manipulators`) without Qt or
OpenGL. A manipulator is a scene object whose design data (category,
boundary condition, intended focus, intended source, notes), parametric
geometry and simulation representation are three separate things.
*Verification:* suite `layering`; `ManipulatorPersistence.*`.

### RES-MANIP-002 — Reflector geometry from its equations

Parabolic, elliptical, spherical, cylindrical-trough and flat reflectors
are generated from their mathematical definitions with validated
parameters, and expose their characteristic points (focus, foci, centre of
curvature, focal line).
*Verification:* `ManipulatorGeometry.*`, `ManipulatorRays.*`.

### RES-MANIP-003 — Lens, waveguide, cavity and diffraction geometry

Spherical-surface lenses, circular (straight or tapered) and rectangular
waveguides, rectangular, cylindrical and spherical cavities and a slit
plate exist as parametric geometry with validated parameters.
*Verification:* `ManipulatorGeometry.*`.

### RES-MANIP-004 — Every manipulator states what is calculated for it

Each kind has a support level (approximate field, analytical estimate,
geometry only) that is shown to the user. A manipulator without a field
model never changes a simulated field, and results say that it was left
out.
*Verification:* `ManipulatorPersistence.RoundTripAndSupportLevels`,
`ManipulatorsUiTest.EveryKindCanBeAddedAndSaysWhatIsCalculatedForIt`,
`ManipulatorExamples.C_*`.

### RES-MANIP-005 — Analytical estimates with stated assumptions

Mirror equation, interface reflection and transmission, thick-lens focal
length with the acoustic refractive index, duct cut-off frequencies, cavity
resonances, Helmholtz and quarter-wave resonators are available as
functions and shown in the editor, each labelled as an estimate with its
assumptions.
*Verification:* `ManipulatorAnalytics.*`,
`ManipulatorsUiTest.LensEstimateUsesTheAssignedMaterialAndTheMedium`.

### RES-MANIP-006 — Reflector field model

A propagation model adds to the free field the sound reflected once by
reflector surfaces (Kirchhoff approximation, Helmholtz–Kirchhoff integral
over surface elements sampled from the equation), with rigid,
pressure-release, impedance and absorbing boundary behaviour. It is a
selectable model, identified in every result, never selected automatically
and never described as full-wave.
*Verification:* `ReflectorModel.*`,
`ManipulatorsUiTest.ReflectorModelChangesTheField*`.

### RES-MANIP-007 — Limits of the model are reported

Results and the user interface state what the reflector model leaves out
(multiple reflections, mutual shadowing, shell thickness, non-reflector
objects, particle velocity). Intensity is shown as not calculated. Hidden
reflectors and manipulators without a model are named in the notes.
*Verification:* `ReflectorModel.CollectReports*`,
`ManipulatorsUiTest.ReflectorModelChangesTheField*`.

### RES-MANIP-008 — Surface resolution, preflight and convergence

The surface resolution is a setting in elements per wavelength. The
preflight reports surface elements and the evaluation count, warns and
refuses beyond limits. The result converges under refinement.
*Verification:* `ReflectorModel.ResultConvergesAsTheSurfaceResolutionIsRefined`,
`ManipulatorsUiTest.FieldSimulationWithAReflectorIsEstimatedRunAndCompared`.

### RES-MANIP-009 — Ray paths as a geometric aid

Rays are reflected specularly off the exact surface and can be drawn for
the selected reflector. They carry no amplitude and are not a field result.
*Verification:* `ManipulatorRays.*`, `ManipulatorsUiTest.FocalMarkersAndRayPathsAreDrawn`.

### RES-MANIP-010 — Focus metrics with definitions

Peak magnitude and location, widths at half the peak along each grid axis,
displacement from the intended point, magnitude there, sidelobe ratio and
focal gain are calculated from field results with documented definitions.
A width that the grid does not resolve is reported as such.
*Verification:* `FocusMetrics.*`.

### RES-MANIP-011 — Comparison of configurations

The results of a session can be compared side by side with their model and
focus metrics, so free-field, active, passive and hybrid configurations are
judged by the same quantities.
*Verification:* `ManipulatorsUiTest.FieldSimulationWithAReflectorIsEstimatedRunAndCompared`.

### RES-MANIP-012 — Hybrid focusing

With the reflector model selected, phase focusing and inverse design use
each element's direct plus reflected response. Time-delay focusing states
that reflected paths are not compensated.
*Verification:* `HybridFocusing.*`, `ManipulatorsUiTest.HybridFocusingThroughThePanelUsesTheReflectedPaths`,
`ManipulatorExamples.D_*`.

### RES-MANIP-013 — Geometry optimization interfaces and demonstration

Design variables with bounds, an objective that runs a physical model, a
derivative-free local search that records every candidate, and
manufacturing constraints. In the application one reflector parameter can
be optimized for the pressure at the intended focus; the project changes
only when the user applies the result, as one undoable step.
*Verification:* `GeometryOptimization.*`,
`ManipulatorsUiTest.GeometryOptimizationNeedsTheModelImprovesTheTargetAndAppliesOnRequest`.

### RES-MANIP-014 — Manufacturing constraints are checked, not enforced

Constraints are separate from physical parameters; violations are reported
with value, limit and suggestion, and the geometry is never changed
silently.
*Verification:* `ManufacturingConstraintsCheck.*`.

### RES-MANIP-015 — User interface

A menu to add every kind, a property editor defined in a `.ui` form,
markers for the shape's focal points and the design intent, model selection
in the Acoustics panel, undo for every edit, persistence in the project
file.
*Verification:* `ManipulatorsUiTest.*`, `ManipulatorPersistence.*`.

### RES-MANIP-016 — Validation and examples

Numerical validation against closed-form references is recorded in
`docs/manipulators/PASSIVE_WAVE_VALIDATION.md`. Five example projects
(parabolic, elliptical, lens estimate, hybrid, optimization) are generated
by the engine and checked on every test run.
*Verification:* `ManipulatorExamples.*`.
