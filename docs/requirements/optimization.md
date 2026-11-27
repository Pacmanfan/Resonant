# Optimization, Inverse Design, Geometry Optimization and Automated Engineering

**Owner of:** `RES-OPT-001..025`
**Source:** REQ-CHAT-20261126-009 (Prompt 13, "Advanced optimization, inverse design, geometry optimization and automated engineering")
**Refines:** `product.md` RES-PRODUCT-004 (inverse field design) and RES-PRODUCT-010 (optimization); `acoustics.md` RES-ACOUSTIC-011
(inverse design for point targets, kept); `wave_manipulators.md` RES-MANIP-013 (geometry optimization interfaces, kept for the
Manipulators panel); `electromagnetics.md` RES-EM-023 (optimization interfaces); `mechanics.md` (vibration targeting, kept)
**Decisions:** ADR-000214
**Documents:** `docs/optimization/`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-OPT-001 — One physics-independent optimization framework

Problems, design variables, objectives, constraints, algorithms, runs,
candidates, histories, comparison, sensitivity, surrogates, robust and
multi-fidelity evaluation, linear inverse design and persistence live in
`src/core/optimization`, which includes no physics domain. The domains
expose themselves through adapters; the adapter registry is filled in
`src/core/coupling`.
*Verification:* `tools/check_layering.py` (layering suite); `OptimizationAdapters.CapabilitiesAreExplicitAndPromotionWritesOnlyTheBoundValues`.

### RES-OPT-002 — Adapters state their capabilities explicitly

An adapter bound to a design snapshot states the variables (bindings) it can
change with suggested bounds, the quantities it calculates with unit,
mathematical definition and the models that calculate them, its models with
name, version, fidelity, assumptions and availability, its constants and its
limitations. A problem is checked against them before anything runs: an
unknown binding, quantity or model, or a quantity the chosen model does not
calculate, is refused. No algorithm assumes that two domains expose the same
quantities.
*Verification:* `OptimizationDesign.ProblemsRoundTripAndAreCheckedAgainstTheAdapter`,
`OptimizationAdapters.CapabilitiesAreExplicitAndPromotionWritesOnlyTheBoundValues`.

### RES-OPT-003 — Design variables

Continuous (optionally stepped), integer and categorical variables with
bounds, initial value, unit, role, group, lock and activation; derived
variables computed by an expression from variables and adapter constants
(dependent dimensions, symmetric tapers, fractions of a wavelength). The
design space maps between the unit cube of the free variables and complete
designs, quantizes discrete values, detects values outside bounds or locks
and refuses cyclic derived variables.
*Verification:* `OptimizationDesign.*`, `OptimizationExamples.SidelobeReductionGivesATradeOffFront`.

### RES-OPT-004 — Objectives with stated definitions

Minimize, maximize, match a target, and satisfy an upper or lower threshold
of an adapter quantity, each normalized by a user scale in the quantity's
unit. Every quantity has a documented definition, unit and normalization
(`docs/optimization/OBJECTIVE_FUNCTIONS.md`); distributions, deviations,
efficiencies and energy outside a region are adapter quantities.
*Verification:* `OptimizationDesign.ObjectivesScoreAndConstraintsMeasureTheirViolation`.

### RES-OPT-005 — Several objectives and their trade-off

Pareto ranking with constraint domination, a Pareto-based method (NSGA-II),
the front of every run, a weighted sum only on request and priorities chosen
after the run. With more than one objective no candidate is named the best:
the run reports its front.
*Verification:* `OptimizationAlgorithms.Nsga2ApproximatesAKnownParetoFront`,
`EmOptimization.SidelobeMetricReproducesDolphChebyshevAndTheFrontTradesItAgainstDirectivity`,
`OptimizationExamples.SidelobeReductionGivesATradeOffFront`.

### RES-OPT-006 — Hard and soft constraints

