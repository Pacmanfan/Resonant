# Acoustic Physics Engine and Wave-Field Simulation

**Owner of:** `RES-ACOUSTIC-001..020`
**Source:** REQ-CHAT-20261126-003 ("Prompt 7")
**Refines:** `product.md` RES-PRODUCT-003, -004, -006, -008, -010, -012
**Decisions:** ADR-000205, ADR-000207
**Documents:** `docs/acoustics/`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-ACOUSTIC-001 — A separate, GUI-independent module

Acoustic physics is a module of the core (`src/core/acoustics`) with
separate parts for domain data, sources, propagation models, field
evaluation, focusing, inverse design, sampling and results. It contains no
Qt or OpenGL code and is usable without the application.
*Verification:* suite `layering`; the core test suite uses it without a GUI.

### RES-ACOUSTIC-002 — Acoustic emitter

An acoustic emitter is a scene object with a stable id, position,
orientation, frequency, amplitude, initial phase, time delay, waveform
designation, directivity model (omnidirectional point, directional point,
baffled piston), active state, physical radius and an optional transducer
profile name. Idealized emitters and stand-ins for physical transducers are
distinguished. The emitter radiates along its local +Z axis; orientation
affects directional models.
*Verification:* `AcousticDirectivity.*`, `AcousticPersistence.*`,
`AcousticsUiTest.EmitterEditor*`.

### RES-ACOUSTIC-003 — Emitter arrays

An acoustic array has a parametric layout (linear, rectangular planar with
independent counts and spacings, circular, cylindrical, spherical, custom),
element defaults, and per-element amplitude, phase, delay, frequency and
activation that survive changes of the array parameters by a documented
rule.
*Verification:* `AcousticArray.*`, `AcousticsUiTest.ArrayEditor*`.

### RES-ACOUSTIC-004 — Explicit propagation medium

The propagation medium is chosen explicitly (air with configurable
temperature, water, custom) and is not derived from the material of any
object. Solid geometry does not influence the field in this milestone, and
every result says so.
*Verification:* `AcousticMedium.*`, `AcousticsUiTest.MediumSettings*`,
`FoundationStillWorksAlongsideAcoustics`.

### RES-ACOUSTIC-005 — Analytical free-field propagation

Harmonic point-source propagation with spherical spreading, complex phase,
optional attenuation, optional directivity and superposition, with
documented source normalization, units, phase and frequency conventions,
assumptions and limits, behind an interface other models can implement.
*Verification:* `AcousticPropagation.*`, `AcousticInterference.*`.

### RES-ACOUSTIC-006 — Source strength has a fixed meaning

A source's amplitude is its own pressure amplitude at 1 m. The number of
sources never changes what an amplitude means.
*Verification:* `AcousticInterference.SourceCountDoesNotRescaleAmplitudes`.

### RES-ACOUSTIC-007 — No singularity near a source

The field is finite at and near every source; samples where the model does
not apply are flagged and kept out of statistics.
*Verification:* `AcousticPropagation.NoSingularityAtOrNearASource`,
`AcousticField.SamplesInsideASource*`.

### RES-ACOUSTIC-008 — Field quantities are distinct

Complex pressure, magnitude, phase, RMS pressure, sound pressure level (with
its reference), time-averaged active intensity from particle velocity, and
normalized relative strength are separate, labelled quantities. A relative
value is never labelled as an absolute pressure or intensity, and the
plane-wave intensity formula is not applied where it does not hold.
*Verification:* `AcousticIntensity.*`,
`AcousticsUiTest.FieldIsDrawnFromStoredData*` (descriptions).

### RES-ACOUSTIC-009 — Focusing by phase or by true time delay

For an array and a target point (an object or coordinates) the engine
calculates per-element phase compensation at a frequency, or per-element
time delays, as two distinct controls, with optional amplitude tapers. The
settings can be inspected before they are applied. A focus is never
assumed: the field is calculated.
*Verification:* `AcousticFocus.*`, `AcousticsUiTest.FocusWorkflow*`,
`TimeDelayFocusWritesDelaysNotPhases`.

