---
id: exp.measurements
title: Measurements
category: Experiments and Hardware
order: 70
status: available
summary: Measured data sets, their kinds and categories, the reproducible processing chain, plots, reports, Touchstone import and archives.
keywords: [measurement, data set, processing, transfer function, spectrum, Touchstone, report]
related: [exp.acquisition, exp.uncertainty, exp.correlation, ref.data-formats]
---

Every run produces **data sets**: the raw readings of each input, the
commanded signal of each output, and whatever the experiment's processing
chain derives from them. Each one has a record: what, where, when, from which
device and sensor, in which unit, how good, and how it was made.

## The Measurements tab

Select an experiment in the Experimental Hardware dock and open
**Measurements**. The tree groups data sets by run (imported data under
*imported*) with the columns **Data set** (the binding, followed by the last
processing operation, for example `ch-mic > calibrate`), **Category**,
**Kind** and **Quantity** (its unit). A data set from a simulated device says
*(simulated device)*.

Selecting a data set plots it and lists its record: category and kind, the
quantity, *Acquired from a SIMULATED device: generated data, not a
measurement* when that applies, the data quality (gaps, dropped and clipped
samples, timestamp problems), the [uncertainty](uncertainty.md) and
every processing step with all its parameters. The plot shows **Magnitude**,
**Magnitude (dB)**, **Phase (deg)**, **Real part** or **Coherence** against
time (s) or frequency (Hz); long data sets are thinned for display only.

## Kinds and categories

| Kind | Data |
|---|---|
| time-series | samples, real or I/Q, on a uniform time base |
| spectrum | complex amplitudes on a frequency axis |
| power-spectrum | one-sided density on a frequency axis |
| transfer-function | a complex ratio on a frequency axis, with the coherence |
| network-parameters | the S matrix per frequency |
| scalar, vector | one value (RMS, delay); values on an axis (a correlation over lag) |

| Category | Meaning |
|---|---|
| raw | readings as acquired, or an imported file: never changed |
| calibrated | raw readings in physical units through a named calibration |
| processed | derived by a recorded processing step |
| command | the signal sent to an output: commanded, not measured |
| simulated, reconstructed | calculated by a model; estimated between measured positions |

Quantities have one SI unit each (V, FS, Pa, m/s², m/s, m, N, m/m, W, V/m,
A/m, Ω, K, rad, s, Hz; S-parameters are dimensionless). Nothing is converted
implicitly.

## Processing

The experiment's processing chain runs at step 12, on the recorded bindings
(never on the commanded signals unless a step names them). The operations are
segment, calibrate, spectrum, psd (Welch), transfer-function (H1 with
coherence), impulse-response, filter (linear-phase FIR), decimate, resample,
cross-correlation, autocorrelation, delay, rms, tone (the complex amplitude
at one frequency), average, integrate-frequency (acceleration to velocity to
displacement) and scale (an inferred quantity with its model stated).

```math
G_xx(f) = 2 ⟨|X(f)|²⟩ / (f_s · Σ w²)
H1(f) = G_xy(f) / G_xx(f)
γ²(f) = |G_xy(f)|² / (G_xx(f) · G_yy(f))
```

`X` is a windowed segment's spectrum, `w` the window, `f_s` the sample rate,
`G_xy` the cross-spectral density between reference `x` and response `y`, `γ²`
the coherence (1 for a perfectly linear, noise-free relation). Integrating
`G_xx` over frequency gives the mean square. H1 is biased low by noise on the
reference and by a delay that is not small against the segment length.

Raw data is never changed: each result is a new data set whose id is a hash
of its sources, the operation and its parameters, so the same chain on the
same data gives the same ids and values. Nothing is windowed, filtered,
normalized or calibrated unless a step says so. Data with gaps is refused
until a gap-free segment is chosen.

## Reports, Touchstone files and archives

- **Save Report...** writes the HTML report of the experiment's last run (it
  is also stored next to the data); before a run it says *run it first*.
- **Import Touchstone...** reads `.s1p` to `.s4p` files (Touchstone 1.1 and
  2.0, any reference impedances) as raw network-parameter data, marked as
  imported, not acquired.
- **Export Archive...** writes a new folder with `experiment.json` (the
  experiment, the calibrations it used with their history, its records,
  spatial data and comparisons) and `data/` with copies of its data sets.
  **Import Archive...** adds one to the project; duplicates are refused.
  The experiment framework can re-run an archive's processing from its raw
  data and reproduces the derived data to 10⁻⁹ (relative); the panel has no
  command for that check in this version.

## Worked example

Run *Speaker characterization* in `examples/experiments/acoustic_experiments.resonant`:
a 0.25 FS log sweep from 50 Hz to 15 kHz, H1 of the microphone over the drive
loopback (16384-sample Hann segments, 50 % overlap), then calibrated with
`cal-sim-mic-1`. The simulated loudspeaker gives 2 Pa at 1 m per FS, so the
calibrated response is about 2 Pa/FS (6 dB re 1 Pa/FS) above its 80 Hz
high-pass; before calibration H1 is 2 × 0.0158 = 0.0316 FS/FS. The automated
tests find H1 within 0.3 dB of that at 1 kHz and 4 kHz with a coherence above
0.99, and the calibrated response within 0.3 dB RMS of an ideal flat source
above 1 kHz; at 100 Hz the high-pass is visible (more than 1.2 dB low).

*Structural modal test* in `mechanical_experiments.resonant` gives three
accelerance transfer functions (acceleration over force, after calibration);
their peaks lie at the simulated modes, 120 Hz and 410 Hz (within 2 Hz in the
tests).

## Limitations

- Processing chains are defined in the experiment; the tab displays the
  results but does not edit or re-run a chain.
- Spatial reconstruction between measured points uses inverse-distance
  weighting only and never extrapolates.

## Related topics

[Acquisition](acquisition.md) · [Uncertainty](uncertainty.md) ·
[Simulation correlation](correlation.md) · [Data formats](../reference/data-formats.md)
