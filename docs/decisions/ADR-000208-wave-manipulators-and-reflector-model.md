# ADR-000208: Wave manipulators — design, geometry and simulation kept apart; a Kirchhoff reflector model as a selectable propagation model; support levels

**Status:** ACCEPTED
**Date:** 2026-11-26
**Type:** architecture
**Requirements:** `docs/requirements/wave_manipulators.md` RES-MANIP-001..016
**Request:** REQ-CHAT-20261126-004
**Builds on:** ADR-000205, ADR-000206, ADR-000207

## Decisions

1. **A manipulator is a scene object with a `ManipulatorComponent`** (design
   data) and an ordinary parametric `Geometry` (kinds registered by
   `registerManipulatorGeometry`). No new object model.
2. **Design intent is recorded, never assumed achieved.** The intended focus
   and source are stored; the achieved focus is calculated from a field.
3. **Support levels.** Every geometry kind declares whether an approximate
   field, an analytical estimate or only geometry exists for it. The level
   is shown wherever the object is edited and in result notes.
4. **The reflector model is a `PropagationModel`** (`acoustic.reflector.kirchhoff`):
   free field plus the Helmholtz–Kirchhoff integral over reflector surfaces
   with tangent-plane (Kirchhoff) boundary values. Chosen because it gives
   phase, interference and aperture diffraction at a cost of one pass over
   the surface per field sample, and has closed-form limits to validate
   against. Being a propagation model, focusing and inverse design use it
   unchanged, which is what makes hybrid focusing possible.
5. **Simulation surfaces are sampled from the equation,** not from the
   display mesh, at a resolution tied to the wavelength.
6. **The field model is a project setting** (`AcousticSettings::modelId`),
   chosen by the user and recorded in every result. Nothing switches model
   automatically.
7. **`FieldEvaluator` is polymorphic.** A model that cannot provide particle
   velocity says so, and intensity is then reported as not calculated
   rather than as zero.
8. **Rays are a geometric aid,** exact against the analytical surface,
   without amplitude or phase.
9. **Optimization interfaces are minimal** (variables, objective,
   constraints, recorded candidates, a local derivative-free search);
   request 009 owns the general framework.
10. **Manufacturing constraints are checks,** separate from the physical
    parameters, and never modify geometry.

## Deviations from the request

* Lenses, waveguides, resonant cavities and diffraction structures have
  geometry and analytical estimates only; no field model. Full-wave solvers
  are the subject of request 005.
* Not provided: freeform reflector and lens surfaces, extruded 2D ellipses,
  curved and branching waveguides, custom cross-sections, layered,
  periodic and metamaterial structures, perforated plates.
* The reflector model has no multiple reflections, no mutual shadowing and
  no particle velocity; no energy-based focusing efficiency is reported.
* Optimization in the user interface: one parameter of one reflector, one
  objective.
* Validation is against closed-form references only.

## Rejected

* Ray tracing as the field model — no phase, no diffraction, a focus of
  zero size.
* Image sources — exact only for infinite planes.
* A boundary-element solver in this milestone — the cost and validation
  work belong with the numerical solvers of request 005.