### RES-ACOUSTIC-010 — Targets

Point targets with desired amplitude, phase, weight and suppression are
implemented. Line, plane, surface and volume targets exist in the data
model and are reported as unsupported rather than solved.
*Verification:* `AcousticInverse.SceneTargetsAreCollected*`,
`AcousticsUiTest.InverseDesign*`.

### RES-ACOUSTIC-011 — Inverse design for multiple point targets

A regularized, weighted least-squares solution through a numerically
stable decomposition (no explicit inverse), with configurable
regularization and amplitude limit, residual, rank, condition number,
per-element complex strengths converted to amplitude and phase, and a clear
statement when the targets are not achieved accurately.
*Verification:* `AcousticInverse.*`.

### RES-ACOUSTIC-012 — Simulation regions

Planes XY, XZ, YZ, an arbitrary plane (a rotated region) and a rectangular
volume, with configurable size and sampling resolution.
*Verification:* `AcousticRegion.GridDimensionsAndPositions`.

### RES-ACOUSTIC-013 — Preflight and resource protection

Before a field is computed the number of points, memory, work and sampling
relative to the wavelength are estimated and shown; a grid above the memory
limit is refused; resolution is never changed silently.
*Verification:* `AcousticRegion.Preflight*`,
`AcousticsUiTest.ExcessiveResolutionIsRefused*`.

### RES-ACOUSTIC-014 — Responsive, cancellable field calculation

Field calculations run as background jobs on several threads with progress
and cancellation; the user interface stays responsive.
*Verification:* `AcousticField.Job*`.

### RES-ACOUSTIC-015 — Visualization of calculated data

2D field maps and slices through a 3D volume of pressure magnitude, phase,
instantaneous pressure and relative intensity, with colour-scale and
normalization controls that state whether values are absolute or
normalized, and an animation of the instantaneous pressure driven by the
stored complex field. Changing the camera, colours or slices does not
recompute or re-upload the field.
*Verification:* `AcousticsUiTest.FieldIsDrawnFromStoredData*`,
`VolumeSimulationRunsWithSlices*`.

### RES-ACOUSTIC-016 — Analysis and probes

An analysis panel shows frequency, medium, active emitters, region size,
sampling, maximum pressure, RMS, peak location, target values, solver time
and model assumptions. Probes report field quantities at their positions,
exportable as CSV and JSON in documented formats.
*Verification:* `AcousticProbe.*`, `AcousticsUiTest.ProbesReportAndExport`,
`FocusWorkflow*`.

### RES-ACOUSTIC-017 — Reproducibility

Every result keeps the project id, simulation id, solver and version,
model, frequency, medium, emitter and array configuration, targets, region,
sampling, numerical options, execution time, assumptions and software
version. Editing the scene never alters an existing result. Several results
can be kept and compared.
*Verification:* `AcousticField.AResultIsASnapshot*`,
`ComputedGridMatchesPointEvaluationAndHasStatistics`,
`AcousticsUiTest.VolumeSimulationRunsWithSlicesAndSeveralResultsCanBeCompared`.

### RES-ACOUSTIC-018 — Numerical validation

The engine is validated against analytical expectations or independent
properties for every item listed in `docs/acoustics/ACOUSTIC_VALIDATION.md`.
*Verification:* that document and the tests it names.

### RES-ACOUSTIC-019 — Example projects

Reproducible examples: two-source interference, a linear array, a focused
planar array and a multiple-target inverse design, each with stated
assumptions, each checked by a test, none able to go stale.
*Verification:* `AcousticExamples.*`, `AcousticsUiTest.ExampleProjectsOpenAndSimulate`.

### RES-ACOUSTIC-020 — The foundation keeps working

The application foundation (RES-APP-001..020) is unchanged in behaviour.
*Verification:* suites `core` and `app` in full.
