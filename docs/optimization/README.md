# Optimization and Inverse Design

Resonant searches, simulates, compares and validates candidate designs: you
state what you want (objectives), what may change (design variables) and what
must hold (constraints), choose the physical model that judges candidates,
and Resonant proposes, evaluates and keeps every candidate with its
provenance. Nothing changes your design until you promote a candidate.

Normative requirements: `docs/requirements/optimization.md` (RES-OPT-001..025).
Decision: `docs/decisions/ADR-000214-optimization-framework.md`.

| Document | Content |
|---|---|
| [OPTIMIZATION_ARCHITECTURE.md](OPTIMIZATION_ARCHITECTURE.md) | modules, the adapter boundary, snapshots, jobs, checkpoints, storage, the exchange format |
| [DESIGN_VARIABLES.md](DESIGN_VARIABLES.md) | variable kinds, bounds, locks, groups, derived variables and expressions, bindings |
| [OBJECTIVE_FUNCTIONS.md](OBJECTIVE_FUNCTIONS.md) | objective senses and scores, every quantity of every adapter with its definition |
| [CONSTRAINT_SYSTEM.md](CONSTRAINT_SYSTEM.md) | hard and soft constraints, violations, feasibility, the merit function |
| [OPTIMIZATION_ALGORITHMS.md](OPTIMIZATION_ALGORITHMS.md) | the methods, their assumptions and stopping rules, the recommendation, what is not implemented |
| [INVERSE_DESIGN.md](INVERSE_DESIGN.md) | linear least-squares source design: regularization, bounds, sparsity, classification |
| [GEOMETRY_OPTIMIZATION.md](GEOMETRY_OPTIMIZATION.md) | parametric geometry, regeneration and validation, passive focusing, export and manufacturing checks |
| [TOPOLOGY_OPTIMIZATION.md](TOPOLOGY_OPTIMIZATION.md) | the SIMP proof of concept and its limits |
| [MULTI_FIDELITY_OPTIMIZATION.md](MULTI_FIDELITY_OPTIMIZATION.md) | model levels, re-evaluation, validation |
| [SURROGATE_MODELS.md](SURROGATE_MODELS.md) | response surfaces, Gaussian processes, active learning, Bayesian optimization |
| [ROBUST_OPTIMIZATION.md](ROBUST_OPTIMIZATION.md) | uncertain parameters, sampling, robust measures, nominal versus robust |
| [DOMAIN_OPTIMIZATION_ADAPTERS.md](DOMAIN_OPTIMIZATION_ADAPTERS.md) | acoustic, mechanical and electromagnetic adapters, their models, variables, templates and limits |
| [OPTIMIZATION_VALIDATION.md](OPTIMIZATION_VALIDATION.md) | the benchmarks, analytical comparisons and examples, with the tests that run them |

The examples are in `examples/optimization/` (see `examples/README.md`); the
Optimization Studio is the "Optimization Studio" dock (menu Optimization).
