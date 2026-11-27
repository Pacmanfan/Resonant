# Shape–Function–Energy: Architectural Requirements

**Owner of:** `RES-SFE-001..012`
**Source:** REQ-CHAT-20261126-015 (Prompt 17, "Shape–Function–Energy Architecture")
**Refines:** `product.md` RES-PRODUCT-006 (physics decides, geometry does not), RES-PRODUCT-010 (optimization), RES-PRODUCT-013 (architecture anticipates all domains), RES-PRODUCT-016 (the long-term workflow)
**Decisions:** ADR-000219
**Documents:** `docs/vision/SHAPE_FUNCTION_ENERGY.md` (the principle), `docs/architecture/SHAPE_FUNCTION_ENERGY_AUDIT.md` (how it maps to the code), `docs/architecture/COUPLED_MULTIPHYSICS_ARCHITECTURE.md`, `docs/physics/`, `docs/materials/`, `docs/geometry/FUNCTIONAL_GEOMETRY.md`, `docs/architecture/ENERGY_MATTER_RESEARCH_ROADMAP.md`
**Status:** normative for every future change. These requirements constrain *how* new capabilities are built; most of the capabilities they govern are planned, not implemented (`IMPLEMENTATION_STATUS.md`, the audit). A requirement below is met by a change when that change follows it; the verification line names how.

### RES-SFE-001 — Function comes from physics, never from labels

An object's physical function (focusing, confining, storing, filtering,
converting, transferring energy) is the result of a physical model applied
to its geometry, materials, excitation and environment. No behavior is
attached to an object because of its type, name or category ("cavity",
"metamaterial", "horn"); categories may only choose which models and
analyses are offered. Generalizes RES-PRODUCT-006 from focusing to every
function.
*Verification:* review of every new domain model against this rule; `docs/vision/SHAPE_FUNCTION_ENERGY.md`.

### RES-SFE-002 — Object-centric and function-centric workflows on one optimization framework

Both workflows are supported: analyzing a given object, and searching for
geometry, materials, internal structure, arrangement or excitation that
produces a desired function. Function-centric design uses the
domain-independent optimization framework (`src/core/optimization`, ADR-000214)
through domain adapters; a new domain adds an adapter and objectives, never a
second optimizer.
*Verification:* `tools/check_layering.py`; new objectives have their quantities documented (RES-OPT-*).

### RES-SFE-003 — Geometry representations are distinct

The architecture distinguishes visual geometry (display meshes), engineering
geometry (the parametric or imported definition), physical boundary geometry
(the surfaces and conditions a solver sees), material regions, simulation
meshes or grids, and functional or periodic internal structure. A visual
representation is never assumed to be fit for simulation: a conversion to a
simulation representation reports its fidelity (resolution against the
wavelength, lost features, approximations).
*Verification:* `docs/geometry/FUNCTIONAL_GEOMETRY.md`; existing setups already report resolution (FDTD and Yee preflight).

### RES-SFE-004 — Resonance is a cross-domain concept with domain-specific solvers

Resonant results of every domain (acoustic cavities and enclosures,
electromagnetic cavities and resonators, structural modes, elastic and
periodic structures, circuits where relevant) are reported with one shared
vocabulary where it applies: natural frequency, mode shape or field, modal
energy, damping, quality factor, bandwidth, coupling and mode splitting,
localization, the model and its assumptions. Each domain keeps its own
equations and solvers; no domain is forced into another's formulation.
*Verification:* `docs/physics/RESONANT_STRUCTURES.md`; a future shared result type is tested per domain.

### RES-SFE-005 — Effective properties are derived, labelled and kept apart

Effective (homogenized, frequency-dependent) properties of an engineered
structure are stored and shown separately from the properties of its
constituent materials, with the model that derived them, its validity
(frequency range, wavelength-to-cell ratio, excitation) and its validation.
Unusual effective properties (negative, anisotropic, dispersive) are never
assigned because a structure is called a metamaterial.
*Verification:* `docs/materials/METAMATERIALS_AND_ENGINEERED_STRUCTURES.md`.

### RES-SFE-006 — Coupling coordinates solvers, it does not replace them

Interactions between domains go through one coordination layer (the Coupled
Energy–Matter Engine) that exchanges named physical quantities with units,
locations and times between independent solvers; transfer operators
(interpolation, projection, conservative remapping) are explicit; one-way,
two-way, sequential, iterative and time-dependent coupling use the same
interface; solvers need not share a mesh, a grid, a time step or a numerical
method. Coupling code lives only in `src/core/coupling` (ADR-000205, ADR-000210).
*Verification:* `tools/check_layering.py` (only coupling uses two domains); `docs/architecture/COUPLED_MULTIPHYSICS_ARCHITECTURE.md`.

### RES-SFE-007 — Energy and momentum are accounted for explicitly

A coupled or single-domain result that claims a balance reports its terms:
stored energy, input, output through boundaries, dissipation, transfer to
other domains, and the residual, each with its definition and units; the
same for momentum and force where defined. A transfer between solvers does
not by itself make a result conservative: conservation is claimed only when
the residual is computed and within a stated tolerance.
*Verification:* `docs/physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md`; the existing energy records (FDTD, Yee, mechanical transient) as the first instances.

### RES-SFE-008 — Material state is explicit and evolving

A material's response may depend on state variables (position, time,
temperature, pressure, stress, strain, frequency, fields, composition,
phase, microstructure, damage, history). The material model represents
which state variables a property depends on, with provenance and validity,
and keeps constitutive models domain-specific (no single property structure
for all domains, ADR-000205). A property used outside its validity is
reported, never silently extrapolated.
*Verification:* `docs/materials/MATERIAL_STATE_ARCHITECTURE.md`.

### RES-SFE-009 — Multiscale modeling through adapters, with provenance

Microscopic, molecular or electronic-structure calculations are used through
optional adapters to external packages (separate processes where their
licenses require it), never as mandatory dependencies. Every value derived
across scales records whether it is measured, theoretical, computed by a
named tool and version, or homogenized, and the scale it came from.
*Verification:* `docs/physics/MULTISCALE_MODELING.md`; the license audit of any adapter (`docs/release/LICENSE_AUDIT.md` practice).

### RES-SFE-010 — Visualization shows several quantities at one place, without implied accuracy

Visualization can show several physical quantities of the same object and
location together (fields, energy density and flux, stress, temperature,
modes, band structures, effective properties) and states the model and
resolution behind each; no display suggests accuracy the model does not have
(smoothing, interpolation and extrapolation are marked).
*Verification:* `docs/vision/SHAPE_FUNCTION_ENERGY.md` (visualization section); existing layers keep units and quantity labels.

### RES-SFE-011 — Status is stated honestly

Documents, help pages, capabilities reported by the automation API and the
user interface distinguish current (implemented and tested), planned
(architecture defined), research prototype, and theoretical future
capabilities. Planned functionality is never documented as implemented.
*Verification:* `scripts/doc_integrity.py`; reviews; `IMPLEMENTATION_STATUS.md`.

### RES-SFE-012 — Conventions are shared

Every domain and every coupling uses SI units internally, the time
convention `e^{+jωt}` with peak amplitudes for phasors, right-handed world
coordinates in meters, and records the provenance of results (model, solver,
version, assumptions, project revision). A coupling converts nothing
implicitly: a quantity crossing domains carries its unit and convention.
*Verification:* the existing conventions (`src/core/acoustics/Conventions.h`, `docs/electromagnetics/MAXWELL_EQUATIONS.md`, `docs/mechanics/FLUID_STRUCTURE_COUPLING.md`), checked consistent by the audit.
