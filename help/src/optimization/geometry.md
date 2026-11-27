---
id: opt.geometry
title: Geometry optimization
category: Optimization
order: 70
status: available
summary: Optimize the parameters of reflectors, lenses, antennas and structures; every candidate is rebuilt from its parametric definition, checked, simulated and exportable with mesh checks.
keywords: [geometry, shape, reflector, parametric, export, STL, manufacturing]
related: [opt.variables, opt.constraints, acoustics.reflectors, opt.topology]
---

Any geometry parameter of a scene object can be a design variable
(`object/<id>/geometry/<parameter>`): reflector focal length, aperture,
semi-axes, curvature radius and thickness; lens, waveguide and cavity
dimensions; antenna length, wire radius, patch and substrate sizes; box and
cylinder dimensions and plate thickness of structures. The **parametric
definition stays the source of truth**: the optimizer changes parameters,
never meshes.

## What happens for each candidate

1. The design is rebuilt from the snapshot taken when the run started.
2. All geometry parameters of an object are set together and the kind's own
   rules are checked. An invalid combination (an aperture larger than twice
   the radius of curvature, for example) is an **invalid design**: recorded,
   infeasible, never simulated.
3. The simulation representation is rebuilt from the definition: reflector
   surface elements at the configured density per wavelength, the structure's
   finite-element mesh, the full-wave grid.
4. The chosen model runs; objectives and constraints are scored.
5. The candidate is stored with its model record.

A result is reused only for an identical design and model: a changed
parameter is a changed design, never a stale result.

## Passive focusing judged by physics

A shape's name does not make a focus. The acoustic adapter evaluates the
actual geometry with the Kirchhoff reflector model (or the full-wave solver)
and reports where the field peaks (`focus.peakOffset`), how wide the focus is
(`focus.width`), its sidelobes and its gain over the free field
(`focus.gain`). Geometry and source variables can be combined in one problem
(reflector parameters together with array phases).

## Worked example

Open `examples/optimization/b-acoustic-reflector.resonant`. A parabolic
reflector (aperture 200 mm, locked; thickness locked) is lit by a source 2 m
away; the target lies 80 mm in front of its vertex. The problem sweeps the
focal length from 50 mm to 110 mm in 31 points with the Kirchhoff model and
maximizes the pressure at the target. The mirror equation predicts

```math
1 / f = 1 / s + 1 / s′ = 1 / 2 m + 1 / 0.08 m   →   f ≈ 76.9 mm
```

with `s` and `s′` the source and image distances. The best candidate lies
within 5 % of it; the remaining difference is the wave nature of the focus
(finite aperture, wavelength 8.6 mm), which the geometric formula ignores.
**Compare Two** on the Candidates tab shows how sharply the pressure depends
on the focal length.

## Export and manufacturing checks

On the Candidates tab, **Export...** writes to a folder the candidate's record
(run, problem, design values, model, lineage, validated or not) and the
meshes of the objects whose geometry it changes, in world coordinates, as
ASCII STL and OBJ in metres. For each mesh it reports:

- closed or not (every edge shared by two triangles after welding), boundary
  and non-manifold edges, degenerate triangles;
- enclosed volume, area and bounds;
- the share of the surface overhanging more than 45° from the build
  direction;
- printability checks of wall thickness, feature size and build volume.

> **Note** A simulated shape is not automatically manufacturable. The export
> says so: the checks are not a manufacturing approval; detail and verify a
> design before fabrication.

## Limitations

- Lenses, waveguides and cavities have no analytical acoustic field model;
  only the full-wave model sees them.
- The Kirchhoff model has one reflection and no shadowing; it degrades when
  surfaces or their radii of curvature are not large against the wavelength.
- Changing an element count changes element indices: per-element variables
  that point beyond the new count give invalid designs, never a silent
  reinterpretation.
- STEP export is not implemented (there is no CAD kernel).

## Related topics

[Design variables](variables.md) · [Constraints](constraints.md) ·
[Reflectors](../acoustics/reflectors.md) · [Topology optimization](topology.md)
