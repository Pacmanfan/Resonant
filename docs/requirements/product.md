# Resonant — Product Vision and Product Requirements

**Owner of:** `RES-PRODUCT-001..016`
**Source:** REQ-CHAT-20261126-001 (project vision submitted by the operator)
**Status:** normative. The vision is the guiding product requirement for all
architectural decisions (`RES-PRODUCT-016`).

**Name:** Resonant — **Product:** Wave Field Engineering Studio —
**Tagline:** Design. Simulate. Focus. Measure.

Resonant is a native desktop engineering application for designing,
simulating, visualizing, optimizing and experimentally investigating wave
propagation and wave interactions in physical environments. It is intended
to become a serious engineering and scientific experimentation platform, not
a phased-array calculator or an audio visualization tool.

**Direction (REQ-CHAT-20261126-015):** the geometry and internal structure
of an object can be the mechanism of its physical function. Resonant aims to
let users design what an object does through its shape, material structure
and interaction with energy, and to predict that behavior from physical
models (`docs/vision/SHAPE_FUNCTION_ENERGY.md`,
`shape_function_energy.md` RES-SFE-001..012). Its first implementation is
the Coupled Energy–Matter Engine (REQ-CHAT-20261126-016,
`coupled_engine.md` RES-CEME-001..016); functional structures make geometry
a physical design variable (REQ-CHAT-20261126-017, `functional_structures.md`
RES-FUNC-001..016), resonance engineering across the domains
(REQ-CHAT-20261126-018, `resonance.md` RES-RSN-001..014), and metamaterials
with engineered effective properties (REQ-CHAT-20261126-019, `metamaterials.md`
RES-META-001..012), multiscale material modeling (REQ-CHAT-20261126-020,
`multiscale.md` RES-MSC-001..014), and dynamic material state
(REQ-CHAT-20261126-021, `material_state.md` RES-DMS-001..014), and unified
accounting of energy, momentum and force (REQ-CHAT-20261126-022,
`accounting.md` RES-ACC-001..016).

Each requirement below names the release in which it must first hold:
**V1** (the initial release, a complete acoustic workflow), or **LATER**
(long-term vision the architecture must anticipate). Domain documents
(`application_foundation.md`, `acoustics.md`, ...) own the detailed,
testable behavior and refer back to these ids.

## Wave domains

### RES-PRODUCT-001 — Multiple wave domains on one architecture

The application supports several physical wave domains through one shared
architecture: acoustic waves (audible sound, ultrasound, infrasound),
mechanical waves (vibration through solids, structures, surfaces,
membranes), electromagnetic waves (RF propagation, antenna arrays,
phased-array systems), and further domains through modular physics
implementations. **V1:** acoustics only. **LATER:** mechanical,
electromagnetic, others. Adding a domain must not require a fundamental
redesign (`RES-PRODUCT-013`).

## Core capabilities

### RES-PRODUCT-002 — Three-dimensional modeling environment

Users create a physical environment in 2D and 3D: create and manipulate
geometric objects, place and orient emitters, create arrays parametrically,
import external CAD geometry, assign materials, define targets and
measurement locations, create simulation regions, define boundary
conditions, and inspect objects and results. **V1:** interactive 3D
viewport, geometry creation and import, emitter placement, parametric
arrays. Resonant is not a CAD package (`RES-PRODUCT-014`).

### RES-PRODUCT-003 — Emitters and phased arrays

Supported sources: point sources, directional sources, speakers, ultrasonic
transducers, mechanical vibration actuators, RF antenna elements and custom
emitter definitions. Supported arrangements: linear, planar, circular,
spherical and arbitrary; regular and irregular arrays. Every emitter has
independently configurable frequency, amplitude, phase, delay and waveform.
**V1:** acoustic emitters and parametric arrays.

### RES-PRODUCT-004 — Inverse field design

Users specify a desired field and Resonant calculates the emitter inputs
required to produce it, by analytical solutions and by numerical
optimization. Target types eventually include single and multiple focal
points, lines, planes, curved surfaces, volumes, desired amplitude and phase
distributions, and regions requiring field suppression. **V1:** phase and
time-delay calculation, point and basic region focusing, an initial
inverse-design solver.

### RES-PRODUCT-005 — Passive wave manipulation

