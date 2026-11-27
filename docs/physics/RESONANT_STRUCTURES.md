# Resonant Structures Across Domains

**Status:** single-domain resonance analyses are current (below); the shared
resonance description is planned (RES-SFE-004, ADR-000219).

Resonance is where geometry most directly becomes function: a structure
admits only fields that fit its boundaries and stores energy in them. Each
domain computes its resonances with its own physics; Resonant describes
them in one vocabulary.

## What each domain computes today

| Domain | Structure | Method | Reports | Missing |
|---|---|---|---|---|
| Acoustics | rectangular, cylindrical, spherical cavities; Helmholtz and quarter-wave resonators (`manipulators/Analytics`) | analytical estimates | resonance frequencies (`Mode`) | Q, wall losses, mode fields, coupling |
| Acoustics | any enclosure built from primitives | FDTD with spectra of recorded signals | spectral peaks at probes | automatic mode extraction, Q from decay |
| Mechanics | plates, beams, membranes, solids, horns | finite elements, eigenvalue solution | natural frequencies, mass-normalized mode shapes, effective mass fractions (`ModalSolution`); harmonic response; transient energy | modal damping per mode as a result (Rayleigh damping is an input), Q |
| Mechanics | periodic rods | Bloch analysis (`mechanics/Bloch`) | band structure, stop bands | 2D/3D cells, local resonators |
| Mechanics (measured) | any | identification from measured transfer functions (`IdentifiedMode`) | frequency, damping, mode shape | comparison with predicted Q |
| Electromagnetics | rectangular cavities, guides (`electromagnetics/Lines`: `CavityMode`, `GuideMode`) | analytical, ideal walls | TE/TM resonance frequencies, cut-offs | wall-loss Q, ports, other shapes |
| Electromagnetics | any geometry | Yee solver with probes and spectra | spectral peaks | mode extraction (e.g. filter diagonalization), Q |

## The shared description (planned `ResonanceDescriptor`)

| Field | Meaning |
|---|---|
| domain, model, solver, version, assumptions | provenance (RES-SFE-012) |
| natural frequency f₀ (Hz), complex frequency where damped | |
| mode shape or field | a reference to the domain's field data, with its normalization |
| modal (stored) energy | at a stated amplitude, by the [accounting definitions](ENERGY_AND_MOMENTUM_ACCOUNTING.md) |
| loss mechanisms and Q | Q = 2π · stored energy / energy lost per cycle; Q = f₀/Δf for an isolated mode |
| bandwidth Δf | −3 dB width of the response |
| participation / coupling | effective mass, coupling coefficients to other modes or ports |
| splitting | when degenerate modes split (perturbation, coupling) |
| localization | where the energy is (a participation ratio or a region) |

Each domain's mode type gets an adapter that fills the fields it can and
leaves the others unset with a reason; a Q is never filled from a model that
has no losses.

## Coupled and engineered resonators

- **Coupled resonators** (two cavities through an aperture, a plate in a
  cavity): mode splitting and hybridization, needing two-way coupling or a
  model of the joint system.
- **Distributed resonators** (rods, lines, plates): many modes; dispersion
  links them to band structures.
- **Local resonances in metamaterials**: resonant inclusions below the Bragg
  limit ([metamaterials](../materials/METAMATERIALS_AND_ENGINEERED_STRUCTURES.md)).
- **Electrical resonant circuits**: RLC equivalents of transducers and
  matching networks (the electromagnetic domain has lines and matching).

## Function-centric resonance design

Objectives already expressible through the optimization adapters: the
natural frequencies f1..fK of a structure and their clearance from a
frequency to avoid (`mechanics/MechanicalOptimization`), and an antenna's
induced-EMF input impedance, whose reactance vanishes at resonance
(`electromagnetics/EmOptimization`).
Planned: a target Q, a mode at a frequency with a field shape, a band gap
between two frequencies, a frequency that stays put as temperature changes
(coupled).

## Validation

Analytical modes (rectangular cavities, simply supported plates, rods),
convergence of numerical frequencies with resolution, Q from analytical
wall losses for simple cavities, and measured modes through the experiment
framework.
