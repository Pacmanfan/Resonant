# Advanced Acoustic Simulation and Full-Wave Numerical Solvers

**Owner of:** `RES-FULLWAVE-001..018`
**Source:** REQ-CHAT-20261126-005 ("Prompt 9")
**Refines:** `product.md` RES-PRODUCT-001, -006, -008, -012
**Decisions:** ADR-000205, ADR-000207, ADR-000208, ADR-000209
**Documents:** `docs/advanced_acoustics/`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-FULLWAVE-001 — Solver registry with declared capabilities

Every acoustic solver declares its id, method, whether it is full-wave,
the phenomena it represents, its cost scaling and its limitations in a
registry. Solvers stay separate implementations.
*Verification:* `FdtdPlan.ObjectsBecome*` (assessment part), suite `layering`.

### RES-FULLWAVE-002 — Assessment, never automatic selection

For the current scene the application states, for every solver, what it
would leave out. It never chooses, switches or substitutes a solver.
*Verification:* `FdtdPlan.ObjectsBecome*`, `FdtdExamples.C_*`,
`FullWaveUiTest.PanelShowsThePlan*`.

### RES-FULLWAVE-003 — Staggered-grid FDTD in two and three dimensions

The linear acoustic first-order system is solved on a uniform staggered
grid, second-order accurate, with pressure and particle velocity as state,
in 2D and in 3D.
*Verification:* `FdtdPropagation.*`.

### RES-FULLWAVE-004 — Stability is enforced before execution

A time step above the Courant limit of the actual discretization,
dimension and fastest material is refused with the limit stated. An
unstable field ends the run as failed.
*Verification:* `FdtdStability.*`, `FdtdRun.*`.

### RES-FULLWAVE-005 — Resolution is the user's and is never lowered

Cells per wavelength and the time-step fraction are settings. A request
that exceeds the limits is refused with what to change.
*Verification:* `FdtdPlan.ResolutionTimeStep*`, `FdtdPlan.Refuses*`,
`FullWaveUiTest.RefusedAndCancelled*`.

### RES-FULLWAVE-006 — Preflight

Grid, time step, steps, simulated time and its basis, resolution,
numerical phase-velocity error, memory, work, a time estimate, sources,
probes, how each object is represented, warnings and assumptions are
reported before a run.
*Verification:* `FdtdPlan.*`, `FullWaveUiTest.PanelShowsThePlan*`.

### RES-FULLWAVE-007 — Domain boundary conditions

Rigid, pressure-release, locally reacting impedance and absorbing faces,
each tested against its reflection coefficient.
*Verification:* `FdtdBoundaries.*`.

### RES-FULLWAVE-008 — Absorbing layers with measured reflection

A perfectly matched layer of configurable thickness outside the region,
with reflection measured at normal and oblique incidence and documented
limits.
*Verification:* `FdtdBoundaries.AbsorbingLayer*`.

### RES-FULLWAVE-009 — Spatially varying materials and interfaces

Density, speed of sound and attenuation per cell; interfaces satisfy the
continuity conditions through the scheme; reflection, transmission, wave
speed, refraction and attenuation are tested against closed forms.
*Verification:* `FdtdMaterials.*`.

### RES-FULLWAVE-010 — Geometry takes part, from its definition

Scene objects become rigid, pressure-release or material cells through an
inside test derived from each geometry kind's definition, never from the
display mesh. Thin shells that are thickened, hidden objects, and settings
that cannot be represented are reported.
*Verification:* `FdtdSolidModel.*`, `FdtdPlan.ObjectsBecome*`.

### RES-FULLWAVE-011 — Transient signals and true time delay

Continuous, tone-burst, Gaussian, Ricker, sweep and sampled signals, with
repetition; an emitter's delay shifts its signal in time.
*Verification:* `FdtdWaveform.*`, `FdtdPropagation.TimeDelay*`,
`FdtdScene.EmitterAmplitude*`.

### RES-FULLWAVE-012 — Source strength keeps its meaning

An emitter's amplitude is its pressure at 1 m in 3D for any signal, and at
1 m at its frequency in 2D, where the run is stated to be two-dimensional.
*Verification:* `FdtdPropagation.PointSourceIn3D*`,
`FdtdPropagation.LineSourceIn2D*`, `FdtdScene.EmitterAmplitude*`,
`FdtdExamples.A_*`.

### RES-FULLWAVE-013 — Recording under control

Probe signals, a chosen number of snapshots, the steady-state field of a
continuous run, and energy are recorded; nothing else is stored. Runs are
cancellable background jobs with progress.
*Verification:* `FdtdRun.*`, `FullWaveUiTest.RefusedAndCancelled*`.

### RES-FULLWAVE-014 — Frequency analysis that states its limits

Spectra of probe signals carry sampling rate, observation time, window,
resolution and aliasing limit; padding never changes the stated
resolution.
*Verification:* `FdtdSpectrum.*`.

### RES-FULLWAVE-015 — Resonance identification, labelled as such

Response peaks are found and compared with analytical modes for simple
enclosures, and are never presented as eigenmodes.
*Verification:* `FdtdResonance.*`, `FdtdExamples.E_*`,
`FullWaveUiTest.ContinuousRun*`.

### RES-FULLWAVE-016 — Results in the application

A steady-state field joins the ordinary results (display, analysis,
comparison, run record); recorded instants are shown as instantaneous
pressure; signals, spectra and energy are plotted; display changes never
recalculate.
*Verification:* `FullWaveUiTest.*`.

### RES-FULLWAVE-017 — Passive and hybrid structures in full-wave runs

A reflector focuses and an array is focused through a reflector with
phases obtained from the full-wave solver itself.
*Verification:* `FdtdExamples.F_*`, `FdtdExamples.G_*`.

### RES-FULLWAVE-018 — Validation and examples

Validation against closed-form references with tolerances is recorded in
`docs/advanced_acoustics/ACOUSTIC_VALIDATION.md`, including refinement in
space and time. Eight example projects (A in 2D and 3D, B to G) are
generated by the engine and checked on every test run.
*Verification:* `FdtdExamples.*`.
