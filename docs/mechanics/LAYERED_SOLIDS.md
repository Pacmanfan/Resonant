# Layered Three-Dimensional Solids and Interfaces

**Status:** current (REQ-CHAT-20261128-004, RES-MECH-019..022, ADR-000232).
Code: `src/core/mechanics/LayeredSolid.{h,cpp}`, and in
`src/core/mechanics/StructuralModel` the 20-node element, the per-element
elasticity and the interface elements. Tests: `tests/core/test_layered_solid.cpp`.

## What it models

A layered or coated body in three dimensions:

- **Reference surface:** a plane, a cylinder, a sphere (with or without its
  pole), or a surface of revolution through a smooth (Catmull–Rom) meridian,
  parameterized by `(u, v)` in `[0, 1]^2`. The first layer lies on it, and
  every further layer is built outwards along the surface normal.
- **Layers:**
  - isotropic;
  - orthotropic, with nine 3D constants and axis 1 at an angle from the
    u direction; the axes follow the surface element by element;
  - graded: a power-law fraction of a second material through the layer or
    along u or v, mixed by the Voigt rule (an upper bound of the stiffness)
    and constant in each element.

  Thickness is constant or linear in u or v. Material fields (stiffness,
  density, a tensor conductivity) may replace a layer's description, element
  by element (`../materials/MATERIAL_FIELDS.md`, REQ-CHAT-20261128-006).
- **Mesh:** quadratic 20-node hexahedra, with `elementsU x elementsV` in the
  surface and `elementsThrough` in every layer. Curvature is resolved to
  second order in each element. Seams (a full sweep) and poles are joined by
  merging coincident nodes within a layer, and the elements are kept
  right-handed for any parameterization.
- **Interfaces between consecutive layers,** each stating its model (none is
  assumed):

| Mechanical model | Representation | Parameters |
|---|---|---|
| perfect-bonding | shared nodes (a declared idealization) | - |
| finite-stiffness | spring layer, `t = diag(ks, ks, kn) [u]` | kn, ks (N/m^3) |
| sliding | normal spring, no shear | kn |
| frictional-contact | penalty contact without tension; stick, or slip at `mu p` | mu (penalties derived as 1000 E / t when not given) |

| Thermal model | Representation |
|---|---|
| perfect | shared nodes |
| contact resistance | a conductance `1 / R` between the two faces |

Any interface may carry:
- **Debonded regions,** given as `(u, v)` boxes. They carry no traction and
  have no contact; an interpenetration there is reported. A perfect bond with
  a debonded region is enforced elsewhere by a stated penalty (1000 E / t).
- **Normal and shear strengths,** for the initiation index
  `(max(t_n, 0) / N)^2 + (t_s / S)^2`. There is no damage evolution, and a
  cohesive-zone model is refused.

## Analyses

| Analysis | Method | Outputs |
|---|---|---|
| static | LDLT with iterative refinement; frictional contact by Newton's method with Coulomb return mapping and a line search | displacement field, element stresses in world and surface axes (`s_uu, s_vv, s_nn, ...`), interface tractions, openings, slips, states, initiation indices |
| modal | the existing eigensolver on `K, M` | frequencies and mode shapes |
| conduction | scalar 20-node elements, temperature / flux / convection conditions | temperatures, heat flow (from the reactions), interface jumps, layer ranges |

The structural model is an ordinary `StructuralModel`. The existing harmonic
and transient analyses apply to it too. Its outer quadratic faces are drawn
curved, with the feature edges as the outline.

Refusals:
- an unheld body, or a layer free to slide (a vanishing pivot);
- the modes of a frictional contact (stick or slip depends on the load);
- a settled frictional state beyond 1 % of the body's size (small-displacement
  theory no longer holds);
- an interface or layer without the model or data the analysis needs;
- orthotropic constants that do not give a positive-definite stiffness.

## Validation

