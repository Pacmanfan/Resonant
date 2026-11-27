---
id: em.arrays
title: Antenna arrays
category: Electromagnetic Engineering
order: 40
status: available
summary: Linear, planar, circular and custom RF arrays, their elements and weights, and the array-factor analysis of their beam.
keywords: [array, array factor, element, spacing, sidelobe, beamwidth, directivity]
related: [em.beamforming, em.patterns, em.antennas, concepts.arrays, acoustics.arrays]
---

An RF array is one scene object holding many identical elements. Resonant
analyses it with the **array-factor model**: the element pattern times the
sum of the elements' weighted contributions, without coupling between the
elements.

## Adding an array

| Menu entry (Electromagnetics) | Created at 1 GHz, half-wavelength spacing |
|---|---|
| Add Linear RF Array | 8 half-wave dipoles along local X |
| Add Planar RF Array | 4 × 4 cosine elements in local XY |
| Add Circular RF Array | 12 half-wave dipoles on a ring in local XY |

In the viewport each element is a small cube whose brightness follows its
relative amplitude (grey when off), with a line along the array's local +Z.

## Settings

The **Electromagnetics** group of the Properties panel holds:

| Field | Meaning |
|---|---|
| Layout | *Linear (along local X)*, *Planar (local XY)*, *Circular (in local XY)*, *Custom positions* |
| Elements, Rows (planar) | element count along X, and rows along Y |
| Spacing X, Spacing Y | element spacing (lengths in the display unit) |
| Ring radius | circular layout |
| Element pattern | *Isotropic*, *Short dipole (along local Z)*, *Half-wave dipole (along local Z)*, *Cosine (half space +Z)* |
| Frequency | MHz |
| Element amplitude | common amplitude (A for dipole elements) |
| Active | an inactive array does not radiate |

The **Elements** table lists every element: **#**, **Position (local)**,
**Amplitude**, **Phase (deg)**, **Delay (ns)** and **On**. Edit a cell to
override one element; overrides are kept by element index and survive
layout changes. **Clear Element Settings** removes them all. The cosine
element is a simple model of a patch on a ground plane.

## The model

```math
F(r̂) = g(r̂) · Σ_n w_n · e^{ j k r̂·r_n − jωτ_n }
```

r̂ is the direction, g the element pattern, r_n the element positions, w_n
the complex weights (amplitude and phase), τ_n the true time delays and
k = 2π/λ. In the **Array** tab, **Analyse Array** samples the sphere every
0.25 - 0.5° and reports the main-lobe direction (θ, φ in the array's local
frame), the directivity of the idealized model in dBi, the half-power
beamwidths in θ and φ, the highest sidelobe relative to the main lobe and
any other maxima within 1 dB of it. A plot shows a cut through the main
lobe. The same summary appears in the panel's report when the array is
selected.

Reference values for a uniform line of N isotropic elements at λ/2:
first sidelobe −13.15 dB for N = 16 (tending to −13.26 dB for long
arrays), beamwidth 0.886 × 2 / N rad (6.3° for N = 16) and directivity N
(12.0 dBi for N = 16).

## Worked example

Open `examples/electromagnetics/c-linear-array.resonant`: 16 half-wave
dipoles at λ/2 along X at 1 GHz, with a Dolph-Chebyshev 30 dB taper stored
as element amplitudes (0.29 at the ends, 1.0 in the middle). **Analyse
Array** reports a highest sidelobe of −30 dB and a main lobe broadside,
perpendicular to the array and to the dipoles (along ±Y: the two
directions are equal maxima). Clear the element settings and analyse again:
the uniform array has −13.2 dB sidelobes and a higher directivity, which is
the price of the taper. Steering and tapering are described in
[Beamforming](beamforming.md).

## Fields of an array

Arrays of half-wave or short dipole elements also radiate in the
analytical field evaluation (Fields tab), as superposed independent
elements. Arrays of isotropic or cosine elements have no field model and
are refused there ([Near field and far field](near-far-field.md)).

## Common mistakes

- **Spacing under a quarter wavelength.** The array factor ignores the
  strong coupling of close elements; the analysis warns about it.
- **Spacing too wide for the steering angle.** Grating lobes appear; the
  analysis predicts and names them ([Beamforming](beamforming.md#grating-lobes)).
- **Forgetting the element pattern.** An isotropic element hides the
  element's own nulls; choose the element that matches your antenna.

## Limitations

- No mutual coupling, scan impedance, edge effects or element-to-element
  impedance: the array is a design starting point, not a guarantee for a
  built array.
- The full-wave Yee solver does not represent arrays.
- All elements share one element pattern and frequency.
