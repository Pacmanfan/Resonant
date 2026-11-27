---
id: exp.correlation
title: Simulation correlation
category: Experiments and Hardware
order: 90
status: available
summary: Compare a measurement with the project's simulation: what is compared, the rules that keep it honest, the metrics, and what a comparison does not show.
keywords: [correlation, comparison, measurement, simulation, validation, error, S11]
related: [exp.measurements, exp.uncertainty, sim.comparison, em.s-parameters]
---

A simulation is only as good as its agreement with reality, and a
measurement is only meaningful against a model when both describe the same
quantity at the same points. **Compare With Simulation** compares a measured
data set with what the project predicts, and refuses when it cannot do so
honestly instead of inventing a simulation.

## Running a comparison

1. Run the experiment (or import a Touchstone file) so that the measurement
   exists.
2. On the **Measurements** tab of the Experimental Hardware dock, select the
   data set.
3. Press **Compare With Simulation**. The text below shows *Compared with:*
   and the simulated side, then the metrics; or *No comparison:* and the
   reason. Each comparison is recorded in the project (undoable) and included
   in archives.

## What the application compares

| Measurement | Simulated side |
|---|---|
| network parameters (a VNA sweep or an imported Touchstone file) | the analytical input impedance of the **first antenna** in the scene, on 201 frequencies over the measured range; metrics on S11 with return loss, VSWR and the 10 dB bandwidth |
| an acoustic experiment's spatial data set (tone amplitudes at measured positions) | free-field point sources at the experiment's driven actuators, with their positions, axes, gains and delays, compared **in shape only** (the sources' absolute levels are not modelled) |

Anything else is refused: *there is no simulation in the project to compare
this measurement with*. S11 without an antenna in the scene is refused with
*the project has no antenna whose simulated impedance could be compared with
this S11*. An acoustic experiment without a spatial data set is refused with
*the experiment has no spatial data set to compare; run it first*: that
includes experiments that measure no tone at known positions, such as the
speaker characterization, even after they have run.

The experiment framework also compares a calibrated loudspeaker response with
the free-field response of an ideal flat source, mechanical accelerances with
the modes of the mechanical solver (resonance frequency error, MAC pairing) and
measured radiation patterns with simulated pattern cuts (pattern difference
above −30 dB, beam pointing error, beamwidths), and computes focus location,
−6 dB width and peak sidelobe of a measured focus. These are verified by the
tests but are not offered by the button in this version.

## The rules

- **Compatible quantities only.** Quantity, ratio and density must agree. A
  reading in instrument units (V, FS) is compared with a physical simulation
  only in shape: both normalized to their maximum, phases referred to the
  strongest point. The result then says that absolute levels were not
  compared.
- **Overlap only.** Frequency series are compared over the common range; the
  simulated values are interpolated (magnitude and unwrapped phase) onto the
  measured frequencies; measured points outside the simulated range are left
  out and counted. Nothing is extrapolated.
- **Frames.** Spatial comparisons need the positions in the simulation's
  frame and compare at the measured positions only.
- **Uncertainty.** When every measured point states an uncertainty, the
  fraction of points whose difference lies within the expanded uncertainty is
  reported; otherwise the result says it was not assessed.

## The metrics

```math
e_i = 20 · log10(|s_i| / |m_i|)
φ_i = wrap(arg s_i − arg m_i)
NMSE = Σ |m_i − s_i|² / Σ |m_i|²
```

`m_i` and `s_i` are the measured and simulated complex values at point `i`;
`e_i` is the level error (dB), `φ_i` the phase error (deg, wrapped to ±180°).
The result gives the RMS and largest level error, the RMS phase error, the
normalized mean square error, the correlation of the magnitudes, the measured
and simulated peak frequencies, the level error at every point and, for
spatial data, the worst point. Points near zero are left out of the dB and
phase metrics, and the result says how many.

## Worked examples

- *Acoustic field mapping* (`examples/experiments/acoustic_experiments.resonant`):
  after the run, select a data set of the experiment and press Compare With
  Simulation. Against the free field of the simulated loudspeaker the shape
  agrees within 0.2 dB RMS and 3° in the automated tests; an uncalibrated
  comparison of absolute levels is refused unless it is relative.
- *Antenna impedance (VNA)* (`rf_experiments.resonant`): the simulated series
  RLC load (73 Ω, 1 GHz, Q 10) is best matched at 1 GHz with a return loss of
  20 log10(123 / 23) = 14.56 dB and a VSWR of 1.46. The example's scene has
  no antenna, so the comparison is refused until you add one, for example
  **Electromagnetics > Add Dipole Antenna** tuned near 1 GHz (see
  [S-parameters](../electromagnetics/s-parameters.md)).

## What a comparison does not show

A small error at the measured positions or frequencies says nothing about the
field between them; agreement in shape says nothing about absolute levels; a
model that leaves out a mechanism (a loudspeaker's high-pass, reflections,
mutual coupling) shows as a systematic difference, which is information, not
noise. Data from simulated instruments verifies the chain against the
instruments' own models, not the physics.

## Related topics

[Measurements](measurements.md) · [Uncertainty](uncertainty.md) ·
[Comparing simulations](../simulation/comparison.md) · [S-parameters](../electromagnetics/s-parameters.md)
