# Geometry Optimization

**Status:** current for REQ-CHAT-20261126-004: the interfaces and a limited
demonstration. A general optimization framework is the subject of request
009.

## Interfaces (`GeometryOptimization.h`)

* `DesignVariable` — a geometry parameter with lower and upper bounds.
* `GeometryObjective` — a function of a candidate geometry, to be
  maximized, that must run a physical model.
* `OptimizationSettings` — evaluation limit, tolerance, manufacturing
  constraints.
* `OptimizationOutcome` — method, variables, the initial and best
  candidates, **every candidate evaluated**, the best geometry, notes.

## Method

Cyclic coordinate search; along each variable a golden-section search of
its bounded interval. Derivative-free. **Local:** golden-section finds the
maximum only if the objective has one peak in the interval, and coordinate
search can stop on a ridge. The outcome states the method and that the
result is a candidate to check, not a proven optimum.

A candidate whose parameters are refused by the geometry, or that violates
a manufacturing constraint, is infeasible; when both probes of a line
search are infeasible the search moves towards the best feasible value
known.

## What the demonstration does

In the reflector's property editor: choose one parameter and a range; the
objective is `|P|` at the reflector's intended focus, calculated by the
reflector model with the scene's current sources. The optimizer works on a
private copy of the scene. Nothing in the project changes until "Apply Best
Geometry", which is one undoable step.

It requires the reflector field model (with the free-field model the
reflector has no effect and the application refuses, with the reason), an
intended focal point and an active emitter.

## Validation

Source 0.5 m from a parabolic reflector, pressure wanted at 70 mm, start at
`f = 40 mm`: the optimizer finds `f = 58 mm`, within 4 mm of the mirror
equation's `1/(1/s + 1/s')`, and raises the pressure at the target more
than threefold (`GeometryOptimization.FindsTheFocalLengthThatFocusesAtTheTarget`;
example E).

## Not implemented

Several variables at once in the user interface (the core accepts them),
other objectives (focal width, sidelobes, uniformity, material volume),
material selection, layered and periodic structures, topology optimization.
