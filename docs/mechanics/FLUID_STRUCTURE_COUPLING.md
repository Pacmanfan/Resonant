# Fluid-Structure Coupling

**Status:** current for REQ-CHAT-20261126-006. One-way only. Code:
`src/core/coupling/StructuralRadiation.*`.

## What is implemented

Sound radiated by a vibrating plate or membrane, from a harmonic result at
one frequency, onto an acoustic field region:

```text
p(x) = (j omega rho / 2 pi) sum_e v_e A_e exp(-j k R_e) / R_e,      v_e = j omega w_e
```

the Rayleigh integral of a flat surface in an infinite rigid baffle, with
each element radiating from its centre with its mean normal velocity.
Convention `e^{+j omega t}`, peak amplitudes. The field becomes an ordinary
acoustic result (display, analysis, comparison), solver id
`coupling.structure-to-acoustic.rayleigh`.

## Assumptions (in every result)

* One-way: the plate's vibration is calculated in vacuo and taken as
  given; the fluid does not load the plate (no radiation damping, no added
  mass). For light fluids (air) and stiff plates this is usually small; in
  water it is not.
* Infinite rigid baffle; the field is calculated only in front of it, at
  least half an element from the surface; other samples are excluded.
* Elements small against the acoustic wavelength; a warning above a sixth.
* Linear acoustics in a homogeneous medium at rest.

## Validation

A rigid piston (all nodes moving equally) of 0.02 m^2 at 500 Hz gives the
far-field pressure `rho omega^2 w A / (2 pi r)` at 10 m within 1 %; an
antisymmetric pattern is silent on the axis (1e-6); beams are refused.

## Not implemented

Two-way coupling (structural-acoustic eigenproblems, fluid loading),
acoustic excitation of structures, radiation from beams, solids or
unbaffled plates, coupling to the FDTD solver, transducer models.
