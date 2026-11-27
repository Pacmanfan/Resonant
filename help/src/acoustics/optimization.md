---
id: acoustics.optimization
title: Acoustic optimization
category: Acoustic Engineering
order: 110
status: available
summary: Inverse design of array excitations for several targets, reflector geometry optimization, and the acoustic problems of the Optimization Studio.
keywords: [inverse design, least squares, targets, null, reflector optimization, regularization]
related: [acoustics.focusing, opt.studio, opt.inverse-design, opt.geometry, acoustics.reflectors]
---

Resonant offers four tools for acoustic design, from a direct solution to a
general search:

| Tool | Where | Solves |
|---|---|---|
| Focusing | Acoustics panel, **Focus** tab | one target, closed form ([Focusing](focusing.md)) |
| Inverse design | Acoustics panel, **Inverse Design** tab | element amplitudes and phases for several point targets, by least squares |
| Reflector optimization | reflector's property editor | one geometry parameter for the pressure at the intended focus |
| Optimization Studio | **Optimization > Optimization Studio** | general problems with variables, objectives, constraints and algorithms |

## Inverse design for several targets

Add **Target Point** objects and set, for each, **Desired amplitude** (Pa),
**Desired phase** (deg), **Weight**, or **Suppress the field here (ask for
zero pressure)**. With `H(m, i)` the pressure at target `m` per unit strength
of source `i`, `b` the desired pressures and `q` the weights, the solver
finds the complex strengths `w`:

```math
minimize   Σ_m q_m · |(H w − b)_m|²  +  λ · σ_max² · ‖w‖²
w = V · diag( σ_i / (σ_i² + λ σ_max²) ) · Uᴴ · diag(√q) · b
```

`λ` is the **Regularization**, relative to the largest singular value
squared `σ_max²`, so it is dimensionless; `U`, `σ_i`, `V` come from the
singular value decomposition of the weighted matrix. With `λ` = 0 this is
the least-squares solution (more targets than sources) or the minimum-norm
solution (more sources than targets).

In the **Inverse Design** tab (**Acoustics > Solve Inverse Design...** opens
it):

1. Choose the **Array**, the **Regularization** (default 1e-6), an optional
   **Amplitude limit** (Pa at 1 m; empty = no limit) and the **Accuracy
   tolerance** (%, default 5).
2. **Solve for Targets**. The summary states whether the targets are
   achieved within tolerance, the residual in Pa and in % of the requested
   field, the number of sources and targets, the rank, the condition number
   and the largest strength. The table lists each **Target** with its
   desired and achieved magnitude (Pa), **Magnitude error (%)**, **Phase
   error (deg)** and **Weight**.
3. Read the notes: clipped strengths, dependent targets (rank deficient),
   more targets than sources, or **TARGETS NOT ACHIEVED ACCURATELY**.
4. **Apply to Array** writes amplitudes and phases into the per-element
   settings (undoable). Run a field (**F5**) to see the result.

With the reflector model selected the propagation matrix includes the
reflected paths, and the note says so.

Worked example: `examples/acoustics/d-multiple-target-points.resonant`, an
8 × 8 array driven for three targets at 100 mm: 60 Pa, 40 Pa at +90°, and a
null. After **F5** the probe table shows each within 0.05 Pa of its request.

Refusals: no active sources, no point targets, sources at different
frequencies, a negative regularization. Line, plane, surface and volume
targets are stored but not used by this solver (each is reported as unused).
Phase-only inverse design is not available here.

## Reflector geometry optimization

In a reflector's property editor, group **Optimize for pressure at the
intended focus**: choose the parameter to **Vary**, the range
(**Between**), and **Optimize**. The search (golden section along the
parameter) works on a private copy of the scene with the reflector model; the
project changes only with **Apply Best Geometry**, one undoable step.

It needs the reflector field model, an **Intended focal point** and an
active emitter. The search is local: check the result with a field
simulation.

Worked example: `examples/manipulators/e-geometry-optimization.resonant`. A
source 0.5 m from a parabolic reflector with f = 40 mm, pressure wanted at
70 mm. The optimizer finds f = 58 mm, within 4 mm of the mirror equation's
61.4 mm, and raises the pressure at the target more than threefold. The
optimized reflector is stored hidden beside the original: show one and hide
the other to compare (a hidden reflector is not simulated).

## Acoustic problems in the Optimization Studio

**New from Template** in the Studio's **Problem** tab offers:

- *Acoustic: focus an array on a point (element phases)*: L-BFGS-B on the
  free-field model; the analytical answer is the phase focus.
- *Acoustic: match several targets (element amplitudes and phases)*.
- *Acoustic: reflector geometry for a focal target*: a sweep of the focal
  length with the Kirchhoff model.
- *Acoustic: focal pressure against sidelobes (Pareto)*: NSGA-II.

The **Physical model** can be the analytical free field, the reflector model,
or the full-wave solver on a configured region (a plane region is a 2D
problem; continuous signals only). The quantities include the pressure,
level, phase and gain at the focus, the target field error, focal contrast,
energy outside the focal radius, −6 dB width and peak sidelobe level on a
line through the focus, drive effort and source clearance. Unlike the
Inverse Design tab, the Studio samples line, plane, surface and volume
targets at half a wavelength. Lenses and waveguides act only in the
full-wave model.

Examples: `examples/optimization/a-acoustic-focal-array.resonant` (reaches
the coherent sum `Σ A / r_i` to 0.1 %) and `b-acoustic-reflector.resonant`
(optimum within 5 % of the mirror equation's 76.9 mm). See
[The Optimization Studio](../optimization/studio.md).

## Common mistakes

- Requesting more independent targets than there are elements.
- Setting the regularization to 0 for an ill-conditioned arrangement: the
  strengths become huge. Raise it, or set an amplitude limit.
- Optimizing a reflector with the free-field model: refused, because the
  reflector would have no effect.
