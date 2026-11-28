# WORK-000030: Energy conversion as ledger stages: a 3D piezoelectric continuum, thermoelectric couples, linear magnetostriction, a single-diode photovoltaic cell, and an acoustic-to-electrical chain

**Status:** completed
**Type:** feature
**Request:** requests/processing/2026-11-28-part-ii-multiphysics-and-energ-req-chat-20261128-005.md
**Requirements:** RES-CONV-001, RES-CONV-002, RES-CONV-003, RES-CONV-004, RES-CONV-005, RES-CONV-006
**Decision:** ADR-000233
**Evidence:** VER-000030
**Test-Suites:** core, gui, cmake, audits
**Test-Run:** 20261128T134105-2870086

## Scope

Energy conversion as ledger stages: a 3D piezoelectric continuum, thermoelectric couples, linear magnetostriction, a single-diode photovoltaic cell, and an acoustic-to-electrical chain

## Implementation

- **`src/core/conversion/ConversionStage.{h,cpp}`:**
  - the stage record: forms, input, output, other inputs and outputs, stored, dissipated, coupling, conditions, assumptions, validity, uncertainty, missing terms;
  - `chainLedger`, the accounting ledger with each stage a body;
  - `evaluateChain`, each stage through `accounting::evaluate` and every join.
- **`src/core/mechanics/PiezoelectricSolid.{h,cpp}`:**
  - constants: stress-charge from strain-charge, PZT-5A data with provenance, rotated with the surface and the poling;
  - 20-node coupling and dielectric matrices; electrodes on layer faces or segments; terminals (ground, driven, floating, load);
  - static solution; short- and open-circuit modes with exact condensation; harmonic base excitation with a loss factor and loads;
  - a scaled and refined solve judged by the normwise backward error.
  - `StructuralModel` exposes `solid20StrainMatrix`; `LayeredMesh` gains per-layer face nodes and node (u, v); `layeredLoadVector` is public.
- **`src/core/thermal/Thermoelectric.{h,cpp}`:**
  - 1D legs with polynomial alpha, rho, kappa of T, finite differences with Picard iteration, the Thomson term;
  - a generator with its load, or a cooler with its current;
  - the heats from the gradients, the power from the voltage; entropy generation.
- **`src/core/mechanics/Magnetostrictive.{h,cpp}`:** linear piezomagnetic constants about a bias with a dynamic-field limit; the actuator and the coil harvester.
- **`src/core/conversion/Photovoltaic.{h,cpp}`:**
  - the single-diode circuit by Newton's method and by Lambert W;
  - a black-body spectrum labelled as a stand-in; the spectral photocurrent; Varshni and I_0(T);
  - the maximum power point and the term-by-term light budget.
- **`src/core/coupling/ConversionChains.{h,cpp}`:** the duct plane-wave chain to an electrical load.
- **`src/core/coupling/MatterExploration.cpp`:** thermoelectric, magnetostrictive and photovoltaic marked `standalone` with their models; the piezoelectric entries name the continuum.

## Tests

- **`tests/core/test_energy_conversion.cpp`:** 9 cases:
  - chains: stage by stage, at every join, missing terms;
  - Lambert W; the diode's closed forms (V_oc, I_sc, Green's fill factor, Newton against Lambert W); the light budget (circuit identity, stage residual, below the detailed-balance limit); V_oc against temperature and the refusal without light;
  - thermoelectric closed forms and eta_max; the cooler's Delta T_max; second-order convergence of the energy residual with temperature-dependent legs, Thomson heat and the Seebeck integral;
  - the magnetostrictive rod's identities and balance.
- **`tests/core/test_piezoelectric_solid.cpp`:** 7 cases:
  - the free plate's -d33 V, -d31 V L / t and free capacitance (exact); open circuit -g33 p t (exact);
  - the series bimorph (3 %);
  - the unimorph's modes against the lumped reference, its coupling with the reference's static-shape overestimate corrected analytically (8 %);
  - the harvester's balance and power against the reference (30 %);
  - refusals;
  - the acoustic-to-electrical chain, verified at and off resonance.
- **`tests/core/test_matter_exploration.cpp`:** the standalone statuses.

The suites (core, gui, cmake/ctest, audits) run through the linked t2 run.

## Regression impact and documentation

- **Existing models:** new modules only. The layered-solid and structural-model additions are additive, and the mechanism registry's statuses change for three standalone models, which keep `implemented()` false.
- **Finding:** the lumped cantilever's coupling exceeds the exact first mode's by about 22 % (the static tip-load shape). This is recorded in `docs/physics/ENERGY_CONVERSION.md` and ADR-000233.
- **Documents:**
  - RES-CONV-001..006 (`docs/requirements/energy_conversion.md`) and ADR-000233;
  - `docs/physics/ENERGY_CONVERSION.md`;
  - the indexes: `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `decisions.txt`, `requirements.txt`, `docs/README.md`;
  - the gap-analysis and demonstration-readiness rows; the help page's mechanism text.

## Traceability

REQ-CHAT-20261128-005 -> RES-CONV-001, RES-CONV-002, RES-CONV-003, RES-CONV-004, RES-CONV-005, RES-CONV-006 -> WORK-000030 -> evidence (pending)

## Transitions

- 2026-11-28T18:38:32.421690+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T18:47:14.909880+00:00 active -> completed by agent-dev-20261126151713-76f632
