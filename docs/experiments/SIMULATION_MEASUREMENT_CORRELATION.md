# Simulation-to-Measurement Correlation

RES-EXP-014, RES-EXP-021..023.

## Rules (`Comparison.h`)

- **Compatible quantities only.** Quantity, ratio and density must agree. A
  reading in instrument units (V, FS) is compared with a physical simulation
  only in shape (`relative`: both normalized to their maximum, phases
  referred to the strongest point) and only when asked; the result says that
  absolute levels were not compared.
- **Overlap only.** Frequency series are compared over the common range;
  simulated values are interpolated (magnitude and unwrapped phase) onto the
  measured frequencies; measured points outside the simulated range are left
  out and counted. Nothing is extrapolated.
- **Frames.** Spatial comparisons need the positions in the simulation's
  frame (a transform may be given); the comparison is at the measured
  positions only.
- **Uncertainty.** When every measured point states an uncertainty, the
  fraction of points whose difference lies within the expanded uncertainty
  is reported; otherwise the result says it was not assessed.

## Generic metrics

RMS and maximum level error (dB), RMS phase error (deg, wrapped),
normalized mean square error, magnitude correlation, measured and simulated
peak frequencies, per-point level errors, the worst point (spatial).

## Domain metrics and the simulated side

| Domain | Measured | Simulated side | Metrics |
|---|---|---|---|
| acoustic | spatial tone amplitudes | free-field point sources at the experiment's actuators (`fieldSampler`, `FieldEvaluator`) | spatial metrics; focus location, -6 dB width, peak sidelobe (`compareFocus`) |
| acoustic | speaker FRF (Pa/FS after calibration) | free-field response of an ideal flat source (`freeFieldResponse`) | frequency metrics |
| mechanical | accelerance FRFs | modes of the mechanical solver (`simulatedShapeAt`) | resonance frequency error, MAC pairing (`pairModes`) |
| RF | S11 (VNA or Touchstone) | analytical impedance of the project's antenna (`impedanceSweep`) | frequency metrics on S11 (`compareReflection`); return loss, VSWR, 10 dB bandwidth |
| RF | angular scan | a simulated pattern cut (`patternCut`) | pattern difference above -30 dB, beam pointing error, beamwidths (`comparePattern`) |

In the application, Measurements > Compare With Simulation records the
comparison in the project. A comparison refuses when the project offers no
simulation for the measurement (for example S11 without an antenna) instead
of inventing one.

## What a comparison does not show

A small error at the measured positions or frequencies says nothing between
them; agreement in shape says nothing about absolute levels; a model that
leaves out a mechanism (a loudspeaker's high-pass, reflections, mutual
coupling) shows as a systematic difference, which is information, not
noise.
