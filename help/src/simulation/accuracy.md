---
id: sim.accuracy
title: Numerical accuracy
category: Simulation
order: 40
status: available
summary: Where the errors of a Resonant result come from: the model's assumptions, the numerical method, and the precision of storage, with the measured size of each.
keywords: [accuracy, error, dispersion, validation, approximation, precision, tolerance]
related: [sim.convergence, sim.mesh-resolution, ref.solver-assumptions, ref.numerical-limits, acoustics.fdtd]
---

A result can be wrong in two different ways. A **model error** comes from
what the model leaves out (an analytical free field ignores a wall); no
resolution fixes it. A **numerical error** comes from how an included effect
is computed (cells, surface elements, time steps); it shrinks as the
resolution grows. Resonant states both: the assumptions with every result,
and the resolution and its expected error in every preflight.

## Analytical free field

Exact within its model: every sample is the closed-form sum of the sources'
fields, computed in double precision. The model error is everything it
leaves out: objects, reflections, materials, transients
([Choosing a solver](solver-selection.md)).

- Sampled fields are **stored** in single precision (8 bytes per complex
  sample): grid values agree with a point evaluation to about 1e-4
  relative. Probe, target and inverse-design values are double precision.
- The baffled-piston directivity is a far-field formula used at every
  distance.
- Within a source's radius the value is held and flagged as not valid.
- Validated against closed forms to 1e-9 to 1e-12 relative (spreading,
  phase, interference, focusing, inverse design).

## Free field + reflectors

Two errors combine:

- **Quadrature** of the surface integral: it converges with the surface
  resolution; at 8 elements per wavelength within 1 % of 16.
- **The Kirchhoff approximation itself**: one reflection, the field of an
  infinite tangent plane on each element, no mutual shadowing. It is good
  for reflectors and curvature radii several wavelengths large. Its error for
  wavelength-scale reflectors is **not quantified**; the convergence above
  shows only that the integral is evaluated accurately.

Validated against image sources (within 4 % of the image field for a rigid
plate 40 wavelengths square), the mirror equation (peak within half a
wavelength) and ray optics.

## Full-wave acoustic solver

```math
sin(ω Δt / 2) = S · sin(k Δx / 2),      S = c Δt / Δx
v_p / c = (π / N) / asin( sin(π S / N) / S )
```

`N` is the number of cells per wavelength and `v_p` the numerical phase
velocity. The main numerical errors:

| Error | Size | What reduces it |
|---|---|---|
| Numerical dispersion | 0.64 % slow at 12 cells per wavelength; accumulates with distance | more cells per wavelength (second order) |
| Staircased surfaces | up to one cell in position | smaller cells (first order) |
| Absorbing-layer reflection | below 0.2 % at normal incidence with 12 cells; more at grazing incidence | thicker layers, sources a few cells away |
| Impedance walls | first order, locally reacting | absorbing layers for open space |
| Spectrum above the resolved frequency | not physical | the panel marks the limit |

A **smaller** time step makes the dispersion error slightly **larger**: the
time and space errors partly cancel. Keep **Time step / stability limit**
near its default of 0.95.

Validated against closed forms: spreading within 3 % (3D point source),
interface reflection and transmission within 0.012, boundary reflection
within 0.01, cavity modes within half the spectral resolution. There is no
comparison with another full-wave code or with measurements, and boundaries
and materials are validated in 2D only.

## Mechanics

- Beams (Euler-Bernoulli frame elements): exact static tip deflection;
  frequencies converge with the fourth power of the element size, from
  above.
- Plates (Kirchhoff, ACM): frequencies converge with the second power, from
  below; −0.83 % at 8 × 8 and −0.21 % at 16 × 16 for the first mode of a
  simply supported plate.
- Solids (trilinear hexahedra): shear locking makes them far too stiff in
  bending unless several elements span the thickness (34 % of the beam
  deflection with 2 elements through it and 5 along it).

## Electromagnetics

The Yee solver is the same kind of scheme (staggered grid, second order,
leapfrog in time), with the same kind of numerical dispersion and staircased
surfaces; thin wires have an effective radius of about 0.23 cells.
Analytical antenna models assume a sinusoidal current on thin wires; array
models assume isolated elements (no mutual coupling).

## Worked example: the error budget of a full-wave focus

`examples/advanced_acoustics/f-passive-focusing.resonant`: 12 cells per
wavelength at 40 kHz, a reflector 100 mm across, emitter 150 mm away.

- Dispersion: 0.64 % slow; over the roughly 22 wavelengths from the emitter
  via the reflector to the focus, the wave lags by about 0.14 wavelength.
  The validated peak is within half a wavelength of the mirror-equation
  image (37.5 mm).
- Staircasing: the curved surface lies within 0.72 mm (one cell) of its
  equation.
- Model: 2D (a trough, not a dish); the solid is rigid.

Rerun at 16 or 20 cells per wavelength and compare
([Convergence](convergence.md)).

## Rules of thumb

- Read the assumptions of a result before its numbers.
- Trust a difference between two results only when it is larger than the
  change between two resolutions.
- Use the absolute colour scale and the comparison table for comparisons;
  see [Comparing simulations](comparison.md).
