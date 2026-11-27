# Demonstration catalog

**Requirements:** RES-DEMO-009, RES-DEMO-010. Generated from `demos/profiles` and
`demos/sequences` at the time of writing; the profiles are the source.

Every demonstration was run unattended from a clean application state by
`tests/app/test_demo_studio.cpp` (and `resonant --run-demo <id>`): all complete,
with the results listed below calculated by Resonant in the run.

| Demonstration | Status | Domain | Project | Steps | Minutes |
|---|---|---|---|---|---|
| A. Resonant overview (`demo-a-overview`) | available | cross-domain | `examples/acoustics/c-focused-planar-array.resonant` | 13 | 5 |
| B. Acoustic focusing (`demo-b-acoustic-focusing`) | available | acoustic | `examples/acoustics/b-linear-array.resonant` | 11 | 6 |
| C. Passive acoustic focusing (`demo-c-passive-focusing`) | available | acoustic | `examples/manipulators/a-parabolic-reflector.resonant` | 8 | 5 |
| D. Mechanical resonance (`demo-d-mechanical-resonance`) | available | mechanical | `examples/mechanics/a-vibrating-beam.resonant` | 13 | 6 |
| E. RF antenna engineering (`demo-e-rf-antennas`) | available | electromagnetic | `examples/electromagnetics/a-half-wave-dipole.resonant` | 12 | 6 |
| F. Inverse design (`demo-f-inverse-design`) | available | optimization | `examples/optimization/a-acoustic-focal-array.resonant` | 10 | 7 |
| G. Experimental correlation (`demo-g-experimental-correlation`) | partial | experimental | `examples/experiments/rf_experiments.resonant` | 7 | 5 |
| H. Python automation (`demo-h-python-automation`) | available | automation | `examples/acoustics/b-linear-array.resonant` | 8 | 5 |

## A. Resonant overview

A tour of one real project: the scene, the 3D viewport, a calculated field, and where mechanics and electromagnetics live.

- **Problem:** Engineers who shape waves need one place to build a design, calculate what it does and see it.
- **Calculated:** The free-field pressure of the 64 elements on the field plane (analytical superposition of point sources).
- **Assumptions:** Free field (no walls), linear acoustics, ideal point-source elements at 40 kHz in air.
- **Measured in a run:** 64-element field on the focal plane; a saved copy of the project in the run folder.
- **Steps:** open → layout → camera → select → explain-scene → field → decibels → view-field → explain-field → mechanics → em → save → help

## B. Acoustic focusing

A 16-element linear array, first with all elements in phase, then focused by phase on a target: the same hardware, a different field.

- **Problem:** Concentrate ultrasound at a chosen point (haptics, levitation, sensing) with a fixed array.
- **Calculated:** The element phases that make the waves arrive in phase at the target, and the pressure field before and after.
- **Assumptions:** Free field, omnidirectional point-source elements, phase control at one frequency (40 kHz).
- **Measured in a run:** the field of the 16-element array uniform, then focused on a target 5 cm away (the maximum moves to the target).
- **Steps:** open → layout → camera → uniform → decibels → explain-uniform → target → focus → focused → inspect → explain-focus

## C. Passive acoustic focusing

A parabolic reflector gathers the sound of a distant source into its focus: focusing by geometry and material, no array needed.

- **Problem:** Concentrate sound without an array: with a shaped, reflecting surface.
- **Calculated:** The field scattered by the reflector with the Kirchhoff (physical-optics) reflector model plus the incident field.
- **Assumptions:** Kirchhoff approximation: the surface is large compared to the wavelength, no multiple reflections, rigid reflector.
- **Measured in a run:** the Kirchhoff reflector model's field: a maximum at the paraboloid's focus.
- **Steps:** open → layout → select → camera → explain-setup → field → decibels → explain-field

## D. Mechanical resonance

An aluminium cantilever: its natural frequencies and mode shapes, then its forced response near the first resonance.

- **Problem:** Know at which frequencies a part resonates, and how it moves there, before it is built.
- **Calculated:** The natural frequencies and mode shapes (finite-element modal analysis), then the steady forced response to a tip force.
- **Assumptions:** Linear elasticity, small displacements, the beam idealization and mesh of the project, light damping.
- **Measured in a run:** natural frequencies 16.33, 98.0, 102.4, 286.6, 482.1, 561.6 Hz; the forced response of the shaker-driven beam.
- **Steps:** open → layout → camera → modal → mode1 → explain-mode1 → mode2 → mode3 → explain-modes → open-forced → harmonic-tab → harmonic → explain-resonance

## E. RF antenna engineering

A half-wave dipole's impedance and 3D radiation pattern, then an 8 x 8 phased array steered to another direction.

- **Problem:** Know how an antenna is matched and where it radiates, and point an array's beam without moving it.
- **Calculated:** The dipole's induced-EMF impedance and directivity and its pattern; the array factor pattern before and after steering.
- **Assumptions:** Thin-wire sinusoidal-current dipole; array factor of identical elements without mutual coupling; far field.
- **Measured in a run:** dipole directivity 1.641 (2.15 dBi); the 8 x 8 array 22.67 dBi steered to (30°, 45°), 23.21 dBi at broadside.
- **Steps:** open → layout → analyse → pattern → camera → explain-dipole → open-array → array-pattern → steer → broadside → camera-array → explain-steering

## F. Inverse design

The Optimization Studio searches the phases of an array for the strongest focus, compares the candidates and promotes the best one into the design.

- **Problem:** Find a design that meets a goal without trying every variant by hand.
- **Calculated:** Candidate designs, each evaluated with the acoustic model, searched by the problem's algorithm.
- **Assumptions:** The objective is evaluated with the analytical free-field model; the algorithm and its seed are those of the saved problem.
- **Measured in a run:** 433 candidates evaluated; the best promoted as a design revision; the promoted design's field.
- **Steps:** open → layout → explain-problem → run → candidates → explain-candidates → promote → layout-field → field → camera

## G. Experimental correlation

A reflection measurement (Touchstone S-parameters, clearly labelled synthetic) is imported and compared with a dipole's simulated reflection.

- **Problem:** Check a design against measurements: how far is the model from what the instrument recorded?
- **Calculated:** The antenna's simulated input reflection over the measured frequencies and the difference to the measured S11.
- **Assumptions:** The imported file is SYNTHETIC example data generated from a series RLC model, not a measurement; the dipole is the analytical model.
- **Measured in a run:** an S11 comparison record (normalized mean-square error 0.184, magnitude correlation 0.871, largest error 12.1 dB) between the synthetic file and the dipole model.
- **Not yet part of it:** applying a calibration record to the data before the comparison; a live acquisition: every bundled experiment drives outputs, which a demonstration never authorizes
- **Data:** examples/experiments/rlc_antenna_and_line.s2p (SYNTHETIC)
- **Steps:** open → layout → antenna → import → explain-import → compare → explain-compare

## H. Python automation

The Python console changes the open project, runs a simulation with the API, exports the result and applies the change as one undoable step.

- **Problem:** Repeat a study, sweep a parameter or connect Resonant to other tools without clicking.
- **Calculated:** In Python: the focusing phases and the field on the plane (the same C++ models as the application).
- **Assumptions:** Python 3 with NumPy and the resonant package; scripts are not sandboxed (they run with your permissions).
- **Measured in a run:** the console adds a target, focuses, calculates and exports `python-field.csv`, and applies the change (undoable).
- **Steps:** open → layout → explain-console → script → field-panel → field → camera → explain-undo