Constraints on quantities or on expressions over design values, with a
relation, a limit, a tolerance, a category (geometric, physical,
manufacturing, computational) and hard or soft. A candidate that misses a
hard constraint, that the model could not evaluate, or whose values leave
their bounds is infeasible and says which constraint it misses and by how
much; soft constraints add a penalty. Hard constraints are never silently
violated.
*Verification:* `OptimizationAlgorithms.HardConstraintsDecideFeasibilityAndReportTheirViolation`,
`OptimizationExamples.MechanicalResonatorMeetsItsFrequencyWithinTheConstraints`.

### RES-OPT-007 — Modular algorithms chosen for expensive simulations

Grid sweeps, random and Latin-hypercube sampling, Nelder-Mead, bound-
constrained L-BFGS with finite-difference gradients, differential evolution,
NSGA-II and Bayesian optimization (Gaussian process with expected
improvement), each with its stated stopping rule, implemented without a
third-party optimization library. Methods for continuous variables refuse
discrete ones. A recommendation chooses methods by number and kind of
variables, objectives, constraints and evaluation cost.
*Verification:* `OptimizationAlgorithms.*`.

### RES-OPT-008 — Gradients and sensitivity

Finite-difference gradients only, labelled as approximations, checked
against one-sided differences with a warning where they disagree; local
sensitivities with normalized values, a ranking, a heat map and negligible
variables; one-variable response curves; Pearson and Spearman correlation
over sampled candidates. Interfaces leave room for exact gradients
(`Capabilities::providesGradients`).
*Verification:* `OptimizationSensitivity.DerivativesRankingsAndReliabilityAreReported`,
`OptimizationAlgorithms.LbfgsWithFiniteDifferencesSolvesSmoothBoundedProblems`.

### RES-OPT-009 — Inverse design of source configurations

A physics-independent complex least-squares solver for `A w ~ b` with row
weights, Tikhonov regularization and a power penalty, the minimum-norm
solution, bounded weights (a convex problem solved by an accelerated
projected gradient method) and sparse excitation (orthogonal matching
pursuit). The result states whether it is exact, an exact minimum-norm
solution of an underdetermined system, or an approximate one, with residual,
condition number, rank and notes. Acoustic (free field or reflectors; point,
line, plane, surface and volume targets sampled at half a wavelength),
mechanical (finite-element transfer of the actuators) and electromagnetic
(array factor toward directions, nulls) adapters provide the response
matrices; the solution is evaluated with the physical model as a candidate.
*Verification:* `OptimizationInverse.*`, `AcousticOptimization.InverseDesignAgreesWithTheAcousticSolverAndTheEvaluation`,
`MechanicalOptimization.ActuatorInverseDesignAgreesWithTheVibrationTargeting`,
`EmOptimization.ANullIsPlacedExactlyByTheLinearInverseAndCheckedByTheEvaluation`.

### RES-OPT-010 — Geometry optimization with regeneration and validation

Geometry parameters of scene objects are design variables. Each candidate
reconstructs the design from the snapshot, applies its parameters together,
checks the geometry kind's own rules, rebuilds the simulation representation
and runs the model. An invalid geometry is an invalid design, never
evaluated; results are reused only when the full dependency state is
identical.
*Verification:* `AcousticOptimization.InvalidGeometryIsDetectedAndNeverEvaluated`,
`OptimizationEngine.IdenticalDependencyStatesAreReusedOthersAreNot`,
`MechanicalOptimization.TuningTheBeamLengthMatchesEulerBernoulli`.

### RES-OPT-011 — Topology optimization proof of concept

Minimum compliance of a 2D plane-stress region by SIMP with a density filter
and optimality-criteria updates on two benchmarks, with a filter radius in
metres for mesh independence, a cleanup (threshold, island removal,
re-analysis of the binary design) and an extruded closed mesh. The result
says it is not a finished physical design.
*Verification:* `OptimizationTopology.*`, `OptimizationUiTest.TheTopologyProofOfConceptRunsInTheBackground`.

