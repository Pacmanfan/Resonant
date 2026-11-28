# Material Fields

**Status:** current (REQ-CHAT-20261128-006, RES-FAM-021, ADR-000234).
Code: `src/core/materials/MaterialField.{h,cpp}`. Tests: `tests/core/test_material_fields.cpp`.

A material field gives one property of a region as a function of position.
The scalar `PropertyField`s of an architecture (one property along one axis
of a stack) remain. Material fields add tensors, coordinate systems and data.

## Properties and representations

| Property | Ranks | Unit | Must be |
|---|---|---|---|
| `mechanical.density` | scalar | kg/m^3 | positive |
| `mechanical.stiffness` | stiffness6: the 21 upper entries of the 6x6 Voigt matrix, mechanics order xx yy zz xy yz zx (engineering shear) | Pa | positive definite |
| `thermal.conductivity` | scalar, symmetric3 [xx yy zz xy yz zx] | W/(m K) | positive definite |
| `electromagnetic.conductivity` | scalar, symmetric3 | S/m | positive definite |
| `electromagnetic.permittivity` | scalar, symmetric3 | F/m | positive definite |
| `piezoelectric.e` | coupling36 (3x6, row-major) | C/m^2 | - |
| `magnetic.permeability` | scalar, symmetric3 | H/m | positive definite |

## Frames

| Frame | Coordinates | Local basis |
|---|---|---|
| Cartesian | (x, y, z) from the origin, in the object's axes | the object's axes |
| Cylindrical | (r, theta, z) about an origin and axis; theta from a reference direction | (e_r, e_theta, e_z) |
| Spherical | (r, polar, azimuth) | (e_r, e_polar, e_azimuth) |

With `frameBasis`, the components are in the local basis and are rotated into
the object's axes:
- `Q T Q^T` for a symmetric tensor;
- `M C M^T` for a stiffness, M the Bond matrix;
- `Q e M^T` for a coupling.

For example, a radially orthotropic conductivity is given once as
(k_rr, k_theta, k_z).

## Form and source

- **Analytic form:** `T = T0 + (T1 - T0) f(s)`, where s is one frame
  coordinate normalized to [q0, q1]. The profile f is constant, linear, a
  power law, an exponential, or a table with values in [0, 1]. A position
  outside [q0, q1] is refused.
- **Sampled form:** samples carry positions, components and optional standard
  uncertainties. They are interpolated by one of:
  - `linear-1d`: along one coordinate, refused outside the samples' range;
  - `idw`: inverse-distance weights within a support distance;
  - `nearest`: within a support distance.

  All three are convex combinations, so positive-definite samples give
  positive-definite values. A point farther than the support from every
  sample is refused, never extrapolated.
- **Source:** `analytic`, `measured`, `imported` or `computed`, stated with
  the provenance, separately from the form. A constant from an atomistic
  calculation is analytic in form and computed in source.

Invalid tensors are refused by `validate`, never repaired. The checks are
symmetry, positive definiteness, the rank against the property, component
counts and blend values in [0, 1].

## Where they are used

- **Layered 3D solid** (`mechanics/LayeredSolid`):
  - `mechanical.stiffness` and `mechanical.density` replace a layer's
    elastic description;
  - `thermal.conductivity` (scalar or tensor) replaces its conductivity;
  - each is evaluated at each element's centre, constant in the element.
- **Piezoelectric continuum** (`mechanics/PiezoelectricSolid`):
  `piezoelectric.e`, `electromagnetic.permittivity` (eps^S) and
  `mechanical.stiffness` (c^E) replace the rotated constants element by
  element.
- **Architectures** (`matter/MatterArchitecture`, `materialFields`):
  - only analyses that declare the `tensor-field` material model read them:
    `layered-solid`, `layered-solid-thermal`. The others refuse the stack and
    name the field;
  - in the layered solids, a field's Cartesian frame is the body's: x along
    the stack, y across it, z its normal from the base;
  - a region with a conductivity field still needs its scalar conductivity in
    the library, because the capability requires it.
- **The atomistic bridge:** a `TransferredProperty` becomes a computed field
  (`coupling/EngineAdapters`, `../architecture/ATOMISTIC_INTEGRATION.md`).
