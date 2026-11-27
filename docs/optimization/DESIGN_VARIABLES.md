# Design Variables

A design variable is one number an optimization may change; its **binding**
is the adapter's key for what it changes. Values are SI (the `unit` names
them).

| Kind | Values | Typical roles |
|---|---|---|
| continuous | any value in [lower, upper]; with `step` > 0 only lower + k step | position, dimension, radius, thickness, spacing, material parameter, frequency, amplitude, phase, delay, orientation |
| integer | whole numbers in [lower, upper] | number of elements, activation (0 / 1) |
| categorical | an index into `choices` (with labels) | material selection, support type, layout |

Each variable has an initial value, a role, a group (a named design
parameter, shown together: "Reflector": focal length, aperture, thickness),
`locked` (kept at its initial value) and `active` (unchecked: not part of the
design at all; the project keeps its own value). Validation refuses ids used
twice, bounds out of order, initial values outside them, categorical
variables without choices and equal bounds on an unlocked variable.

## Derived variables

A derived variable is computed, never chosen by a method:

```text
id = expression over variable ids and adapter constants
```

The expression language has `+ - * / ^`, parentheses, `sqrt abs sin cos
tan exp log min max pow` and `pi`; names are letters, digits, `_` and `.`;
an id with other characters (every binding-shaped id) is written in brackets:
`[object/3f2a/geometry/aperture] / 4`. Constants come from the adapter:
`wavelength`, `frequency`, `soundSpeed` (acoustic), `frequency` (mechanical),
`wavelength`, `frequency` (electromagnetic). A derived variable with a binding
is applied like a variable (example F mirrors five free amplitudes onto the
other five elements: `a9 = a0`); without a binding it is a value for
constraints and reports. Derived variables that depend on each other are
evaluated in dependency order; a cycle is refused. The language has no
units: the author keeps expressions dimensionally consistent.

## The unit cube

Methods see the free variables (active and unlocked) in [0, 1]^n. A
continuous variable maps linearly; an integer variable with N values gives
each value an equal share of [0, 1]; a categorical variable likewise per
choice. `DesignSpace::fromUnit` quantizes and adds locked and derived values;
`toUnit` maps back (round trip exact for representable values).

## Bindings every adapter has

```text
object/<id>/position/{x,y,z}      m, local translation
object/<id>/rotation/{x,y,z}      rad, local Euler angles (Z-Y-X)
object/<id>/geometry/<parameter>  the geometry parameter (its SI unit)
object/<id>/material              categorical: library materials of the domain
```

Geometry parameters of one object are applied together; the kind's own rules
are checked after all of them are set (an aperture larger than twice the
radius of curvature is an invalid design). Suggested bounds: the parameter's
range within a quarter and four times the present value; positions +/- 0.1 m.
The domain bindings are listed in `DOMAIN_OPTIMIZATION_ADAPTERS.md`.

## Invalid designs

A design that cannot be built (an unknown binding, a value a component
rejects, a violated geometry rule, an element index beyond the array) is an
**invalid design**: it is recorded with the reason, it is infeasible, and the
model never runs on it.
