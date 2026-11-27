# Solver Selection and Applicability

**Status:** current for REQ-CHAT-20261126-005.

## The rule

The application never chooses a solver and never substitutes one for
another. Each solver is run by its own command. What the application does
is tell the user, for the scene as it is, what each solver would leave out.

## Registry (`src/core/solvers/SolverRegistry.h`)

A `SolverDescriptor` declares: id, name, version, domain, method, whether
it is full-wave, whether it works in the time domain, 2D and 3D support,
the phenomena it represents, how its cost grows, when to use it, and its
limitations. Solvers remain separate implementations with their own
formulations; the registry holds declarations only.

Phenomena: direct radiation, interference, source directivity, single
reflection, multiple reflections, diffraction, shadowing, transmission,
refraction, absorption, resonance, transient signals, broadband signals,
heterogeneous media, particle velocity, shear waves, nonlinearity, flow.

| | Free field | Reflectors (Kirchhoff) | FDTD |
|---|---|---|---|
| Full-wave | no | no | yes |
| Direct radiation, interference | yes | yes | yes |
| Source directivity | yes | yes | **no** |
| Single reflection | no | yes | yes |
| Multiple reflections, resonance | no | no | yes |
| Diffraction | no | aperture of a reflector | yes |
| Shadowing by objects | no | no | yes |
| Transmission, refraction | no | no | yes (fluid-like) |
| Absorption in the medium | yes | yes | yes, frequency-independent |
| Transient and broadband signals | no | no | yes |
| Particle velocity | yes | no | calculated, not yet stored or displayed |
| Shear waves, nonlinearity, flow | no | no | no |
| Cost | samples × sources | samples × surface elements | cells × time steps |

## Assessment

`SolverRegistry::assess(traits)` takes what the problem contains —
dimensions of the region, numbers of reflectors, other manipulators, plain
objects and penetrable objects, whether the signal is transient, whether
domain walls reflect, medium attenuation, directional sources, the size in
wavelengths, the estimated grid work — and returns for every solver whether
it is complete for this problem and the list of what it would leave out,
with remarks on cost and accuracy. The Full-Wave Solver panel shows it:

```text
Analytical free field [approximate model]: would LEAVE OUT 1 reflector would
be ignored; 1 object with geometry would be ignored (no scattering or shadow).
Note: it would still run, and its result would not include the items above.
Full-wave time domain (FDTD) [full-wave]: represents everything in this problem.
Note: on a plane region this solver calculates a TWO-DIMENSIONAL problem ...
```

## Guidance

* Open space, emitters and arrays only, one frequency: the analytical free
  field. It is exact within its model and thousands of times cheaper.
* One reflection from reflectors several wavelengths large, in 3D, at one
  frequency: the reflector model.
* Anything with diffraction, obstacles, lenses, cavities, waveguides,
  several reflections, materials, pulses: FDTD — in 2D when the geometry
  allows it, in 3D for regions of a few tens of wavelengths at most.

A plane region means different things: the analytical solvers sample a
three-dimensional field on the plane; FDTD solves a two-dimensional problem
in it (line sources, objects as infinite cylinders). Their amplitudes are
not comparable; patterns of a geometry that really is uniform along the
third direction are.

## Not implemented

FEM, BEM, angular spectrum, pseudospectral and modal solvers. FEM and BEM
need a mesh generator (none is integrated; Gmsh was not available in the
build environment) and a sparse or dense linear solver. Angular spectrum
would be a cheap addition for planar arrays in open space, where the
analytical solver is already exact.
