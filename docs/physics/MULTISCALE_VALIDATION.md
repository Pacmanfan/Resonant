# Multiscale Validation

**Status:** current (REQ-CHAT-20261126-020). **Requirements:** RES-MSC-001..014.

| Check | Reference | Result | Test |
|---|---|---|---|
| FCC lattice sums | Kittel: A12 = 12.13188, A6 = 14.45392; an independent brute-force sum with its tail | 1e-5, 2e-4 | `Atomistic.FccLatticeSumsMatchKittelAndTheIndependentSum` |
| equilibrium and cohesive energy | r0 = (2A12/A6)^(1/6) σ = 1.09018 σ, E = −8.6102 ε | 5e-5 σ, 1e-4 ε | same |
| elastic constants | Cauchy C12 = C44; B = (C11 + 2 C12)/3 against the E(V) curve (75.2 ε/σ³) | 4e-7; 1e-3 | `Atomistic.ElasticConstantsObeyCauchyAndGiveKittelsBulkModulus` |
| cutoff | tail-corrected 2.5 σ vs 15 σ | within 1 %; the bare sum is off by more | `Atomistic.TheTailCorrectionMakesShortCutoffsAgree` |
| imports | XYZ round trip, plain XYZ without a cell, POSCAR; VASP 4 refused | exact | `Atomistic.StructuresImportFromXyzAndPoscarWithoutInventedData` |
| NVE dynamics | drift ∝ dt² (velocity Verlet), equipartition T → T0/2, g(r) peak at the neighbour distance | ratio 2.5–6 (argon example 4.01) | `Atomistic.NveDynamicsConservesEnergyWithTheTimeStepSquared` |
| homogeneous RVE | the constituent's tensor | 1e-8 | `PotentialHomogenization.AHomogeneousRveRecoversItsConstituent` |
| layered RVE | arithmetic and harmonic means | 1e-8 | `PotentialHomogenization.LayersGiveTheArithmeticAndHarmonicMeansExactly` |
| dilute sphere | Maxwell–Garnett (Rayleigh within 0.1 %); voxel staircase error falls with resolution | +1.5 % at 36³ | `PotentialHomogenization.ADiluteSphereConvergesToMaxwellGarnettWithResolution` |
| random spheres | Hashin–Shtrikman bounds; flux ≤ periodic, mixed ≤ linear | holds; Hill–Mandel 1e-14 | `PotentialHomogenization.RandomSpheresLieInsideTheBounds…` |
| untextured polycrystal | Voigt/Reuss bounds; self-consistent estimate | within 6 % | `PotentialHomogenization.AnUntexturedPolycrystal…`, `MultiscaleDerivations.AnUntexturedPolycrystal…` |
| porous alumina (20 %) | Maxwell–Garnett with alumina as the matrix (21.8 W/(m K)) | 21.51 ± 0.05 W/(m K); flux / linear apparent 2.1 / 21.9 (pores cut by the boundary) | `MultiscaleDerivations.RveStudyConvergesAndGivesAnUncertainty` |
| arrangement | the same porosity as layers: series and parallel exactly | 0.130 across, 24.0 along | `MultiscaleDerivations.TheArrangementNotTheFractionSetsTheConductivity` |
| porous dielectric (30 %) | Maxwell–Garnett ε = 6.46 | 6.434 ± 0.0015 | `MultiscaleExamples.PorousDielectricBecomesALayerOfAMetamaterial` |
| argon to a cantilever | Euler–Bernoulli with the derived E and ρ | 1e-3 | `MultiscaleExamples.ArgonCrystalToACantilever` |
| iterative coupling | a fixed point: the conductivity used equals the one derived at the resulting temperature | 4 iterations, 1e-6 | `MultiscaleServices.IterativeCouplingReachesAFixedPoint` |
| lineage | save and reload; stale after an RVE, constituent or value change | | `MultiscaleModel.*`, `MultiscaleServices.*` |
| clients | Python, MCP, the Material Scales panel | | `python/tests/test_multiscale_api.py`, `MaterialScalesPanelTest.*` |

## Not validated

Against measured microstructures or measured properties; any molecular or
electronic-structure calculation (none is integrated).
