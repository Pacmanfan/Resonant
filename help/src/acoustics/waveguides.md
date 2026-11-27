---
id: acoustics.waveguides
title: Acoustic waveguides
category: Acoustic Engineering
order: 80
status: experimental
summary: Circular and rectangular ducts and closed cavities: geometry, analytical cut-off and resonance estimates, and full-wave simulation of their walls.
keywords: [waveguide, duct, cut-off, mode, cavity, resonance, helmholtz]
related: [concepts.manipulators, acoustics.fdtd, acoustics.physics, sim.solver-selection]
---

A waveguide confines sound between walls and carries it along its length.
Resonant provides waveguide and cavity geometry with analytical estimates of
their cut-off and resonance frequencies. The analytical free field and the
reflector model **ignore** these objects; the full-wave solver represents
their walls.

> **Note** This page is marked experimental: the estimates are tested, and
> the full-wave solver simulates the walls, but no waveguide field has been
> validated against a reference solution. The full-wave resonance of a
> rigid cavity is validated (example E below).

## Creating a waveguide

**Wave Manipulators > Circular Waveguide** or **Rectangular Waveguide**. The
guide runs along its local Z axis.

| Kind | Parameters |
|---|---|
| Circular | Inner radius (entry), Inner radius (exit), Length (Z), Wall thickness |
| Rectangular | Inner width (X), Inner height (Y), Length (Z), Wall thickness |

Equal entry and exit radii give a straight tube; different radii a linear
taper. Curved paths, bends, branches, elliptical and custom cross-sections
are not available.

## Cut-off estimates

Above its cut-off frequency a mode propagates along the guide; below it, it
decays. The plane wave has no cut-off. For rigid walls, a lossless fluid and
a uniform cross-section:

```math
rectangular a × b:   f_mn = (c / 2) · √( (m/a)² + (n/b)² )
circular radius a:   f_mn = c · j′_mn / (2π a),      j′_10 = 1.8412
```

`c` is the speed of sound of the medium, `m`, `n` the mode numbers and
`j′_mn` the zeros of the derivative of the Bessel function `J_m`. The
editor's **Geometry and analytical estimates** box lists the four lowest
cut-offs and says whether only the plane wave propagates at the project
frequency. For a tapered guide the estimates are for the entry
cross-section only, and the editor says so. No propagation constant,
attenuation, wall impedance or field inside the guide is calculated
analytically.

Example: the default rectangular guide, 40 × 20 mm in air at 20 °C, has its
first cut-off, mode (1,0), at `c / (2a)` = 4.29 kHz. At 40 kHz higher modes
propagate; for the plane wave alone at 40 kHz the larger side must stay
below half a wavelength, 4.29 mm.

## Closed cavities

**Wave Manipulators > Rectangular Cavity**, **Cylindrical Cavity** and
**Spherical Cavity** add rigid enclosures (inner sizes and a wall
thickness). The editor lists their lowest resonances for rigid walls:

```math
box:        f = (c / 2) · √( (n_x/L_x)² + (n_y/L_y)² + (n_z/L_z)² )
cylinder:   f = (c / 2π) · √( (j′_mn / a)² + (qπ / L)² )
sphere:     f = c · z / (2π a),      lowest z = 2.0816
```

`L` are the inner sizes, `a` the radius, `q` the axial mode number and `z` a
zero of the derivative of a spherical Bessel function. Real cavities differ
through wall compliance, damping, openings and temperature; mode shapes are
not calculated analytically.

## Full-wave simulation

The full-wave solver represents the walls by their cells, according to the
**Boundary** setting: rigid, pressure release, or the assigned material.
Shells thinner than two cells are thickened to two cells, and the preflight
names each such object. A guide that lies along a plane region is simulated
as a 2D channel between two walls.

Worked example, `examples/advanced_acoustics/e-resonant-cavity.resonant`: a
rigid 40 × 30 mm cavity cut by a plane region, excited in one corner by a
Ricker wavelet and recorded in the opposite corner, 30 cells per wavelength,
10 ms.

1. Press **F6** (**Simulation > Run Full-Wave Simulation**).
2. In **Analysis > Time Signals** choose **Show: Spectrum magnitude**.
3. The response peaks of modes (1,0), (0,1), (1,1) and (2,0) lie within
   100 Hz of the cavity's frequencies as the grid holds it, and within 4 % of
   the designed ones (4.29, 5.72, 7.15 and 8.58 kHz). The difference is
   staircasing: the walls lie where the grid puts them.

These are response peaks, not eigenmodes: a mode with a node at the source or
the probe is missed, and two modes closer than the spectral resolution (here
200 Hz with the Hann window) appear as one. Resonant has no eigenmode solver
for acoustic enclosures.

## Common mistakes

- Expecting a waveguide to change an analytical field.
- Reading the estimates of a tapered guide as valid along its length.
- Placing source and probe on symmetry planes of a cavity, where they miss
  modes.
