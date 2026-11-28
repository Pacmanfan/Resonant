# Functional Structures and Geometry-Driven Engineering

**Owner of:** `RES-FUNC-001..016`
**Source:** REQ-CHAT-20261126-017 (Prompt 19, "Geometry-Driven Energy Transformation and Functional Structures")
**Refines:** `shape_function_energy.md` RES-SFE-001 (function computed), RES-SFE-002 (object- and function-centric workflows), RES-SFE-003 (geometry representations), RES-SFE-004 (resonance); `coupled_engine.md` RES-CEME-005 (physical systems)
**Decisions:** ADR-000221 (builds on ADR-000219, ADR-000220, ADR-000214)
**Documents:** `docs/architecture/FUNCTIONAL_STRUCTURE_ARCHITECTURE.md`, `docs/architecture/FUNCTIONAL_STRUCTURE_AUDIT.md`, `docs/geometry/GEOMETRY_PARAMETERIZATION.md`, `docs/physics/ENERGY_TRANSFORMATION.md`, `docs/physics/RESONANT_STRUCTURE_ENGINEERING.md`, `docs/optimization/FUNCTION_DRIVEN_DESIGN.md`, `docs/physics/FUNCTIONAL_STRUCTURE_VALIDATION.md`, `docs/electromagnetics/CAVITY_RESONATORS.md`, `docs/electromagnetics/OPTICAL_RESONATORS.md`, `docs/electromagnetics/PERIODIC_LAYERS.md`, `docs/manipulators/ACOUSTIC_HORNS.md`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-FUNC-001 — Functional structures are project resources, not shape categories

A functional structure (`resonant.functional-structure` v1, project
extension `functional`) references scene objects by role, material regions,
boundaries, domains, excitation, environment, a frequency range, an analysis
(kind, model, settings), objectives, constraints and optionally a physical
system of the coupled engine. It carries no physical behavior of its own:
every metric is computed by the analysis it names. Elements have stable ids;
storing, replacing and deleting are undoable; references survive save, load
and renames; projects without the extension open unchanged.
*Verification:* `FunctionalStructures.*Serialization*`, `FunctionalServices.*`.

### RES-FUNC-002 — Distinct, traceable geometry representations

Visual geometry (the viewport's tessellation), engineering geometry (scene
objects and their parameters), physical geometry (the structure's regions
and boundaries on those objects), simulation geometry (the discretization a
model builds: a Yee grid, a finite-element mesh, Webster steps) and
functional geometry (the structure's named parameters and objectives) are
separate; each analysis result names the engineering objects, the regions
and the discretization it used.
*Verification:* `FunctionalStructures.ResultsNameTheirRepresentations`.

### RES-FUNC-003 — Named geometric parameters

Parameters have an id, name, unit, range, default and group, and are bound
to a scene object's geometry parameter or position (the optimization
framework's scene bindings), free (a value stored in the structure), or
derived (an expression of other parameters). Values are read from the
project; changes go through the geometry kinds' own rules (range, integer,
validity) and are applied as one undoable step; a derived parameter cannot
be set; a range or validity violation is refused with the reason;
dependency cycles are refused.
*Verification:* `FunctionalStructures.Parameters*`.

### RES-FUNC-004 — Revisions and staleness

Every result records the dependency fingerprints it was computed from:
the referenced objects' geometry and placement, the referenced materials,
the structure definition and the discretization settings. A result whose
fingerprints differ from the current project's is reported stale with the
reasons (geometry, material, definition); a job whose project changed while
it ran yields a result marked stale; a simulation geometry built for a
fingerprint is reused only while the fingerprint is unchanged.
*Verification:* `FunctionalStructures.Staleness*`, `FunctionalStructures.SimulationGeometryCache*`, `FunctionalServices.*Stale*`.

### RES-FUNC-005 — Analyses from domain models

An analysis kind declares its domains, energy-transformation category
(propagation, reflection, focusing, confinement, dissipation, force), the
behavior it can establish (resonant, broadband, non-resonant), models with
fidelity, assumptions and validity, and the metrics it computes with units
and definitions. The functional module (`src/core/functional`) contains no
physics and includes no domain; the domains compute; analyses are registered
in `src/core/coupling`. An analysis reports the model it actually used and
refuses configurations it does not support.
*Verification:* `tools/check_layering.py`, `FunctionalAnalyses.Registry*`.

### RES-FUNC-006 — Microwave cavity resonator

Rectangular cavities with conducting walls and a dielectric fill: TE and TM
modes, field distributions, stored electric and magnetic energy, wall loss
(surface resistance of the wall material), dielectric and conduction loss of
the fill, unloaded, external and loaded Q, bandwidth, the reflection of a
coupled port near resonance, and dimensional sensitivity. A full-wave model
(Yee, PEC walls) gives the resonant frequencies numerically with mesh
convergence and the decay rate of a lossy fill. Validation: frequencies
against the closed form, wall Q against the closed form for TE10p, energy
equipartition at resonance, Q of the fill = ωε′/σ_eff, the half-power
bandwidth of the port response = f₀/Q_L, the Yee frequencies converging to
the analytical ones and the Yee decay rate equal to σ/ε.
*Verification:* `CavityResonator.*`, `FunctionalExamples.MicrowaveCavity*`.

### RES-FUNC-007 — Two-mirror optical resonator

