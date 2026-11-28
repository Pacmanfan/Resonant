# Laboratory Audit: the Experiment Framework Before the Laboratory

**Status:** the audit behind REQ-CHAT-20261126-023 (RES-LAB-001, ADR-000227), taken on
2026-11-28. What existed, and what the laboratory adds without duplicating it.

| Area | Existing (RES-EXP-001..027) | Missing before the laboratory |
|---|---|---|
| Experiment | `experiments::Experiment`: devices, sensors, actuators, channels, acquisition, stimulus, synchronization, scan, environment, safety, processing, history | an objective or hypothesis; links to models and their revisions; a specimen; nominal against measured configuration |
| Records and store | `MeasurementRecord` (kind, category raw / calibrated / processed / simulated, quantity with SI unit, time base, pose, source ids, history, uncertainty, quality), an immutable HDF5 or binary store | instrument identity as metadata only; no content digest |
| Import | Touchstone (RF), calibration JSON, scan positions, archives; two legacy CSV importers in the GUI | CSV, TSV, JSON and WAV datasets; an import report |
| Processing | segment, calibrate, spectrum, PSD, transfer function, impulse response, filter, decimate, resample, correlations, delay, RMS, tone, average, integrate, scale | resonance extraction as an analysis of a record |
| Calibration | instrument and sensor calibration records with traceability, an append-only library | any calibration of a physical model's parameters; identifiability |
| Uncertainty | GUM budgets, uncorrelated root-sum-square, coverage factor, propagated through processing | stated correlations; Monte Carlo; propagation through models |
| Comparison | frequency series (dB, phase, NMSE, correlation, peaks, fraction within uncertainty), spatial (with a transform); acoustic, mechanical (MAC), RF metrics | scalar E_n; transients; the simulation's uncertainty; links to the coupled engine, functional structures, metamaterials, multiscale, material state, accounting, optimization |
| Resonance | matrix pencil and Lorentzian fits (dsp), peak picking (mechanics) | classification of measured peaks with evidence |
| Services and clients | none; `resonant.experiments` Python helpers | a service, a contract, MCP tools |
| GUI | the Experiments workspace (devices, experiments, calibration, run, measurements) | a laboratory workflow |

## Architecture map

```text
experiments (existing)                       laboratory (new, domain-independent)        coupling (new adapters)
 Experiment, runner, hardware, acquisition    Study: objective, models@revision,          LaboratoryModels: resonator, layered EM,
 MeasurementRecord + store (raw immutable) ◄── deviations, dataset roles                   coupled evolution, J2, functional metric,
 Processing, Calibration (instruments)        Import (CSV/TSV/JSON/WAV) + report           material property, design revision
 Comparison (frequency, spatial)          ◄── Validation (E_n, responses, transients,     LaboratoryStudies: comparisons, calibration,
 UncertaintyBudget                             fields), ErrorBudget + propagation           validation, resonance, retrieval,
 dsp: matrix pencil, Lorentzian           ◄── ExperimentalResonance                         explanations, reports
 accounting (REQ-022)                     ◄── ExperimentalEnergy (measured ledgers)        services: LaboratoryService
                                              ParameterEstimation (LM, identifiability)
```
