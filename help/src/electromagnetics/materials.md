---
id: em.materials
title: RF materials
category: Electromagnetic Engineering
order: 20
status: available
summary: Permittivity, permeability, conductivity and loss tangent of a material, what Resonant derives from them and where they are used.
keywords: [permittivity, permeability, conductivity, loss tangent, skin depth, conductor, dielectric]
related: [concepts.materials, em.maxwell, em.antennas, em.solvers, mechanics.materials]
---

A material record has an electromagnetic domain next to its acoustic and
mechanical ones. It decides the conductor loss of wire antennas, the
substrate of a patch, the filling of a waveguide and the cells of the
full-wave solver.

## The properties

Open **Materials > Material Library...**, select the material, set
**Physics domain** to *Electromagnetic* and tick **Material has properties
in this domain**.

| Property | Unit | Required |
|---|---|---|
| Relative permittivity εr | - | yes |
| Relative permeability μr | - | yes |
| Electrical conductivity σ | S/m | no |
| Dielectric loss tangent tan δ | - | no |

Values may be tabulated over frequency (the value at the frequency of use
is interpolated) and may state a validity range; a frequency outside it is
flagged in the result. Each value carries its provenance. A material
without permittivity and permeability is reported as having no
electromagnetic data; nothing is invented for it.

## What Resonant derives

At the frequency of use f (ω = 2πf):

```math
ε_c = ε₀ εr (1 − j tan δ) − j σ / ω             complex permittivity
loss ratio = σ / (ω ε₀ εr) + tan δ
k = ω √(μ ε_c),      η = √(μ / ε_c)              wavenumber, wave impedance
δ_s = 1 / |Im k|                                 skin depth (exact for any loss)
R_s = 1 / (σ δ_s)                                surface resistance of a good conductor
Γ = (η₂ − η₁) / (η₂ + η₁)                        reflection at normal incidence
```

The material is classified as a **good conductor** (loss ratio above 100),
a **dielectric** (below 0.01) or a **lossy dielectric** in between.

**Worked example.** Copper (σ = 5.8 × 10⁷ S/m) at 1 GHz has a skin depth
of 2.09 µm and a surface resistance of 8.25 mΩ. On the 1 mm wire of the
half-wave dipole in `examples/electromagnetics/a-half-wave-dipole.resonant`
this gives a loss resistance far below the 73 ohm radiation resistance:
the report shows a radiation efficiency above 99 %.

## Where the properties are used

| Use | Effect |
|---|---|
| Wire antennas (dipole, monopole, loop) | the conductor's surface resistance gives the loss resistance and the radiation efficiency |
| Patch antennas | the substrate permittivity enters the resonant-length estimate |
| Waveguides and cavities | a non-conducting filling's εr and μr shift the cut-offs; a conductor is ignored with a note |
| Yee full-wave solver | good conductors become perfect conductors; other materials keep εr, μr and an equivalent conductivity σ + ω ε₀ εr tan δ at the run frequency |

In the full-wave solver, objects without a material, or with one that has
no electromagnetic data, are treated as perfect conductors with a warning.

## The example library

The electromagnetic values of the built-in materials are **illustrative**
and labelled so, each with a note on what they depend on (frequency,
purity, temperature, cold work):

| Material | εr | μr | σ (S/m) |
|---|---|---|---|
| Copper | 1 | 1 | 5.8 × 10⁷ |
| Aluminum | 1 | 1 | 3.5 × 10⁷ |
| Brass | 1 | 1 | 1.5 × 10⁷ |
| Steel | 1 | 100 (placeholder) | 6 × 10⁶ |
| Stainless steel | 1 | 1 | 1.4 × 10⁶ |
| Titanium alloy | 1 | 1 | 5.8 × 10⁵ |
| Glass | 6 | 1 | 10⁻¹² |
| Acrylic | 3 | 1 | 10⁻¹⁴ |
| Polycarbonate | 2.9 | 1 | 10⁻¹⁴ |
| Alumina ceramic | 9 | 1 | 10⁻¹² |
| Water | 80 | 1 | 0.005 |
| Air | 1.0006 | 1 | 0 |

For a conductor, εr = 1 is a placeholder: permittivity is not a meaningful
quantity there. For a real design use measured data for the actual
material at the frequency of interest.

## Common mistakes

- **Leaving a wire without a material.** The efficiency is then 1: no
  conductor loss is calculated.
- **Using the static permittivity of water at RF.** It is strongly
  frequency dependent; enter a table or a value for your frequency.
- **A metal as guide filling.** A conducting material assigned to a guide
  is ignored and the guide is taken as empty.

## Limitations

- Dispersive models (Debye, Drude, Lorentz) are not implemented; use
  tabulated values.
- Isotropic materials only; no magnetic loss, no ferrites.
- Antenna efficiency includes conductor loss only; dielectric and ground
  losses are not included.