A paraxial (scalar Gaussian-beam) model: stability g₁g₂ (stable, marginal,
unstable), waist size and position, spot sizes on the mirrors, Rayleigh
range, longitudinal mode spacing, transverse mode spacing from the Gouy
phase, mode frequencies, finesse from mirror reflectivities and aperture
clipping, linewidth, photon lifetime, Q, transmission on resonance and
stored energy for a circulating power. Its assumptions and validity
(paraxial: waist ≫ wavelength; aligned mirrors) are stated with every
result; an unstable resonator has no Gaussian mode and is reported as such.
Validation: the beam parameter reproduces itself after one ABCD round trip,
the confocal closed forms, and the free spectral range and linewidth of a
plane Fabry–Perot computed independently by the exact layered-media
solution.
*Verification:* `OpticalResonator.*`, `FunctionalExamples.OpticalResonator*`.

### RES-FUNC-008 — Acoustic horn

Axisymmetric horns (conical, exponential, hyperbolic) by the Webster
equation, terminated by the radiation impedance of a baffled piston: throat
impedance, normalized throat resistance (impedance transformation),
reflection at the throat, pressure, particle velocity and intensity along
the axis, radiated power, mouth directivity and directivity index, and
comparison of profiles under equal throat, mouth, length and drive.
Validation: a uniform duct against the exact transmission-line formula, an
exponential horn terminated by its outgoing-wave impedance against the
infinite-horn closed form, a conical horn against the spherical-wave
impedance, power conservation from throat to mouth, step convergence, and
the piston radiation impedance against its limits.
*Verification:* `AcousticHorn.*`, `FunctionalExamples.AcousticHorn*`.

### RES-FUNC-009 — Mechanical resonator

The existing mechanical studies (finite elements, analytical beams) as an
analysis: natural frequencies, effective modal masses, mode shapes,
analytical comparison where a closed form exists, mesh convergence and
geometric sensitivity.
*Verification:* `FunctionalExamples.MechanicalResonator*`.

### RES-FUNC-010 — Periodic unit cells

A two-layer electromagnetic unit cell at normal incidence: the exact Bloch
dispersion, band gaps and their edges, the attenuation inside a gap, the
transmission of a finite stack (the exact layered-media solution), and the
long-wavelength effective index with its homogenization validity
(a/λ < 0.1) and the quasi-static parallel and perpendicular permittivities
labelled as homogenized values; the existing periodic elastic rod as an
analysis. No effective property is ever entered by hand. Validation: the
stack's transmission falls inside the predicted gap at the predicted rate,
and the effective index tends to the quasi-static value.
*Verification:* `PeriodicLayers.*`, `FunctionalExamples.UnitCell*`.

### RES-FUNC-011 — Sweeps, sensitivity and mode tracking

One- and multi-parameter sweeps (grids) evaluate an analysis per point on
its own copy of the design; local sensitivities by central differences
(absolute and normalized); modes are tracked across a sweep by label when
the model labels them, otherwise by continuity, and a correspondence that
is ambiguous (two candidates within the tolerance) is flagged, never
guessed silently.
*Verification:* `FunctionalStructures.Sweep*`, `FunctionalStructures.ModeTracking*`, `FunctionalExamples.*Sensitivity*`.

### RES-FUNC-012 — Objectives, constraints and explainable candidates

Objectives name a metric, its unit, a sense (minimize, maximize, target,
within a range), a target and a weight; constraints are expressions of
parameters with a relation and limit, optionally tied to a manufacturing
process (minimum wall, feature, channel or spacing, tolerance); hard
constraints reject a candidate, soft ones penalize it. Candidates come from
sweeps or from the existing optimization framework (an adapter, not a new
optimizer). A recommended candidate is explained: parameters changed,
materials and operating conditions, model, objectives improved and worsened,
active constraints, the physical mechanism and remaining validity limits.
*Verification:* `FunctionalStructures.Objectives*`, `FunctionalStructures.Constraints*`, `FunctionalAnalyses.OptimizationAdapter*`.

### RES-FUNC-013 — Coupled behavior through the engine

A structure that references a physical system of the Coupled Energy–Matter
Engine is analyzed by running that system, with the thickness of each region
linked to a scene object taken from the object; parameters of those objects
therefore drive coupled results (absorbed power, temperature, stress). A
structure whose behavior has no feedback uses its single-domain analysis.
*Verification:* `FunctionalExamples.CoupledSlab*`.

### RES-FUNC-014 — Services, contract and clients

`FunctionalStructureService` in the application services and the protobuf
contract: analysis kinds, list, get, set, delete, parameters, set
parameters, validate, analyze, sweep, sensitivity, result (with its current
staleness), optimize and explain. Analyses, sweeps and sensitivities run as
jobs; results are registered documents with provenance (structure, parameter
values, fingerprints, model, solver version, assumptions). The GUI, Python
and MCP use the same services; permissions apply.
*Verification:* `FunctionalServices.*`, `python/tests/test_functional_api.py`.

### RES-FUNC-015 — Workflow in the application

A Functional Structures panel lists the structures, edits parameters (an
undoable change of the scene), runs the analysis as a job, shows metrics
with units, modes, the model and assumptions and whether the result is
stale, and runs a one-parameter sweep shown as a curve. Changing the view
does not recompute.
*Verification:* `FunctionalPanel.*` (app suite).

### RES-FUNC-016 — Reference examples and honest status

Example projects for the microwave cavity, the optical resonator, the
acoustic horn, the mechanical resonator, the photonic unit cell and a
coupled slab run their analyses in tests; every metric they show is
computed; documentation distinguishes implemented, partial and planned
functionality.
*Verification:* `FunctionalExamples.*`, `Examples.EveryExampleProjectLoads`.
