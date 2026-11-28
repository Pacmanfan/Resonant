# Functionally Architected Matter

**Owner of:** `RES-FAM-001..021`
**Source:** REQ-CHAT-20261127-001 (the foundational architecture prompt "Functionally Architected Matter and Emergent Physical Systems")
**Refines:** `multiscale.md` RES-MSC-001..014 (lineage, homogenization), `metamaterials.md` RES-META-001..012 (effective properties), `coupled_engine.md` RES-CEME-001..016 (physical systems), `material_state.md` RES-DMS-001..014, `optimization.md` RES-OPT-001..025, `laboratory.md` RES-LAB-001..018, `accounting.md` RES-ACC-001..016
**Decisions:** ADR-000228 (builds on ADR-000220, ADR-000224, ADR-000225, ADR-000226, ADR-000227)
**Documents:** `docs/materials/ARCHITECTED_MATTER.md`, `docs/materials/ARCHITECTED_MATTER_AUDIT.md`, `docs/materials/INTERFACE_ENGINEERING.md`, `docs/materials/ARCHITECTED_MATTER_INTEGRATION.md`, `docs/materials/MANUFACTURING_AWARENESS.md`, `docs/materials/SOLVER_CAPABILITY_MATRIX.md`, `docs/materials/ARCHITECTED_MATTER_VALIDATION.md`, `docs/materials/ARCHITECTED_MATTER_ROADMAP.md`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

The organization of matter is a design variable. An architecture says how the
matter of one or more scene objects is organized; the solvers predict what that
organization does. Nothing here replaces the material library, the scene, the
multiscale lineage, the coupled engine, the optimizer or the laboratory.

### RES-FAM-001 — Audit and reuse

The architecture builds on the existing material library, scene objects,
multiscale representations and lineage, effective materials, the coupled engine,
material-state studies, the optimizer, the laboratory and accounting. It creates
no second material, geometry, solver, optimization or experiment infrastructure,
and projects without an architecture are unchanged.
*Verification:* `docs/materials/ARCHITECTED_MATTER_AUDIT.md`; `tools/check_layering.py`; `MatterArchitecture.ProjectsWithoutAnArchitectureAreUnchanged`.

### RES-FAM-002 — The canonical architecture

An architecture is a hierarchical record. It holds:
- its objects (scene objects);
- functional regions;
- layer stacks of ordered layers;
- interfaces between regions;
- property fields;
- intended functions and transformation pathways;
- the adaptive class and states;
- the manufacturing processes, constraints and records;
- provenance.

Each element has a stable identity, units are SI, and coordinate frames are
named. Validation reports dangling references, fractions that do not sum to one,
non-positive thicknesses, and interfaces between regions that do not touch. The
architecture is serialized to the project extension `matter` and reloads without
loss, and its revision is a content fingerprint.
*Verification:* `MatterArchitecture.*`.

### RES-FAM-003 — Composition and phases

A region's composition lists its constituents:
- each constituent is a library material, or a named substance with a
  formula and a material class;
- each has a fraction (volume, mass or atomic) with its uncertainty.

Its phases have:
- an identity, a material, a fraction and a spatial distribution;
- the conditions (temperature, pressure, field) under which they are stable;
- optionally, a link to a material-state study.

Atomic-level composition is never required. A bulk material is a valid
composition of one constituent.
*Verification:* `MatterArchitecture.CompositionPhasesAndMicrostructure`.

### RES-FAM-004 — Microstructure, orientation and anisotropy

A region may describe its microstructure:
- crystal structure and lattice parameters;
- grain size and its distribution, texture, porosity;
- defects, inclusions and domains;
- a link to a multiscale representation.

