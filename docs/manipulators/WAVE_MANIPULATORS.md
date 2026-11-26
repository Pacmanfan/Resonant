# Wave Manipulators

**Status:** current for REQ-CHAT-20261126-004. Requirements:
`docs/requirements/wave_manipulators.md`. Decision: ADR-000208.

A wave manipulator is a physical structure that shapes a wave field through
its geometry, material and boundary behaviour. The module is
`src/core/manipulators`; it builds on the acoustic module and is used by it
only through interfaces.

## Three separate things

| | What it is | Where |
|---|---|---|
| Design | category, boundary condition, design intent (intended focus, intended source), notes | `ManipulatorComponent` on the scene object |
| Geometry | a parametric shape from its mathematical definition | the object's `Geometry`; kinds in `ManipulatorGeometry.cpp` |
| Simulation representation | what a solver derives from the geometry | for reflectors: surface elements (`ReflectorSurface.h`) |

The design intent records what the designer wants. Nothing assumes the
structure achieves it: the achieved focus is calculated and compared with
the intended one.

## Three strategies

* **Passive** — geometry and material only (a source and a reflector).
* **Active** — independently driven emitters (`docs/acoustics/ACOUSTIC_FOCUSING.md`).
* **Hybrid** — both, solved together (`HYBRID_FOCUSING.md`).

All three are evaluated by field simulations of the same region and
compared with the same metrics (`PASSIVE_FOCUSING.md`).

## Categories and what is calculated for each

| Category | Geometry kinds | Support |
|---|---|---|
| Reflector | parabolic, elliptical, spherical, cylindrical (trough), flat | **approximate field**: the reflector model (Kirchhoff approximation) and ray paths |
| Lens | spherical surfaces (convex, concave, plano-, bi-) | **analytical estimate**: thick-lens focal length, interface transmission. No field effect. |
| Waveguide | circular (straight or tapered), rectangular | **analytical estimate**: cut-off frequencies. No field effect. |
| Resonant structure | rectangular, cylindrical, spherical cavity; Helmholtz and quarter-wave formulas | **analytical estimate**: resonance frequencies. No field effect. |
| Diffraction structure | plate with a slit | **geometry only**. No field effect. |

`supportOf(geometry)` returns the level and `supportDescription(geometry)` a
sentence for the user; the property editor always shows it. A manipulator
without a field model is reported by the reflector model as not affecting
the result, never silently ignored.

Not present as geometry kinds: custom freeform reflector and lens surfaces,
curved and branching waveguides, custom cross-sections, layered resonant
structures, perforated plates, periodic and metamaterial structures. The
architecture adds a kind by registering it (`GeometryRegistry`); a field
model for it is a separate piece of work.

## Boundary conditions

Rigid (reflection +1), pressure release (−1), locally reacting impedance
`ρc` of the assigned material, absorbing (0). They are used by the reflector
model; for other categories the choice is stored with the design and no
model uses it.

## Manufacturing constraints

`ManufacturingConstraints` (minimum wall thickness, maximum dimension,
minimum curvature radius, minimum feature size) are kept apart from the
physical parameters. `checkConstraints` reports each violation with the
actual value, the limit and a suggested correction, and never modifies the
geometry. The optimizer treats a violating candidate as infeasible.
Layer thickness, material availability, symmetry, periodicity and mesh
quality are not checked.

## In the application

**Wave Manipulators** menu adds each kind. The property editor shows the
category, what is calculated, the boundary condition, the design intent,
the geometry's characteristic points and analytical estimates, and for
reflectors a geometry optimization. The viewport draws the shape's own
focal points (violet), the intended focus (green) and the intended source
(orange) as separate markers, and on request ray paths for the selected
reflector.