### RES-OPT-012 — Shape-based passive focusing judged by a physical model

Reflector, lens and other manipulator geometry is optimized for focus
quantities computed by a model of the actual geometry (Kirchhoff reflector
model, or the full-wave solver), never by the shape's label. The focal
line quantities report where the field actually peaks.
*Verification:* `AcousticOptimization.ReflectorGeometryFocusesWhereTheMirrorEquationSays`,
`OptimizationExamples.AcousticReflectorCandidatesPeakNearTheMirrorEquation`.

### RES-OPT-013 — Materials: library selection or labelled hypothetical values

A material variable chooses among the library's materials of the domain.
Scaling a material property is a HYPOTHETICAL material: it gets its own id,
a theoretical provenance and a note in every evaluation; the library's
materials are left unchanged.
*Verification:* `MechanicalOptimization.HypotheticalMaterialsAreLabelledAndOriginalsKept`,
`OptimizationExamples.MechanicalResonatorMeetsItsFrequencyWithinTheConstraints`.

### RES-OPT-014 — Several model fidelities, never merged

Analytical, reduced, high-fidelity and experimental levels; every candidate
records the model that evaluated it. Candidates can be re-evaluated with
another model as new candidates linked to their parent; values of different
models are shown side by side, never combined or ranked together. A design is
validated only when evaluated with the problem's validation model or a model
of high fidelity.
*Verification:* `OptimizationFidelity.ReevaluationCreatesLinkedCandidatesAndNeverMergesModels`,
`AcousticOptimization.FullWaveReevaluationValidatesTheFocusedDesign`,
`OptimizationExamples.MechanicalResonatorMeetsItsFrequencyWithinTheConstraints`.

### RES-OPT-015 — Surrogate models

Quadratic response surfaces, radial-basis interpolation and Gaussian
processes trained on candidates, with their inputs, output, training points,
training and leave-one-out errors and version; active learning picks the
least certain points. A prediction is never a simulation result: the
Bayesian method records it in the candidate's origin and evaluates every
chosen design with the physical model.
*Verification:* `OptimizationSurrogates.ModelsReproduceKnownFunctionsAndReportTheirErrors`,
`OptimizationAlgorithms.BayesianOptimizationNeedsFewEvaluations`.

### RES-OPT-016 — Robust optimization with stated distributions

Uncertain parameters (any binding the adapter offers) with a uniform
tolerance or a normal distribution, absolute or relative, and a required
source; stratified common samples for every design; the mean, mean plus k
standard deviations or worst case as the robust score; the probability of
meeting each hard constraint. Nominal and robust results are distinguished.
*Verification:* `OptimizationRobust.StatisticsMatchKnownDistributions`,
`OptimizationExamples.RobustDesignTradesNominalGainForTheWorstCaseOverTheBand`.

### RES-OPT-017 — Design-space exploration

One- and multi-variable sweeps, grids, random and Latin-hypercube designs of
experiments, candidate filtering and ranking, Pareto views, response curves.
*Verification:* `OptimizationAlgorithms.SamplingMethodsCoverTheSpaceAsDefined`.

### RES-OPT-018 — Optimization jobs

Runs are background jobs with progress and live candidates, a limit of
parallel evaluations (half the hardware threads by default), a time limit
and a memory budget per evaluation, cancellation, periodic checkpoints and
restart that replays the recorded candidates without physics and reaches the
uninterrupted result. Each evaluation works on its own reconstruction of the
design snapshot; results do not depend on thread timing. One optimization job
runs at a time.
*Verification:* `OptimizationEngine.*`, `OptimizationUiTest.StoppingKeepsACheckpointAndResumingFinishesTheRun`.

### RES-OPT-019 — Candidates, comparison, promotion and lineage