The microstructure is recorded with its provenance. A property that depends on
it comes from a derivation, a measurement or a stated source, never from a
lookup. A region may carry an orientation (Euler angles in its stack's frame),
and anisotropic property sets:
- orthotropic elastic constants;
- principal conductivities;
- piezoelectric coefficients, with their poling direction.

Solvers that cannot use anisotropy refuse such regions.
*Verification:* `MatterArchitecture.CompositionPhasesAndMicrostructure`, `MatterAnalyses.OrientationRotatesTheProperties`.

### RES-FAM-005 — Functional regions

A functional region is bound to a scene object. It occupies the whole object, a
layer of a stack, or a sub-volume in the object's frame. It names:
- its role and the material source: a library material, an effective
  material derived from its composition, or a graded field;
- composition, phases, microstructure, orientation and anisotropic
  properties;
- operating conditions and the manufacturing process;
- property uncertainty and intended functions.

A single object may hold several regions.
*Verification:* `MatterArchitecture.AMultimaterialStructureWithRegions`.

### RES-FAM-006 — Layer stacks and patterns

A stack lies on an object along a named axis of its frame. Its layers are
ordered, and each has a region, a thickness and an in-plane orientation. Each
pair of consecutive layers has an interface (perfect only when declared). A
layer may be patterned: uniform, segmented (electrodes with their coverage and
gaps), periodic (period and fill) or with inclusions. A solver uses a pattern
only where it has a model for it, and refuses it otherwise.
*Verification:* `MatterArchitecture.StacksLayersAndPatterns`, `MatterAnalyses.UnsupportedFeaturesAreRefused`.

### RES-FAM-007 — Spatial property fields

A property may vary in space along an axis of a frame:
- constant, linear, power-law, exponential or tabulated;
- with its unit and its bounds;
- marked idealized (a mathematical distribution) or realizable (measured or
  manufactured, with the process).

Fields are evaluated, averaged in series and in parallel, and discretized into
sublayers with the error of the discretization.
*Verification:* `MatterFields.*`.

### RES-FAM-008 — Property classes and effective properties

Properties are distinguished as:
- intrinsic (a constituent);
- interface;
- effective (a combination, with its method);
- geometry-dependent;
- frequency-, temperature- or state-dependent.

An effective property of a composition is given only by a method that applies
(Voigt, Reuss and Hill; Hashin–Shtrikman bounds; Maxwell Garnett for dilute
inclusions; Gibson–Ashby scaling for cellular solids), together with the
rigorous bounds, the method's validity and its assumptions. An unqualified
average is never given. Missing constituent properties are reported, never
substituted.
*Verification:* `MatterMixing.*`.

### RES-FAM-009 — Interfaces

An interface joins two regions. Its models are:
- perfect bonding;
- finite stiffness (normal and shear);
- cohesive zone (strength, fracture energy);
- frictional contact;
- thermal contact resistance;
- electrical contact resistance;
- dielectric, charge-transfer, diffusive or reactive interfaces.

Each parameter has a unit, a provenance and an uncertainty. An interface may also
record:
- its adhesion energy and residual stress;
- its defects and chemical compatibility;
- its preparation.

Derived quantities are reported with the formula they come from:
- normal-incidence acoustic transmission across the interface;
- the thermal-mismatch stress and curvature of the joined layers;
- the temperature jump under a heat flux.

No interface is assumed perfect: one without a model is reported as undefined.
*Verification:* `MatterInterfaces.*`.

### RES-FAM-010 — Solver capability declarations

Every analysis that consumes architectures declares:
- the physics, material models and interface models it supports;
- its dimensionality, scales and couplings;
- its nonlinearities and numerical limitations;
- its inputs and outputs.

An architecture is checked against the declaration before it is run. What the
analysis cannot represent is refused with the reason, never silently dropped
or approximated. A field that can be expressed in a solver's input does not
imply that the solver supports it.
*Verification:* `MatterAnalyses.UnsupportedFeaturesAreRefused`; `docs/materials/SOLVER_CAPABILITY_MATRIX.md`.

### RES-FAM-011 — Physical analyses of stacks

A stack is analyzed by:
- **classical lamination theory:** the extension, coupling and bending
  matrices, effective in-plane moduli, the neutral surface, reduced bending
  stiffness, and thermal curvature and ply stresses;
- **through-thickness conduction:** series resistances with contact
  resistances, effective conductivities, the temperature profile with its jumps;
- **through-thickness electrical analysis:** series resistance with contact
  resistances, and the dielectric capacitance;
- **the layered electromagnetic solver:** reflection, transmission and
  absorption against frequency;
- **plate and beam modes:** the modes of the coated and of the bare structure.

Each result carries its assumptions and validity, and a change of architecture
changes the prediction as the physics says.
*Verification:* `MatterAnalyses.*`, `MatterMechanics.*`.

### RES-FAM-012 — Energy transformation pathways

A function may declare a pathway of stages, each with:
- the input and output domains;
- the mechanism and the regions it acts in.

Each stage is matched to an implemented model, or reported as not modeled. An
evaluated stage reports:
- input, useful output, stored energy and dissipated power;
- conversion efficiency;
- uncertainty and operating conditions.

Its balance is checked by the accounting framework. A chain's efficiency is
given only when every stage is evaluated. Resonance is one mechanism among
others, and no stage may create energy.
*Verification:* `MatterPathways.*`.

### RES-FAM-013 — Coupled-engine integration

A stack becomes a physical system of the coupled engine:
- a layered slab with one region per layer, bound to the stack's object;
- graded layers as sublayers with constant laws carrying the field's values;
- effective properties as laws carrying the method;
- thermal contact resistances on the thermal participant's interfaces.

The engine runs it. Features the engine cannot represent are refused.
*Verification:* `MatterCoupling.*`.

### RES-FAM-014 — Inverse design

The optimizer's architecture adapter exposes design variables:
- layer thicknesses and material choices per region;
- interface parameters, in-plane orientations, electrode coverage.

Its quantities come from the stack analyses (stiffness, mass, frequencies,
thermal resistance, harvested power, absorption) and from the manufacturing
check. An architecture that cannot be manufactured is rejected or constrained,
by choice. Several objectives give a Pareto set whose trade-offs are reported,
and an optimized architecture is marked not validated.
*Verification:* `MatterOptimization.*`.

### RES-FAM-015 — Manufacturing awareness

Each region names its process (additive, deposition, lamination, bonding,
sintering, machining and others). The architecture states each process's
constraints:
- minimum and maximum layer thickness and minimum feature size;
- process temperature and compatible material classes;
- roughness and residual-stress limits.

The check reports violations, unassessable items (a missing limit) and the
thermal-mismatch stress built in at the process temperature. Manufacturing
records keep:
- batch, composition, process and parameters;
- deposition conditions and heat treatment;
- layer sequence, interface preparation and post-processing;
- inspections and measured properties (linked to laboratory datasets).

A theoretical architecture is never presented as manufacturable.
*Verification:* `MatterManufacturing.*`.

### RES-FAM-016 — Adaptive architectures and material state

An architecture has a class:
- static;
- passive adaptive;
- externally controlled (naming the control input);
- actively reconfigurable.

It names its states, each with the operating condition it holds in and an
optional material-state study. Reversibility is declared, never assumed.
*Verification:* `MatterArchitecture.AdaptiveClassesAndStates`.

### RES-FAM-017 — Exploration and evidence

**Direction B (an existing architecture: what can it do).** The exploration
lists:
- the applicable analyses and the implemented pathways;
- the missing properties;
- the risks, each computed with its numbers:
  - thermal-expansion mismatch stress against the interface strength;
  - a transduction layer near the neutral surface;
  - dielectric loss;
  - electrical leakage or short paths;
  - delamination;
  - manufacturing incompatibility.

**Direction A (a desired transformation).** The mechanisms between the input
and output domains are listed with:
- their implementation status;
- the properties they need;
- which regions and library materials have them.

In both directions, findings (from records and computations) are kept apart
from hypotheses, which are marked. The result is deterministic and changes
nothing.
*Verification:* `MatterExploration.*`.

### RES-FAM-018 — Experimental integration

An architecture can be a laboratory model link, with its revision. A laboratory
model predicts stack metrics, so that measurements calibrate layer and interface
parameters (validated on independent data by the laboratory's rules).
Manufacturing records reference measured datasets.
*Verification:* `MatterCoupling.InterfaceResistanceCalibratedFromMeasurements`.

### RES-FAM-019 — Services, clients, panel and examples

The MatterService provides:
- architectures and their validation;
- capabilities and compatibility checks;
- analyses and effective properties;
- interfaces and manufacturing checks;
- exploration, mechanisms and pathways;
- physical systems, comparisons of alternatives, manufacturing records and
  reports.

It is available through the automation API, Python and MCP. The Matter panel
shows regions, the layer stack (labelled as an idealized model, to scale in
thickness), interfaces, analyses, manufacturability and exploration. Each view
states whether it shows data, a computed result, an idealized model or a
schematic. Examples cover the benchmark architectures.
*Verification:* `MatterServices.*`, `python/tests/test_matter_api.py`, `MatterPanelTest.*`, `MatterExamples.*`.

### RES-FAM-020 — Benchmarks

The benchmarks are:
- a homogeneous material, a multilayer, a graded material and a
  multimaterial 3D structure, with orientation, interfaces and provenance,
  each surviving a round trip;
- the laminate against closed forms: the bilayer bending stiffness, the
  Timoshenko bimetal curvature, and cross-ply moduli;
- the series thermal resistance with a contact resistance, and the
  logarithmic formula of a linearly graded conductivity;
- the power-law graded plate against its integrals;
- the coated cantilever against the Euler–Bernoulli frequency;
- the piezoelectric unimorph:
  - the short- and open-circuit frequencies;
  - the optimum load;
  - the energy balance;
  - the in-plane orientation of an anisotropic film;
- an optimization variable that moves a result in the expected direction,
  competing objectives kept apart, and an infeasible architecture identified.
*Verification:* `docs/materials/ARCHITECTED_MATTER_VALIDATION.md`.

### RES-FAM-021 — Tensor-valued material fields

A region's density, elastic stiffness, thermal and electrical conductivity,
permittivity, piezoelectric coupling or magnetic permeability may vary in
space as a material field. Each field states:
- its coordinate system (Cartesian, cylindrical, spherical);
- its tensor representation and the basis of its components;
- its form and its source (analytic, measured, imported, computed).

Analytic fields blend two tensors along a coordinate. Sampled fields
interpolate convexly within their support and are never extrapolated.
Invalid tensors are refused. The three-dimensional analyses read the fields
element by element; an analysis that does not read them refuses the stack
with the reason.
*Verification:* `MaterialFields.*`, `MatterAnalyses.MaterialFieldsReachTheThreeDimensionalSolidAndOnlyIt`.

