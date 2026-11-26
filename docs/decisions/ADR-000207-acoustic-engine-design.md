# ADR-000207: Acoustic engine — conventions, source normalization, free-field model, arrays as one object, results as snapshots

**Status:** ACCEPTED
**Date:** 2026-11-26
**Type:** architecture
**Requirements:** `docs/requirements/acoustics.md` RES-ACOUSTIC-001..020
**Request:** REQ-CHAT-20261126-003
**Builds on:** ADR-000205, ADR-000206

## Decisions

1. **Time convention `e^{+jωt}`,** outgoing waves `e^{-jkr}`, peak complex
   amplitudes. One convention for the whole acoustic module, recorded in
   `Conventions.h` and in every exported file.
2. **Source strength is the pressure amplitude at 1 m on axis.** It is a
   property of each source; nothing is normalized by the number of sources.
   Chosen over volume velocity because it is what a transducer data sheet
   states and what a user can check with one microphone.
3. **Emitters radiate along local +Z.**
4. **The propagation medium is a project setting,** never inferred from
   object materials. Air uses the ideal-gas relations (theoretical); water
   uses fixed nominal values marked illustrative.
5. **One propagation interface, one model for now.** `PropagationModel`
   gives the response of a unit source at a point; `FreeFieldModel`
   implements it. The evaluator, the inverse solver and the field job depend
   on the interface only.
6. **Near-source handling by clamping and flagging,** not by smoothing the
   Green's function: the field is exact outside the sphere and visibly
   marked as invalid inside it.
7. **Intensity from particle velocity.** The time-averaged intensity is
   `½ Re{P V*}` with `V` from the analytical gradient; the plane-wave formula
   exists only as a labelled approximation.
8. **An array is one scene object with generated elements** and a table of
   per-element overrides keyed by index. Thousands of elements do not become
   thousands of scene objects, and layout changes cannot orphan settings.
9. **Phase focusing and time-delay focusing are different settings** stored
   in different fields; applying one clears the other for that element.
10. **Inverse design by SVD with relative Tikhonov regularization.**
    Stable for rank-deficient and ill-conditioned problems, gives the
    minimum-norm or least-squares solution as appropriate, and reports rank
    and condition number.
11. **Regions are scene objects in their own local frame,** so an arbitrary
    plane is a rotated plane region.
12. **Results are immutable snapshots** holding sources, medium and
    assumptions; a field is stored in single precision (8 bytes per sample),
    point values are computed in double precision.
13. **Sampled fields are not written into the project file.** The project
    keeps the configuration and a record of each run; the format reserves
    binary side files for a later version (ADR-000206 decision 9).
14. **Display from stored data.** The complex field is uploaded once as a
    two-channel texture; quantity, scale and animation phase are shader
    uniforms. A generic `ComplexFieldGrid` keeps the render library free of
    acoustic types, so other domains can reuse the layer.
15. **Project-level settings live in `extensions`** and are changed through
    an undoable command; run records are kept under their own key so undoing
    a settings change cannot remove one.

## Deviations from the request

* No separate array-factor routine: the exact element sum is used
  everywhere and contains the array factor as its far-field limit.
* Isosurfaces are not implemented; volumes are shown as slices.
* Temperature dependence is modelled for air only.
* "Compare results" is switching between stored results with their own
  summaries; there is no side-by-side or difference view yet.
