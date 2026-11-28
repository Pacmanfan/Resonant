# WORK-000024: Experimental laboratory: studies linking experiments to models, validated imports, uncertainty-aware comparison, error budgets, model calibration with identifiability apart from validation, experimental resonance and energy accounting

**Status:** completed
**Type:** FEATURE
**Request:** requests/processing/2026-11-26-prompt-26-experimental-energy--req-chat-20261126-023.md
**Requirements:** RES-LAB-001, RES-LAB-002, RES-LAB-003, RES-LAB-004, RES-LAB-005, RES-LAB-006, RES-LAB-007, RES-LAB-008, RES-LAB-009, RES-LAB-010, RES-LAB-011, RES-LAB-012, RES-LAB-013, RES-LAB-014, RES-LAB-015, RES-LAB-016, RES-LAB-017, RES-LAB-018
**Decision:** ADR-000227
**Evidence:** VER-000024
**Test-Suites:** core, app, cmake, layering, audits
**Test-Run:** 20261128T021903-1231985

## Scope

Experimental laboratory: studies linking experiments to models, validated imports, uncertainty-aware comparison, error budgets, model calibration with identifiability apart from validation, experimental resonance and energy accounting

## Implementation

- `src/core/laboratory/` (new infrastructure module, no domains):
  - `Laboratory`: studies, model links with revisions, deviations, dataset roles, and the extension `laboratory`.
  - `LabImport`: CSV, TSV, JSON and WAV imports with an import report.
  - `Validation`: scalar E_n, response, transient and field comparisons.
  - `ErrorBudget`: budgets with Welch–Satterthwaite and Student t, plus linear and Monte Carlo propagation.
  - `ParameterEstimation`: weighted Levenberg–Marquardt, bounds, priors, covariance, identifiability.
  - `ExperimentalResonance`: classified peaks and ring-downs.
  - `ExperimentalEnergy`: measured accounting ledgers.
  - `Sha256`.
- `src/core/foundation/Dimensions`: the deg unit.
- `src/core/accounting`: holdings with uncertainty; measured ledgers judged against the expanded measurement uncertainty.
- `src/core/coupling/LaboratoryModels`: predictions from the solvers:
  - lumped resonators;
  - layered electromagnetics;
  - the coupled evolution;
  - J2;
  - functional metrics through `withParameters`;
  - library properties;
  - design revisions.
- `src/core/coupling/LaboratoryStudies`: comparisons, calibration, validation on independent data, resonance extraction, retrieval, explanations, reports.
- `src/core/services/LaboratoryServices.cpp`, `Services.h`, `ProjectServices.cpp`: `LaboratoryService` (imports to the measurement store, calibration jobs, keep, validate, apply as new revisions); `ApplicationServices.cpp` adds the capabilities.
- Contract and clients: `proto/resonant/v1/laboratory/laboratory_service.proto` (24 RPCs), `src/automation_api/Handlers.cpp`, and `python/resonant_api/mcp.py` (24 tools at their levels).
- GUI: `src/app/LaboratoryController`, `LaboratoryPanel`, `forms/LaboratoryPanel.ui`, and the MainWindow dock.
- `tools/check_layering.py`: laboratory is infrastructure.
- `examples/laboratory/` a–g, with synthetic data in `data/`.

## Tests

- New core tests:
  - `tests/core/test_laboratory_model.cpp`, `test_laboratory_import.cpp`, `test_laboratory_comparison.cpp`;
  - `test_laboratory_uncertainty.cpp`, `test_parameter_estimation.cpp`, `test_experimental_resonance.cpp`;
  - `test_laboratory_energy.cpp`, `test_laboratory_models.cpp`, `test_laboratory_calibration.cpp`;
  - `test_laboratory_integration.cpp`, `test_laboratory_services.cpp`, `test_laboratory_examples.cpp`;
  - `LaboratoryFixture.h`.
- `tests/core/test_dimensions.cpp` covers deg.
- `tests/app/test_laboratory_panel.cpp`: import, compare, calibrate, keep, validate and report in the real main window.
- `python/tests/test_laboratory_api.py`: the API against a real headless host, and the MCP levels.
- The benchmarks with known ground truth are in `docs/experiments/LABORATORY_VALIDATION.md`.

## Regression impact and documentation

- **Accounting:** measured ledgers are a new mode, and simulated ledgers are judged as before; the full core suite runs.
- **Units:** the deg unit is an addition.
- **Help and protobuf:** the help coverage and `docs/reference/PROTOBUF_API.md` were regenerated.
- **Documentation:**
  - `docs/requirements/laboratory.md` and ADR-000227;
  - `docs/experiments/LABORATORY_AUDIT.md`, `LABORATORY_ARCHITECTURE.md`, `DATA_IMPORT.md`, `MODEL_CALIBRATION.md`, `LABORATORY_VALIDATION.md`, and `docs/experiments/README.md`;
  - `help/src/experiments/laboratory.md`;
  - the indexes (DECISIONS.md, decisions.txt, requirements.txt, docs/README.md, the architecture README, overview and roadmap M20, product.md, examples/README.md, IMPLEMENTATION_STATUS.md, README.md).
- **Deviations from the request:** listed in ADR-000227.

## Traceability

REQ-CHAT-20261126-023 -> RES-LAB-001, RES-LAB-002, RES-LAB-003, RES-LAB-004, RES-LAB-005, RES-LAB-006, RES-LAB-007, RES-LAB-008, RES-LAB-009, RES-LAB-010, RES-LAB-011, RES-LAB-012, RES-LAB-013, RES-LAB-014, RES-LAB-015, RES-LAB-016, RES-LAB-017, RES-LAB-018 -> WORK-000024 -> evidence (pending)

## Transitions

- 2026-11-28T06:25:39.928869+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-28T07:24:58.464959+00:00 active -> completed by agent-dev-20261126151713-76f632
