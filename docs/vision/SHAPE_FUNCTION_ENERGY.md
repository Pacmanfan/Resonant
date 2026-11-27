# Shape–Function–Energy

**Status:** the guiding principle of Resonant's scientific direction
(REQ-CHAT-20261126-015, `docs/requirements/shape_function_energy.md`
RES-SFE-001..012, ADR-000219). This page explains the idea; how it maps to
the code is in [the audit](../architecture/SHAPE_FUNCTION_ENERGY_AUDIT.md),
how it will be built in [the architecture](../architecture/COUPLED_MULTIPHYSICS_ARCHITECTURE.md)
and [the research roadmap](../architecture/ENERGY_MATTER_RESEARCH_ROADMAP.md).

## The principle

> **The geometry and internal structure of an object can be the mechanism
> through which it performs its physical function.**

A dish focuses sound because its curvature turns every reflected path
toward one point at the same phase; a cavity rings because its walls allow
only waves that fit between them; a horn matches a small source to open air
because its cross-section changes slowly compared with the wavelength. In
each case the function is not a property of a label attached to the object:
it *emerges* from the interaction of geometry, materials, excitation and
environment, and Resonant must compute it from a physical model
(RES-SFE-001, generalizing RES-PRODUCT-006).

```text
  geometry ─┐
  materials ┼─► physical interaction ─► energy and matter response ─► functional behavior
  excitation┤    (a model: equations,      (fields, modes, forces,       (focuses, confines,
  environment┘    boundaries, solver)       heat, deformation)            filters, converts ...)
                         ▲                                                     │
                         └──────────── design: change geometry / materials ◄───┘
```

The arrow back is design: changing the shape, the material structure or the
excitation and computing again, by hand or through optimization.

## What enters the model

| Group | Examples |
|---|---|
| Geometry | shape, dimensions, topology, curvature and orientation of surfaces, cavities, channels, layers, interfaces, periodic structure, spatial distributions, connections, tolerances, boundary conditions |
| Material properties | density, elasticity, plasticity, viscosity, damping, acoustic impedance, conductivity, permittivity, permeability, thermal conductivity, heat capacity, optical and magnetic properties; anisotropy; dependence on frequency, temperature and amplitude |
| Excitation and environment | frequency and wavelength, amplitude, phase, waveform, direction, polarization, time dependence, source geometry and impedance, surrounding medium, pressure, temperature |
| Interaction (computed) | reflection, transmission, refraction, diffraction, scattering, absorption, resonance, interference, coupling, dissipation, energy storage, momentum transfer, field concentration, confinement, deformation, heating |
| Function (an objective, never a guarantee) | concentrate, distribute, redirect, confine, store, convert or transfer energy; filter frequencies; suppress modes; amplify by resonance; attenuate vibration; produce forces; shape stress; control heat; set effective properties; shape propagation |

## Examples, and where Resonant stands

| Object | Function | Mechanism | Model in Resonant | Status |
|---|---|---|---|---|
| Parabolic or elliptical reflector | redirect and concentrate sound | path length and phase from curvature | Kirchhoff reflector model, full-wave FDTD | current |
| Acoustic cavity, Helmholtz resonator | store energy at selected frequencies | standing waves fitting the walls | analytical estimates (rectangular, cylindrical, spherical, Helmholtz, quarter-wave); FDTD for the field | current (estimates and full wave); no Q or wall loss in the estimates |
| Microwave cavity | confine electromagnetic modes | boundary conditions on conducting walls | TE/TM modes of a rectangular cavity (ideal walls); Yee solver | current (rectangular, lossless) |
| Waveguide | guide selected modes | cut-off set by the cross-section | electromagnetic guide modes; acoustic cut-off estimates | current (simple sections) |
| Ultrasonic horn | transform vibration velocity | change of cross-section along a resonant rod | finite elements (`rod.horn`, conical, exponential, stepped) | current |
| Mechanical resonator (plate, beam, membrane) | store and exchange energy | mass and stiffness distribution | modal, harmonic and transient finite-element analyses | current |
| Phononic crystal | stop elastic waves in a band | Bragg scattering in a periodic structure | Bloch analysis of a periodic rod (1D, longitudinal) | current (1D); 2D/3D planned |
| Radiating plate | convert vibration to sound | surface velocity drives the fluid | one-way Rayleigh coupling | current (one-way) |
| Acoustic horn as impedance transformer | match a source to free air | slow change of cross-section | FDTD can simulate the shape; no impedance analysis | planned |
| Optical resonator | stable optical modes | mirror curvature and spacing | none (the electromagnetic domain is RF) | theoretical future |
| Metamaterial unit cell | effective properties absent in its constituents | engineered local resonances or geometry | none | planned (stage 7) |
| Heated, expanding absorber | absorb, heat, deform, detune | electromagnetic loss → heat → strain → geometry | none (no thermal domain, no two-way coupling) | planned (stages 4–5, 10) |

