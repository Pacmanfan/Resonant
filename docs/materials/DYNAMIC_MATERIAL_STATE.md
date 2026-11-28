# Dynamic Material State

**Status:** current (REQ-CHAT-20261126-021, RES-DMS-001..014, ADR-000225).
Constitutive models with state, time-stepped evolutions of physical systems,
state records in the project, and commits of an evolved state to the library
with its lineage. The models' equations are in
[CONSTITUTIVE_MODELS.md](../physics/CONSTITUTIVE_MODELS.md), the evolution in
[STATE_EVOLUTION.md](../physics/STATE_EVOLUTION.md), the checks in
[MATERIAL_STATE_VALIDATION.md](../physics/MATERIAL_STATE_VALIDATION.md).

## Definitions and states

A library material is a **definition**: values with provenance, validity and,
for derived values, a derivation record. It never holds an evolving state.
A **state** belongs to a constitutive model: the model declares its
variables, each with a unit and a class.

| Class | Meaning | Examples |
|---|---|---|
| reversible | returns when the load is removed | elastic strain, temperature, reversible magnetization |
| irreversible | only grows | equivalent plastic strain, damage, dissipated energy |
| history | records the path | back stress, the largest strain reached, the Preisach relay states |

```text
src/core/materialstate          (domain-independent: no physics)
  StateVariable, Parameter, ModelCard        what a model is and what it excludes
  PointState, StateField, summary()          values per material point, over a region
  StateRecord, Workspace                     a stored run; extension "material-state"

src/core/mechanics      Plasticity (J2), Damage, DamagedBeam, Viscoelasticity (SLS)
src/core/thermal        PhaseChange (enthalpy method)
src/core/electromagnetics  Hysteresis (Preisach)
src/core/coupling       MaterialStateModels (catalogue and runners), StateEvolution,
                        MaterialStateAnalyses (the damaged cantilever as a functional analysis)
src/core/services       MaterialStateService (studies, runs, evolutions, records, commits)
```

The constitutive models stay in their domains. `materialstate` knows no
domain (rule in `tools/check_layering.py`), and only coupling knows them all.

## The catalogue

Every model has a card: variables, parameters with defaults, assumptions,
excluded effects, validity, method, reference and loading presets.

| Model | Domain | State |
|---|---|---|
| `mechanics.j2.point` | mechanical | plastic strain, back stress, equivalent plastic strain |
| `mechanics.j2.bar` | mechanical | the same, at each of two segments of an indeterminate bar |
| `mechanics.damage.point` | mechanical | damage, the largest strain reached |
| `mechanics.damaged-cantilever` | mechanical | damage per element, the beam's modes |
| `mechanics.viscoelastic.sls` | mechanical | the branch's viscous strain |
| `thermal.phase-change.slab` | thermal | enthalpy, temperature and liquid fraction per cell |
| `em.preisach` | electromagnetic | relay states (return-point memory), magnetization |
| `coupled.evolution` | multiphysics | the temperature field, law values, stress, thickness |

Not implemented, and listed as such: crack propagation, fatigue, finite-strain
plasticity, hyperelasticity, nonlinear dielectrics, nonlinear optics,
nonlinear acoustics and reaction kinetics. A study naming one is refused
with "not implemented".

## Runs and records

A study is `{"model", "parameters", "loading"}` (or `"system"` for an
evolution) and is stored in the project. A run is a job and gives a document
(format `resonant.material-state-run`, or `resonant.material-state-evolution`
for an evolution) with the following parts:

- the history, one entry per step;
- the response curves and the spatial profiles at snapshots;
- the final state;
- the energy account: work, stored, dissipated and the residual;
- convergence (Newton iterations and residuals);
- the values that can be committed;
- reliability: whether the run is reliable, and if not, why. For example,
  an exponential softening law localizes in a structure.

**Save record** stores the document as a `StateRecord` in the project's
`material-state` extension, as one undoable step. A saved record survives
save and reload.

## Commit: an evolved state as a material value

A record's committable values, for example a damaged secant modulus
`(1 − D) E`, are written to a library material:

```text
record rec-damage (mechanics.damage.point)
   │ CommitMaterialState(record, material)
   ▼
material.youngsModulus = (1 − D) E     provenance computed, derivation drv-…
multiscale model: representation state-rec-damage (continuum-material, the record's final state)
                  derivation drv-… (method state.mechanics.damage.point, inputs = the run's settings, parent = the previous derivation)
                  relationships  material → representation (derived-from), derivation → representation (computed-from)
```

The commit uses the multiscale lineage, so its consequences follow from it
([MATERIAL_LINEAGE.md](MATERIAL_LINEAGE.md)):

- The lineage of the committed value leads to the state record.
- Effective materials derived from the changed material (an RVE with it as
  a phase) become stale, with the reason.
- One undo reverts the commit.

## Interfaces

- `MaterialStateService` with 12 RPCs (`proto/resonant/v1/materialstate`),
  the Python client and MCP tools: reads are read-level, studies, records
  and commits are modify-level, runs and evolutions are execute-level.
- The **Material State** panel shows the following:
  - studies and model cards, including what is not implemented;
  - a run with its curves, history table and profile per snapshot;
  - Save record, and Commit to a chosen material.
- Examples are in `examples/materialstate/` (a–f), and help is the topic
  `concepts.material-state`.