A major product requirement. Users create physical shapes that manipulate
wave propagation without independently controlled emitters: parabolic,
elliptical and spherical reflectors, acoustic lenses, waveguides, resonant
cavities, diffraction structures, layered structures, metamaterial
structures and custom parametric geometries, with modeling and eventual
optimization. Active (emitter control), passive (geometry) and combined
manipulation are all supported. **LATER** (after the V1 acoustic workflow).

### RES-PRODUCT-006 — Physics decides, geometry does not

The resulting field is calculated from the actual geometry and material
properties under the applicable physical model. A geometric shape is never
assumed to produce a particular focusing result. RES-SFE-001 extends this
to every physical function (confining, storing, filtering, converting
energy, and the others).

### RES-PRODUCT-007 — Materials library

An extensible material library with separate property sets per domain:

* acoustic — density, speed of sound, acoustic impedance, attenuation,
  absorption, dispersion, frequency dependence;
* mechanical — density, Young's modulus, Poisson's ratio, damping,
  elasticity, anisotropy, frequency-dependent response;
* electromagnetic — relative permittivity, relative permeability,
  electrical conductivity, dielectric loss, frequency dependence,
  anisotropy.

Layered and composite materials are supported. Every property value records
whether it is measured, manufacturer-supplied, literature-derived or
theoretical. **V1:** an initial acoustic material library.

### RES-PRODUCT-008 — Visualization of real data

The 3D viewport is a central feature: instantaneous wave fields,
time-averaged intensity or energy-related quantities, phase, pressure,
displacement, electric and magnetic fields, volumetric rendering,
isosurfaces, 2D cross sections, arbitrary clipping planes, animated
propagation and field comparisons. Visualization uses actual simulation
data, never decorative approximations. **V1:** 2D and 3D field
visualization of acoustic results.

### RES-PRODUCT-009 — Experiments and measurements

Users eventually connect physical hardware, generate signals, collect
measurements, calibrate emitters and compare measured results with simulated
predictions, with reproducible experiment configurations. **LATER.** V1 does
not require hardware (`RES-PRODUCT-014`).

### RES-PRODUCT-010 — Optimization

Automated optimization of emitter configurations and of geometry.
**V1:** the initial inverse-design solver for emitter weights. **LATER:**
geometry optimization.

## Qualities

### RES-PRODUCT-011 — Development priorities

In this order: (1) correct physical modeling, (2) extensible software
architecture, (3) reproducible results, (4) usable 3D engineering
environment, (5) performance and GPU acceleration, (6) hardware
independence, (7) long-term maintainability. Architecture is not sacrificed
for superficial early demonstrations, and complex physics is not implemented
prematurely.

### RES-PRODUCT-012 — Reproducibility and persistence

Simulation configuration is explicit and reproducible; projects are saved
and loaded. **V1.**

### RES-PRODUCT-013 — Architecture anticipates all domains

Resonant is built incrementally, but the complete architectural foundation
is established early: the architecture anticipates mechanical and
electromagnetic simulation without a fundamental redesign.

### RES-PRODUCT-014 — V1 non-goals

V1 does not implement all physical domains, does not recreate a commercial
CAD package, does not require physical hardware, never represents simplified
analytical calculations as full physical simulations, and introduces no
complex GPU computing dependency before a working CPU reference
implementation exists.

### RES-PRODUCT-015 — V1 scope

V1 provides a complete working acoustic simulation workflow: a native
desktop application; an interactive 3D viewport; geometry creation and
import; emitter placement; parametric array generation; an initial acoustic
material library; analytical acoustic propagation; phase and time-delay
calculations; point and basic region focusing; an initial inverse-design
solver; 2D and 3D field visualization; simulation configuration and
reproducibility; project save/load; an initial Python scripting interface.

### RES-PRODUCT-016 — The long-term workflow guides every decision

A user should eventually be able to: (1) create a physical environment,
(2) design an emitter configuration, (3) define materials and geometry,
(4) specify a desired wave field, (5) calculate the required emitter inputs,
(6) simulate propagation, (7) optimize passive and active wave manipulation,
(8) visualize and analyze results, (9) export signals to physical hardware,
(10) measure the real-world result, (11) update the model using experimental
data. This workflow is the guiding product requirement for all architectural
decisions.

## Verification

These are product-level requirements. Each is verified through the domain
requirements that refine it (see `requirements.txt`) and the tests those
name. The roadmap (`docs/architecture/roadmap.md`) records which milestone
delivers which requirement, and the implementation status of each milestone.
