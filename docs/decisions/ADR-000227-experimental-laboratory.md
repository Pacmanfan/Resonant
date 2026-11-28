# ADR-000227: Experimental laboratory: studies linking experiments to models, validated imports, uncertainty-aware comparison, model calibration with identifiability kept apart from validation

**Status:** ACCEPTED
**Date:** 2026-11-28
**Type:** architecture
**Requirements:** `docs/requirements/laboratory.md` RES-LAB-001..018
**Request:** REQ-CHAT-20261126-023
**Builds on:** ADR-000213 (the experiment framework), ADR-000226 (accounting), ADR-000224 (lineage), ADR-000214 (optimization)

## Context

The experiment framework measures well: instruments, acquisition,
instrument calibration, immutable records, processing and some comparisons.
It has no link to the physical models, no import of ordinary data files
with validation, no comparison that weighs uncertainty, no calibration of a
model's physical parameters, no identifiability, no separation of
calibration from validation, and no service or automation layer.

## Decisions

1. **A laboratory module over the experiment framework**
   (`src/core/laboratory`, domain-independent, like `experiments`). It holds:
   - studies, their model links and deviations;
   - importers;
   - comparisons;
   - error budgets and propagation;
   - parameter estimation;
   - experimental resonance;
   - experimental energy ledgers.

   Measurement records and their data stay the experiment framework's,
   in `extensions["experiments"]` and the measurement store. The
   laboratory's own records (studies, import reports, comparisons,
   calibrations, validations) are in `extensions["laboratory"]`.
2. **Model links carry revisions.** A link stores the content fingerprint of
   what it points to (system, structure, material, study, design revision),
   so a later change makes the comparison stale. No experiment ever changes
   a model.
3. **Predictions come from coupling.** `coupling/LaboratoryModels` wraps the
   existing solvers as prediction functions of named physical parameters:
   - the lumped resonator;
   - the evolution of a physical system;
   - layered electromagnetics;
   - the J2 material point;
   - functional analyses.

   Calibration varies only parameters that have a physical meaning in the
   model, never a correction factor.
4. **Calibration, validation and application are three operations.**
   - A calibration record stores everything needed to repeat it.
   - A validation needs disjoint datasets.
   - Applying a result creates a new material revision (provenance
     *fitted*, with its record), on explicit request only.
5. **Uncertainty decides.** Comparisons use E_n with the combined expanded
   uncertainties. Measured energy balances use the measurement uncertainty
   instead of a numerical tolerance (the accounting module gains `measured`
   ledgers).
6. **LaboratoryService, contract, Python, MCP and the Laboratory panel.** The
   explanations are deterministic evidence summaries for assistants, not
   generated text.

## Consequences

- Imported data needs a saved project, because the data goes to the
  measurement store next to the project file.
- The existing ALSA, simulated and SoapySDR (discovery) instruments remain
  the hardware layer. No new hardware adapter is added.

## Deviations from the request

- No NetCDF, TDMS, HDF5 import of external files or image formats. CSV,
  TSV, JSON, WAV and Touchstone are supported.
- No Bayesian sampling: maximum a posteriori with Gaussian priors only.
- No new instrument protocol adapters (VISA, SCPI, serial), and no automated
  instrument control through the automation API: acquisition stays in the
  Experiments workspace with its safety rules.
- Mode-shape comparison of measured spatial modes remains the mechanics
  module's MAC. Microstructure images are not imported.
