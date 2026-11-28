# WORK-000022: Dynamic material state: states separate from definitions, J2 plasticity, damage and the damaged cantilever, viscoelasticity, phase change with latent heat, Preisach hysteresis, time-stepped evolution through the coupled engine, records, commits with lineage

**Status:** completed
**Type:** FEATURE
**Request:** requests/processing/2026-11-26-prompt-23-dynamic-material-sta-req-chat-20261126-021.md
**Requirements:** RES-DMS-001, RES-DMS-002, RES-DMS-003, RES-DMS-004, RES-DMS-005, RES-DMS-006, RES-DMS-007, RES-DMS-008, RES-DMS-009, RES-DMS-010, RES-DMS-011, RES-DMS-012, RES-DMS-013, RES-DMS-014
**Decision:** ADR-000225
**Evidence:** VER-000022
**Test-Suites:** core, app, cmake, layering, audits
**Test-Run:** 20261128T001259-980638

## Scope

Dynamic material state: states separate from definitions, J2 plasticity, damage and the damaged cantilever, viscoelasticity, phase change with latent heat, Preisach hysteresis, time-stepped evolution through the coupled engine, records, commits with lineage

## Implementation

- `src/core/materialstate/StateModel.{h,cpp}` (new, no physics): state variables with unit and class (reversible, irreversible, history), parameters, model cards, point states, fields with summaries, step convergence, state records and the project workspace (extension `material-state`, format `resonant.material-state`).
- `src/core/mechanics/Plasticity` (J2 with isotropic and kinematic hardening, radial return, consistent tangent, uniaxial-stress Newton driver, indeterminate bar), `Damage` (exponential and power laws, secant unloading, dissipation), `DamagedBeam` (Hermite cantilever accumulating damage, modes from the damaged stiffness, trapezoidal dissipation), `Viscoelasticity` (standard linear solid, exact update and dissipation, storage and loss moduli).
- `src/core/thermal/PhaseChange` (explicit enthalpy method, phase properties, stability limit with refusal, energy split, the Stefan coefficient).
- `src/core/electromagnetics/Hysteresis` (Preisach relays with uniform or gaussian density and a reversible part).
- `src/core/coupling/StateEvolution` (time-stepped evolution of a physical system: each step a coupled run continuing from the previous temperature field); `MultiphysicsParticipants.cpp` (the transient thermal participant accepts an initial temperature field); `MaterialStateModels` (the catalogue with the models that are not implemented, the runners and the run document); `MaterialStateAnalyses` + `FunctionalAnalyses.cpp` (the functional analysis `mechanics.damaged-cantilever`).
- `src/core/services/MaterialStateServices.cpp`, `Services.h`, `ProjectServices.cpp`, `ApplicationServices.cpp`: `MaterialStateService` (models, studies, runs and evolutions as jobs, results, records, commits through the multiscale lineage) and the capabilities.
- `proto/resonant/v1/materialstate/*.proto`, `src/automation_api/Handlers.cpp`: 12 RPCs; `python/resonant_api/mcp.py`: 12 MCP tools at read, modify and execute levels.
- `src/app/MaterialStateController.*`, `MaterialStatePanel.*`, `forms/MaterialStatePanel.ui`, `MainWindow.{h,cpp}`, `forms/MainWindow.ui`: the Material State dock.
- `tools/check_layering.py`: `materialstate` is domain-independent.
- `examples/materialstate/` a–f with README.

## Tests

- New core suites: `test_plasticity.cpp` (6), `test_damage.cpp` (5, with the damaged cantilever and the standard linear solid), `test_phase_change.cpp` (3), `test_hysteresis.cpp` (4), `test_state_evolution.cpp` (4), `test_material_state_model.cpp` (3), `test_material_state_services.cpp` (2), `test_material_state_examples.cpp` (4); fixture `MaterialStateFixture.h`.
- `python/tests/test_material_state_api.py` (4) against a real headless host; the schema guard `test_automation_schema.py` passes with the new contract.
- `tests/app/test_material_state_panel.cpp` (2) in the real main window.
- The t2 run is linked below as **Test-Run:**.

## Regression impact and documentation

- The thermal participant's new `initialTemperature` setting is optional; existing systems run unchanged (the coupled-engine suites are in t2). The new dock is tabified with the existing material panels.
- Documentation: `docs/requirements/material_state.md` (RES-DMS-001..014), ADR-000225, `docs/materials/DYNAMIC_MATERIAL_STATE.md`, `docs/physics/CONSTITUTIVE_MODELS.md`, `docs/physics/STATE_EVOLUTION.md`, `docs/physics/MATERIAL_STATE_VALIDATION.md`; updated `MATERIAL_STATE_ARCHITECTURE.md`, `COUPLED_ENGINE_ARCHITECTURE.md`, `ENERGY_MATTER_RESEARCH_ROADMAP.md`, the roadmap (M18), overview, architecture README, `docs/README.md`, `product.md`, both manifests, `DECISIONS.md`, `IMPLEMENTATION_STATUS.md`, `examples/README.md`, the README, `PROTOBUF_API.md` (regenerated), help `core-concepts/material-state.md` and `materials.md`, `DOCUMENTATION_COVERAGE.md` (regenerated).
- Deviations from the request, stated in ADR-000225 and `IMPLEMENTATION_STATUS.md`: small strain only; crack propagation, fatigue, finite-strain plasticity, hyperelasticity, nonlinear dielectrics, optics and acoustics, and reaction kinetics are catalogued as not implemented and refused; the coupled solvers carry temperature-dependent laws only (plastic and damage states run in their own studies); phase change is one-dimensional.

## Traceability

REQ-CHAT-20261126-021 -> RES-DMS-001, RES-DMS-002, RES-DMS-003, RES-DMS-004, RES-DMS-005, RES-DMS-006, RES-DMS-007, RES-DMS-008, RES-DMS-009, RES-DMS-010, RES-DMS-011, RES-DMS-012, RES-DMS-013, RES-DMS-014 -> WORK-000022 -> evidence (pending)

## Transitions

- 2026-11-28T04:40:31.507720+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T05:18:31.280407+00:00 active -> completed by agent-dev-20261126151713-76f632
