# Laboratory Architecture

**Status:** current (REQ-CHAT-20261126-023, RES-LAB-001..018, ADR-000227). The
laboratory connects the experiment framework to the physics:

```text
theory -> model -> prediction -> experiment -> measurement -> comparison -> calibration -> independent validation
```

None of these steps is silently replaced by another. The imports are described in
[DATA_IMPORT.md](DATA_IMPORT.md), the calibrations in
[MODEL_CALIBRATION.md](MODEL_CALIBRATION.md), and the benchmarks in
[LABORATORY_VALIDATION.md](LABORATORY_VALIDATION.md). The audit is
[LABORATORY_AUDIT.md](LABORATORY_AUDIT.md).

## What is where

| Module | Holds |
|---|---|
| `laboratory/Laboratory` | studies: objective, hypothesis, domain, specimen, operator, the experiment run, model links with their revisions, nominal against measured deviations, conditions, datasets with roles (raw, calibration, validation, holdout, reference, environment) and status; the extension `laboratory` (studies, import reports, comparisons, calibrations, validations, raw sources) |
| `laboratory/LabImport` | CSV, TSV, JSON and WAV into raw measurement records, with an import report |
| `laboratory/Validation` | scalar E_n, frequency responses (on the experiment framework's comparison), transients, spatial fields |
| `laboratory/ErrorBudget` | budgets with distributions, sensitivities, stated correlations, Welch–Satterthwaite, Student t; first-order and Monte Carlo propagation |
| `laboratory/ParameterEstimation` | weighted Levenberg–Marquardt, bounds, priors, joint fits, covariance, intervals, identifiability |
| `laboratory/ExperimentalResonance` | measured peaks classified with evidence; ring-downs |
| `laboratory/ExperimentalEnergy` | measured energy balances as accounting ledgers (measured mode) |
| `coupling/LaboratoryModels` | predictions from the solvers (see below) |
| `coupling/LaboratoryStudies` | comparisons, calibration, validation, resonance extraction, retrieval, explanations, reports, link revisions |
| `services/LaboratoryServices` | `LaboratoryService`: 24 RPCs, Python, MCP |
| `app/LaboratoryPanel` | the Laboratory dock |

The measurement records and their data remain the experiment framework's. They live
in `extensions["experiments"]` and in the measurement store next to the project
file, so an import needs a saved project. The original file is kept read-only under
its SHA-256 in `<project>.data/laboratory/sources/`.

## Predictions

| Model | From | Physical parameters |
|---|---|---|
| `resonator.lumped` | `oscillators::drivenResponse` | mass, stiffness, damping (or a network's elements) |
| `em.layered-reflection` | `em::LayeredSolution` | each layer's permittivity, loss tangent, thickness |
| `thermal.evolution-probe` | `coupling::evolveSystem` (the coupled engine per step) | `<material>.thermal.<key>` of the system's regions |
| `material-state.j2-uniaxial` | `mechanics::j2Uniaxial` | Young's modulus, yield stress, isotropic and kinematic hardening |
| `functional.metric` | `functional::analyzeDocument` on `withParameters` | the structure's free and bound parameters |
| `material.property` | the library material (with its provenance and derivation) | none |
| `design-revision.metric` | a promoted candidate's evaluation | none |

A calibration may vary only these parameters, never a correction factor.

## Guardrails, and where they are enforced

- **Synthetic and measured data.** Synthetic data is labelled on import
  (`provenance: synthetic`, the record marked generated). Comparisons and
  calibrations on synthetic data say that they verify the method and are not
  physical evidence.
- **Raw data.** Raw records have no processing history. Import conversions are
  recorded in `acquisition.importSteps`, and processing creates new derived
  records.
- **Compatible quantities.** Quantities, units, frames and ranges are checked
  before any comparison. Conversions and alignments are recorded. Different
  frames without a transform are refused, and so are incompatible quantities.
- **Two kinds of calibration.** Instrument calibration is the experiment
  framework's. Model calibration is the laboratory's, and the two are never
  mixed.
- **Validation needs other data.** Validation on calibration data is refused as
  "not independent". A study cannot give one dataset both roles.
- **Identifiability.** Identifiability is examined in every calibration. A
  parameter is never reported as determined when other values explain the data
  equally well.
- **Models change only on request.** Applying a calibration is an explicit
  action that creates a new revision. Explanations change nothing.
- **Peaks are classified.** A spectral peak is classified with its evidence,
  never assumed to be a resonance.
- **Measured energy balances.** These are judged against the measurement
  uncertainty, and unmeasured pathways are declared, so the balance is reported
  as incomplete.
- **Operating range.** A validation outside the calibrated range is reported as
  an extrapolation.
- **Instruments and acquisition** remain in the Experiments workspace, with its
  safety rules. Nothing in the laboratory drives an output.

## Evidence summaries for assistants

`ExplainRecord` produces a deterministic summary of a record:

- **Findings:** each finding carries the record field it comes from.
- **Hypotheses:** each is marked as a hypothesis, with its basis. They include
  the study's recorded deviations.
- **Missing information:** for example, no uncertainty, no instrument, no
  validation, or a stale link.
- **Suggested measurements.**

The summary carries a disclaimer: no model or parameter was changed. It is the
evidence an assistant (through MCP) can inspect. Nothing is generated beyond it.
