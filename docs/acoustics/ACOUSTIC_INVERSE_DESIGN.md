# Acoustic Inverse Design

**Status:** current for REQ-CHAT-20261126-003.

## Problem

For `N` sources and `M` point targets, the propagation matrix is
`H(m, i) = G(x_m, x_i)`: the pressure at target `m` per unit strength of
source `i` under the selected propagation model. With `b` the desired
complex pressures and `q_m` the target weights, the solver finds the
complex source strengths `w` that

```text
minimize   Σ_m q_m |(H w − b)_m|²  +  λ σ_max² ‖w‖²
```

`λ` is **relative** to the largest singular value squared of the weighted
matrix, so it is dimensionless.

## Method

The row-weighted matrix `diag(√q) H` is decomposed by a singular value
decomposition (Eigen `BDCSVD`). The solution is
`w = V diag(σ_i/(σ_i² + λσ_max²)) Uᴴ diag(√q) b`. No matrix is inverted.
With `λ = 0` this is the pseudo-inverse: the least-squares solution when
there are more targets than sources, the minimum-norm solution when there
are more sources than targets. Singular values below
`σ_max · 10⁻¹³ · max(M, N)` are treated as zero.

## What the solver reports

Per-source complex strengths (converted to amplitude and phase when
applied), the field achieved at each target, the residual norm in pascal,
the residual relative to `‖b‖`, the rank, the condition number, whether an
amplitude limit clipped any strength, whether the result is within the
accuracy tolerance, and notes in plain language:

* strengths were clipped to the amplitude limit (the achieved field is
  recalculated with the clipped strengths);
* the targets are not independent for this arrangement (rank deficient);
* there are more targets than sources;
* **the requested values are not achieved accurately**, with the residual.

## Refusals

No active sources; no point targets; sources at different frequencies; a
propagation matrix that is zero or not finite; a negative regularization.

## Targets

Point targets are solved: a desired amplitude and phase, a weight, or
"suppress" (zero pressure). The data model also has line, plane, surface
and volume targets with an extent; no solver uses them yet, and the
application reports each such target as unused instead of treating it as a
point.

## Scope

Amplitude and phase control at one frequency under the free-field model.
Phase-only optimization, constraints other than a magnitude clip, and
region targets are not implemented.
