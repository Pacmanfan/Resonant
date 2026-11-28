# Multiscale Modeling

**Status:** partly current (REQ-CHAT-20261126-020, ADR-000224): the
multiscale model (representations from the electronic to the continuum
scale, relationships, derivation records, model cards), voxel RVEs with
computational homogenization, Lennard-Jones reference calculations for
crystals, derivations assigned to library materials with lineage, and
sequential and iterative coupling ([architecture](../materials/MULTISCALE_ARCHITECTURE.md),
[lineage](../materials/MATERIAL_LINEAGE.md), [RVEs](RVE_HOMOGENIZATION.md),
[atoms](ATOMISTIC_MODELS.md), [validation](MULTISCALE_VALIDATION.md)). External
engines remain planned integrations (RES-SFE-009, ADR-000219).

## Scales

| Scale | Objects | Typical tools | What it can give the scale above |
|---|---|---|---|
| Macroscopic | components, cavities, waveguides, structures, fields | Resonant's solvers | the engineering result |
| Mesoscopic | grains, pores, fibers, inclusions, layers, unit cells | finite elements on cells, homogenization (SfePy), Resonant's planned Bloch analysis | effective stiffness, density, permittivity, absorption, band structure |
| Molecular | molecular arrangements, bonds, dynamics | molecular dynamics (LAMMPS, GROMACS) | elastic constants, thermal conductivity, viscosity, their temperature dependence |
| Atomic | crystal structures, lattice vibrations, defects, dislocations | lattice dynamics (phonopy), atomistic simulation (ASE drivers) | phonon dispersion, heat capacity, thermal expansion |
| Quantum | electronic states and transitions | density functional theory (Quantum ESPRESSO, CP2K) | dielectric functions, elastic constants, magnetic properties from first principles |

Resonant integrates the lower scales rather than reimplementing them: the
built-in Lennard-Jones calculations are reference implementations for pair
potentials, and the RVE solver covers steady potential problems.

## Integration architecture

```text
Resonant material model  ◄── PropertyModel(provenance = Computed{tool, version, inputs, scale})
        ▲
        │ import / fit (with uncertainty)
 Multiscale adapter (optional plugin or Python package)
        │  writes inputs, runs the tool (a separate process), reads outputs
        ▼
 external tool  (its own license, installed by the user)
```

- **Adapters, not dependencies.** Each tool is reached through an optional
  adapter: a Python module (the existing plugin system, ADR-000215) or a
  native plugin, starting the tool as a separate process. Nothing becomes a
  mandatory build or runtime dependency; GPL-licensed tools are never
  linked (RES-SFE-009).
- **Results become property models** with provenance `Computed`: tool,
  version, input files' checksums, scale, method (for example "DFT, PBE,
  plane-wave cutoff 60 Ry"), and an uncertainty where it is known.
- **Homogenized results** (mesoscale) have provenance `Homogenized` and the
  validity of their model ([metamaterials](../materials/METAMATERIALS_AND_ENGINEERED_STRUCTURES.md)).
- **Four kinds of value stay distinguishable** everywhere they are shown:
  measured, theoretical (published model), computed (a tool run here), and
  homogenized.

## What microscopic calculations could inform

Elastic constants and their temperature dependence, thermal conductivity,
heat capacity and expansion, dielectric functions and losses, magnetic
permeability, phonon behavior (thermal transport, phononic band structure at
small scales), and response under pressure or temperature — each as a
property model with its validity.

## Risks and limits

Large and unknown model errors at each scale (DFT functionals, interatomic
potentials), scale separation that does not hold (features near the
wavelength), cost, and the gap between ideal computed crystals and real
manufactured materials. Measured values keep priority in a material unless
the user chooses otherwise; comparison against measurement (the experiment
framework) is the check.

## First steps (stage 9 of the roadmap)

1. The `Computed` provenance with derivation records and uncertainty -
   current.
2. One mesoscale path inside Resonant (RVE homogenization) - current.
3. One external adapter as a reference implementation (phonopy or ASE, both
   permissively licensed Python), run as a separate process from a plugin -
   planned: no engine is installed in the reference environment.
