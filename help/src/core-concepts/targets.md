---
id: concepts.targets
title: Targets
category: Core Concepts
order: 60
status: available
summary: Targets say what field you want where - a pressure at a point, a null, a vibration amplitude - and are used by focusing, inverse design, targeting and optimization.
keywords: [target, focus, inverse design, null, suppression, weight, vibration target]
related: [acoustics.focusing, opt.inverse-design, opt.objectives, concepts.sensors, concepts.arrays]
---

A **target** states a wish: *this pressure here*, *silence there*,
*10 micrometres of motion at this point*. Solvers that design source
settings read targets; field solvers only report the value found at them.
A target never claims that the wish is met: the achieved value is always
calculated and shown next to the desired one.

## Acoustic target points

**Acoustics > Target Point** adds a target 100 mm above the origin. Its
editor (**Properties > Acoustic target**):

| Field | Unit | Meaning |
|---|---|---|
| Kind | | *Point*; *Line*, *Plane*, *Surface* and *Volume* can be stored but are marked *not solved yet* |
| Desired amplitude | Pa | the peak pressure wanted at the point |
| Desired phase | deg | the phase wanted there |
| Weight | | relative importance among several targets (greater than zero) |
| Suppress the field here | | ask for zero pressure (a null) |

The viewport draws a target as a small cross; a suppression target has its
own colour.

### Who uses acoustic targets

| Tool | How |
|---|---|
| **Acoustics > Focus Array on Target...** (Focus tab) | the target's position: element phases or delays are set so all waves arrive together there |
| **Acoustics > Solve Inverse Design...** (Inverse Design tab) | every point target: desired amplitude and phase, weight, suppression |
| **Analysis > Field Analysis** | lists each target's achieved pressure beside the desired one (*Include targets*), and measures the focus distance from the first target |
| Full-wave solver | a target inside the region records the pressure over time, like a probe |
| [Optimization Studio](../optimization/studio.md) | objectives can ask for the pressure at a target |

### Inverse design

With `N` sources and `M` point targets, the solver finds the complex
source strengths `w` that best meet the desired pressures `b`:

```math
minimize  Σ_m q_m · |(H·w − b)_m|²  +  λ · σ_max² · ‖w‖²
```

`H` is the propagation matrix (its entry `H(m, i)` is the pressure at
target `m` per unit strength of source `i`), `q_m` the target weights, `λ` the
relative *Regularization* of the Inverse Design tab and `σ_max` the
largest singular value of the weighted matrix. The tab reports, for each
target, *Desired*, *Achieved*, *Magnitude error (%)* and *Phase error
(deg)*, and says plainly when the requested values are not achieved
accurately. **Apply to Array** writes the amplitudes and phases to the
array's elements.

## Mechanical vibration targets

**Mechanics > Add Vibration Target** places a target on a structure:
*Wanted displacement* (um), *Phase* (deg), *Weight*, *Quiet point (wanted
displacement zero)* and *On*. The displacement is along the target's local
+Z axis. The Mechanics panel's **Targeting** tab solves the actuator
inputs at a chosen *Frequency* with a *Regularization* (**Solve Actuator
Inputs**) and writes them with **Apply to Actuators**.

## Other design intents

- A reflector's *Intended focal point* and *Intended source position* (Wave Manipulators editor) are design intents, not target objects; the reflector optimization and the focus metrics use them.
- Electromagnetic beam and null directions are entered as a table in the **Electromagnetics** panel's *Array* tab (beam synthesis), not as scene objects.

## Worked example

Open `examples/acoustics/d-multiple-target-points.resonant`: an array
driven by an inverse-design solution for three targets 100 mm above it -
60 Pa at x = -20 mm, 40 Pa with a phase of +90 degrees at x = +20 mm, and
a null between them. Press F5; **Analysis > Field Analysis**
shows each target within 0.05 Pa of its desired value. Open the Inverse
Design tab, raise the *Regularization* and solve again: the strengths
shrink and the targets are met less exactly.

`examples/mechanics/f-vibration-concentration.resonant` does the same for
a plate: four actuators, one 10 um target and two quiet points at 150 Hz.

## Common mistakes

- **More targets than sources:** the solver can only find the best compromise and says the values are not achieved accurately.
- **Expecting a focus at the target from focusing alone:** focusing aligns arrivals; a small aperture far away puts the largest pressure near the array.
- **Using line, plane or volume targets:** they are stored but no solver uses them yet; place several point targets instead.

## Limitations

- Acoustic inverse design controls amplitude and phase at one frequency under the free-field model; phase-only design is not implemented.
- The only constraint on strengths is an optional *Amplitude limit* that clips them.
