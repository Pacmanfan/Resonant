---
id: concepts.sensors
title: Sensors
category: Core Concepts
order: 80
status: available
summary: Points where Resonant reports what a field does - acoustic probes, mechanical sensors, electromagnetic probe points - and how they relate to the sensors of real experiments.
keywords: [probe, sensor, measurement point, accelerometer, pressure, time signal]
related: [acoustics.field-analysis, mechanics.forced-response, exp.sensors, em.near-far-field, concepts.targets]
---

A **sensor** in a simulation is a point where you want numbers: the
pressure at a microphone position, the acceleration where an accelerometer
will sit, the field strength at a probe. Sensors never change a field;
they only report it. Unlike a [target](targets.md), a sensor
asks for nothing.

| Domain | Object | Created with | Reports |
|---|---|---|---|
| Acoustics | Probe | **Acoustics > Probe** | pressure, phase, RMS, SPL, intensity; in full-wave runs the pressure over time |
| Mechanics | Sensor | **Mechanics > Add Sensor** | displacement, velocity or acceleration along its local +Z |
| Electromagnetics | probe point (not an object) | **Electromagnetics** panel, *Fields* tab, *Probe (world coordinates)* | E, H and power density at that point |
| Experiments | hardware sensors | **Experiments > Experimental Hardware Panel** | measured data ([Sensors](../experiments/sensors.md)) |

## Acoustic probes

**Acoustics > Probe** adds a probe 100 mm above the origin; place it with
the transform in **Properties**. A probe has no settings of its own.

- **Analytical field (F5):** **Analysis > Field Analysis > Probes and Targets** lists each probe with *X*, *Y*, *Z*, *|P| (Pa peak)*, *Phase (deg)*, *RMS (Pa)*, *SPL (dB)*, *Intensity (W/m^2)* and a *Note*. Probe values are computed at the exact point in double precision, not read from the field grid. **Export Points...** writes them as CSV or JSON.
- **Full-wave run (F6):** every probe inside the region records the pressure at every time step. **Analysis > Time Signals** plots *Pressure over time*, *Spectrum magnitude*, *Spectrum phase* (with a *Hann* or *Rectangular* window) and *Energy over time*; **Export Signals...** writes CSV.

The quantities follow from the complex pressure `P` at the probe:

```math
p_rms = |P| / √2
SPL = 20 · log10( p_rms / p_ref )
I = ½ · Re{ P · V* },    V = −∇P / (j ω ρ)
```

`p_ref` is 20 µPa in air and 1 µPa in water and is always stated, `V` is
the particle velocity, `ρ` the medium density and `I` the active
intensity. Intensity is exact for omnidirectional sources and approximate
when a source has a directivity; the *Note* column then says so, as it
does for a probe inside a source's radius.

## Mechanical sensors

**Mechanics > Add Sensor** puts a sensor on top of the selected structure
(or the one chosen in the Mechanics panel). In its editor: *Measures*
(*Displacement*, *Velocity*, *Acceleration*), *On*, and calibration fields
(*Sensitivity*, *Sensitivity unit*, *Sensor model*) that describe a
physical sensor for measured data; no solver uses them. Sensors appear in
the frequency-response plots (*Response at*) and record transient
histories.

## Electromagnetic probe points

In the **Fields** tab, enter a point in *Probe (world coordinates)* (in
metres) and press **Probe**: the analytical fields of all active radiators
are evaluated there. Field regions give the same quantities over planes and
volumes ([Simulation regions](regions.md)).

## Worked example

Open `examples/mechanics/c-forced-resonance.resonant`: the aluminium
cantilever with an ideal force at its tip and two accelerometers. Run
**Mechanics > Calculate Frequency Response**; choose a sensor under
*Response at* to see its accelerance from 5 to 500 Hz. The peaks fall on
the natural frequencies (compare with **Calculate Modes**) within the sweep
spacing.

For acoustics, `examples/advanced_acoustics/c-diffraction.resonant` has one
probe on the axis of a slit and one in the geometric shadow: after F6 the
shadow probe still records sound, which the analytical model could not
predict.

## Simulated and real sensors

A probe or mechanical sensor is an ideal point: no size, no frequency
response, no noise. The sensors of an experiment - microphones,
accelerometers, RF receivers - are physical devices with sensitivities,
calibrations and uncertainties, defined in the **Experimental Hardware**
panel; the viewport shows them as markers for the selected experiment.
Comparing measured and simulated values is described in
[Simulation correlation](../experiments/correlation.md).

## Limitations

- Probes and sensors are points; there is no line or area averaging.
- Particle-velocity histories are not recorded in full-wave runs; probes record pressure only.
- Mechanical sensors measure along one axis; strain is not reported.
