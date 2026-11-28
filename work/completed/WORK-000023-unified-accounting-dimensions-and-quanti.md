# WORK-000023: Unified accounting: dimensions and quantity families, ledger classified by system boundaries, momentum and forces, conservation and consistency checks, adapters on the coupled engine, lumped networks, layered radiation force and material state

**Status:** completed
**Type:** FEATURE
**Request:** requests/processing/2026-11-26-prompt-24-unified-energy-momen-req-chat-20261126-022.md
**Requirements:** RES-ACC-001, RES-ACC-002, RES-ACC-003, RES-ACC-004, RES-ACC-005, RES-ACC-006, RES-ACC-007, RES-ACC-008, RES-ACC-009, RES-ACC-010, RES-ACC-011, RES-ACC-012, RES-ACC-013, RES-ACC-014, RES-ACC-015, RES-ACC-016
**Decision:** ADR-000226
**Evidence:** VER-000023
**Test-Suites:** core, app, cmake, layering, audits
**Test-Run:** 20261128T011130-1098689

## Scope

Unified accounting: dimensions and quantity families, ledger classified by system boundaries, momentum and forces, conservation and consistency checks, adapters on the coupled engine, lumped networks, layered radiation force and material state

## Implementation

- `src/core/foundation/Dimensions.{h,cpp}` (new): unit expressions to SI dimension exponents and factors, checked conversions, affine scales refused.
- `src/core/accounting/` (new, domain-independent): `Quantities` (families with dimension and conserved flag, typed quantities with context), `AccountingLedger` (entities, holdings, exchanges, generations, force records with pairs, missing contributions, stated numerical errors, scale contracts, checks; JSON), `ControlVolume` (systems, classification by membership), `Conservation` (registry of conservation laws and consistency relations, evaluation with statuses, breakdown, flows, forces, histories).
- Domain outputs added in their domains: `oscillators/LumpedNetwork` (optional element flows: each element's work and impulse on its ends, applied work and impulse, velocities, with the integrator's own stages; the absolute energy residual), `electromagnetics/RadiationForce` (new: vacuum Maxwell stress at the control surfaces, Lorentz volume force, local absorption), `electromagnetics/Hysteresis` (the hysteron loop loss), `thermal/PhaseChange` (entropy change, inflow and production).
- Model corrections found by the accounting: `mechanics/Damage` dissipation integrated trapezoidally in kappa (it was first order); the Preisach runner in `coupling/MaterialStateModels` measures each loop on its second, closed cycle; the runners state their work-quadrature errors and the loop error bound, and the viscoelastic runner reports its work.
- `src/core/coupling/AccountingAdapters` (new: network, driven network with the Q check, buildup and decay, coupled ledger, evolution, radiation, material state) and `AccountingStudies` (new: studies, refinement, reclassification, comparison, queries, export, records); `StateEvolution` records each step's ledger.
- `src/core/services/AccountingServices.cpp` (new), `Services.h`, `ProjectServices.cpp`, `ApplicationServices.cpp` (capabilities), `MultiphysicsServices.cpp` (coupled results carry their accounting).
- `proto/resonant/v1/accounting/*.proto`, `src/automation_api/Handlers.cpp`: 17 RPCs; `python/resonant_api/mcp.py`: 17 MCP tools.
- `src/app/AccountingController.*`, `AccountingPanel.*`, `EnergyFlowView.*`, `forms/AccountingPanel.ui`, `MainWindow.{h,cpp}`, `forms/MainWindow.ui`: the Accounting dock.
- `tools/check_layering.py`: `accounting` is domain-independent. `examples/accounting/` a–h with README.

## Tests

- New core suites: `test_dimensions.cpp` (3), `test_accounting_quantities.cpp` (2), `test_accounting_ledger.cpp` (2), `test_accounting_conservation.cpp` (4), `test_accounting_networks.cpp` (7), `test_accounting_radiation.cpp` (4), `test_accounting_coupled.cpp` (2), `test_accounting_material_state.cpp` (3), `test_accounting_services.cpp` (4), `test_accounting_examples.cpp` (4); `Hysteresis.LoopLossEqualsTheHysteronSum`, `PhaseChange.EntropyProductionIsNonNegative`; fixture `AccountingFixture.h`.
- `python/tests/test_accounting_api.py` (3) against a real headless host; the schema guard passes with the new contract.
- `tests/app/test_accounting_panel.cpp` (3) in the real main window.
- The full core suite (654) passed before t2; the t2 run is linked below as **Test-Run:**.

## Regression impact and documentation

- The integrator's element flows are opt-in; its existing outputs are unchanged. The damage dissipation and the Preisach minor-loop losses change value (now consistent with their models); the REQ-021 tests and examples pass unchanged. Coupled results gain an `accounting` section; evolution steps a `ledger`.
- Documentation: `docs/requirements/accounting.md` (RES-ACC-001..016), ADR-000226, `docs/architecture/ACCOUNTING_AUDIT.md`, `docs/architecture/ACCOUNTING_ARCHITECTURE.md`, `docs/physics/FORCE_AND_MOMENTUM.md`, `docs/physics/CONSERVATION_MONITORING.md`, `docs/physics/ACCOUNTING_VALIDATION.md`; updated `ENERGY_AND_MOMENTUM_ACCOUNTING.md`, `PHYSICAL_QUANTITY_SYSTEM.md`, `CONSTITUTIVE_MODELS.md`, the research roadmap (stage 3), the roadmap (M19), overview, architecture README, `docs/README.md`, `product.md`, both manifests, `DECISIONS.md`, `IMPLEMENTATION_STATUS.md`, `examples/README.md`, the README, `PROTOBUF_API.md` and `DOCUMENTATION_COVERAGE.md` (regenerated), help `core-concepts/energy-accounting.md`.
- Deviations from the request, stated in ADR-000226 and `IMPLEMENTATION_STATUS.md`: momentum is accounted for lumped networks and the layered radiation force only (the coupled participants, acoustic and full-wave solvers report none: insufficient data); entropy where a model provides it; moving control volumes, mass and charge balances are registered without data; spatial views are the layered profiles.

## Traceability

REQ-CHAT-20261126-022 -> RES-ACC-001, RES-ACC-002, RES-ACC-003, RES-ACC-004, RES-ACC-005, RES-ACC-006, RES-ACC-007, RES-ACC-008, RES-ACC-009, RES-ACC-010, RES-ACC-011, RES-ACC-012, RES-ACC-013, RES-ACC-014, RES-ACC-015, RES-ACC-016 -> WORK-000023 -> evidence (pending)

## Transitions

- 2026-11-28T05:24:52.901040+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T06:17:12.795111+00:00 active -> completed by agent-dev-20261126151713-76f632
