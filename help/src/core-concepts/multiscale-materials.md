---
id: concepts.multiscale
title: Material Scales and Lineage
category: Core Concepts
order: 32
status: available
summary: Representations of a material from atoms to the continuum, properties derived from them with their model, conditions and uncertainty, and the lineage that leads a library value back to its source.
keywords: [multiscale, microstructure, RVE, homogenization, atomistic, crystal, lineage, provenance, uncertainty, derived property]
related: [concepts.materials, opt.metamaterials, opt.functional-structures]
---

A library material is a set of engineering values. **Material Scales**
(**Panels > Material Scales**) shows where those values can come from: a
material's **representations** at smaller scales, and the calculations that
derive properties from them.

| Scale | Representation | What Resonant computes from it |
|---|---|---|
| continuum | the library material | the engineering solvers' inputs |
| effective | an effective material of an organized structure | see [Metamaterials](../optimization/metamaterials.md) |
| microstructure | a voxel RVE: phases, inclusions, pores, grains with orientations | conductivity tensors (thermal, electrical, static permittivity), density, specific heat |
| atomistic | a crystal or imported atoms (XYZ, POSCAR) with a force field | elastic constants, moduli, density and sound speed at 0 K (Lennard-Jones lattice statics); molecular dynamics as a check |
| molecular, electronic | data models only | nothing: no molecular or electronic-structure engine is integrated |

Open a project in `examples/multiscale`, select a representation, pick a
method and **Run**. The result lists the derived values; **Assign** writes
them into the chosen material - one step you can undo.

## What a derived value carries

- **Provenance**: *homogenized* (from a microstructure) or *computed* (from
  atoms), next to measured, data-sheet and literature values.
- **A derivation record**: the representation and its revision, the method
  and its model card, the solver, boundary conditions, temperature,
  convergence, resolution and sampling.
- **An uncertainty** where it is known: the spread over random RVE
  realizations, or the sensitivity to the atomistic cutoff.

The **Lineage** table traces each value of a material back to the
representation it came from, and through that representation's own
constituents further down. When the RVE, a constituent or the value itself
changes, the value is marked **stale** with the reason.

## Checks worth reading

- RVE results come with a size study and, for random structures, several
  realizations: *representative* means the spread and the change with size
  are below the tolerance.
- Periodic boundaries need a periodic structure; the flux and linear
  boundaries bracket the result.
- A calculation can run and still not be reliable: the result says why
  (one realization, energy drift, a structure that is not stable).
- A local state (a region's temperature in a coupled simulation) can set the
  temperature of a derivation; the iterative coupling repeats this until the
  two scales agree.

## Limits

Synthetic RVEs are generated, not measured. Lennard-Jones results are
static and classical (no zero-point motion). External engines (LAMMPS,
OpenMM, Quantum ESPRESSO, CP2K, GPAW, phonopy) are listed with their status;
none is integrated, and no electronic-structure result is ever shown.
Elastic homogenization of RVEs and FE2 are planned.
