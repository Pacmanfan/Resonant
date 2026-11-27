# Multiscale Modeling

**Status:** theoretical future and planned integration architecture
(RES-SFE-009, ADR-000219). Resonant computes at the macroscopic scale only
today; material values come from measurements, data sheets, literature or
nominal values (the material provenance).

## Scales

| Scale | Objects | Typical tools | What it can give the scale above |
|---|---|---|---|
| Macroscopic | components, cavities, waveguides, structures, fields | Resonant's solvers | the engineering result |
| Mesoscopic | grains, pores, fibers, inclusions, layers, unit cells | finite elements on cells, homogenization (SfePy), Resonant's planned Bloch analysis | effective stiffness, density, permittivity, absorption, band structure |
| Molecular | molecular arrangements, bonds, dynamics | molecular dynamics (LAMMPS, GROMACS) | elastic constants, thermal conductivity, viscosity, their temperature dependence |
| Atomic | crystal structures, lattice vibrations, defects, dislocations | lattice dynamics (phonopy), atomistic simulation (ASE drivers) | phonon dispersion, heat capacity, thermal expansion |
| Quantum | electronic states and transitions | density functional theory (Quantum ESPRESSO, CP2K) | dielectric functions, elastic constants, magnetic properties from first principles |

Resonant does not implement the lower scales. It integrates them.

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

1. The `Computed` provenance and import of tabulated results with
   uncertainty.
2. One mesoscale path inside Resonant (unit-cell homogenization).
3. One external adapter as a reference implementation (phonopy or ASE, both
   permissively licensed Python), run as a separate process from a plugin.
