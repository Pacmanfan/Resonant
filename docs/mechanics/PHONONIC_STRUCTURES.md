# Phononic Structures

**Status:** current for REQ-CHAT-20261126-006. One tractable case: the
longitudinal band structure of a periodic rod. Code:
`src/core/mechanics/Bloch.*`.

## Geometry

`rod.periodic`: `cells` repetitions of segment A (radius `rA`, length `lA`)
and segment B (`rB`, `lB`), one material, along local Z. As a structure it
is a beam (frame elements with the section of their midpoint; segment
boundaries fall on element boundaries when the element size divides the
segment lengths).

## Bloch analysis

The unit cell is discretized with linear rod elements (24 per segment,
consistent mass). With `u(right end) = exp(j q a) u(left end)` (`a = lA +
lB`), the reduced matrices are Hermitian and their eigenvalues are the
frequencies of the Bloch waves of wavenumber `q`, for `q a / pi` from 0 to
1. A stop band is a frequency interval between the top of one band and the
bottom of the next.

## Reference

The classical dispersion relation of a two-layer 1D medium,

```text
cos(q a) = cos(k lA) cos(k lB) - (1/2) (ZA/ZB + ZB/ZA) sin(k lA) sin(k lB),     k = omega / c,  Z ∝ A
```

has no real `q` where the right side exceeds 1 in magnitude. For a steel
rod of 10 / 4 mm radius and 60 / 40 mm segments the finite-element stop
bands are 12 428 - 36 627 Hz and 44 266 - 58 608 Hz; the reference gives
12 428 - 36 612 Hz and 44 232 - 58 563 Hz (within 0.1 %; the test allows
0.5 %). Equal segment lengths close every second gap exactly (the example
uses unequal lengths for that reason).

## Finite structure

Example G: 8 cells, axial drive at one end, response at the other, loss
factor 0.002. In the middle of the first stop band the transmitted
response is below 1/1000 of the largest response in the pass band below
it. This agrees with the Bloch analysis; a band gap is never claimed from
low response alone.

## Limits

1D rod theory: longitudinal waves only (bending and torsional bands are not
calculated; lateral inertia of the segments is neglected, which matters
when the radius is not small against the wavelength). No 2D/3D unit cells,
no locally resonant inclusions, no layered plates.
