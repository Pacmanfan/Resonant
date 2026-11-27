# Optimization Validation

What the optimization framework is checked against, and the tests that do it
(core suite: `tests/core/test_optimization.cpp`, `test_optimization_domains.cpp`,
`test_optimization_examples.cpp`; GUI suite: `tests/app/test_optimization_ui.cpp`).

## Framework (`test_optimization.cpp`)

| Check | Reference | Test |
|---|---|---|
| Nelder-Mead on Rosenbrock | minimum (1, 1), within 2e-3 / 4e-3 | `OptimizationAlgorithms.NelderMeadSolvesRosenbrock` |
| L-BFGS-B on a shifted sphere; a linear function on a box; a kink | the known minimum to 1e-4; the corner; an "unreliable gradient" note | `OptimizationAlgorithms.LbfgsWithFiniteDifferencesSolvesSmoothBoundedProblems` |
| Differential evolution on Rastrigin | the global minimum (0, 0) within 0.02, where Nelder-Mead stays in a local one | `OptimizationAlgorithms.DifferentialEvolutionFindsTheGlobalMinimumOfAMultimodalFunction` |
| NSGA-II on ZDT1 | mean distance to f2 = 1 - sqrt(f1) below 0.05; the front non-dominated; no single best | `OptimizationAlgorithms.Nsga2ApproximatesAKnownParetoFront` |
| Bayesian optimization | the minimum within 30 evaluations; predictions labelled | `OptimizationAlgorithms.BayesianOptimizationNeedsFewEvaluations` |
| Sweeps and Latin hypercubes | the grid's points; one point per stratum; the same seed, the same samples | `OptimizationAlgorithms.SamplingMethodsCoverTheSpaceAsDefined` |
| Discrete variables | refused by Nelder-Mead; DE finds the nearest integer | `OptimizationAlgorithms.DiscreteVariablesNeedASuitableMethod` |
| Hard constraints | min x^2 + y^2, x + y >= 1: (0.5, 0.5); violations reported; an invalid design infeasible | `OptimizationAlgorithms.HardConstraintsDecideFeasibilityAndReportTheirViolation` |
| Caching | identical dependency states reused, a changed snapshot never | `OptimizationEngine.IdenticalDependencyStatesAreReusedOthersAreNot` |
| Parallel evaluation | the same candidates as sequential | `OptimizationEngine.ParallelEvaluationGivesTheSameCandidatesAsSequential` |
| Cancellation | the job ends cancelled; no partial candidate | `OptimizationEngine.CancellationStopsTheJobAndStoresOnlyCompleteCandidates` |
| Checkpoint and restart | through a file; the finished run equals the uninterrupted one, fewer evaluations repeated | `OptimizationEngine.CheckpointAndRestartReproduceTheUninterruptedRun` |
| Provenance | run records round-trip with model, adapter and snapshot | `OptimizationEngine.RunRecordsRoundTripWithProvenance` |
| Linear inverse | exact square solutions, normal equations, Tikhonov closed form, pseudo-inverse, KKT of bounded weights, planted sparse support | `OptimizationInverse.*` |
| Surrogates | a quadratic reproduced; GP interpolation and uncertainty; RBF; expected improvement | `OptimizationSurrogates.ModelsReproduceKnownFunctionsAndReportTheirErrors` |
| Sensitivity | d(3 x0 + 0.001 x1) = (3, 0.001); a kink flagged; correlations | `OptimizationSensitivity.DerivativesRankingsAndReliabilityAreReported` |
| Robust statistics | uniform: mean, sigma = w / sqrt 3, max; normal sigma; constraint probability 0.5 | `OptimizationRobust.StatisticsMatchKnownDistributions` |
| Multi-fidelity | linked re-evaluations, a known shift, rank agreement 1, no mixing | `OptimizationFidelity.ReevaluationCreatesLinkedCandidatesAndNeverMergesModels` |
| Persistence | the workspace round-trips and stays out of the design snapshot | `OptimizationPersistence.WorkspacesRoundTripAndStayOutOfTheDesignSnapshot` |
| Mesh checks | a closed box of the right volume; an open plane; printability failures; overhangs; STL / OBJ | `OptimizationManufacturing.MeshChecksFindClosedSolidsHolesAndOverhangs` |
| Topology | beam theory within 6 %; compliance, volume, symmetry, closed export; mesh independence within 10 % | `OptimizationTopology.*` |

