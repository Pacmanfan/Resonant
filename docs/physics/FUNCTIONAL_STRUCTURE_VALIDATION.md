# Functional Structure Validation

**Status:** every case below runs in the core suite (RES-FUNC-006..010,
RES-FUNC-013, RES-FUNC-016). Each compares a physical result with a closed
form or an independent computation.

| Structure | Check | Reference | Tolerance | Test |
|---|---|---|---|---|
| Microwave cavity | resonant frequencies | f = (c/2)√((m/a)² + (n/b)² + (p/d)²) | 1e-12 | `CavityResonator.FrequenciesAreTheClosedForm…` |
| | energy equipartition at resonance | W_e = W_m | 1e-12 | same |
| | wall Q of TE10p | Pozar eq. 6.46 (independent closed form) | 1e-10 | `…WallQOfTE10pMatchesTheClosedForm` |
| | fill Q | ε′/ε″_eff = 1/tan δ, ωε′/σ | 1e-7 | `…FillQIsTheInverse…` |
| | port response | half-power bandwidth of 1 − |Γ|² = f₀/Q_L; |Γ(f₀)| = |β−1|/(β+1) | 1e-9 | `…PortResponseHalfPowerBandwidth…` |
| | boundary conditions, field integrals | tangential E = 0 on the walls; midpoint quadrature of |E|² = W_e | 1e-12, 1e-10 | `…ModeFieldsMeetTheWalls…` |
| | sensitivity | finite differences of the closed form | 1e-5 | `…SensitivityToTheDimensions…` |
| | full-wave frequencies, mesh convergence | Yee (PEC box 30 × 18 × 42 mm) at 6 and 12 cells across b | error falls with refinement, < 0.5 % | `…YeeResonancesConverge…` |
| | full-wave material loss | energy decay rate of a conducting fill = σ/ε | 3 % | `…YeeEnergyDecay…` |
| | the example (PTFE, copper, critical coupling) | closed forms; Yee at 8 cells (length rounded to 30.48 mm) | 2 % (Yee), fill Q from decay 10 % | `FunctionalExamples.MicrowaveCavity…` |
| Optical resonator | stability | g₁g₂ classes (plane, confocal, concentric, stable, unstable) | exact | `OpticalResonator.StabilityIsClassified…` |
| | confocal closed forms | w₀ = √(λL/2π), w_mirror = √2 w₀, Δf_T = FSR/2 | 1e-12 | `…ConfocalClosedForms` |
| | self-consistency | the beam parameter after one ABCD round trip; spot sizes from q; wavefronts match the mirrors | 1e-12, 1e-9 | `…ReproducesItselfAfterOneRoundTrip` |
| | spectrum (independent) | FSR and linewidth of a plane Fabry–Perot from the exact layered-media solution | 0.5 %, 3 % | `…PlaneFabryPerotSpectrumMatchesTheExactLayeredSolution` |
| | paraxial assumptions | stated with every result; waist < 10 λ warned | — | — |
| Acoustic horn | piston radiation impedance | power series of Struve H1; (ka)²/2 and 8ka/3π limits; R → 1 | 1e-9, 1 %, 2 % | `AcousticHorn.PistonRadiationImpedance…` |
| | uniform duct | Z_in = Z_c (Z_L + jZ_c tan kL)/(Z_c + jZ_L tan kL) | 1e-9 | `…AUniformDuct…` |
| | exponential horn | infinite-horn impedance √(1 − (m/2k)²) + j m/2k with the outgoing load; reactive below cut-off | 1e-8 | `…InfiniteHorn` |
| | conical horn | spherical-wave impedance jkx/(1 + jkx) | 1e-7 | `…ConicalHorn…` |
| | power accounting, convergence | throat = mouth = every axial sample; fourth-order convergence (ratio > 10 for halved steps) | 1e-8 | `…PowerIsConserved…` |
| | profile comparison | exponential vs conical above cut-off | — | `…ProfilesCompare…`, `FunctionalExamples.AcousticHorn…` |
| Mechanical resonator | modal benchmark | Euler–Bernoulli cantilever | 1 % | `FunctionalExamples.MechanicalResonator…` |
| | sensitivity | d ln f / d ln L = −2, d ln f / d ln t = 1 | 1e-5 | same |
| | mesh reuse and convergence | the existing mechanics suites (`MechanicalFrame.*`) | as there | — |
| Photonic unit cell | band gap | quarter-wave stack: centre f₀, width (4/π) arcsin(|n₂−n₁|/(n₂+n₁)), attenuation ln(n₂/n₁) per cell | 1e-8, 1e-6 | `PeriodicLayers.AQuarterWaveStack…` |
| | full structure | finite-stack transmission (exact layered media) decays at −2 Im(K) a per cell | 2 % | `…FollowsTheBlochAttenuation` |
| | homogenization validity | Bloch index → √ε_∥ at low frequency; 0.13 % off at a/λ = 0.1 | 1e-5, 2e-3 | `…EffectiveIndex…`, `FunctionalExamples.UnitCell…` |
| Coupled slab | engine | converged, balanced ledger; thickness from the object changes the absorptance | balance tolerance 1e-6 | `FunctionalExamples.CoupledSlab…` |

Framework checks (parameters, staleness, sweeps, sensitivity exactness, mode
tracking, objectives, constraints, explanations) are in
`tests/core/test_functional_structures.cpp`; services, jobs, optimization and
permissions in `test_functional_services.cpp`; the contract end to end in
`python/tests/test_functional_api.py`; the panel in `tests/app/test_functional_panel.cpp`.

## Provenance in every result

Model id, name, version and fidelity; solver; geometry, material, definition
and discretization keys; the parameter values; the discretization (grid,
mesh, steps); assumptions; warnings; application version and time. Failures
(a model refusing a configuration, an invalid geometry, cancellation) are
reported as failures, never as results.
