# Waveguides

**Status:** current for REQ-CHAT-20261126-004. **Waveguides have no field
model.** Geometry and analytical cut-off estimates only.

## Geometry

* `waveguide.circular` — inner radius at the entry and at the exit (equal:
  straight; different: a linear taper), length, wall thickness.
* `waveguide.rectangular` — inner width and height, length, wall thickness.

Curved paths, bends, branches, elliptical and custom cross-sections are not
available.

## Estimates (rigid walls, lossless fluid, uniform cross-section)

Above its cut-off frequency a mode propagates; below it, it decays along
the guide. The plane wave has no cut-off.

* Rectangular duct `a × b`: `f_mn = (c/2) √((m/a)² + (n/b)²)`.
* Circular duct of radius `a`: `f_mn = c j'_mn / (2π a)`, `j'_mn` a zero of
  the derivative of the Bessel function `J_m` (first: 1.8412, mode (1,0)).

The editor lists the lowest cut-offs and says whether only the plane wave
propagates at the simulation frequency. For a tapered guide the estimates
are for the entry cross-section only, and the editor says so. The tabulated
Bessel zeros are verified numerically in the tests.

No propagation constant, attenuation, wall impedance or field inside the
guide is calculated.
