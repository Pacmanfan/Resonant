---
id: sim.solver-selection
title: Choosing a solver
category: Simulation
order: 20
status: available
summary: What each solver represents and leaves out, how Resonant assesses your scene against them, and which one to use for which problem.
keywords: [solver, assessment, free field, kirchhoff, fdtd, full-wave, model]
related: [concepts.solvers, sim.architecture, acoustics.fdtd, acoustics.reflectors, em.solvers]
---

Resonant never chooses a solver for you and never replaces one by another.
Each solver is run by its own command. What Resonant does is tell you, for
the scene as it is, what each solver would leave out, so that you can choose
knowingly.

## The acoustic solvers

| Phenomenon | Analytical free field | Free field + reflectors | Full-wave (FDTD) |
|---|---|---|---|
| Direct radiation, interference | yes | yes | yes |
| Source directivity | yes | yes | no (monopoles) |
| Single reflection | no | yes | yes |
| Multiple reflections, resonance | no | no | yes |
| Diffraction | no | by a reflector's aperture | yes |
| Shadowing by objects | no | behind reflectors only | yes |
| Transmission, refraction | no | no | yes (fluid-like) |
| Absorption in the medium | yes | yes | yes, frequency-independent |
| Pulses, broadband signals | no | no | yes |
| Particle velocity, intensity | yes | no | computed, not recorded |
| Shear waves, nonlinearity, flow | no | no | no |
| Cost grows with | samples × sources | samples × surface elements | cells × time steps |

The analytical models solve one frequency in steady state; the full-wave
solver works in the time domain.

## The assessment

The **Full-Wave Solver** panel's box **Which solver represents this
problem** looks at the scene and the chosen region: reflectors, other
manipulators, plain objects, materials, whether the signal is transient,
whether the domain walls reflect, attenuation, directional sources, the size
in wavelengths and the estimated grid work. For each solver it states
whether it represents everything in the problem, or lists what it would
LEAVE OUT, with remarks such as:

```text
Analytical free field [approximate model]: would LEAVE OUT 1 reflector would
be ignored; 1 object with geometry would be ignored (no scattering or shadow).
Note: it would still run, and its result would not include the items above.
Full-wave time domain (FDTD) [full-wave]: represents everything in this problem.
```

The **Acoustics** panel's model note says the same for the selected
analytical model, and every result repeats its assumptions.

## Which one to use

- **Emitters and arrays in open space, one frequency**: the analytical free
  field. It is exact within its model and thousands of times cheaper.
- **One reflection from reflectors several wavelengths large, in 3D, at one
  frequency**: free field + reflectors (Kirchhoff approximation).
- **Obstacles, slits, lenses, cavities, waveguides, several reflections,
  materials, pulses**: the full-wave solver, in 2D when the geometry allows
  it, in 3D for regions of a few tens of wavelengths at most.

The model of the analytical solvers is chosen in the **Acoustics** panel,
tab **Medium**, **Model**; the full-wave solver is run from its own panel.

## A plane region means two different things

The analytical solvers sample the three-dimensional field on the plane. The
full-wave solver solves a **two-dimensional** problem in the plane: emitters
are line sources and objects are infinitely long cylinders of the
cross-section the plane cuts. Their amplitudes are not comparable; field
patterns of a geometry that really is uniform along the plane's normal are.

## Worked example

Open `examples/advanced_acoustics/c-diffraction.resonant` (a rigid plate
with a 5 mm slit) and read the assessment: the analytical free field would
leave out the plate. Run both:

1. **F5** with the analytical model: the field passes through the plate as if
   it were not there.
2. **F6** with the full-wave solver: the field behind the plate comes only
   through the slit, and the probe in the geometric shadow still reads more
   than 30 % of the on-axis value (diffraction).

## Other domains

- **Mechanics**: you choose the idealization of each object (beam, plate,
  membrane, solid) and the analysis (modes, frequency response, transient,
  static). For a frequency response, *Direct (exact for the model)* or
  *Modal superposition (truncated)*. See [Modal analysis](../mechanics/modal.md).
- **Electromagnetics**: analytical antenna and array models (array factor,
  no mutual coupling) or the full-wave Yee solver, which represents dipoles,
  monopoles, current elements and bodies but not arrays, loops, patches,
  horns or helices. See [Electromagnetic solvers](../electromagnetics/solvers.md).
- **Optimization**: each problem names its **Physical model** and an
  optional higher-fidelity **Validation model**
  ([Multi-fidelity optimization](../optimization/multi-fidelity.md)).

## Not available

No finite-element, boundary-element, angular-spectrum, pseudospectral or
eigenmode solver exists for acoustics. Resonances of enclosures are found as
response peaks of a full-wave run.
