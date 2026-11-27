# Structural Modal Analysis

**Status:** current for REQ-CHAT-20261126-006. Code:
`src/core/mechanics/Eigensolver.*`, `Analyses.cpp` (`solveModal`).

(The acoustic counterpart, resonance identification from responses, is
`docs/advanced_acoustics/MODAL_ANALYSIS.md`. This is a true eigenvalue
analysis.)

## Problem

```text
K phi = omega^2 M phi
```

over the free dofs. The lowest `n` pairs (setting, default 12).

## Solvers

* Up to 1200 free dofs: dense generalized symmetric eigensolver (all
  eigenpairs).
* Above: shift-invert Lanczos with full re-orthogonalization in the M inner
  product. One sparse factorization of `K - sigma M`, with `sigma` slightly
  below zero so that free structures (rigid-body modes at zero frequency)
  factorize; Krylov basis of `(K - sigma M)^-1 M`, Ritz values from the
  tridiagonal matrix, basis enlarged until every requested pair converges.
  Fixed start vector: reproducible.

Every pair carries its backward error
`||K phi - lambda M phi|| / ((||K|| + |lambda| ||M||) ||phi||)` (below 1e-8
in all tests). The two solvers agree to 1e-7 in frequency.

## Outputs

* Natural frequency (Hz).
* Mode shape, mass-normalized (`phi^T M phi = 1`), sign fixed so the largest
  component is positive. **Its amplitude is arbitrary**; results say so.
* Effective mass fraction per world direction, `Gamma^2 / m_total` with
  `Gamma = phi^T M r` for rigid translation `r`, and the cumulative sum.
* A warning when the highest mode has fewer than 6 elements per structural
  wavelength.

## Display

The mode shape deformed by a magnification that makes its largest
displacement 10 % of the structure (or a factor the user sets), the
undeformed outline, colour by displacement magnitude or a signed component
(diverging colours: white where a component changes sign — nodal lines),
animation `phi cos(phase)` with a fixed colour scale. The display text
states that the amplitude is arbitrary and gives the magnification.
Changing any display setting never recalculates.

## Validation (`MECHANICAL_VALIDATION.md`)

Euler-Bernoulli beams in four support cases (first four bending modes,
1e-4), rod axial and torsional modes (1e-3), simply supported plate (Navier,
1 %), clamped plate (Leissa 35.99, 1 %), rectangular membrane (1 %), solid
bar axial (1e-3), plate nodal line on the symmetry axis (1e-6 of the
largest displacement), mesh convergence of beams (`h^4`) and plates (`h^2`).