## Domains (`test_optimization_domains.cpp`)

| Check | Reference | Test |
|---|---|---|
| Acoustic phase focus | the coherent sum sum A_i / r_i to 1e-5; phases k r_i to 2e-3 rad; the project unchanged | `AcousticOptimization.PhaseFocusingReachesTheCoherentSumAndThePhaseFocus` |
| Acoustic inverse design | the acoustic module's own `solveInverse` to 1e-9; the evaluated error equals the residual | `AcousticOptimization.InverseDesignAgreesWithTheAcousticSolverAndTheEvaluation` |
| Reflector geometry | the mirror equation within 5 %; focal gain above 5 | `AcousticOptimization.ReflectorGeometryFocusesWhereTheMirrorEquationSays` |
| Invalid geometry | an aperture above twice the curvature radius never evaluated; unknown bindings refused | `AcousticOptimization.InvalidGeometryIsDetectedAndNeverEvaluated` |
| Full-wave re-evaluation | validated, linked; the focused design still better | `AcousticOptimization.FullWaveReevaluationValidatesTheFocusedDesign` |
| Analytical against finite-element beams | f1 within 1 %, f2 within 2 %, mass | `MechanicalOptimization.AnalyticalAndFiniteElementBeamsAgree` |
| Frequency tuning | L0 / sqrt(1.2) to 1e-4; finite elements within 1 % | `MechanicalOptimization.TuningTheBeamLengthMatchesEulerBernoulli` |
| Vibration inverse design | `solveTargets` to 1e-8; the optimizer within the same quality | `MechanicalOptimization.ActuatorInverseDesignAgreesWithTheVibrationTargeting` |
| Hypothetical materials | f ~ sqrt(E); labelled; the library unchanged | `MechanicalOptimization.HypotheticalMaterialsAreLabelledAndOriginalsKept` |
| Beam steering | phase steering's directivity within 0.02 dB; phase steps k d sin(theta) | `EmOptimization.SteeringByPhasesMatchesPhaseSteering` |
| Sidelobes | uniform 8 elements -12.8 dB; Dolph-Chebyshev 30 dB reproduced within 0.6 dB; a front below -20 dB | `EmOptimization.SidelobeMetricReproducesDolphChebyshevAndTheFrontTradesItAgainstDirectivity` |
| Nulls | exact minimum-norm solution, more than 100 dB down | `EmOptimization.ANullIsPlacedExactlyByTheLinearInverseAndCheckedByTheEvaluation` |
| Dipole resonance | `resonantDipoleLength` to 1e-4 | `EmOptimization.DipoleLengthForZeroReactanceMatchesTheResonantLength` |
| Exact fields against the array factor | 0.3 dB in directivity at 100 wavelengths; refused for isotropic elements | `EmOptimization.ExactDipoleFieldsAgreeWithTheArrayFactorFarAway` |
| Capabilities and promotion | definitions everywhere; `apply` changes only the bound object; snapshot keys | `OptimizationAdapters.CapabilitiesAreExplicitAndPromotionWritesOnlyTheBoundValues` |

## Examples (`test_optimization_examples.cpp`)

Each example project in `examples/optimization/` must match its builder and
reach what it claims with the real adapters: A the coherent sum (1e-3), B the
mirror equation (5 %), C vibration reduced 20-fold, D 25 Hz within 1 % on the
analytical model and 2 % on the finite elements within the mass and
thickness constraints, E at least the phase-steering directivity, F a
trade-off front below -20 dB, G robust and nominal designs each best in their
own measure.

## GUI (`test_optimization_ui.cpp`)

The dock and menu; templates of every domain; a background run with live
candidates; the design unchanged until promotion; promotion undoable and
recorded; stop, checkpoint file, resume and replay; inverse design,
sensitivity and full-wave re-evaluation from the Studio; candidate and run
export; the topology job.

## Known limits of the validation

* No measured data validates any optimized design; validation is analytical
  and between models.
* The 2D full-wave re-evaluation checks the trend (focusing), not absolute
  agreement with 3D models.
* Timing-dependent behaviour (time limits) is not covered by the
  reproducibility tests.
