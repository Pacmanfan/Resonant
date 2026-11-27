---
id: opt.variables
title: Design variables
category: Optimization
order: 20
status: available
summary: The numbers an optimization may change: kinds, bounds, locks, groups, bindings to the design, derived variables and invalid designs.
keywords: [design variable, bounds, binding, derived variable, categorical, lock]
related: [opt.studio, opt.constraints, opt.geometry, opt.algorithms]
---

A design variable is one number an optimization may change. Its **binding**
says what it changes in the design, for example
`object/<id>/geometry/focalLength` or `object/<id>/array/element/3/phase`.
Values are in SI units; the **Unit** column names them.

## Kinds

| Kind | Values | Typical use |
|---|---|---|
| continuous | any value in [lower, upper]; with a step > 0 only lower + k · step | positions, dimensions, thicknesses, spacings, amplitudes, phases, delays, frequencies |
| integer | whole numbers in [lower, upper] | element counts, activation (0 or 1) |
| categorical | one of a list of choices, with labels | material selection, support type |

Every variable also has an **initial** value (the present design), a role, a
**group** (a named design parameter shown together, such as *Reflector*:
focal length, aperture, thickness), **locked** (kept at its initial value)
and **active** (unchecked: not part of the design at all; the project keeps
its own value).

## The Variables tab

The table has the columns **Use** (active), **Lock**, **Variable** (the
tooltip shows the binding, kind and role), **Unit**, **Lower**, **Upper**,
**Initial** and **Step**. Lower, upper and initial are edited for continuous
and integer variables; Step only for continuous ones; a categorical variable
shows *-* and *N choices*. To add a variable, choose it in the list of what
the physics adapter can change in this project and press **Add**; **Remove**
deletes the selected row. Store the edits with **Apply Changes** on the
Problem tab (Start applies them too).

Validation refuses an id used twice, bounds out of order, an initial value
outside its bounds, a categorical variable without choices, and equal bounds
on a variable that is not locked. The adapter refuses a binding it cannot
change in this project.

## Bindings

Every adapter offers:

```text
object/<id>/position/{x,y,z}      m, local translation
object/<id>/rotation/{x,y,z}      rad, local Euler angles (Z-Y-X)
object/<id>/geometry/<parameter>  the geometry parameter, in its SI unit
object/<id>/material              categorical: library materials of the domain
```

The domain adapters add, among others: `acoustics/frequency` and
`acoustics/medium/temperature`; emitter and array element amplitude, phase,
delay and activation, array spacing, radius and counts (acoustic); structure
thickness, supports, actuator amplitude, phase and delay, damping, the
harmonic frequency, and hypothetical scalings of Young's modulus and density
(mechanical: a labelled copy of the material, never the library entry);
array frequency, spacing, counts and element weights, antenna frequency and
reference impedance (electromagnetic). Suggested bounds are a quarter to four
times the present value (within the parameter's range); positions ±0.1 m.

## Derived variables

A derived variable is computed, never chosen by a method:

```text
a9 = a0
gap = [object/b/geometry/aperture] / 4
```

Expressions use `+ - * / ^`, parentheses, `sqrt abs sin cos tan exp log min
max pow` and `pi`. Names are letters, digits, `_` and `.`; an id with other
characters (every binding) is written in brackets. Constants come from the
adapter: `wavelength`, `frequency`, `soundSpeed` (acoustic), `frequency`
(mechanical), `wavelength`, `frequency` (electromagnetic). A derived variable
with a binding is applied like a variable; without one it is a value for
constraints and reports. A cycle is refused. **Expressions have no units**:
keep them dimensionally consistent yourself.

The Variables tab lists derived variables below the table (*Derived: a9 =
a0, ...*). They come from templates, example projects or the problem file;
the Studio does not edit them in this version.

## The unit cube

Methods see the free variables (active and unlocked) as a point in
[0, 1]ⁿ. A continuous variable maps linearly; an integer or categorical
variable with N values gives each value an equal share of [0, 1]. Locked and
derived values are added when the point is turned back into a design.

## Invalid designs

A design that cannot be built (a value a component rejects, a violated
geometry rule such as an aperture larger than twice the radius of curvature,
an element index beyond the array) is an **invalid design**: recorded with
its reason, infeasible, and never simulated.

## Worked examples

- `examples/optimization/d-mechanical-resonator.resonant`: three variables of
  one group, *Aluminium cantilever*: its length (*Width (X)*, 0.3-0.8 m), its
  thickness (*Height (Z)*, 3-10 mm in 0.5 mm stock steps) and its material (a
  categorical variable over the 16 library materials).
- `examples/optimization/f-sidelobe-reduction.resonant`: five free amplitudes
  `a0`..`a4` (0.05-1) and five derived ones, `a9 = a0` to `a5 = a4`, so the
  taper of the 10-element array stays symmetric.

## Common mistakes

- Per-element variables together with an element-count variable: indices
  that no longer exist give invalid designs.
- A step on a variable used with Nelder-Mead, L-BFGS-B or the Bayesian
  method: these refuse stepped, integer and categorical variables.
- Bounds so wide that most designs are invalid: the budget is spent on
  designs that are never simulated.

## Related topics

[Constraints](constraints.md) · [Geometry optimization](geometry.md) ·
[Algorithms](algorithms.md) · [The Optimization Studio](studio.md)
