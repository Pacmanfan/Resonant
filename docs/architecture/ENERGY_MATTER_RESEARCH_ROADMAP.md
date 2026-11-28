# Energy–Matter Research Roadmap

The staged plan for the Shape–Function–Energy direction (ADR-000219,
RES-SFE-001..012). It extends the milestone roadmap (`roadmap.md`, M0–M11),
whose milestones are delivered; nothing below is implemented unless a stage
says "current". Requests already queued for these stages are named; their
own scope decides what each delivers.

## Overview

```text
 1 conventions ─► 2 material state & quantities ─► 3 accounting ─► 4 coupling interfaces ─► 5 one-/two-way coupling
                                     │                                   │                        │
                                     ▼                                   ▼                        ▼
                         10 dynamic material response        6 resonant systems ─► 7 metamaterials ─► 8 homogenization
                                     ▲                                                              │
                                     └──────────── 9 multiscale adapters ◄──────────────────────────┘
                              11 function-driven inverse design (uses all)      12 experimental validation (checks all)
```

## Stage 1 — Shape–Function–Energy architecture and documentation

- **Goals:** the principle, requirements, architecture, audit and this plan.
- **Work:** documents only (`docs/vision`, `docs/physics`, `docs/materials`, `docs/geometry`, this directory).
- **Status:** current (REQ-CHAT-20261126-015).
- **Acceptance:** RES-SFE-001..012 recorded; the audit maps every subsystem; documents pass `scripts/doc_integrity.py`.

## Stage 2 — Unified material state and physical quantity conventions

- **Status:** partly current (REQ-CHAT-20261126-016): the quantity registry, region laws with validity, the temperature state and the thermal property set; anisotropic tensors and laws in the material library remain.

- **Goals:** `PhysicalQuantity` registry; `PropertyModel` (constant, table, formula, constitutive) with state variables, validity and the `Computed` / `Homogenized` provenance; a thermal property set.
- **Work:** `src/core/materials`, `src/core/foundation`; project schema migration; services and contract additions.
- **Depends on:** stage 1. **Reuses:** material provenance, validity ranges, frequency tables.
- **Capabilities:** temperature-dependent properties evaluated with reported extrapolation; anisotropic tensors in material frames.
- **Risks:** a property model too generic to validate; breaking existing projects.
- **Validation:** unit tests per model; old projects load identically (regression on every example).
- **Acceptance:** every existing material round-trips; a temperature-dependent value is used by one solver with its validity reported.
- **Dynamic state:** current since REQ-CHAT-20261126-021 (ADR-000225) for state variables with classes, fields and records (`docs/materials/DYNAMIC_MATERIAL_STATE.md`).

## Stage 3 — Energy and momentum accounting

- **Status:** current (REQ-CHAT-20261126-022, ADR-000226): the unified ledger for any system boundary, energy across domains, linear and angular momentum and forces for lumped networks, the radiation force on layered bodies by the vacuum Maxwell stress and the Lorentz volume force, conservation checks with honest statuses (`docs/architecture/ACCOUNTING_ARCHITECTURE.md`); adapters from the FDTD and Yee energy records, the acoustic radiation force and the full-wave Maxwell stress remain.

- **Goals:** `EnergyLedger`; the definitions of [the accounting page](../physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md) per domain; radiation force and Maxwell stress where force is a function.
- **Work:** ledger type in the core; adapters from the existing FDTD, Yee and mechanical transient energy records; time-averaged intensity and Poynting results; display.
- **Depends on:** stage 2 (quantities). **Reuses:** the three energy records.
- **Risks:** inconsistent definitions across discretizations (staggered grids); dispersive materials.
- **Validation:** closed lossless domains conserve to rounding; analytical radiation pressure on a plane reflector; Poynting through a closed surface equals radiated power of a dipole.
- **Acceptance:** each solver reports a ledger with a residual; force results state their surface and order.

## Stage 4 — Coupled solver interfaces

- **Status:** current for layered systems (REQ-CHAT-20261126-016): participants, field data, transfers, the scheduler and coupled results; StructuralRadiation as a participant pair remains.

