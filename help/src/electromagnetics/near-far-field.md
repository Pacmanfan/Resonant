---
id: em.near-far-field
title: Near field and far field
category: Electromagnetic Engineering
order: 110
status: available
summary: Exact analytical fields at any distance from antennas and sources, on field regions and at probe points, and where the far field begins.
keywords: [near field, far field, field region, probe, Hertzian dipole, power density, field display]
related: [em.patterns, em.solvers, em.antennas, concepts.regions]
---

Close to an antenna the field has stored (reactive) parts that fall off
faster than 1/r; far away only the radiating 1/r part remains and the
pattern no longer changes with distance. Resonant's analytical field models
are **exact at every distance**, not far-field approximations, so you can
study both zones with the same tools.

## The field models

| Radiator | Field model |
|---|---|
| Dipole antenna | closed-form E and H of a sinusoidal current filament, valid at every point off the wire |
| Electric dipole source (current element) | complete 1/r, 1/r² and 1/r³ terms |
| Magnetic dipole source, small loop | the dual of the current element |
| Monopole antenna | the dipole of twice the height by images, above the ground plane only |
| Plane wave | E along local X, travelling along local +Z |
| RF array of dipole elements | the elements' fields superposed (no coupling) |

For a current element of moment I·l (the source's **Amplitude**, A m)
along local Z:

```math
E_r = (η I l cos θ / (2π r²)) · (1 + 1/(jkr)) · e^{−jkr}
E_θ = (jη k I l sin θ / (4π r)) · (1 + 1/(jkr) − 1/(kr)²) · e^{−jkr}
H_φ = (j k I l sin θ / (4π r)) · (1 + 1/(jkr)) · e^{−jkr}
```

η is the wave impedance and k = 2π/λ. Where kr ≫ 1 only the 1/r terms
remain and |E| / |H| tends to η₀ = 376.7 ohm. Points on a wire or source,
where a model is singular, and points below a monopole's ground plane are
**excluded**: not drawn and not reported, never given an arbitrary value.

## Where the far field begins

Each antenna report gives the far-field distance

```math
r_ff = max(2 D² / λ, 3λ, 3D)
```

with D the largest dimension of the antenna (with its image for a
monopole). Patterns ([Radiation patterns](patterns.md)) are
far-field quantities.

## Evaluating fields on a region

1. Add a field region: **Acoustics > Field Plane** or **Field Volume** (the
   same region objects serve both domains) and size it in the Properties
   panel.
2. In the Electromagnetics panel's **Fields** tab choose the **Region** and
   press **Evaluate Analytical Field** (also **Electromagnetics > Evaluate
   Analytical Field**).
3. All active antennas, sources and dipole-element arrays at the frequency
   of the first active radiator are included; radiators at other
   frequencies are left out and reported. Arrays of isotropic or cosine
   elements have no field model and are refused.

For a single point, enter **Probe (world coordinates)** X, Y, Z in metres
and press **Probe**: the text gives each component of E and H as a peak
magnitude and phase, |E|, |H| and the power density.

## The field display

The **Field display** group shows one result at a time in the viewport:

- **Result**: *None (hide)* or a result, labelled [analytical] or [full-wave].
- **Component**: |E| (V/m), Ex, Ey, Ez, |H| (A/m), Hx, Hy, Hz, or
  Power density |Re S| (W/m^2), the time-averaged Poynting vector.
- **Show**: *Magnitude (linear)*, *Phase*, *Instantaneous value* or *Level
  in dB (40 dB range)* (the default).
- **Colour scale**: **Normalized** to the result, or untick it and enter an
  absolute maximum.

**Animate** runs the instantaneous value through a period; **Remove
Result** deletes the shown result. Results are kept for the session, not
in the project file.

## Worked example

Open `examples/electromagnetics/e-near-field.resonant`: the copper
half-wave dipole at 1 GHz (1 A) and an XZ field plane two wavelengths
(0.6 m) wide with 81 × 81 samples. Press **Evaluate Analytical Field** and
show |E| in dB: the samples on the wire are excluded, the field is
strongest close to the wire and spreads broadside, perpendicular to it. At the plane's
corners |E| / |H| is within 6 % of η₀. Integrating the Poynting vector of
this near field over a sphere 0.8 λ around the dipole gives
½ |I|² R_in = 36.54 W, the power the impedance model predicts (agreement
1e-4).

## Common mistakes

- **Mixing frequencies.** Only radiators at the first active radiator's
  frequency are evaluated together.
- **Expecting scattering.** The analytical fields ignore every other
  object in the scene; use the full-wave solver for bodies
  ([Electromagnetic solvers](solvers.md)).
- **Reading a normalized scale as absolute.** Untick **Normalized** to set
  an absolute colour scale.

## Limitations

- No mutual coupling between radiators; fields are superposed.
- The analytical evaluation gives E and H; the full-wave result gives E
  only.
