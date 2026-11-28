# Metamaterials and Engineered Structures

**Status:** one-dimensional structures are current: the periodic rod
(`docs/mechanics/PHONONIC_STRUCTURES.md`), the two-layer photonic cell, and the
effective-medium analyses of REQ-CHAT-20261126-019 (locally resonant
mass-in-mass lattice, Helmholtz-loaded duct, layered dielectric with retrieval,
split-ring circuit model, layered thermal medium; effective materials saved
into the library) — see [the architecture](METAMATERIAL_ARCHITECTURE.md),
[effective properties](EFFECTIVE_PROPERTIES.md),
[validity](../physics/HOMOGENIZATION_AND_VALIDITY.md) and
[validation](../physics/METAMATERIAL_VALIDATION.md). 2D/3D lattices and the
rest of the planned capabilities below remain planned (RES-SFE-005, ADR-000219, ADR-000223).

An engineered structure gets its behavior from its internal geometry: a
periodic arrangement scatters waves coherently (Bragg band gaps), small
resonant inclusions act together below the Bragg limit (local resonances),
layers and lattices give directional or graded responses. The resulting
*effective* behavior can differ from every constituent material's — and it
is a result of a model, never a label (RES-SFE-001, RES-SFE-005).

## Kinds of structures

| Structure | Mechanism | Typical effective behavior |
|---|---|---|
| Phononic crystal (elastic, acoustic) | Bragg scattering, cell ~ λ/2 | band gaps, slow waves, directional propagation |
| Photonic crystal / EM periodic structure | Bragg scattering | band gaps, frequency-selective surfaces |
| Locally resonant metamaterial | sub-wavelength resonators | band gaps below Bragg, effective density or modulus with resonant (possibly negative) behavior near the resonance |
| Split-ring / wire media (EM) | magnetic and electric resonances | effective permeability and permittivity, possibly negative in a band |
| Layered composite | layer impedances | anisotropic effective properties, transmission filters |
| Architected lattice | geometry of struts and nodes | effective stiffness, density, Poisson's ratio (incl. auxetic) |
| Graded structure | spatially varying cell | lenses, cloaks in the ray limit, impedance tapers |
| Porous material | viscous and thermal boundary layers | equivalent-fluid absorption |

## Five things kept apart

1. **Constituent properties** — the materials the structure is made of (the
   material library).
2. **The full structural response** — the whole structure solved with its
   geometry resolved (FDTD, Yee, finite elements). The reference.
3. **Band structure** — Bloch analysis of one unit cell (dispersion, gaps).
4. **Effective (homogenized) properties** — derived by a homogenization
   (asymptotic, static) or retrieval (from simulated or measured reflection
   and transmission) model, frequency-dependent, valid only where the model
   is (cell small against the wavelength, away from band edges, in the bulk).
5. **Localized resonant effects** — modes of an inclusion or a defect, which
   effective properties may hide.

Every effective-property result records the model, the unit cell, the
frequency range, the cell-to-wavelength ratio, and how it was validated
against the full structure (RES-SFE-005).

## Planned capabilities (stage 7 and 8 of the roadmap)

Current since REQ-CHAT-20261126-019: Bloch analysis of lumped 1D cells,
S-parameter retrieval of slabs with branch tracking and passivity checks,
static homogenization of layered media (dielectric and thermal), and
band-gap and effective-property metrics as optimization targets. Planned:

- **Unit cells and lattices** as engineering geometry (`UnitCell`,
  `Lattice`: Bravais vectors, the cell's geometry and material regions),
  instantiated into finite structures (RES-SFE-003).
- **Bloch analysis** in 2D and 3D: elastic (finite elements with Bloch
  boundary conditions, generalizing `mechanics/Bloch`), acoustic, and
  electromagnetic (the Yee solver already has periodic boundaries; phase-
  shifted periodic boundaries are needed for Bloch wavevectors).
- **Retrieval** of effective parameters from S-parameters (Nicolson–Ross–Weir
  style for EM, its acoustic analogue), with branch selection and passivity
  checks; negative parameters only where retrieval and the full structure
  agree.
- **Homogenization** (asymptotic) for static and low-frequency effective
  stiffness, density, permittivity.
- **Function-centric design**: band-gap position and width, effective
  impedance or index targets, through the optimization framework (topology
  optimization of unit cells extends the existing SIMP work).

## Validation

Analytical cases (1D layered media, the periodic rod already validated),
published band diagrams of standard cells, convergence in the cell mesh,
effective properties compared with full-structure simulations of finite
slabs, and measured transmission through fabricated samples (experiment
framework).

## Risks

Retrieval ambiguity (branches), effective parameters outside validity near
resonances and band edges, boundary layers of finite samples, fabrication
tolerances that move narrow resonances (tolerances belong in robust
optimization, already available), and losses that wipe out predicted
negative-index bands.
