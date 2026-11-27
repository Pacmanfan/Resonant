# Geometry Optimization

Any geometry parameter of a scene object can be a design variable
(`object/<id>/geometry/<parameter>`): reflectors (focal length, aperture,
semi-axes, curvature radius, thickness), lenses, waveguides, cavities,
antennas (length, wire radius, patch and substrate sizes), structures (box
and cylinder dimensions, plate thickness). The parametric definition stays
the source of truth: the optimizer changes parameters, never meshes.

## Each candidate

1. the design is reconstructed from the snapshot;
2. all geometry parameters of an object are set together and the kind's own
   rules are checked (an invalid combination is an invalid design: recorded,
   infeasible, never evaluated);
3. the simulation representation is rebuilt from the definition (reflector
   surface elements at the configured density per wavelength, the
   finite-element mesh of the structure, the full-wave grid);
4. the chosen model runs;
5. objectives and constraints are scored;
6. the candidate is stored with its model record.

Results are reused only for an identical dependency state (`OPTIMIZATION_ARCHITECTURE.md`):
a changed parameter is a changed design, never a stale result.

## Passive focusing judged by physics

A shape's label does not make a focus. The acoustic adapter evaluates the
actual geometry with the Kirchhoff reflector model (or the full-wave solver)
and reports where the field peaks (`focus.peakOffset`), how wide the focus is
(`focus.width`), its sidelobes and its gain over the free field. Example B
sweeps the focal length of a parabolic reflector lit by a source 2 m away
for the largest pressure at a target 80 mm in front of the vertex: the
optimum lies within 5 % of the mirror equation's 76.9 mm
(`AcousticOptimization.ReflectorGeometryFocusesWhereTheMirrorEquationSays`); the
remaining difference is the wave nature of the focus (finite aperture,
wavelength 8.6 mm), which the geometric formula ignores.

Hybrid designs combine geometry and source variables in one problem
(reflector parameters together with array phases).

## Limitations

* Lenses, waveguides and cavities have no analytical acoustic field model;
  only the full-wave model sees them.
* The Kirchhoff model has one reflection and no shadowing; it degrades when
  surfaces or their radii of curvature are not large against the wavelength.
* Element counts change element indices: per-element variables of an array
  whose count is also a variable refer to indices that may not exist (such a
  design is invalid, not silently reinterpreted).

## Export and manufacturing

A candidate exports (Candidates tab, Export) its record - run, problem,
design values, model, lineage, validated or not - and the meshes of the
objects whose geometry it changes, in world coordinates, as ASCII STL and
OBJ (metres). `geometry/MeshChecks` reports for each mesh: closed (every
edge shared by two triangles after welding), boundary and non-manifold
edges, degenerate triangles, enclosed volume, area, bounds, and the share of
the surface overhanging more than 45 degrees from the build direction
(excluding the face on the plate). `checkPrintability` compares wall
thickness and feature size (known from the parameters) and the build volume
with limits. A simulated shape is not automatically manufacturable; the
export says the checks are not a manufacturing approval. STEP export is not
implemented (no CAD kernel).
