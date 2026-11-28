# Experimental Energy–Matter Laboratory and Physics-Based Validation

**Owner of:** `RES-LAB-001..018`
**Source:** REQ-CHAT-20261126-023 (Prompt 26, "Experimental Energy–Matter Laboratory and Physics-Based Validation")
**Refines:** `experiments.md` RES-EXP-001..027 (the experiment framework it builds on), `accounting.md` RES-ACC-010, `resonance.md` RES-RSN-001, `material_state.md` RES-DMS-005, `metamaterials.md`, `multiscale.md` RES-MSC-003, `optimization.md`
**Decisions:** ADR-000227 (builds on ADR-000213, ADR-000226)
**Documents:** `docs/experiments/LABORATORY_AUDIT.md`, `docs/experiments/LABORATORY_ARCHITECTURE.md`, `docs/experiments/DATA_IMPORT.md`, `docs/experiments/MODEL_CALIBRATION.md`, `docs/experiments/LABORATORY_VALIDATION.md`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-LAB-001 — Audit and reuse

The laboratory builds on the experiment framework (experiments, measurement
records and store, processing, instrument calibration, hardware, comparison,
uncertainty, reports) and adds what links experiments to physical models; it
creates no second experiment, measurement, material, geometry or solver
infrastructure.
*Verification:* `docs/experiments/LABORATORY_AUDIT.md`; `Laboratory*` tests use the experiment framework's types.

### RES-LAB-002 — Laboratory studies

A study names its objective and hypothesis, domain, specimen, operator, the
experiment it runs (if any), the models it is compared with - physical
systems, functional structures, materials, material-state studies, design
revisions of optimization candidates, multiscale representations, scene
objects - each with the revision (content fingerprint) it was linked at, the
nominal and measured configuration (deviations with uncertainty), the
conditions, its datasets with their roles (raw, calibration, validation,
holdout, reference) and its status; a linked model whose content changed is
reported stale.
*Verification:* `LaboratoryModel.*`.

### RES-LAB-003 — Import with a report

CSV, TSV, JSON and WAV datasets (and Touchstone through the RF importer) are
imported into measurement records with units from the header (`name [unit]`),
metadata from comment lines, conversions recorded in the history, and an
import report listing every issue - missing or unknown units, malformed rows,
missing values, NaN and infinities, non-monotonic or duplicate timestamps,
inconsistent sampling, missing channels, missing frames or instrument
metadata - with its row and column; nothing is discarded silently and an
import with errors creates no dataset unless partial acceptance is asked
for, which records what was left out.
*Verification:* `LaboratoryImport.*`.

### RES-LAB-004 — Raw data and provenance

The original file is kept unchanged with its SHA-256; raw records are written
once to the measurement store; every derived dataset names its sources and
history; synthetic datasets are labelled synthetic and never presented as
measurements.
*Verification:* `LaboratoryImport.*`, `LaboratoryServices.*`.

### RES-LAB-005 — Uncertainty-aware comparison

Scalar comparisons give the difference, the relative difference and the
normalized error E_n = |m − p| / sqrt(U_m² + U_p²) (consistent when E_n ≤ 1,
not assessable without uncertainties); frequency responses reuse the
framework's comparison and add frequency and amplitude offsets of the peaks,
spectral similarity and the fraction within the combined uncertainty;
transient responses are compared on their overlap with RMS, normalized RMS,
maximum deviation, correlation and the time shift; spatial fields after a
frame transform and interpolation, with per-point residuals.
*Verification:* `LaboratoryComparison.*`.

### RES-LAB-006 — Compatibility before comparison

Quantities, units, frames, frequency ranges and time intervals are checked
before any comparison; conversions and alignments are recorded; incompatible
datasets are refused with the reason.
*Verification:* `LaboratoryComparison.IncompatibleDataIsRefused`.

### RES-LAB-007 — Error budgets and propagation

Error budgets of components by source (instrument, sensor, calibration,
resolution, repeatability, reproducibility, sampling, timing, position,
environment, manufacturing, material, numerical, model form) with
distribution, sensitivity, degrees of freedom and stated correlations, their
contributions, the Welch–Satterthwaite degrees of freedom and coverage;
first-order and Monte Carlo propagation through a function with correlated
inputs. Resolution, accuracy, repeatability and reproducibility stay distinct
components.
*Verification:* `LaboratoryUncertainty.*`.

### RES-LAB-008 — Parameter estimation

