# Periodic Layers (Photonic Unit Cells)

**Status:** current (REQ-CHAT-20261126-017, RES-FUNC-010):
`src/core/electromagnetics/PeriodicLayers`, the analysis `em.periodic.layers`.

A two-layer cell (d₁, ε₁; d₂, ε₂), lossless and non-magnetic, at normal
incidence:

    cos(K a) = cos(k₁d₁) cos(k₂d₂) − ½ (n₁/n₂ + n₂/n₁) sin(k₁d₁) sin(k₂d₂),   a = d₁ + d₂

| Result | How |
|---|---|
| Bloch wavenumber | Re K in [0, π/a] in a band; Im K ≥ 0 in a gap (Np/m) |
| band gaps | |cos K a| > 1, edges by bisection to 1e-10; peak attenuation max Im(K) a |
| finite stack | `periodicStack` → the exact layered-media solution (`LayeredMedia`) |
| homogenized medium | long-wavelength index K c/ω → √ε_∥; quasi-static ε_∥ = (ε₁d₁ + ε₂d₂)/a, ε_⊥ = a/(d₁/ε₁ + d₂/ε₂); valid below a/λ = 0.1 |

Effective properties are derived from the cell, labelled as homogenized and
reported with their model and validity limit; none is entered by hand, and
none is given inside or above a band gap.

## Validation

Quarter-wave stack: gap centre, width (4/π) arcsin(|n₂ − n₁|/(n₂ + n₁)) and
attenuation ln(n₂/n₁) per cell; the stack's transmission decaying at
−2 Im(K) a per cell (2 %); the effective index at the homogenization limit
within 0.13 % of √ε_∥.

## Limits

This analysis is lossless and at normal incidence. Oblique incidence (TE and
TM), lossy layers, the uniaxial effective slab and parameter retrieval are in
`em.layered-effective-medium` (`ObliqueLayers`, `Retrieval`;
[effective properties](../materials/EFFECTIVE_PROPERTIES.md), REQ-CHAT-20261126-019).
2D/3D lattices are not modeled; the elastic counterpart is the periodic rod
(`mechanics/Bloch`).
