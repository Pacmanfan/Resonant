# Resonance Validation

**Status:** every case runs in the core suite (RES-RSN-014). Each compares
with a closed form, an independent model or an independent computation.

| Model | Check | Reference | Tolerance | Test |
|---|---|---|---|---|
| Microwave cavity | eigenfrequencies, wall and fill Q, port bandwidth, field boundary conditions | closed forms, Pozar's TE10p Q | 1e-12 … 1e-9 | `CavityResonator.*` |
| | mesh convergence of the full-wave resonances | Yee at 6 and 12 cells | error falls, < 0.5 % | `CavityResonator.YeeResonancesConverge…` |
| | ring-down identification | poles of the Yee record against f₁ and ωε/σ | 0.5 %, 3 % | `ResonanceExamples.CavityRingDown…` |
| | Q against extraction | loaded Q falls and β/(1+β) rises with the coupling | monotonic; ½ at β = 1 | `ResonanceExamples.QAgainstExtraction…` |
| Optical resonator | stability, FSR, finesse, linewidth, beam parameter | ABCD round trip, confocal closed forms, the exact layered Fabry–Perot | see `docs/physics/FUNCTIONAL_STRUCTURE_VALIDATION.md` | `OpticalResonator.*` |
| Helmholtz | resonance and losses | closed forms; 1/Q = Σ 1/Q_i; bandwidth f₀/Q | 1e-9, 2 % | `HelmholtzResonator.LumpedModel…` |
| | end correction and convergence | plane-wave stepped duct: deviation falls with the cavity length; resonance at L + 8a/3π (within 2 %), not at L (> 12 % off) | | `HelmholtzResonator.TheSteppedDuct…` |
| | tuning | d ln f₀ / d ln V = −½ | 1e-6 | `ResonanceExamples.Helmholtz…` |
| Cantilever | modes | Euler–Bernoulli (λL = 1.8751, 4.6941); tip mass = ¼ of the beam | 1e-9, 1e-6 | `ResonanceExamples.Cantilever…` |
| | ring-down | Q = 1/η from the simulated decay | 0.5 % | same |
| | finite elements | the existing modal benchmark (`FunctionalExamples.MechanicalResonator…`) | 1 % | — |
| Coupled resonators | eigenfrequencies | the 2 × 2 closed form (identical and detuned) | 1e-9 | `LumpedNetwork.*` |
| | splitting and avoided crossing | minimum gap of a detuning sweep = the coupling splitting | 2 % | `ResonanceExamples.CoupledCantilevers…` |
| | energy exchange | full transfer at half a beat (identical); coupled-mode theory's 4κ²/(4κ²+Δ²) (detuned) | 5 % | `LumpedNetwork.EnergyExchanges…` |
| | damping and buildup | envelope 1 − e^{−t/τ}, decay e^{−ωt/Q}, losses by mechanism | 3 %, 2 % | `LumpedNetwork.BuildupAndDecay…`, `…DrivenPeaks…` |
| | conservation | supplied = stored + dissipated, coupling energy once | 1e-6 relative | `LumpedNetwork.CouplingEnergyIsCountedOnce…` |
| | coupled Helmholtz | the 2 × 2 quadratic of inertances and compliances | 1e-4 (damping) | `ResonanceExamples.CoupledHelmholtz…` |
| Nonlinear | Duffing backbone, bistability | time integration | 0.4 % + O((k₃A²)²); 5 % | `NonlinearResonance.*` |
| | Mathieu tongue | second-order Mathieu boundaries; damped threshold 4ζ | 2e-4 | `NonlinearResonance.MathieuTongue…` |
| Multiphysics resonator | resonance shift, heating, feedback | an independent scalar fixed point T = T₀ + P(T)/G | 1e-5 K | `CavityHeating.FeedbackConverges…` |
| | one-way against feedback, revisions, ledger | full absorption one-way; balanced ledger; geometry revision | 1e-9, 1e-6 | `CavityHeating.*` |
| Spectroscopy | poles and fits | synthetic signals and spectra | 1e-6 … 5 % | `ResonanceSpectroscopy.*` |

Every analysis result records the model and version, assumptions, geometry
and material keys, the discretization, warnings and, for identified
resonances, the method and uncertainty.
