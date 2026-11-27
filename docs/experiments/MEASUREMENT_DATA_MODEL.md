# Measurement Data Model

RES-EXP-011, RES-EXP-018.

## The record

`MeasurementRecord` (the metadata of one data set): id, experiment, run,
binding, sensor, device, channel, domain, kind, category, quantity and unit,
time base (rate, start, clock domain, hardware or software timestamps,
timing correction), pose and frame, data reference, shape, native sample
format, whether it came from a simulated device, calibration applied,
sources, processing history, uncertainty, data quality, acquisition
settings, creation time, note.

| Kind | Data |
|---|---|
| time-series | samples (real, or I/Q) on a uniform time base |
| spectrum | complex amplitudes on a frequency axis |
| power-spectrum | one-sided density on a frequency axis |
| transfer-function | complex ratio on a frequency axis, with the coherence |
| network-parameters | S matrix per frequency (row-major) |
| scalar | one value (RMS, delay, ...) |
| vector | values on an axis (a correlation over lag) |

| Category | Meaning |
|---|---|
| raw | instrument readings as acquired (or an imported file): never changed |
| calibrated | raw readings in physical units through a named calibration |
| processed | derived by a recorded processing step |
| simulated | calculated by a model |
| reconstructed | estimated between measured positions |
| command | the signal sent to an output: commanded, not measured |

Raw records have no history; every other record names its sources and the
complete history that produced it.

## Quantities and units

Quantities have one SI unit each (`ExperimentTypes.h`): voltage V, full
scale FS (an uncalibrated instrument reading), pressure Pa, acceleration
m/s^2, velocity m/s, displacement m, force N, strain m/m, stress Pa, power W,
electric field V/m, magnetic field A/m, S-parameter 1, impedance ohm,
temperature K, angle rad, time s, frequency Hz. A ratio (`per`) and a power
spectral density (`(unit)^2/Hz`) are stated in the quantity. Nothing is
converted implicitly; a stated unit that is not the SI unit is refused.

## Storage

```
<project dir>/<project name>.data/measurements/<experiment id>/<measurement id>.h5     HDF5
<project dir>/<project name>.data/measurements/<experiment id>/<measurement id>.rmd/   binary fallback
<project dir>/<project name>.data/measurements/<experiment id>/report-<run>.html       reports
```

HDF5 files hold the data sets `real` (N), `complex` (N x 2), `axis`,
`auxiliary` and the record as JSON (`metadata`); the binary format holds the
same as little-endian float64 files and `metadata.json`. Data sets are
created exclusively and made read-only when finished: a raw measurement is
never overwritten. Raw streams are flushed periodically; a recording that
ends without finishing (cancelled, failed, crashed) keeps its metadata,
marked incomplete. The project file holds the records under
`extensions.experiments` (version 1); a project without experiments has no
such key and loads unchanged.

## Archives

`exportExperiment` writes a new directory with `experiment.json` (the
experiment, the calibrations it used with their history, its records,
spatial data and comparisons) and `data/` (copies of its data sets).
`importExperiment` reads it with its own store; `mergeArchive` adds it to a
project; `reproduceProcessing` re-runs every derived data set's last step on
its sources and compares (ids must match; values to 1e-9 relative).