- **Goals:** `CouplingParticipant`, `FieldData` on `Support`, `TransferOperator`, `CouplingGraph`, `CouplingScheduler`, `CoupledResult` ([architecture](COUPLED_MULTIPHYSICS_ARCHITECTURE.md)).
- **Work:** `src/core/coupling`; wrap `StructuralRadiation` as the first participant pair; jobs and services for coupled runs.
- **Depends on:** stages 2–3. **Reuses:** solver setups, jobs, compute backends, provenance.
- **Risks:** interfaces that leak solver internals; transfer operators that silently lose conservation.
- **Validation:** the plate-radiation regression through the new interface; conservative projection tests (integral preserved to rounding).
- **Acceptance:** a one-way coupled run through the graph gives the same result as today's direct coupling.
- **Queued:** REQ-CHAT-20261126-016 (Prompt 18, Coupled Energy–Matter Engine).

## Stage 5 — One-way and two-way multiphysics coupling

- **Status:** partly current (REQ-CHAT-20261126-016): the thermal domain, EM loss → heat (one- and two-way), expansion and stress, geometry feedback, all validated in 1D; fluid loading and 3D systems remain.

- **Goals:** a thermal domain (conduction, sources, boundary conditions); electromagnetic loss → heat; heat → expansion and thermal stress; two-way iteration with convergence control; fluid loading of structures (two-way vibro-acoustics).
- **Work:** `src/core/thermal` (new domain, its own module and solver), participants, scheduler modes.
- **Depends on:** stage 4.
- **Risks:** time-scale separation, iteration divergence, cost of 3D two-way problems.
- **Validation:** analytical slab heating, thermoelastic expansion, a published microwave-heating benchmark, added mass of a plate in water.
- **Acceptance:** each coupled example reports convergence history and a ledger residual below its tolerance.
- **Queued:** REQ-CHAT-20261126-016.

## Stage 6 — Geometry-driven resonant systems

- **Status:** current for lumped and closed-form models (REQ-CHAT-20261126-017, REQ-CHAT-20261126-018): one resonance description (nature, Q by mechanism), spectroscopy, Helmholtz resonators, coupled resonators with splitting, energy exchange and avoided crossings, ring-down of the cavity's Yee model; resonances of 3D finite-element and full-wave coupled structures remain.

- **Goals:** `ResonanceDescriptor` across domains; Q from losses; mode extraction from time-domain runs; coupled resonators and mode splitting; boundary and region objects; conversion fidelity records.
- **Depends on:** stages 3–4. **Reuses:** every domain's mode types, spectra, modal analysis.
- **Risks:** Q from inadequate loss models; spurious modes from discretization.
- **Validation:** analytical cavity Q, simply supported plate modes, two coupled cavities' splitting.
- **Acceptance:** every domain's resonances appear in one table with provenance; Q is never shown without a loss model.
- **Queued:** REQ-CHAT-20261126-017 (Prompt 19, geometry-driven energy transformation), REQ-CHAT-20261126-018 (Prompt 20, resonant structures and coupled oscillatory systems).

## Stage 7 — Metamaterials and periodic structures

- **Status:** 1D current (REQ-CHAT-20261126-017): the two-layer electromagnetic cell (exact Bloch, gaps, finite-stack transmission) and the periodic rod as analyses; since REQ-CHAT-20261126-019 also locally resonant lattices (mass-in-mass, Helmholtz-loaded duct) with Bloch analysis of lumped cells, oblique layered stacks and split-ring arrays (circuit model), each checked against a finite structure (`docs/physics/METAMATERIAL_VALIDATION.md`); 2D/3D lattices remain.

- **Goals:** `UnitCell` and `Lattice`; 2D/3D Bloch analysis (elastic, acoustic, electromagnetic with phase-shifted periodic boundaries); band diagrams; finite structures from lattices.
- **Depends on:** stage 6. **Reuses:** `mechanics/Bloch`, Yee periodic boundaries, finite elements.
- **Risks:** cost of 3D band structures; boundary effects in finite samples.
- **Validation:** published band diagrams; finite-slab transmission against the band gaps.
- **Acceptance:** band gaps predicted by cells match finite-structure transmission within stated tolerances.
- **Done:** REQ-CHAT-20261126-019 (Prompt 21, metamaterials and effective properties; ADR-000223).

## Stage 8 — Microstructure and homogenization

