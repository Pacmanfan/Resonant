---
id: opt.metamaterials
title: Metamaterials and Effective Properties
category: Optimization
order: 85
status: available
summary: Effective properties derived from organized structures - band gaps, negative effective mass and modulus, anisotropic layers, split rings - with their validity, and effective materials saved into the library.
keywords: [metamaterial, effective medium, homogenization, band gap, Bloch, negative mass, split ring, retrieval, anisotropy, homogenized]
related: [opt.functional-structures, ui.panels]
---

A **metamaterial** in Resonant is a functional structure whose analysis
computes what its organization does: how a unit cell of ordinary materials,
repeated, behaves as a medium. No exotic value is typed in; every effective
property is a model result with its method, the frequencies where it holds
and the structure it came from.

Open one of the projects in `examples/metamaterials` and choose
**Panels > Functional Structures**:

| Example | What the organization does |
|---|---|
| `a-mass-in-mass-lattice` | steel blocks carrying lead cores on a soft coating: a band gap at the cores' resonance, far below the chain's own Bragg frequency, where the cell's effective mass is negative |
| `b-resonator-duct` | a duct with a Helmholtz resonator every 50 mm: sound slows down below the resonance and is blocked just above it (negative bulk modulus); a Bragg gap at c / 2a |
| `c-layered-anisotropy` | alumina and PTFE layers: two isotropic dielectrics make a uniaxial medium; at 45 degrees TM waves feel both permittivities |
| `d-split-ring-array` | copper split rings: a magnetic resonance and a band of negative permeability (a circuit approximation) |
| `e-layered-thermal` | a copper-epoxy laminate: heat spreads along the copper but crosses through the epoxy |

## What a result shows

- **Metrics**: gap edges and widths, attenuation per cell, effective values,
  and the checks - the numerical Bloch solution against the closed form, a
  finite array against the Bloch attenuation, the explicit layers against
  the effective slab, retrieval from computed reflection and transmission.
- **Curves**: dispersion, attenuation, effective mass or modulus against
  frequency, transmission of a finite array.
- **Model and assumptions** lists the effective material (its kind:
  homogenized, retrieved or an equivalent model), each property with its
  principal component, the validity range and the limitations.

## Saving an effective material

**Save effective material** adds the result's effective material to the
project's library with provenance **homogenized**; each value's source names
the method, the structure and the result. It is one step you can undo. The
button is available only while the result is current and the design lies
inside the validity of its effective description. The library holds
isotropic values, so a saved material carries one principal component
(across the layers for the laminate; choose another through
`FunctionalStructureService.CreateEffectiveMaterial` with `components`).
A saved material works like any other, for example in a coupled simulation.

## Limits

Effective properties hold only where the cell is small against the
wavelength (or the length over which temperature varies); outside that range
no effective material is reported. Negative bands are reported as results but
not stored. The split ring is a quasi-static circuit model, not a full-wave
calculation. Lattices are one-dimensional; there is no 2D or 3D Bloch solver
for continua yet.