The status words follow RES-SFE-011: **current** is implemented and
tested; **planned** has its architecture defined; a **research prototype**
would be an experimental implementation (none exists yet); **theoretical
future** names a direction without a plan.

## Two ways to work

```text
object-centric:    object ──► model ──► behavior        "what does this do?"
function-centric:  wanted behavior ──► search over geometry, materials,
                   structure, arrangement, excitation ──► candidate objects ──► model ──► check
```

Both exist today for the implemented domains: analysis in the domain
panels, and design through the Optimization Studio, whose
domain-independent algorithms reach each domain through an adapter
(ADR-000214). New functions add objectives and adapters, never a second
optimizer (RES-SFE-002). A function-centric result is a candidate whose
behavior was computed by the same models, with the same provenance, as any
object-centric result.

## Resonance everywhere

Resonance is the clearest case of shape making function: cavities,
enclosures, plates, rods, antennas, guides, circuits and metamaterial
inclusions all resonate, each by its own physics. Resonant reports them in
one vocabulary (natural frequency, mode, modal energy, damping, Q,
bandwidth, coupling, localization) while each domain keeps its equations
([resonant structures](../physics/RESONANT_STRUCTURES.md), RES-SFE-004).

## Energy moving between domains

Many functions are conversions: vibration to sound, field to heat, heat to
strain, strain to a changed geometry that changes the field again. The
Coupled Energy–Matter Engine will coordinate the existing solvers through
such chains, with every exchange and every balance accounted for
([coupled energy–matter](../physics/COUPLED_ENERGY_MATTER.md),
[energy and momentum accounting](../physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md),
RES-SFE-006, RES-SFE-007).

## Matter that changes

A material's response can depend on temperature, stress, history or its
microstructure. Material state makes those dependencies explicit
([material state](../materials/MATERIAL_STATE_ARCHITECTURE.md)), engineered
structures get effective properties only from a model
([metamaterials](../materials/METAMATERIALS_AND_ENGINEERED_STRUCTURES.md)),
and smaller scales inform material models through external tools
([multiscale modeling](../physics/MULTISCALE_MODELING.md)).

## Seeing it

The aim is to see how energy meets an object's geometry and internal
structure: fields, energy density and flux, momentum where defined,
standing waves and modes, stress, strain and deformation, temperature,
material phase, charges and currents, microstructure and unit cells, band
structures, and spatially varying effective properties — several of them at
the same place at once, each with the model and resolution behind it and no
smoothing or extrapolation that suggests more accuracy than the model has
(RES-SFE-010). Today Resonant shows complex fields on planes and volumes,
deformed meshes with scalar colouring, patterns, time signals and spectra;
the rest is planned ([the roadmap](../architecture/ENERGY_MATTER_RESEARCH_ROADMAP.md)).

## What this principle does not mean

- It is not a catalogue of shapes with behaviors attached.
- It does not promise that a shape achieves a function: objectives are
  checked by computation, and a candidate that fails is reported as failing.
- It does not merge the domains into one solver: each keeps the equations
  and numerical methods its physics needs.
- Nothing here is implemented merely by being described: the status column
  above and `IMPLEMENTATION_STATUS.md` are the record.
