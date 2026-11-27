---
id: em.waveguides
title: Waveguides
category: Electromagnetic Engineering
order: 100
status: available
summary: Modes, cut-off frequencies and propagation of rectangular, circular and coaxial guides, rectangular cavity resonances and the TE10 field.
keywords: [waveguide, cut-off, TE10, mode, cavity, guide wavelength, evanescent]
related: [em.transmission-lines, em.solvers, em.maxwell, acoustics.waveguides]
---

A hollow metal guide carries electromagnetic waves only above the cut-off
frequency of each mode. Resonant lists a guide's lowest modes with their
cut-offs, tells you which propagate at your frequency and how fast the
others decay, finds cavity resonances, and shows the field of the
dominant TE10 mode.

## Guide objects

| Menu entry (Electromagnetics) | Default | Geometry parameters |
|---|---|---|
| Add Rectangular Waveguide (EM) | WR-90, 22.86 × 10.16 mm, 100 mm long | Inner width a (X), Inner height b (Y), Length (Z) |
| Add Circular Waveguide (EM) | radius 10 mm | Inner radius, Length (Z) |
| Add Coaxial Line | 0.45 / 1.47 mm radii | Inner conductor radius, Outer conductor inner radius, Length (Z) |
| Add Rectangular Cavity | 30 × 18 × 42 mm | Inner size a (X), Inner size b (Y), Inner size d (Z) |

The guide runs along its local Z axis; the walls are drawn as thin solids
and are perfect conductors in the model. The object's material is the
**filling** (vacuum when there is none; a conducting material is ignored
with a note). The Properties panel's Electromagnetics group holds the
**Frequency of interest** (MHz, default 10 GHz).

## Modes and propagation

```math
f_c = c / (2 √(εr μr)) · √((m / a)² + (n / b)²)          rectangular guide
β = k √(1 − (f_c / f)²),      λ_g = 2π / β              propagation constant, guide wavelength
Z_TE = η / √(1 − (f_c / f)²),      Z_TM = η √(1 − (f_c / f)²)
v_p · v_g = c² / (εr μr)
```

TE_mn modes have m, n ≥ 0 (not both zero), TM_mn modes m, n ≥ 1. For a
circular guide of radius a the cut-offs follow from the zeros of the
Bessel functions J_n′ (TE) and J_n (TM). Below cut-off a mode is
evanescent: it decays with α = k_c √(1 − (f / f_c)²), reported in dB/m.

Select the guide and press **Analyse**: the report lists the lowest modes
with "cut-off ... GHz" and either "propagates: guide wavelength ..., wave
impedance ..., group velocity ... c" or "evanescent: decays ... dB/m".

## Worked example

Open `examples/electromagnetics/f-rectangular-waveguide.resonant`: a WR-90
guide at 10 GHz.

| Mode | Cut-off | At 10 GHz |
|---|---|---|
| TE10 | 6.557 GHz | propagates: guide wavelength 39.71 mm, wave impedance 499.0 ohm, group velocity 0.755 c |
| TE20 | 13.114 GHz | evanescent, about 1540 dB/m |
| TE01 | 14.754 GHz | evanescent |
| TE11, TM11 | 16.145 GHz | evanescent |

The guide is single-mode between 6.557 and 13.114 GHz. In the **Fields**
tab, press **TE10 Field of the Guide** to show the field over the
cross-section:

```math
E_y = E₀ sin(πx / a) · e^{−jβz},      E₀ = 1 V/m
```

with the matching H_x and H_z. Its power flux equals the closed-form TE10
power (checked to 1e-6). The field is refused below the TE10 cut-off.

## Cavities

A rectangular cavity a × b × d resonates at

```math
f_mnp = (c / (2 √(εr μr))) · √((m / a)² + (n / b)² + (p / d)²)
```

and the report lists the TE_mnp and TM_mnp resonances in ascending order.
The default cavity (30 × 18 × 42 mm, empty) has its lowest resonance,
TE101, at 6.140 GHz, followed by TE102 at 8.713 GHz and TE011 at 9.060 GHz.

## Common mistakes

- **Swapping a and b.** a is the wide side along local X; the TE10 cut-off
  is c / (2a).
- **Expecting a dielectric's loss in the cut-offs.** The filling's εr and
  μr shift the cut-offs; its losses are not included.
- **Looking for the field of a higher mode.** Only the TE10 field of a
  rectangular guide is drawn.

## Limitations

- Ideal uniform guides with perfectly conducting walls: no wall loss, no
  Q of cavities, no coupling ports, irises, bends or transitions.
- Fields of modes other than TE10 are not implemented.
- Parallel-plate modes exist in the engine but there is no parallel-plate
  object.
- Guide objects are also conducting bodies for the full-wave solver, which
  can model what the closed forms cannot
  ([Electromagnetic solvers](solvers.md)).
