---
id: mechanics.optimization
title: Mechanical optimization
category: Mechanical Engineering
order: 90
status: available
summary: Solve actuator inputs for vibration targets, tune structures in the Optimization Studio, and try 2D topology optimization.
keywords: [targeting, vibration suppression, frequency tuning, optimization, topology, actuator inputs]
related: [opt.studio, opt.inverse-design, opt.topology, mechanics.actuators, opt.multi-fidelity]
---

Three tools change a mechanical design to meet a goal:

| Tool | Changes | Where |
|---|---|---|
| Vibration targeting | the actuators' amplitudes and phases, at one frequency, in one linear solve | Mechanics panel, **Targeting** tab |
| Optimization Studio, mechanical adapter | geometry, thickness, supports, materials, damping, actuators | **Optimization > Optimization Studio** |
| Topology optimization | the material layout of a 2D plane-stress region | Optimization Studio, **Topology** tab |

## Vibration targeting

At one frequency the structure is linear: the displacements t at the
vibration targets follow from the actuator forces f through a transfer
matrix H, calculated with one factorization of the dynamic stiffness. The
forces are found by weighted, regularized least squares:

```math
minimize  ‖W (H f − t_wanted)‖² + (λ σ_max)² ‖f‖²
```

W holds the targets' weights, λ the **Regularization** and σ_max the
largest singular value of H.

1. Place actuators and vibration targets on the structure
   ([Actuators](actuators.md)); mark points that must stay
   still as **Quiet point**.
2. In the **Targeting** tab set the **Frequency** (Hz) and the
   **Regularization**, and press **Solve Actuator Inputs**.
3. The table lists each target's **Wanted (um)**, **Achieved (um)** and
   **Phase (deg)**; the text gives each actuator's force and phase, the
   relative residual and notes (more targets than actuators, a nearly
   singular transfer matrix).
4. **Apply to Actuators** writes the forces into the actuators as one undo
   step. Nothing changes before that.

**Worked example.** `examples/mechanics/f-vibration-concentration.resonant`:
four actuators on a simply supported aluminium plate, one 10 µm target and
two quiet points at 150 Hz, loss factor 0.01. The stored (solved) inputs
give 10 µm at the target within 1 % and less than 1 % of that at the quiet
points. Driven plainly (1 N each, in phase) the quiet points move more than
10 % of the target. Nothing guarantees that a wanted pattern is reachable;
check the residual.

## The mechanical adapter of the Optimization Studio

Choose **Mechanical** templates under **New from Template**:
*Mechanical: tune the first natural frequency* and *Mechanical: suppress
vibration at the targets (actuator forces)*. The physical models are:

| Model | Fidelity | What it is |
|---|---|---|
| mechanical.analytical | analytical | Euler-Bernoulli box and cylinder beams, simply supported Kirchhoff box plates; frequencies and mass only |
| mechanical.fe.coarse | reduced | the finite-element study with elements twice the size |
| mechanical.fe | high fidelity | the finite-element study as configured |

Variables include the structure's geometry and thickness, its supports
(free, pinned, clamped), a library material, hypothetical scalings of
Young's modulus and density (labelled HYPOTHETICAL in every evaluation),
the loss factor, the harmonic frequency and the actuators' amplitude,
phase, delay, activity and position. Quantities:

| Quantity | Unit | Meaning |
|---|---|---|
| modal.f1 ... modal.fK | Hz | the lowest natural frequencies |
| modal.clearance | Hz | distance of the nearest natural frequency from a frequency to avoid |
| mass | kg | total mass |
| vibration.targetError | - | weighted relative error at the vibration targets |
| vibration.suppressionMax | m | largest motion at the quiet points |
| static.maxDisplacement, static.maxVonMises | m, Pa | static results of the actuator forces |
| actuators.effort | N² | sum of the squared force amplitudes |

**Worked example: a tuned resonator.**
`examples/optimization/d-mechanical-resonator.resonant` tunes an aluminium
cantilever to a first natural frequency of 25 Hz by its length (0.3 -
0.8 m), its thickness (3 - 10 mm in 0.5 mm stock steps) and a library
material, with at most 250 g and at least 4 mm thickness for the clamp.
Differential evolution screens 1200 candidates with the analytical model;
re-evaluate the best with *mechanical.fe* (**Re-evaluate with Model** in
the Candidates tab). The best candidate meets 25 Hz within 1 % and stays
within 2 % with the finite elements.

**Worked example: suppression.**
`examples/optimization/c-mechanical-vibration-suppression.resonant` chooses
the amplitudes and phases of two actuators on a plate at 120 Hz so that a
quiet point stays still and another point moves 2 µm (Nelder-Mead, finite
elements). The run lowers the target error below 5 % of its starting value.
The same problem has a direct linear solution (**Inverse source design** in
the Analysis tab, or the Targeting tab above).

## Topology optimization

The Studio's **Topology** tab distributes material in a 2D plane-stress
region for the stiffest structure of a given volume (SIMP with a density
filter): choose the **Benchmark** (*Cantilever (load at mid-height)* or
*Half MBB beam*),
**Elements x**, **Elements y**, **Volume fraction**, **Penalty**, **Filter
radius** and **Iterations**, press **Run**, and **Export STL...** the
cleaned, extruded result. It is a proof of concept, not a finished design;
see [Topology optimization](../optimization/topology.md).

## Limitations

- Linear, small displacements; the analytical model has no forced
  response or stress, and free structures show rigid-body modes near 0 Hz
  in the finite-element frequencies.
- Shape optimization of arbitrary geometry is not available: only the
  parameters of the structured shapes can change.
- Topology optimization is 2D, static and single-load-case only.
