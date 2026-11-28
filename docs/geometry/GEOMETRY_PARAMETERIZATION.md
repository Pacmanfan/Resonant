# Geometry Parameterization

**Status:** implemented (RES-FUNC-002..004, ADR-000221): `src/core/functional/Parameters`, `Dependencies`.

## Parameters

A functional structure names the parameters that control its behavior:

| Kind | Value | Changed by |
|---|---|---|
| bound | a scene object's geometry parameter (`object/<id>/geometry/<key>`) or position (`object/<id>/position/<x\|y\|z>`): the optimization framework's scene bindings | the geometry kind's own rules (range, integer, joint validity), applied to the object |
| free | stored in the structure (a wavelength, a reflectivity, an operating temperature) | the structure |
| derived | an expression of other parameters (`1 - L / R1`, `d1 + d2`), evaluated with `optimization/Expression` | never set: computed; cycles are refused |

Every parameter has an id (an identifier usable in expressions), a name, a
unit, a range (the structure's range narrowed by the geometry kind's), a
default and a group. Setting several parameters applies them together, so a
joint rule (an aperture no larger than twice the curvature radius) sees the
final values. A change through the services is one undoable step that may
change several objects and the structure together; a stale expected
revision is a conflict.

Examples: cavity `a`, `b`, `d` (bound), `temperature` (free), `aspect = d / a`
(derived); resonator separation `L` (bound to a mirror's position), radii
(bound), reflectivity `R` and `wavelength` (free), `g1`, `g2` (derived); horn
`length`, `mouth`, `profile`, `wall` (bound); beam `length`, `thickness`
(bound), `slenderness` (derived).

## Revisions and staleness

Results record the content keys (FNV-1a of canonical JSON) of what they depend on:

| Key | Content |
|---|---|
| geometry | each referenced object's geometry kind and parameters, component settings, material id and world placement |
| materials | each referenced material's full record |
| definition | the structure document |
| discretization | the analysis kind, model, model version and settings |

```text
 analyze ──► result {dependencies: keys}                     (project revision r)
 edit a parameter / a material / the structure ──► keys change
 GetFunctionalResult ──► compares recorded and current keys ──► stale: [geometry: "the geometry … of 'X-band cavity' changed"]
 undo ──► keys equal again ──► current
```

Keys survive save and load and see changes from any client; a counter could
not. A job works on a copy of the project; if the project changed while it
ran, its result is marked `staleOnCompletion`. Simulation geometry (a
prepared finite-element study) is cached under the geometry key plus the
discretization key and reused only while both are unchanged
(`FunctionalExamples.MechanicalResonator…ReusesItsMesh…`).

## Predictable updates

Applying parameters never edits tessellation or meshes directly: the scene
object changes, the viewport retessellates it, and each model builds its own
discretization from the new geometry on its next run.

## Limits

Parameters bind to parametric geometry kinds, positions and structure
values; imported meshes or CAD features cannot be parameterized (no CAD
kernel). Rotations and material choices are not yet parameter kinds of
structures (the optimization framework has them).
