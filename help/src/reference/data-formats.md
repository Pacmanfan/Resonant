---
id: ref.data-formats
title: Data formats
category: Technical Reference
order: 40
status: available
summary: The structure of every file Resonant writes or reads besides the project file - CSV, JSON, Touchstone, STL, OBJ, HDF5 and its binary measurement format.
keywords: [CSV, JSON, Touchstone, HDF5, STL, OBJ, file format]
related: [ref.import-export, ref.project-format, ref.units, exp.measurements, trouble.calibration]
---

All files use SI units unless a header says otherwise, `.` as the decimal
separator and UTF-8 text. JSON files that other programs may read carry a
`format` name and a `version`; a reader should refuse a version it does not
know. Which menu or button writes or reads each file is listed on
[File import and export](import-export.md).

## Point values (CSV and JSON)

Written by **Export Points...** on the **Field Analysis** tab of the
**Analysis** panel. The CSV has
one row per probe or target and the columns

```text
name,x_m,y_m,z_m,pressure_real_Pa,pressure_imag_Pa,magnitude_Pa_peak,phase_rad,rms_Pa,spl_dB,
spl_reference_Pa,intensity_x_W_m2,intensity_y_W_m2,intensity_z_W_m2,intensity_exact,near_source,intensity_available
```

(one header line in the file). The last three columns are 0 or 1. The JSON
file (`"format": "resonant-point-report"`, `"version": 1`) holds the same
values per point (`position`, `pressure` as `[re, im]`, `magnitudePa`,
`phaseRad`, `rmsPa`, `splDb`, `splReferencePa`, `activeIntensity`, flags,
and `desiredPressure` for targets) together with `frequencyHz`, the
`medium`, the `model`, the `timeConvention` and `"amplitudes": "peak"`.

## Probe signals (CSV)

Written by **Export Signals...** on the **Time Signals** tab of the
**Analysis** panel, for a full-wave result: a comment line naming the result, the solver and the time
step, then a header `time_s,"<probe 1>","<probe 2>",...` and one row per
time step with the pressure in Pa.

## Measured frequency responses (CSV)

Read by **Import Measured Response...** on the Mechanics panel. Lines that
start with `#` are comments and may carry the keys `sensor`, `quantity`
(displacement, velocity or acceleration), `unit`, `position` (x y z in m),
`direction`, `sensitivity` and `source`. The header needs a column starting
with `frequency` and either `magnitude` and `phase` (degrees) or `real` and
`imag`:

```text
# sensor: tip accelerometer
# quantity: acceleration
# unit: m/s^2/N
frequency_Hz,magnitude,phase_deg
100,0.52,-12.5
110,0.61,-15.0
```

Frequencies must be positive and increasing, and at least two data lines are
needed; a bad line is reported by its number.

## Touchstone

S-parameter files `.s1p` to `.sNp`, versions 1.1 and 2.0, in RI, MA or DB
format with any frequency unit. Reading keeps the reference impedance of
every port, the format, the frequency unit, the version and the comments.
Only full S matrices are read: Y, Z, H and G parameters, mixed-mode data,
noise data and triangular matrices are refused, not converted. The
Electromagnetics panel exports one-port data as version 1 in GHz and RI.

## Calibration records (JSON)

One record per file, read by **Import Calibration...**. The main keys are
`id`, `type`, `sensorId` (or `deviceId` and `channelId`, or `actuatorId`),
`unit`, `scalar`, `phaseRad`, `delaySeconds`, `response` (rows
`[frequencyHz, re, im, relativeUncertainty]`), `scalarJustification`,
`frequencyRangeHz`, `validUntilUtc` (ISO 8601), `uncertainty`, `procedure`,
`source`, `supersedes`, `version` and `fromSimulatedDevice`. The `type` is
one of `amplitude`, `phase`, `timing`, `frequency`, `position`,
`orientation`, `sensor-sensitivity`, `actuator-transfer-function` and
`channel-to-channel`. An example is on
[Calibration problems](../troubleshooting/calibration.md).

## Measurement data

Data sets are stored next to the project, never inside it:

```text
<project>.data/measurements/<experiment id>/<measurement id>.h5     HDF5 builds
<project>.data/measurements/<experiment id>/<measurement id>.rmd/   other builds
<project>.data/measurements/<experiment id>/report-<run>.html       reports
```

An HDF5 file holds the data sets `real` (N values), `complex` (N × 2),
`axis` and `auxiliary`, and the measurement record as JSON in `metadata`. A
`.rmd` folder holds the same as little-endian 64-bit floating-point files
`real.f64`, `complex.f64`, `axis.f64` and `auxiliary.f64` and a
`metadata.json`. Data sets are made read-only when finished; one that was
cancelled or failed keeps its metadata, marked incomplete. A build reads
only its own format.

| Kind | Data |
|---|---|
| time-series | samples, real or I/Q, on a uniform time base |
| spectrum, transfer-function | complex values on a frequency axis (with the coherence for a transfer function) |
| power-spectrum | a one-sided density on a frequency axis |
| network-parameters | the S matrix per frequency, row by row |
| scalar, vector | one value, or values on an axis |

## Experiment archives

**Export Archive...** writes a new folder with `experiment.json` (the
experiment, the calibrations it used with their history, its records,
spatial data and comparisons) and `data/` with copies of its data sets.

## Optimization files

| File | Content |
|---|---|
| run record, JSON | `"format": "resonant.optimization.run"`, version 1: the problem as run and every candidate with its design values, objective values, constraint results and the model that evaluated it |
| candidates, CSV | `id,origin,model,fidelity,status,feasible`, then one column per active variable, objective and constraint violation, named `<id> [<unit>]` |
| candidate, JSON | `"format": "resonant.optimization.candidate"`, version 1: the run, the problem, the candidate, its bindings, whether it was validated, and the mesh checks of each exported mesh |

`examples/optimization/read_run.py` reads a run record with the Python
standard library.

## Meshes

STL files are ASCII and OBJ files Wavefront text, both in **metres**.
Candidate meshes are in world coordinates, one file per changed object; the
topology export is one STL of the cleaned, extruded design. STL has no unit
field: programs that assume millimetres must scale by 1000.

## Related topics

- [File import and export](import-export.md)
- [Project file format](project-format.md)
- [Units](units.md)
- [Measurements](../experiments/measurements.md)
