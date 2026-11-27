---
id: opt.objectives
title: Objectives
category: Optimization
order: 30
status: available
summary: What an optimization wants: the quantity, the goal, the normalization scale, weights and Pareto fronts, and the quantities each physics adapter calculates.
keywords: [objective, goal, scale, weight, Pareto, quantity, score]
related: [opt.constraints, opt.algorithms, opt.studio, opt.multi-fidelity]
---

An objective names a **quantity** that the problem's physics adapter
calculates and says what is wanted of it. Resonant turns it into a
dimensionless **score** to minimize, using your normalization **scale** (in
the quantity's unit). The scale is never guessed: it says how large a
difference matters to you.

## Goals and scores

```math
minimize:   score = v / scale
maximize:   score = −v / scale
target:     score = |v − t| / scale
at-most:    score = max(0, v − t) / scale
at-least:   score = max(0, t − v) / scale
```

`v` is the quantity's value for a candidate and `t` the **Target**. With one
objective the score is minimized. With several, Pareto methods (NSGA-II)
compare the scores separately; single-objective methods minimize the
weighted sum Σ weight · score (plus soft-constraint penalties). A run with
several objectives always reports its **Pareto front** and names no single
best candidate.

## The Objectives tab

The table has **Quantity**, **Goal** (minimize, maximize, target, at-most,
at-least), **Target**, **Weight**, **Scale** and **Unit**. Choose a quantity
in the list below it (what the physics adapter calculates; its definition is
shown under the list), then **Add Objective** or **Add Constraint**.
**Remove** deletes the selected objective. A quantity the chosen physical
model does not calculate is refused when the problem is applied.

On the **Trade-offs** tab choose two objectives for **x:** and **y:**: the
feasible candidates are plotted, the non-dominated ones (the Pareto front)
highlighted. Type **Priorities** (one weight per objective, for example
`1, 0.5`) and press **Rank** to order the candidates after the run, feasible
ones first.

## Acoustic quantities

| Key | Unit | Meaning |
|---|---|---|
| focus.pressure, focus.pressureDb | Pa, dB re 1 Pa | the pressure magnitude at the focus target |
| focus.phase | rad | its phase |
| focus.gain | 1 | the field at the focus over the free field of the same sources |
| targets.error, targets.magnitudeError, targets.phaseErrorRms | 1, 1, rad | weighted relative error against every target (complex, magnitude only, phase) |
| targets.minPressure, suppression.maxPressure | Pa | the weakest focal sample; the strongest suppression sample |
| focus.contrastDb, focus.energyOutside | dB, 1 | focus against the region outside the focal radius |
| focus.peakOffset | m | distance from the focus to where the field actually peaks |
| focus.width, focus.sidelobeDb | m, dB | width at half the peak and peak sidelobe on the focal line |
| region.peakPressure | Pa | the largest pressure in the region |
| sources.effort, sources.maxAmplitude, sources.clearance | Pa², Pa, m | drive effort, the largest source, the smallest gap between sources |
| manipulators.minThickness, manipulators.maxExtent | m | for manufacturing limits |

## Mechanical quantities

| Key | Unit | Meaning |
|---|---|---|
| modal.f1 ... modal.fK | Hz | the K lowest natural frequencies (6 by default) |
| modal.clearance | Hz | the distance of the nearest mode from a frequency to avoid |
| modal.massFractionZ1 ... | 1 | effective modal mass along Z over the total |
| mass | kg | total mass |
| vibration.targetError, vibration.suppressionMax, vibration.targetMin, vibration.sensorMax | 1 or m | response at vibration targets and sensors at the harmonic frequency |
| static.maxDisplacement, static.maxVonMises, static.stressConcentration, static.compliance | m, Pa, 1, J | the actuator forces as static loads |
| actuators.effort | N² | sum of the squared force amplitudes |

## Electromagnetic quantities

| Key | Unit | Meaning |
|---|---|---|
| pattern.directivityDbi | dBi | directivity of the array |
| pattern.levelDb/label, pattern.gainDbi/label | dB, dBi | level and gain toward a configured direction |
| pattern.sidelobeDb | dB | the highest level outside the main lobe |
| pattern.beamwidthThetaDeg, pattern.beamwidthPhiDeg, pattern.mainLobeErrorDeg | deg | beamwidths; pointing error toward the first direction |
| array.effort, array.maxWeight, array.minSpacingWavelengths | 1 | weights; the smallest spacing in wavelengths |
| antenna.inputResistance, antenna.inputReactance, antenna.returnLossDb, antenna.vswr, antenna.gainDbi, ... | Ω, dB, 1, dBi | dipole, monopole and small-loop antennas |

Sampled quantities (lines, planes, regions, pattern spheres) depend on their
sampling, which is part of their definition; the Objectives tab shows the
exact definition of each.

## Worked example

`examples/optimization/d-mechanical-resonator.resonant` has one objective:
*First natural frequency*, `modal.f1`, goal *target*, target 25 Hz, scale
0.25 Hz. A candidate at 25.5 Hz scores 2; the best candidate found lies within
0.25 Hz of 25 Hz (score below 1), within the mass and thickness constraints.

`examples/optimization/f-sidelobe-reduction.resonant` maximizes
`pattern.directivityDbi` and minimizes `pattern.sidelobeDb`: the result is a
front of at least eight non-dominated tapers, reaching below −20 dB sidelobes
at one end and close to the uniform array's 10 dBi at the other.

## Common mistakes

- Leaving every scale at 1 when the quantities differ by orders of
  magnitude: the weighted sum then follows the largest one.
- Comparing scores between domains: the same number means nothing in common.
- Expecting one best design from two competing objectives: choose on the
  front by priority.

## Related topics

[Constraints](constraints.md) · [Algorithms](algorithms.md) ·
[The Optimization Studio](studio.md) · [Multi-fidelity optimization](multi-fidelity.md)
