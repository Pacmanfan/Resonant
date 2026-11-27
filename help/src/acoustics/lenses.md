---
id: acoustics.lenses
title: Acoustic lenses
category: Acoustic Engineering
order: 70
status: experimental
summary: Spherical-surface lenses: their geometry, the analytical focal-length and transmission estimate, and how to see a lens act in a full-wave simulation.
keywords: [lens, refraction, refractive index, lensmaker, transmission, focal length]
related: [concepts.manipulators, acoustics.materials, acoustics.fdtd, acoustics.reflectors]
---

An acoustic lens focuses sound by refraction: sound travels at a different
speed inside it. Resonant provides the lens geometry and a first-order
analytical estimate. The two frequency-domain models (analytical free field,
free field + reflectors) **ignore lenses**; only the full-wave solver lets a
lens act on the field.

> **Note** This page is marked experimental: the geometry and the estimate
> are tested, and the full-wave solver simulates a lens as its material, but
> no lens field has yet been validated against a reference solution.

## Creating a lens

**Wave Manipulators > Acoustic Lens** adds a lens on the local Z axis.
Parameters in the **Properties** panel:

| Parameter | Meaning |
|---|---|
| **Aperture diameter** | the lens diameter |
| **Surface 1 radius (0 = flat)** | radius of the first surface; its sign says on which side its centre lies |
| **Surface 2 radius (0 = flat)** | radius of the second surface |
| **Centre thickness** | thickness on the axis |

This covers biconvex, biconcave, plano-convex, plano-concave and meniscus
lenses. Surfaces that would intersect, and radii smaller than half the
aperture, are refused. Assign the lens material with **Materials > Assign to
Selection**; it needs an acoustic density and speed of sound. The
surrounding medium is the project's medium (**Acoustics** panel, **Medium**
tab). Freeform lens profiles are not available.

## The analytical estimate

The editor's **Geometry and analytical estimates** box shows:

```math
n = c_medium / c_lens
1/f = (n − 1) · [ 1/R₁ − 1/R₂ + (n − 1) · d / (n · R₁ · R₂) ]
T = 4 Z₁ Z₂ / (Z₁ + Z₂)²,      Z = ρ c
```

`n` is the acoustic refractive index, `R₁`, `R₂` the surface radii, `d` the
centre thickness, `f` the focal length (thick-lens, paraxial) and `T` the
fraction of intensity transmitted through one flat interface at normal
incidence, given for each surface.

Acoustics is not optics: most solids are **faster** than air or water, so
`n < 1`, and a **concave** lens converges while a convex one diverges (the
estimate prints "DIVERGING" for a negative focal length). A solid lens in
air reflects almost all of the sound; in water it transmits most of it. The
estimate treats the lens as a fluid and ignores absorption, internal
reflections, aberrations and diffraction at the aperture.

## Worked example

`examples/manipulators/c-acoustic-lens.resonant`: a biconcave acrylic lens
(radii −80 mm and +80 mm, aperture 80 mm, centre thickness 30 mm) in water,
40 kHz.

- `n` = 1482 / 2730 = 0.543: acrylic is faster than water, so the concave
  lens converges.
- The thick-lens estimate for the stored 30 mm thickness is about 104 mm;
  the thin-lens value of the same surfaces is 87.5 mm.
- About 86 % of the intensity passes each surface.

The field this project shows after **F5** is the free field of the emitter
alone: the analytical model ignores the lens, and the editor's **What is
calculated** line says so.

## Seeing a lens act: the full-wave solver

The full-wave solver represents a lens as a fluid region of its material
(density, speed of sound and, if given, attenuation). A lens without a usable
material is simulated as rigid, with a warning that it does not act as a
lens. To try it:

1. Use a field region that contains both the emitter and the lens: the
   solver is refused when no emitter lies in the region's plane (within one
   cell). In the lens example the emitter is 0.5 m away, outside the field
   plane; move it closer or enlarge the region.
2. Open **View > Panels > Full-Wave Solver**, choose the region and a
   continuous signal, and press **Check**. The preflight lists the lens
   under OBJECTS IN THE CALCULATION as "material 'Acrylic' as a fluid".
3. **Run Full-Wave Simulation** (F6). The steady-state field appears in the
   viewport and in the Acoustics panel's results.

On a plane region the calculation is two-dimensional: the lens is the
cross-section the plane cuts, extended infinitely along the plane's normal
(a cylindrical lens), so the amplitudes are those of a 2D problem. Its
surfaces are stepped at the cell size; use enough cells per wavelength for
the curvature to be resolved. Shear waves in the lens are not represented.

## Common mistakes

- Expecting the analytical or reflector model to show the lens: it never
  changes their field.
- Designing an acoustic lens with optical intuition (convex converges).
- Forgetting the impedance mismatch: a lens in air transmits very little.

## Related

[Reflectors](reflectors.md) focus without transmission and are
supported by the reflector model. [Acoustic materials](materials.md)
explains how materials enter the full-wave solver.