Every candidate keeps id, parents, origin, design values, model record,
quantities, objective values, constraint results, feasibility, robust
statistics and whether it was reused. Two candidates are compared side by
side with their model differences. A candidate becomes part of the design
only by promotion: one undoable step, recorded as a revision with its lineage,
its validation state and the values it replaced; a candidate evaluated on a
design that has changed is not promoted without the user's consent. A run
never changes the design.
*Verification:* `OptimizationUiTest.ARunIsABackgroundJobAndTheDesignIsUnchangedUntilACandidateIsPromoted`,
`AcousticOptimization.PhaseFocusingReachesTheCoherentSumAndThePhaseFocus`, `OptimizationEngine.RunRecordsRoundTripWithProvenance`.

### RES-OPT-020 — Domain adapters

Acoustic: emitters, arrays (spacing, count, per-element amplitude, phase,
delay, activation), manipulator geometry, position and material, frequency
and air temperature; free-field, Kirchhoff-reflector and full-wave models.
Mechanical: structure geometry, thickness, supports, material and
hypothetical material scalings, loss factor, harmonic frequency, actuator
excitation and position; analytical beam and plate, coarse and standard
finite-element models. Electromagnetic: array frequency, spacing, count and
per-element excitation, antenna geometry, frequency and reference impedance;
array-factor, exact-dipole-field and induced-EMF models. Each states its
limitations. Templates exist for common tasks of each domain.
*Verification:* `AcousticOptimization.*`, `MechanicalOptimization.*`, `EmOptimization.*`.

### RES-OPT-021 — Preparation for Python workflows

Problems ("resonant.optimization.problem") and runs
("resonant.optimization.run", also the checkpoint format) are versioned JSON
documents; candidates export as CSV. The C++ core needs no Python. A reader
using only the standard library (NumPy and matplotlib optional) is an
example. Python bindings are the subject of REQ-CHAT-20261126-010.
*Verification:* `OptimizationEngine.RunRecordsRoundTripWithProvenance`,
`OptimizationUiTest.ExportWritesTheCandidateRecordAndItsGeometryMeshes`.

### RES-OPT-022 — Geometry and manufacturing export

A candidate exports its record (lineage, values, model) and the meshes of its
changed geometry as STL and OBJ with integrity checks (closed, volume,
non-manifold edges, overhang); printability checks compare wall thickness,
feature size and build volume with limits. The export says that the checks
are not a manufacturing approval. STEP is not implemented.
*Verification:* `OptimizationManufacturing.MeshChecksFindClosedSolidsHolesAndOverhangs`,
`OptimizationUiTest.ExportWritesTheCandidateRecordAndItsGeometryMeshes`.

### RES-OPT-023 — Optimization Studio

A dock with problem definition from templates, variable, objective,
constraint and uncertainty editors, method, model and validation model,
resource limits, run controls with live progress and convergence, the
candidate browser with details, comparison, re-evaluation, promotion and
export, the Pareto view with priorities, sensitivity, inverse design,
surrogates and the topology proof of concept; the selected model and its
fidelity are shown; `--run-optimization` runs a problem from the command line.
*Verification:* `OptimizationUiTest.*`.

### RES-OPT-024 — Example projects

Seven reproducible projects in `examples/optimization/` (acoustic focal array,
acoustic reflector, mechanical vibration suppression, mechanical resonator,
electromagnetic beam steering, sidelobe reduction, robust design), each
holding its problem, each run by the tests with the real physics.
*Verification:* `OptimizationExamples.*`, `Examples.EveryExampleProjectLoads`.

### RES-OPT-025 — Validation and reproducibility

Algorithms against problems with known optima, inverse design against closed
forms and the domains' own solvers, geometry against the mirror equation and
Euler-Bernoulli theory, steering against phase steering, sidelobes against
Dolph-Chebyshev, robust statistics against known distributions; the same seed
gives the same candidates on every platform; the original project is never
changed by a run (`docs/optimization/OPTIMIZATION_VALIDATION.md`).
*Verification:* the suites above; `OptimizationEngine.ParallelEvaluationGivesTheSameCandidatesAsSequential`.
