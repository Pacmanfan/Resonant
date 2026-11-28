# Effective Properties

**Status:** current (REQ-CHAT-20261126-019). **Requirements:** RES-META-004..011.

What each effective-medium analysis derives, from what, and how.

## Mass-in-mass lattice (mechanical)

Host masses m₁ on springs k₁ at spacing a, each carrying a resonator m₂ on k₂:

- dispersion `4 k₁ sin²(qa/2) = ω² m_eff(ω)`, effective dynamic mass
  `m_eff = m₁ + m₂ ω₂² / (ω₂² − ω²)`, ω₂ = √(k₂/m₂);
- m_eff < 0 between ω₂ and ω₂√(1 + m₂/m₁): the locally resonant gap, whose
  upper edge is exactly the optical branch at q = 0;
- long waves move the whole cell: `c = a √(k₁ / (m₁ + m₂))`.

In the example the cell's masses come from its geometry (a 20 mm steel block,
an 8 mm lead core: derived parameters `7850 w³` and `11340 (4/3) π r³`).

## Duct with Helmholtz resonators (acoustic)

Plane waves; each cell is a half segment, the resonator's shunt admittance
Y = 1/Z_H (lumped, with end corrections and viscous loss), a half segment.

- Bloch: `cos(qa) = (T₁₁ + T₂₂)/2` of the cell matrix;
- effective bulk modulus `1/B_eff = 1/B₀ + Y/(jωSa)`; at low frequency
  `B_eff = B₀ / (1 + V/(Sa))` and `c_eff = c / √(1 + V/(Sa))`;
- lossless: B_eff < 0 between f₀ and f₀√(1 + V/(Sa)) (the hybridization gap);
  a Bragg gap opens at c/2a.

The effective material holds ρ₀ and the sound speed as a frequency table up to
half the resonance.

## Layered dielectric (electromagnetic)

Two isotropic layers d₁, d₂ (complex ε from ε', tan δ and σ at the reference
frequency):

- `ε_∥ = (d₁ε₁ + d₂ε₂)/a` (fields along the layers),
  `ε_⊥ = a/(d₁/ε₁ + d₂/ε₂)` (fields across them);
- the uniaxial slab: TE `k_z = k₀√(ε_∥ − sin²θ)`, TM `k_z = k₀√(ε_∥(1 − sin²θ/ε_⊥))`;
- checked against the explicit symmetric stack (layer₁/2, layer₂, layer₁/2 per
  period) at the chosen angle, against the cell's oblique Bloch waves, and by
  retrieval at normal incidence.

## Retrieval (electromagnetic)

`Retrieval.h`: from a slab's r and t (e^{+jωt}), `z = ±√(((1+r)²−t²)/((1−r)²−t²))`
with Re z ≥ 0, `P = t/(1 − r(z−1)/(z+1)) = e^{−jnk₀d}`, Re n from the phase
unwrapped along frequency, `ε = n/z`, `μ = nz`. Each point is flagged when the
slab is not passive (Im n > 0), when the impedance is ill-conditioned, when the
cell exceeds a tenth of the effective wavelength, and when a passive slab gives
Im ε > 0 or Im μ > 0 — the antiresonance of a periodic cell, a sign of spatial
dispersion.

## Split-ring array (electromagnetic, a circuit model)

`L = μ₀ r (ln(8r/r_w) − 2)`, `C = ε₀ ε_eff (wt/g + w + t)`, skin-effect R and the
substrate's dielectric loss; `μ_eff = 1 − Fω²/(ω² − ω₀² − jωΓ)`, F = πr²/a².
Re μ < 0 from f₀ to f₀/√(1 − F). Checked for passivity and causality
(Kramers–Kronig residual); not compared with a full-wave calculation.

## Layered thermal medium

`k_∥ = Σ dᵢkᵢ / a`, `k_⊥ = a / Σ(dᵢ/kᵢ)` (exact for steady conduction without
interface resistance), density thickness-weighted, specific heat
mass-weighted; checked by finite volumes on the explicit layers.

## Provenance and components

A saved effective material's values have provenance `homogenized`. The
library holds isotropic values; an anisotropic result offers its principal
components and a saved material carries one of them (the first listed unless
chosen). Negative values (a negative mass, modulus or permeability) are
results, not library values.
