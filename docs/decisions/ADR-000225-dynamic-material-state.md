# ADR-000225: Dynamic material state: states separate from definitions, constitutive models in their domains, a trial-commit update contract, time-stepped evolution through the coupled engine

**Status:** ACCEPTED
**Date:** 2026-11-28
**Type:** architecture
**Requirements:** `docs/requirements/material_state.md` RES-DMS-001..014
**Request:** REQ-CHAT-20261126-021
**Builds on:** ADR-000220 (coupled engine and its material state), ADR-000224 (lineage), ADR-000222 (resonance)

## Context

The coupled engine evaluated temperature laws of properties and kept a
temperature history, but had no internal variables (plastic strain, damage,
phase, magnetization), no history-dependent constitutive model and no time
stepping across coupled runs.

## Decisions

1. **States are not materials.** `src/core/materialstate` (no physics)
   defines state variables (unit, class: reversible, irreversible, history),
   material-point states, state fields over a region's cells, histories and
   records, model cards and the update contract. The library never stores a
   state; a commit writes chosen values to a material as derived values with
   a derivation record (ADR-000224).
2. **Constitutive models live in their domains:** J2 plasticity, scalar
   damage, the damaged cantilever, viscoelasticity (mechanics); phase change
   by the enthalpy method (thermal); Preisach hysteresis (electromagnetics).
   Each returns a trial state with its energies and convergence; the caller
   commits.
3. **Time-stepped evolution through the coupled engine**
   (`coupling/StateEvolution`): each step is a coupled run whose transient
   thermal participant starts from the previous step's temperature field
   (new setting `initialTemperature`); the material laws are re-evaluated
   every step; the per-step ledgers sum to the run's energy account.
4. **Records** of runs are stored in the project extension `material-state`
   (studies and records), one undoable step through `MaterialStateService`.
5. **Not implemented, and said so:** crack propagation, fatigue, finite-strain
   plasticity, hyperelasticity, nonlinear optics and acoustics, concurrent
   coupling of internal variables into the full-wave and finite-element
   solvers.

## Consequences

- One temperature-step per coupled run: within a step the coupling converges
  by the engine's iterative scheme; between steps the coupling is explicit in
  the state (first order in time).
- The damaged cantilever is an Euler–Bernoulli beam model; damage in the 3D
  finite-element solvers is planned.
- The catalogue (`coupling/MaterialStateModels`) gives every model a card;
  a study of a model that is not implemented is refused, never approximated.
- `materialstate` is domain-independent (`tools/check_layering.py`); only
  coupling knows the domains' models.

## Deviations from the request

- Small strain only; no finite-strain plasticity or hyperelasticity.
- No crack propagation, fatigue, nonlinear dielectrics, nonlinear optics,
  nonlinear acoustics or reaction kinetics (catalogued as not implemented).
- The coupled solvers carry temperature-dependent laws only; plastic and
  damage states run in their own studies, and a committed value (with its
  lineage) is how they reach a coupled run.
- Phase change is one-dimensional (a slab), and hysteresis scalar and
  rate-independent.
