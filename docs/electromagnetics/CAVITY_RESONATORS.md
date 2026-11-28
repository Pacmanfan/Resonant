# Cavity Resonators

**Status:** current (REQ-CHAT-20261126-017, RES-FUNC-006):
`src/core/electromagnetics/CavityResonator`, the analysis `em.cavity.rectangular`.

A rectangular box a × b × d (x, y, z) with conducting walls and a fill of
ε′, tan δ, σ, μ_r. Modes TE_mnp and TM_mnp with respect to z, e^{+jωt}, peak
amplitudes, normalized to a peak |E| of 1 V/m.

## Closed-form model (`em.cavity.rectangular.analytical`)

| Quantity | Expression |
|---|---|
| frequency | f = (c/2√(ε_r μ_r)) √((m/a)² + (n/b)² + (p/d)²) |
| fields | TE: H_z = cos(k_x x) cos(k_y y) sin(k_z z) and the transverse fields from it; TM: E_z = sin sin cos (`cavityModeField`) |
| stored energy | W_e = (ε′/4)∫|E|², W_m = (μ/4)∫|H|² (exact separable integrals); W_e = W_m at resonance |
| wall loss | P_c = (R_s/2)∮|H_t|² dS with R_s = √(ωμ₀μ_w/2σ_w): the ideal mode on walls of finite conductivity (perturbation) |
| fill loss | P_d = (ω ε″_eff/2)∫|E|², ε″_eff = ε′ tan δ + σ/ω |
| Q | Q_c = ωW/P_c, Q_d = ωW/P_d = ε′/ε″_eff, 1/Q₀ = 1/Q_c + 1/Q_d, 1/Q_L = 1/Q₀ + 1/Q_e |
| port | a lumped coupling of one mode: β = Q₀/Q_e, y = (1 + jQ₀(f/f₀ − f₀/f))/β, Γ = (1 − y)/(1 + y); bandwidth f₀/Q_L |
| sensitivity | ∂f/∂a = −(c²/4) m²/(a³ f), and likewise for b and d |
| temperature | walls expanding uniformly: dimensions × (1 + α (T − 293.15 K)), α from the wall material's thermal set |

## Full-wave model (`em.cavity.rectangular.yee`)

The Yee solver on a box of perfectly conducting faces, rounded to whole cells
(`cellsAcrossSmallest`, default 8), excited by Ricker pulses on all three E
components at a point without symmetry; the resonances are peaks of the summed
spectra at a second point. A loss tangent is represented by the conductivity
it equals at the lowest resonance; the energy record's decay rate gives
σ/ε. Wall losses are not part of this model.

## Validation

See [the validation table](../physics/FUNCTIONAL_STRUCTURE_VALIDATION.md):
closed forms to 1e-12, the wall Q against Pozar's TE10p formula, the
port bandwidth, the Yee frequencies converging (< 0.5 % at 12 cells) and the
Yee decay rate within 3 % of σ/ε.

## Limits

Rectangular cavities only (cylindrical and other shapes are not analyzed);
good-conductor walls; losses small (Q ≫ 1); the port is a lumped external
Q, not a modeled aperture or probe; dispersion of the fill is not modeled
(values at the frequency of interest).
