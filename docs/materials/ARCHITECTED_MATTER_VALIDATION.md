# Architected Matter Validation

**Status:** current (REQ-CHAT-20261127-001, RES-FAM-020). Every benchmark below is an
automated test. The architectures in `examples/matter` use representative handbook
values that are labelled as such; they verify the methods and are not measurements.

## Data model

| Requirement (section 19) | Test |
|---|---|
| a homogeneous material, a multilayer, a graded material, a multimaterial 3D structure | `MatterArchitecture.EveryExampleRoundTripsWithoutLossAndValidates` (examples a, b, d, f) |
| spatially varying properties, orientation, interfaces, provenance | `MatterArchitecture.*`, `MatterFields.*`, `MatterAnalyses.OrientationRotatesTheProperties` |
| serialized and reloaded without loss | the round trip of every example, through the project file |

## Physical benchmarks against closed forms

| Benchmark | Reference | Tolerance | Test |
|---|---|---|---|
| bilayer modulus, neutral axis and bending stiffness | rule of mixtures (equal Poisson's ratios), parallel-axis theorem | 1e-10 | `MatterMechanics.BilayerStiffnessNeutralAxisAndVoigtModulus` |
| cross-ply [0/90]s moduli | the cross-ply formula from Q11, Q22, Q12 | 1e-12 | `MatterMechanics.CrossPlyModuliAndAnInvalidPly` |
| thermal curvature of a bimetal | Timoshenko (1925) | 1e-10 | `MatterMechanics.ThermalCurvatureIsTheTimoshenkoBimetal` |
| power-law graded plate | the moments of E(z) | 1e-5 | `MatterMechanics.PowerLawGradedPlateAgainstItsIntegrals` |
| cantilever and plate modes | Euler–Bernoulli roots, the Navier plate | 1e-9 | `MatterMechanics.CantileverAndPlateModes` |
| coated against bare cantilever | the parallel-axis theorem over every layer | 1e-9 | `MatterAnalyses.TheCoatedCantileverIsNotTheBareOne` |
| unimorph short- and open-circuit frequencies | `f_oc / f_sc = sqrt(1 + k^2)` | 1e-12 | `MatterMechanics.UnimorphFrequenciesCouplingAndEnergyBalance` |
| the unimorph's energy balance | input = damping + electrical | 1e-12 | the same |
| optimum load, weak coupling | `1 / (omega C)` | 1 % | `MatterMechanics.OptimumLoadAndWeakCoupling` |
| electrode coverage | `phi'(0.8 L) / phi'(L) = 0.96` | 1e-12 | `MatterAnalyses.TheEnergyConverterHarvestsWithABalancedLedger` |
| poling of a bimorph | equal poling cancels in parallel | 1e-9 | `MatterMechanics.PolingElectrodesAndDamping` |
| film orientation (PVDF) | `theta` scales by `d32 / d31` | 1e-9 | `MatterAnalyses.OrientationRotatesTheProperties` |
| series thermal resistance with a contact resistance | `sum h / k + R` and the jump `q R` | 1e-10 | `ThermalContact.SeriesResistanceAndTheTemperatureJump`, `MatterAnalyses.TheThermalBarrierInSeriesWithItsContactResistance` |
| the same in the coupled engine | the series formula | 1e-6 | `MatterCoupling.TheBarrierStackRunsInTheCoupledEngine` |
| linearly graded conductivity | `(k2 - k1) / ln(k2 / k1)` | 1e-12 | `MatterFields.LinearGradingCombinesInSeriesByTheLogarithmicMean` |
| graded composition in series and in the engine | Simpson's rule of `1 / k(s)` | 1e-4, 2e-3 (40 sublayers) | `MatterAnalyses.AGradedLayer...`, `MatterCoupling.GradedLayersBecomeSublayersWithLaws` |
| Hashin–Shtrikman, Maxwell Garnett, Bruggeman, Gibson–Ashby | their closed forms; nested bounds | 1e-12 | `MatterMixing.*` |
| Salisbury screen | the quarter-wave frequency `c / (4 d sqrt(eps_r))` | 3 % | `MatterAnalyses.TheSalisburyScreenAbsorbsAtItsQuarterWave` |

## Architecture changes behaviour

The tests check that each change moves the prediction:
- a bonded PZT plate shifts the first mode by more than 5 %, and a thin film by
  less;
- a graded layer and two discrete layers of the same constituents differ in
  thermal resistance by more than 5 %;
- turning the PVDF film by 90° cuts the coupling to `d32 / d31`.

## Optimization, manufacturing, exploration

| Check | Test |
|---|---|
| a thickness moves the frequency the expected way | `MatterOptimization.AThicknessVariableMovesTheFrequencyTheExpectedWay` |
| a material choice changes the result | `MatterOptimization.AMaterialChoiceChangesTheResult` |
| unmanufacturable architectures are constrained or rejected | `MatterOptimization.ArchitecturesThatCannotBeMadeAreIdentified` |
| competing objectives give a Pareto set (power against stiffness) | `MatterOptimization.CompetingObjectivesGiveAParetoSet` |
| the film delaminates, the bonded plate does not, a hot cure is flagged | `MatterManufacturing.*` |
| findings apart from hypotheses; neutral surface, electrodes, delamination | `MatterExploration.*` |
| a contact resistance calibrated from measurements and validated on another set | `MatterCoupling.InterfaceResistanceCalibratedFromMeasurements` |

## User acceptance

`MatterPanelTest.*` drives the workflow in the real main window:
- the regions, stack and interfaces of a multimaterial structure;
- an analysis run and plotted, and a refused one with its reason;
- manufacturability and exploration;
- an alternative duplicated, compared and undone;
- a stack sent to the coupled engine.

The project round trip covers reopening without loss.

## Performance

| Operation | Measured time |
|---|---|
| a stack analysis | about 0.1–1 ms |
| exploring an architecture (about twenty analyses) | about 10 ms |
| an NSGA-II run of 160 evaluations | about 0.1 s |

These were measured by the tests on the debug build of the reference machine.
Graded layers cost linearly in their sublayers (40 by default; 200 for
conduction). A contact-resistance calibration takes about 10 ms. The analyses
are closed forms, so architectures with tens of layers stay interactive. The cost
lies in the coupled engine (its cells) and in optimization (the evaluations).
