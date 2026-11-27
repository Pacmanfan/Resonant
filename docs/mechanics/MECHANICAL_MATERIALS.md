# Mechanical Materials

**Status:** current for REQ-CHAT-20261126-006. Code:
`src/core/mechanics/ElasticMaterial.*`, `src/core/materials/ExampleMaterials.cpp`.

## Model

Linear-elastic, isotropic. The material record's mechanical property set
holds density, Young's modulus, Poisson's ratio and optionally a loss
factor, each with provenance, source, validity range and an optional
frequency table (the general material model of the application). From `E`,
`nu`, `rho` the solver derives:

| | |
|---|---|
| Shear modulus | `G = E / (2 (1 + nu))` |
| Bulk modulus | `K = E / (3 (1 - 2 nu))` |
| Lame constant | `lambda = E nu / ((1 + nu)(1 - 2 nu))` |
| Rod wave speed | `sqrt(E / rho)` |
| Bulk P / S wave speeds | `sqrt((lambda + 2G) / rho)`, `sqrt(G / rho)` |
| Plate bending stiffness | `E h^3 / (12 (1 - nu^2))` |

Storing only the independent constants means they cannot contradict each
other. `-1 < nu < 0.5` is enforced. A material without the three required
values is refused by name ("material 'Water' has no mechanical density,
Young's modulus, Poisson's ratio").

## Library

Eighteen example materials, all values **illustrative** (typical handbook
values, not authoritative), each with a note on what changes them: air,
water, aluminium, steel, copper, glass, acrylic, concrete, rubber, wood,
and added for this milestone stainless steel (austenitic), brass, titanium
alloy (Ti-6Al-4V), ABS, PLA (bulk; printed parts much softer and
anisotropic), nylon (dry), polycarbonate, alumina ceramic. Air and water
have no mechanical properties. Replace the values with measured or cited
ones before engineering use.

## Not represented

* Anisotropy: wood is stored with its along-the-grain modulus and is
  treated as isotropic; its note says so. Fibre-reinforced composites are
  not in the library: they need an orthotropic constitutive tensor
  (9 constants), which the element formulations would take as `C` in place
  of the isotropic one.
* Viscoelasticity, frequency and temperature dependence of the modulus
  (frequency tables are stored but the mechanical solvers use the nominal
  value).
* Plasticity, damage.

## Damping data

A material's loss factor, when stated, is used by the structural
loss-factor damping model (`FORCED_RESPONSE.md`). The library states none:
damping depends on joints and mounting more than on the material, and is
an analysis setting.
