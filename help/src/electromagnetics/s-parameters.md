---
id: em.s-parameters
title: S-parameters
category: Electromagnetic Engineering
order: 80
status: available
summary: S11 of an antenna, export of calculated one-port data and import of measured Touchstone files beside the model.
keywords: [s-parameters, S11, touchstone, s1p, reflection, measured data, VNA]
related: [em.impedance, exp.correlation, exp.measurements, ref.data-formats]
---

Scattering parameters describe a network by the waves going into and out
of its ports. An antenna is a one-port: its only S-parameter, S11, is its
reflection coefficient against the reference impedance. Resonant
calculates S11 from the analytical impedance, exports it as a Touchstone
file, and shows measured Touchstone data beside the model without ever
mixing the two.

## S11 of an antenna

```math
S11 = Γ = (Z − Z₀) / (Z + Z₀)
RL = −20 log10 |S11|,      VSWR = (1 + |S11|) / (1 − |S11|)
```

Z is the antenna's input impedance and Z₀ its **Reference impedance**
(see [Impedance](impedance.md)). With e^{+jωt} and peak phasors, the
phase of S11 is that of Γ.

## Exporting calculated data

In the **Impedance** tab, calculate a sweep and press **Export .s1p...**.
The file holds S11 in GHz with real and imaginary parts (RI), against the
antenna's reference impedance, with comments that name the model and
state that the data is calculated, not measured.

## Importing measured data

**Import Measured .s1p...** reads a one-port Touchstone 1.x file:

```text
! Antenna measured at the bench
# MHz S MA R 50
900   0.42  -35
1000  0.37  -60
1100  0.45  -95
```

- Frequency units Hz, kHz, MHz or GHz; formats RI, MA (magnitude, angle in
  degrees) or DB (dB, angle); the reference impedance from the option line
  (`R 50`); comment lines (`!`) are kept.
- Multi-port data, Y/Z/H/G parameters, decreasing frequencies and empty
  files are refused, with the line number.
- The data is stored in the project (undoable) and listed in **Measured
  data** with its number of points and reference impedance. Choose it to
  draw it as **Measured: <file>** next to the calculated curve, in the same
  view (resistance and reactance, return loss or VSWR).

## Worked example

Open `examples/electromagnetics/a-half-wave-dipole.resonant`, press
**Calculate Sweep** (500 - 1500 MHz) and **Export .s1p...**. At 1 GHz the
file holds S11 = (23.08 + j42.51) / (123.08 + j42.51), a magnitude of 0.371
(return loss 8.60 dB). Import the file again with **Import Measured
.s1p...**: the measured curve lies exactly on the calculated one, which is
a quick check of your file handling before you import real measurements.

## Multi-port files and comparison with measurements

The Electromagnetics panel handles one-port data. The **Experimental
Hardware** panel's **Measurements** tab has **Import Touchstone...** for
Touchstone 1.1 and 2.0 files of any port count (the file dialog offers
`.s1p` to `.s4p`), kept as raw network-parameter measurements with their reference
impedances, and **Compare With Simulation**, which compares a measured S11
with the analytical impedance of the project's antenna (return loss, VSWR,
10 dB bandwidth). The example `examples/experiments/rlc_antenna_and_line.s2p`
is a synthetic two-port file for trying it. See
[Measurements](../experiments/measurements.md) and
[Simulation correlation](../experiments/correlation.md).

## Common mistakes

- **Different reference impedances.** A file measured against 50 ohm
  compared with an antenna set to 75 ohm shows a false mismatch; the
  measured entry states its Z₀.
- **Expecting S21 from the models.** No model calculates transmission
  between two ports.
- **Treating exported data as a measurement.** The exported file says it is
  calculated; keep it apart from bench data.

## Limitations

- No network simulation of multi-port circuits, de-embedding or
  calibration of S-parameters in the Electromagnetics panel.
- No instrument is controlled from the Electromagnetics panel; vector
  network analysers are driven by experiments (simulated devices are
  provided).
