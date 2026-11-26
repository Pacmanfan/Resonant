# Acoustic Lenses

**Status:** current for REQ-CHAT-20261126-004. **Lenses have no field
model.** What exists is the parametric geometry and an analytical
first-order estimate. A lens in the scene does not change any simulated
field, and the application says so in the editor and in every result's
notes.

## Geometry

`lens.spherical`: two spherical surfaces on a common axis (Z), an aperture
diameter and a centre thickness. A radius of 0 is a flat surface; the sign
of a radius says on which side its centre of curvature lies. This covers
biconvex, biconcave, plano-convex, plano-concave and meniscus lenses.
Surfaces that would intersect, and radii smaller than half the aperture, are
refused. Freeform lens profiles are not available.

The model keeps apart: the shape, the lens material (the object's assigned
material, with its acoustic density and speed of sound and their
provenance), and the surrounding medium (the project's propagation medium).

## The estimate

Acoustic refractive index `n = c_medium / c_lens`, and the thick-lens
(lensmaker's) equation

```text
1/f = (n − 1) [ 1/R₁ − 1/R₂ + (n − 1) d / (n R₁ R₂) ]
```

plus the fraction of intensity transmitted through one flat interface at
normal incidence, `T = 4 Z₁Z₂ / (Z₁ + Z₂)²`, `Z = ρc`, for each surface.

## Why this is not optics

* Most solids are **faster** than air or water, so `n < 1`: a convex lens
  **diverges** and a concave lens converges. The estimate reports this.
* The impedance mismatch is usually large. An acrylic lens in water
  transmits most of the energy; a solid lens in air reflects almost all of
  it. The transmission values make that visible.
* A solid carries shear waves as well as longitudinal waves; the estimate
  treats the lens as a fluid.
* The estimate is paraxial and ignores absorption, multiple internal
  reflections, aberrations and diffraction at the aperture.

It is a starting point for a design, to be followed by a full-wave
simulation when one exists (request 005).

## Example

`examples/manipulators/c-acoustic-lens.resonant`: a biconcave acrylic lens
in water, `n = 0.54`, estimated focal length about 87 mm. The field shown
for this project is the free field of the emitter alone.
