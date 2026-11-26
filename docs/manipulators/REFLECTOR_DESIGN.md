# Reflector Design

**Status:** current for REQ-CHAT-20261126-004.

Every reflector is generated from its equation. In the object's local frame
the reflecting side faces +Z.

## Parabolic reflector

`z = r² / (4f)`, `r² = x² + y²`, vertex at the origin, axis +Z.

* Focus at `(0, 0, f)`; directrix `z = −f`.
* Curvature: the radius of curvature at the vertex is `2f`.
* Rim depth for aperture diameter `D`: `D² / (16 f)`.
* Area: `(8π f²/3) [ (1 + (D/4f)²)^{3/2} − 1 ]`.

Ideal property (geometrical acoustics): rays parallel to the axis meet at
the focus, and rays from the focus leave parallel to the axis. Tested to
1e-13 m.

## Elliptical reflector

Prolate spheroid `r²/b² + z²/a² = 1`, `a > b`, centred on the origin, kept
for `z ≤ cut·a`.

* Foci at `(0, 0, ∓c)`, `c = √(a² − b²)`.
* For every surface point the distances to the two foci add up to `2a`.

Ideal property: rays from one focus meet at the other. Parameters: semi-major
axis, semi-minor axis, aperture cut, thickness; the position and orientation
of the foci in the world come from the object's transform. A 2D elliptical
section extruded along Y is not provided.

## Spherical reflector

`z = R − √(R² − r²)`, vertex at the origin, centre of curvature `(0, 0, R)`.

* Paraxial focus `R/2`.
* A ray parallel to the axis at height `h` crosses the axis at
  `R − R / (2 cos θ)`, `sin θ = h/R`: closer to the mirror than `R/2`
  (spherical aberration), which the estimates show for the rim.

## Cylindrical (trough) reflector

`z = x² / (4f)` extruded along Y; focal line `x = 0, z = f`.

## Flat reflector

A rectangle in `z = 0`.

## What the ideal properties do not tell you

They are statements about rays. A real field also depends on the
wavelength, the aperture, where the source actually is, its directivity,
the boundary condition and the material. With a source at a finite distance
`s` on the axis a parabolic reflector focuses near the image distance of the
mirror equation `1/s + 1/s' = 1/f`, not at `f`; the focal spot is about a
wavelength wide, not a point; and a spherical reflector's focus is smeared
by aberration. Resonant therefore never reports a focus from the shape: it
calculates the field (`PASSIVE_FOCUSING.md`).

## Simulation surface

For the reflector model a reflector is sampled into flat surface elements
directly from its equation (`sampleReflectorSurface`), with positions,
unit normals and areas, no larger than the wavelength divided by the
chosen number of elements per wavelength (default 6). The display mesh is
not used. Tests check that the elements lie on the equation, that the
normals bisect the focal directions, and that the element areas add up to
the closed-form area within 0.2 %.

## Rays

`reflectRay` intersects a ray with the exact surface and reflects it
specularly; `traceRays` draws a fan from a source. Rays carry no amplitude
or phase and are a check of the shape, not a field result.
