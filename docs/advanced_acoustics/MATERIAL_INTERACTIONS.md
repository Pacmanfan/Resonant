# Geometry, Materials and Interfaces

**Status:** current for REQ-CHAT-20261126-005.

## Four kinds of geometry

| | What | Where |
|---|---|---|
| Parametric engineering geometry | the shape and its parameters | `Geometry` |
| Display mesh | triangles for the viewport | `Geometry::tessellate` |
| Simulation geometry | "is this point inside the body?" from the shape's definition | `fdtd/SolidModel.h` |
| Numerical grid | cells with a material index | `GridSpec::cellMaterial` |

The display mesh is never used for simulation. Every geometry kind has an
inside test written from its equation: box, sphere, ellipsoid, cylinder,
cone; plane and disc (surfaces without thickness, represented as plates);
parabolic, spherical, elliptical, trough and flat reflectors as shells;
lenses between their two spherical surfaces; waveguide and cavity walls;
the slit plate.

A cell belongs to an object when its **centre** is inside the body. There
is no mesh generator, no mesh refinement and no partial-cell treatment:
surfaces are stepped at the cell size. The "mesh quality" of this grid is
one number, the cells per wavelength, plus the count of cells of each kind
that the preflight reports. Later objects in the scene overwrite earlier
ones where they overlap.

Shells and plates thinner than two cells would leak, so they are
represented two cells thick; the preflight names each such object and both
thicknesses. Hidden objects are left out and named.

## Materials

The medium of the project is material 0. An object's assigned material is
used when it has an acoustic density and speed of sound; its attenuation
is used if present. Provenance stays with the material (the example
library is illustrative, not authoritative). Up to 254 materials.

**Solids are treated as fluids.** A solid also carries shear waves, which
change reflection and transmission away from normal incidence and allow
mode conversion and structural resonances. None of that is represented.
For stiff, dense objects in air this hardly matters (almost everything is
reflected); for solids in water it does.

## Interfaces

Pressure and normal velocity are continuous across an interface in the
scheme because each face uses the mean density of its two cells and each
cell its own bulk modulus. Nothing is multiplied by a coefficient: the
reflection and transmission below come out of the calculation.

Measured, plane pulse at normal incidence from the test gas into a second
medium:

| Second medium (rho, c) | R expected | T expected | Agreement |
|---|---|---|---|
| 4.8 kg/m^3, 510 m/s | +0.714 | 1.714 | 0.012 |
| 0.6 kg/m^3, 250 m/s | −0.462 | 0.538 | 0.012 |
| 0.8 kg/m^3, 510 m/s (same impedance) | 0 | 1 | 0.012 |

with `R = (Z2 − Z1)/(Z2 + Z1)`, `T = 2 Z2/(Z2 + Z1)`, and energy flux
conserved (`R^2 + (Z1/Z2) T^2 = 1` within 2 %). The pulse travels at the
second medium's speed within 0.5 %.

Refraction: a point source above an interface with a medium twice as fast;
the first arrival at a probe below and to the side is at the least-time
(Snell) path's travel time within a quarter period, and earlier than a
straight path would allow.

## Attenuation

`gamma = 2 alpha c` in the pressure equation attenuates a plane wave by
`alpha` nepers per metre at **all** frequencies (measured within 1 %).
Real absorption grows with frequency; the value used is the material's
single coefficient, whatever frequency it was stated for. Frequency-
dependent, dispersive and anisotropic materials are not implemented.

## Rigid bodies, shadows, diffraction

A rigid wall across the domain leaves less than 1e-6 of the field behind
it. Through a slit one wavelength wide the field reaches far off the axis,
at 63 degrees, where no straight path exists; in front of the wall a
standing wave forms with maxima more than 1.6 times the free field.

## Layered and nonuniform media

Several material objects side by side are a layered medium. A continuous
variation (a temperature gradient) has to be built from layers; there is
no field of material properties. The analytical solvers assume a
homogeneous medium at rest and ignore every object that is not a
reflector, which the assessment states.
