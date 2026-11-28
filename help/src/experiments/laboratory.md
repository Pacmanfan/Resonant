---
id: exp.laboratory
title: The Laboratory: Measurements Against Models
category: Experiments and Hardware
order: 90
status: available
summary: Import measurements with a report of every problem, compare them with a model's prediction weighing their uncertainty, calibrate a model's physical parameters - and find out whether the data can determine them - then test the result on other data.
keywords: [laboratory, measurement, import, CSV, calibration, parameter estimation, identifiability, validation, uncertainty, error budget, comparison, ring-down, resonance]
related: [exp.correlation, exp.calibration, exp.uncertainty, concepts.accounting]
---

A simulation is a prediction, and a measurement is an observation. The
**Laboratory** (**Panels > Laboratory**) keeps them apart and puts them side by
side. Open a project in `examples/laboratory` and save it first: imported data is
kept next to the project file.

## A study

A **study** is a short record of the experiment. It names:

- the objective and the hypothesis;
- the specimen;
- the models it is compared with, each at the revision it was linked;
- what was measured differently from the drawing (an actual length, a
  material batch);
- its datasets, each with a role: calibration, validation and so on.

**Import study files** reads the study's files. Every problem is listed with its
row and column, for example:

- a missing unit;
- a malformed row;
- a NaN value;
- a time that goes backwards.

Nothing is dropped silently. The original file is kept unchanged.

## Compare, calibrate, validate

- **Compare** puts the measurement and the model's prediction on one plot, with
  the residual below. The verdict weighs both uncertainties: *consistent*,
  *inconsistent*, or *not assessable* when the measurement states no
  uncertainty.
- **Calibrate** estimates the parameters you tick. These are physical
  parameters such as a mass, a stiffness, a conductivity or a yield stress,
  never a fudge factor. You get each estimate with a 95 % interval and whether
  the data actually determine it. *Not identifiable* means other values explain
  the measurement just as well.
- **Keep** saves the calibration in the project. **Validate** tests it against
  the study's *other* data. Testing on the data it was fitted to is refused.
- **Apply…** creates a new material, or a new material-state study, with the
  fitted values. The original is never changed, and you can undo it.

## Read the evidence

**Generate report** writes the study up:

- its configuration;
- its datasets;
- its comparisons, calibrations and validations;
- its limitations.

**Explain the latest record** lists what the records establish, possible causes
(clearly marked as hypotheses), what is missing and what to measure next. It
changes nothing.

## Limits

- **Formats:** CSV, TSV, JSON and WAV (Touchstone through the Experiments
  workspace). NetCDF, TDMS and images are not imported.
- **Statistics:** fits are weighted least squares with optional Gaussian
  priors. There is no Bayesian sampling.
- **Instruments:** instruments and acquisition stay in the Experiments
  workspace.
- **Synthetic examples:** the example data are synthetic and say so. They show
  the method, not physical evidence.
