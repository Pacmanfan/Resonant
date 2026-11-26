# Resonant Cavities

**Status:** current for REQ-CHAT-20261126-004. **Cavities have no field
model.** Geometry and analytical resonance estimates only.

## Geometry

`cavity.rectangular` (inner sizes), `cavity.cylindrical` (inner radius and
length), `cavity.spherical` (inner radius), each with a wall thickness.
Custom enclosed volumes and tunable cavities are not available.

## Estimates (rigid walls, lossless fluid, ideal shape)

* Rectangular box: `f = (c/2) √((n_x/L_x)² + (n_y/L_y)² + (n_z/L_z)²)`.
* Closed cylinder: `f = (c/2π) √((j'_mn/a)² + (qπ/L)²)`.
* Sphere: `f = c z / (2π a)`, `z` a zero of the derivative of a spherical
  Bessel function (lowest: 2.0816).

Also available as functions:

* Helmholtz resonator: `f = (c/2π) √(A / (V L_eff))`, `L_eff = L + 1.7 r`
  (both ends flanged); valid when every dimension is small against the
  wavelength.
* Quarter-wave tube: `f_n = (2n − 1) c / (4L)`, no end correction.

## Assumptions

Perfectly rigid walls, no losses, no openings, no fill material; the
frequencies of a real cavity are lower in quality and shifted by wall
compliance, damping, ports and temperature. Mode shapes, nodal surfaces and
frequency-response curves are not calculated; they need an eigenmode or
full-wave solver.
