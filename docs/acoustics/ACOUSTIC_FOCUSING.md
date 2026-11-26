# Acoustic Focusing

**Status:** current for REQ-CHAT-20261126-003.

For an element at `x_i` and a target `x_t`, `r_i = ‖x_t − x_i‖`. The
element's wave arrives with phase `−k r_i` after a travel time `r_i/c`.

## Two controls, never mixed up

| | Narrowband phase | True time delay |
|---|---|---|
| Setting | phase `φ_i = k r_i mod 2π` | delay `τ_i = (r_max − r_i)/c ≥ 0` |
| Effect | cancels the propagation phase at **one** frequency | every wavefront arrives at the same instant |
| At another frequency | wrong | still aligned |
| Stored as | per-element phase override (delay override cleared) | per-element delay override (phase override cleared) |

A delay is not stored as a phase and a phase is not presented as a delay.
Test `TimeDelayFocusIsBroadbandAndPhaseFocusIsNot` shows the difference: the
delays keep the arrivals aligned at 0.6 and 1.7 times the design frequency,
the phases do not.

## Amplitude taper

Optional weights across the aperture as a function of the distance `d` of
an element from the array's centroid, with `d' ` one element pitch beyond
the largest `d`: uniform (1), Hann `½(1 + cos(π d/d'))`, Hamming
`0.54 + 0.46 cos(π d/d')`, cosine `cos(π d/2d')`. A non-uniform taper sets
each element's amplitude to the default amplitude times its weight.

## Workflow

1. Select an array. 2. Give a target: a target object or coordinates.
3. Choose phase or time-delay control (and a taper). 4. Calculate: the
table shows each element's distance, phase or delay, and weight; nothing in
the scene changes. 5. Apply: one undoable step. 6. Run a field simulation.
7. Inspect the field and the value at the target.

## What focusing does not promise

Focusing aligns the arrivals. It does not guarantee a focus. A small
aperture far from its target focuses weakly, and the largest pressure in a
region is then next to the array, not at the target. The field is always
calculated with the propagation model and that is what the application
shows. At the target the result equals `Σ A_i/r_i` (the largest magnitude
these sources can produce there), which the tests verify.