Weighted nonlinear least squares (Levenberg–Marquardt) with bounds, optional
Gaussian priors (maximum a posteriori), joint fits over several datasets; the
covariance, standard errors, confidence intervals, the correlation matrix and
the reduced chi-square.
*Verification:* `ParameterEstimation.*`.

### RES-LAB-009 — Identifiability

The singular values of the weighted, scaled Jacobian, its condition number,
the parameter correlations and the relative standard errors classify each
parameter as identified, poorly identified or not identifiable, and name the
combinations that the data determine; a parameter is never reported as
determined when other values explain the data equally well.
*Verification:* `ParameterEstimation.IdentifiabilityIsAssessed`, `LaboratoryCalibration.*`.

### RES-LAB-010 — Calibration records, separate from validation

A calibration of a model's physical parameters records the model and its
revision, the original and estimated values, the datasets used, the method,
residuals, uncertainty, identifiability and date; validation uses datasets
not used for calibration (overlap is refused as "not independent") and
reports calibration and validation performance separately with the operating
range the data cover; applying a calibration is an explicit action that
creates a new material revision (provenance fitted) and never overwrites the
original.
*Verification:* `LaboratoryCalibration.*`.

### RES-LAB-011 — Physical models for prediction and calibration

Predictions come from the existing solvers: a lumped resonator's driven
response (receptance, mobility, accelerance), a time-stepped evolution of a
physical system (temperature at a sensor), a layered electromagnetic
reflection, a J2 material point's stress-strain curve, and a functional
structure's analysis metrics.
*Verification:* `LaboratoryModels.*`.

### RES-LAB-012 — Experimental resonance

Resonances identified from measured frequency responses (Lorentzian fits) and
ring-downs (matrix pencil) with frequency, Q, damping ratio, bandwidth and
uncertainty; each peak classified with its evidence (natural mode, driven
peak, excitation or instrument peak, possible interference, artifact,
unclassified) rather than every peak taken as a resonance; frequency
stability across the record; stored-energy decay labelled model-derived.
*Verification:* `ExperimentalResonance.*`.

### RES-LAB-013 — Experimental energy accounting

Measured energies and powers with their uncertainties and derivations enter
an accounting ledger marked measured; the balance is judged against the
expanded measurement uncertainty, unmeasured pathways are declared missing
(insufficient data), and inconsistency is distinguished from model
disagreement.
*Verification:* `LaboratoryEnergy.*`.

### RES-LAB-014 — Integration across the architecture

Thermal measurements are compared with time-stepped coupled runs of a
physical system; measured resonances with functional-structure analyses;
measured S-parameters with effective-property retrieval and its
homogenization regime; measured properties with multiscale derivations;
stress-strain data with material-state models; a design revision of an
optimization candidate with its measured performance, without modifying it.
*Verification:* `LaboratoryIntegration.*`.

### RES-LAB-015 — Evidence summaries

An explanation of a comparison or calibration lists established findings
(with the records they come from), hypotheses (marked as such), missing
information and suggested measurements; it never invents data, changes a
model or declares validation without evidence.
*Verification:* `LaboratoryServices.EvidenceSummaries`.

### RES-LAB-016 — Instruments and acquisition

The experiment framework's instruments, capabilities, acquisition, status
and calibration are the laboratory's: an experiment's runs are its datasets;
acquisition stays off the user-interface thread.
*Verification:* `test_experiments*` (existing), `LaboratoryServices.*`.

### RES-LAB-017 — Services, panel, clients and reports

`LaboratoryService` (studies, imports, datasets, processing, resonance
extraction, comparisons, budgets, calibration jobs, validation, explicit
application, explanations, reports and exports), Python and MCP; the
Laboratory panel (studies, data with import reports and plots, comparisons
with overlays and residuals, calibration with estimates, intervals,
correlations and identifiability, reports).
*Verification:* `LaboratoryServices.*`, `python/tests/test_laboratory_api.py`, `LaboratoryPanelTest.*`.

### RES-LAB-018 — Benchmarks with known ground truth

Synthetic benchmarks (labelled synthetic) for resonator identification,
frequency-response alignment, parameter recovery and identifiability,
spatial comparison, thermal comparison through the coupled engine, an
experimental energy balance with a missing pathway, invalid datasets, and
calibration against one dataset validated against another.
*Verification:* `LaboratoryExamples.*`, `docs/experiments/LABORATORY_VALIDATION.md`.