| Case | Reference | Tolerance |
|---|---|---|
| cantilever strip, tip load | Timoshenko beam | 1 % |
| cantilever strip, first mode | Euler–Bernoulli | 1.5 % |
| steel / aluminium bilayer, bonded | lamination theory (narrow beam), the thin limit | 1.5 % |
| thick cylinder, internal pressure (quarter, plane strain) | Lamé: inner displacement, hoop and radial stress | 0.5 %, 2 % p, 3 % p |
| full ring (seam merged) | Lamé | 1 % |
| compound cylinder, spring interface | two-cylinder Lamé with the interface compliance | 0.5 %, 2 % |
| thick sphere with its pole (octant) | Lamé (spherical) | 1 %, 3 % p |
| revolved profile | the same cylinder | 0.5 % |
| two layers and springs, tension and simple shear | series compliance | 1e-6 (exact) |
| sliding interface | no shear; the deflection of two independent beams | 1e-9, 2 % |
| frictional contact | stick below the limit; slip at `mu p`, never above; open under tension | 0.1 % |
| debonded region | traction-free; deflection between bonded and sliding | - |
| initiation index, uniform tension | `(sigma / N)^2` | 1e-6 (springs), 1e-3 (shared nodes) |
| tapered layer | exact volume; tapered Euler–Bernoulli + shear | 1e-9, 3 % |
| graded layer (power 2, 8 elements) | EI of the continuous profile | 2 % |
| orthotropic layer at 0 and 90 deg | `sigma L / E1`, `sigma L / E2` | 1e-6 |
| two layers with a contact resistance | series conduction; convection | 1e-6 |
| coated pipe | logarithmic conduction with the resistance | 0.5 % |

Through architected matter (`MatterAnalyses.*`):
- the 3D conduction of a flat barrier equals the series model (1 %);
- the 3D modes of a bilayer lie between the narrow-beam and wide-plate
  lamination predictions.

## In architected matter

The analyses `layered-solid` and `layered-solid-thermal`
(`coupling/MatterAnalyses`) build the body from a stack. Graded regions become
sublayers of constant properties; each declared interface maps to its model.
The capability check refuses:
- cohesive zones;
- segmented patterns;
- in-plane orthotropic sets, which do not give the nine 3D constants.

The options:

```json
{"surface": {"kind": "plane|cylinder|sphere|revolution", "length": 0.1, "width": 0.02, "radius": 0.05,
             "sweep": 6.283, "polarStart": 0, "polarEnd": 1.571, "profile": [[0.05, 0], [0.05, 0.1]]},
 "mesh": {"elementsU": 12, "elementsV": 2, "elementsThrough": 2}, "sublayers": 4,
 "analysis": "modal|static", "modes": 4,
 "supports": {"clamped": ["u0"], "symmetry": [{"axis": "x", "value": 0}], "heldAxes": ["z"], "innerFaceClamped": false},
 "loads": {"innerPressure": 0, "outerPressure": 0, "outerTraction": [0, 0, 0], "edgeForces": [{"edge": "u1", "force": [0, 0, -1]}]},
 "debonded": {"interface-id": [[0.3, 0.7, 0.0, 1.0]]},
 "thermal": {"innerTemperature": 373.15, "outerTemperature": 293.15, "innerFlux": 0, "outerConvection": [10, 293.15]}}
```

The defaults:
- mechanical: modes with the stack clamped at `u = 0`;
- thermal: 373.15 K inside and 293.15 K outside, stated in the assumptions.

Interface strengths are optional parameters (`normalStrength`,
`shearStrength`) of an interface's mechanical model.

## Limits

- **Mechanics:** linear elasticity and small displacements. Larger static
  displacements are flagged; a frictional case is refused.
- **Friction:** holonomic (total jump, no history); the tangent is held
  symmetric (a quasi-Newton step).
- **Damage and contact:** no damage evolution, no cohesive zones, no contact in
  debonded regions.
- **Materials:** graded layers by element-constant properties (Voigt);
  orthotropic layers need all nine constants.
- **Conduction:** isotropic, or anisotropic through a conductivity field; no
  radiation; faces without a condition are
  insulated.
- **Size:** at most 60000 elements; a sparse direct solver.
