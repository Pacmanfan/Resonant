---
id: opt.inverse-design
title: Inverse design
category: Optimization
order: 60
status: available
summary: Find the source weights that produce a wanted field at target points by regularized linear least squares, with bounds, sparsity and an honest classification of the result.
keywords: [inverse design, least squares, Tikhonov, regularization, source weights, nulls]
related: [opt.studio, opt.algorithms, acoustics.optimization, em.beamforming]
---

For linear physics at one frequency, the field at the targets is a linear
function of the complex source weights. Inverse design solves for the weights
directly, without searching: it is fast, exact for the model, and states
whether the wanted field can be reached at all.

## The problem

```math
achieved = A w
minimize  Σ_m q_m |(A w − b)_m|² + (λ + μ) σ_max² ‖w‖²
subject to |w_i| ≤ W          (when a largest weight is given)
           w_i = 0, i ∉ S      (when the active sources are limited)
```

`A` (targets × sources) holds the response of each target to a unit source,
`w` the complex weights, `b` the wanted values, `q_m` the target weights,
`λ` the **Regularization** and `μ` the **Power penalty**, both relative to the
largest singular value squared `σ_max²`, so they are dimensionless and
comparable between problems. `‖w‖²` is proportional to the drive power of
identical sources: `μ` is an engineering trade-off, `λ` a numerical
stabilization. Both bias the solution toward smaller weights, and the result
says so.

## Running it

1. Create a problem whose sources are design variables, for example from the
   template *Acoustic: match several targets* or *Mechanical: suppress
   vibration at the targets*, and choose a physical model that has a response
   matrix (below).
2. Open the **Analysis** tab, group *Inverse source design (linear least
   squares)*.
3. Choose **Method**: *Regularized (Tikhonov)* (by the singular value
   decomposition; nothing is inverted) or *Minimum norm (pseudo-inverse)*
   (the exact solution of smallest norm for a consistent system).
4. Set **Regularization** (10⁻⁶ by default), **Power penalty** (0),
   **Largest weight** (0 = no bound; otherwise a bounded problem solved by an
   accelerated projected gradient method) and **Active sources** (0 = all;
   otherwise orthogonal matching pursuit picks at most that many sources).
5. Press **Solve**.

The text shows the classification, the relative residual, the condition
number, the rank, the number of active sources and whether weights sit at the
bound, then the notes. The solution is **evaluated with the problem's
physical model** and stored as a candidate of a new *linear-inverse* run on
the Candidates tab, where it is compared, re-evaluated and promoted like any
other candidate.

## Classification

- an exact solution (the residual is at numerical precision);
- an exact solution of smallest norm of an underdetermined system (other
  exact solutions exist);
- an approximate least-squares solution;
- an approximate solution that misses the targets: *the wanted field is not
  reachable with these sources*.

The notes state the regularization bias, ill-conditioning and rank
deficiency, and that the solution is exact for the model only.

## Where the response matrix comes from

| Adapter | Sources | Targets | Response |
|---|---|---|---|
| acoustic | emitters and array elements at the analysis frequency | point targets (amplitude and phase, or zero to suppress); line, plane, surface and volume targets sampled at half a wavelength | free field, or free field plus a Kirchhoff reflection; the full-wave model has no matrix (re-evaluate the solution with it) |
| mechanical | actuators | vibration targets (displacement along their axis, zero to suppress) | the finite-element transfer at the harmonic frequency |
| electromagnetic | array elements | configured directions with a wanted level (dB) or a null | element pattern × array factor |

The weights become design values: the amplitude and phase of each source,
with delays set to zero (at one frequency a delay is a phase).

## Worked examples

- `examples/optimization/c-mechanical-vibration-suppression.resonant`: two
  actuators on a simply supported aluminium plate at 120 Hz, a quiet point
  and a point that should move 2 µm. Solve gives the actuator forces
  directly; Nelder-Mead on the same problem reduces the target error more
  than 20-fold from the initial design.
- Electromagnetic null placement: the minimum-norm solution puts a null more
  than 100 dB down, confirmed by evaluating the resulting pattern.
- The framework reproduces the acoustic module's own inverse solution to
  10⁻⁹ and the mechanical vibration targeting to 10⁻⁸.

## Common mistakes

- Solving with the full-wave acoustic model selected: it has no response
  matrix; select the free-field or reflector model, solve, then re-evaluate.
- Reading a small residual as a good design: the residual is measured only at
  the targets; check the field elsewhere (sidelobes, energy outside).
- A large regularization chosen to hide ill-conditioning: the result then
  misses the targets by design, and says so.

## Related topics

[The Optimization Studio](studio.md) · [Algorithms](algorithms.md) ·
[Acoustic optimization](../acoustics/optimization.md) · [Beamforming](../electromagnetics/beamforming.md)
