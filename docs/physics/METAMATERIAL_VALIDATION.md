# Metamaterial Validation

**Status:** current (REQ-CHAT-20261126-019). **Requirements:** RES-META-003..012.

Each check runs in the core test suite; the numbers are the examples' results.

| Check | Reference | Result | Test |
|---|---|---|---|
| Bloch eigenproblem of lumped cells | monatomic and diatomic chains (closed form) | 1e-9 | `Bloch.*` |
| mass-in-mass cell Bloch | the closed-form quadratic | 4e-14 (example) | `LocallyResonantLattice.NumericalBlochMatchesTheClosedForm` |
| locally resonant gap | upper edge = ω₂√(1 + m₂/m₁) = end of negative mass | exact; example 297.6–352.5 Hz, f₂ = 299.3 Hz, host zone edge 1796 Hz | `LocallyResonantLattice.TheGapIsWhereTheEffectiveMassIsNegative` |
| finite chain in the gap | Bloch attenuation Im(q)a | 0.3864 vs 0.3864 Np per cell | `LocallyResonantLattice.AFiniteChainDecaysAtTheBlochRate` |
| long waves | a√(k₁/(m₁+m₂)) | 95.83 m/s both | `MetamaterialExamples.MassInMassGapComesFromTheCoreResonance` |
| duct low-frequency speed | c/√(1 + V/(Sa)) | 267.84 vs 267.82 m/s | `ResonatorDuct.TheCellIsReciprocalAndLongWavesAreSlowed` |
| duct hybridization gap | effective medium: B_eff < 0 from f₀ to f₀√(1 + V/(Sa)) | gap 459–594 Hz; prediction 466–597 Hz | `ResonatorDuct.TheHybridizationGapStraddlesTheResonance` |
| duct Bragg gap | opens at c/2a exactly | 3430 Hz | same |
| finite resonator array | Bloch attenuation | 4.597 vs 4.598 dB per cell | `ResonatorDuct.AFiniteArrayAttenuatesAtTheBlochRate` |
| oblique stacks | layered-media solver (normal), Airy formula, Brewster angle | 1e-12; R_TM(θ_B) < 1e-24 | `ObliqueLayers.*` |
| homogenization convergence | (a/λ)² | error ratio 4 ± 0.4 per halving | `ObliqueLayers.HomogenizationConvergesAsThePeriodShrinks` |
| layered dielectric vs explicit at 45° | uniaxial slab | max abs(Δt) TE 0.0034, TM 0.0016 inside; 0.23 / 0.088 beyond; isotropic ε_∥ misses TM by 0.13 | `MetamaterialExamples.LayeredAnisotropy…` |
| oblique Bloch waves | uniaxial dispersion | 7e-5 (TE), 4e-5 (TM) | same |
| retrieval | homogeneous magnetic lossy slab over several wavelengths; negative-index slab | 1e-8; negative branch | `Retrieval.*` |
| retrieval of the layered slab | ε_∥, μ = 1 | 4.39998, 1.00001; antiresonance Im μ ≤ 5e-7 | `Retrieval.ALayeredSlab…`, `Retrieval.ALossyLayeredSlab…` |
| split ring | circuit formulas; Lorentz limits; passivity; Kramers–Kronig | f₀ 7.43 GHz, f_mp 9.71 GHz, KK residual 7e-9 | `SplitRing.*` |
| layered conductivity | finite volumes on the explicit layers | 2e-11 (perpendicular), 7e-14 (parallel) | `LayeredConductivity.*` |
| effective material in the coupled engine | ten explicit layers in a steady thermal system | heat flow equal to 1e-9 | `MetamaterialServices.AHomogenizedRegionReproducesTheExplicitLaminateInTheCoupledEngine` |
| services, Python, GUI | saving, undo, stale refusal, components | — | `MetamaterialServices.*`, `python/tests/test_metamaterial_api.py`, `FunctionalPanelTest.SavesAnEffectiveMaterialIntoTheLibraryUndoably` |

## Not validated

The split ring against a full-wave calculation or a measurement; 2D/3D
lattices; nonlinear or time-varying metamaterials.