- **Status:** partly current (REQ-CHAT-20261126-019): effective materials (`functional/EffectiveMaterial`) from static layered homogenization (dielectric, thermal), long-wavelength limits (resonator duct) and S-parameter retrieval, each with its validity; saved into the library with provenance `homogenized` and used by the coupled engine. Asymptotic homogenization of 2D/3D cells remains.
- **Goals:** `EffectiveProperties` from asymptotic homogenization and from S-parameter retrieval, with validity and passivity checks.
- **Depends on:** stage 7.
- **Risks:** retrieval branches; validity near resonances.
- **Validation:** layered media (exact), comparison of homogenized and full-structure responses.
- **Acceptance:** effective properties never shown without their model, cell and frequency range.
- **Done:** REQ-CHAT-20261126-019; REQ-CHAT-20261126-020 (Prompt 22, multiscale material modeling: voxel RVEs and computational homogenization of the potential problem with size studies and uncertainty, ADR-000224). Elastic RVE homogenization remains.

## Stage 9 — Multiscale external solver integration

- **Goals:** adapters to external tools as separate processes; `Computed` property models with uncertainty ([multiscale](../physics/MULTISCALE_MODELING.md)).
- **Depends on:** stages 2 and 8. **Reuses:** the plugin system and the Python worker.
- **Risks:** licenses, installation burden, unknown model errors.
- **Validation:** a phonopy- or ASE-based property compared with literature values.
- **Acceptance:** one adapter end to end, optional, with a license review.
- **Status:** the interfaces are current (REQ-CHAT-20261126-020, ADR-000224): the `Computed` provenance with derivation records and uncertainty, external engines listed with detection and requirements, and built-in Lennard-Jones reference calculations (lattice statics, NVE dynamics) as the first atomistic pathway. No external engine is installed in the reference environment, so no adapter exists yet.

## Stage 10 — Dynamic material response

- **Status:** lumped nonlinear and parametric resonance (Duffing, Mathieu) current (REQ-CHAT-20261126-018); plastic (J2), damaging, viscoelastic (standard linear solid), phase-changing (enthalpy method) and hysteretic (Preisach) materials, and state fields evolving over time-stepped coupled runs, current (REQ-CHAT-20261126-021, ADR-000225, `docs/physics/CONSTITUTIVE_MODELS.md`); finite strain, fracture, fatigue and nonlinear optics and acoustics remain.

- **Goals:** nonlinear, viscoelastic, plastic, damaging and phase-changing materials; state fields evolving during coupled runs.
- **Depends on:** stages 2, 5.
- **Risks:** stability of nonlinear solvers; parameter identification.
- **Validation:** standard material tests (relaxation, hysteresis), published benchmarks.
- **Acceptance:** each model validated against its reference before it is offered (`docs/physics/MATERIAL_STATE_VALIDATION.md`).

## Stage 11 — Function-driven inverse design

- **Status:** partly current (REQ-CHAT-20261126-017): objectives, constraints (manufacturing), sweeps, sensitivities and the optimization framework over structure parameters, with explained candidates; unit-cell topology optimization remains.

- **Goals:** objectives over coupled and cross-domain quantities (a resonance that does not drift with temperature, a band gap, an effective impedance); unit-cell topology optimization.
- **Depends on:** stages 5–8. **Reuses:** the optimization framework (all algorithms, multi-fidelity, robust, surrogates, SIMP).
- **Risks:** expensive objectives; designs exploiting model weaknesses.
- **Validation:** designs re-checked by a higher-fidelity model (multi-fidelity is available) and by experiment.
- **Acceptance:** every designed candidate re-evaluated by the full model with provenance.
- **Queued:** REQ-CHAT-20261127-001 (functionally architected matter and emergent function).

## Stage 12 — Experimental validation

- **Goals:** measurements of temperatures, frequency drift, Q, band gaps and effective properties compared with predictions, with uncertainty.
- **Depends on:** the stages whose predictions it checks. **Reuses:** the experiment framework (calibration, comparison, uncertainty).
- **Risks:** sample fabrication; sensors.
- **Validation:** the comparison is the validation.
- **Acceptance:** each validated capability has a recorded measurement comparison.
- **Queued:** REQ-CHAT-20261126-023 (Prompt 26, experimental energy–matter laboratory).

## Rules for every stage

RES-SFE-001..012 apply: computed function, one optimization framework,
distinct geometry representations, shared resonance vocabulary, derived
effective properties, coordination of independent solvers, explicit
accounting, explicit material state, multiscale through adapters, honest
visualization and status, shared conventions.
