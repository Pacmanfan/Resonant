# Metamaterials and Engineered Effective Properties

**Owner of:** `RES-META-001..012`
**Source:** REQ-CHAT-20261126-019 (Prompt 21, "Metamaterials and Engineered Effective Properties")
**Refines:** `shape_function_energy.md` RES-SFE-005 (effective properties derived and labelled); `functional_structures.md` RES-FUNC-010 (periodic cells); `resonance.md` RES-RSN-004 (lumped networks)
**Decisions:** ADR-000223 (builds on ADR-000221, ADR-000222, ADR-000220)
**Documents:** `docs/materials/METAMATERIAL_ARCHITECTURE.md`, `docs/materials/EFFECTIVE_PROPERTIES.md`, `docs/physics/HOMOGENIZATION_AND_VALIDITY.md`, `docs/physics/METAMATERIAL_VALIDATION.md`, `docs/materials/METAMATERIALS_AND_ENGINEERED_STRUCTURES.md` (updated)
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-META-001 — Metamaterials are structured assemblies, not materials with exotic values

A metamaterial is a functional structure whose analysis computes effective
behavior from constituent materials, unit-cell geometry and organization. No
effective value (negative or otherwise) is entered by hand; every one is a
model result with its method, validity and derivation.
*Verification:* `MetamaterialExamples.*`, `MetamaterialServices.*`.

### RES-META-002 — Distinct, traceable representations

Constituent materials, microstructure (unit-cell geometry), the periodic
structure, the effective material and an equivalent (reduced) model are
reported separately; an effective material names the structure and the
geometry and material keys it was derived from.
*Verification:* `MetamaterialServices.EffectiveMaterialsKeepTheirDerivation`.

### RES-META-003 — Bloch–Floquet analysis

Dispersion and band structures with the Brillouin-zone wavenumber, gaps and
their attenuation, for lumped lattices (numerical Bloch eigenproblems of a
unit cell with phase-shifted couplings, checked against closed forms),
acoustic ducts (cell transfer matrices) and layered electromagnetic cells at
oblique incidence (TE and TM).
*Verification:* `Bloch.*`, `LocallyResonantLattice.*`, `ResonatorDuct.*`, `ObliqueLayers.*`.

### RES-META-004 — Effective electromagnetic parameters by retrieval

ε and μ of a finite slab retrieved from its reflection and transmission (the
impedance and index method) with branch tracking by phase continuity,
passivity checks and flags where retrieval is unstable or the slab is not
local (a/λ, Fabry–Pérot zeros of the denominator).
*Verification:* `Retrieval.*`.

### RES-META-005 — Anisotropic layered homogenization

A layered medium's quasi-static uniaxial permittivity (ε_∥ arithmetic, ε_⊥
harmonic) and the explicit multilayer at oblique incidence (TE and TM) are
compared; the difference is quantified and grows with a/λ; TE does not see
ε_⊥, TM does.
*Verification:* `ObliqueLayers.*`, `MetamaterialExamples.LayeredAnisotropy*`.

### RES-META-006 — Resonant mechanical lattice

A mass-in-mass lattice: the closed-form dispersion, the locally resonant gap,
the effective dynamic mass m_eff(ω) = m₁ + m₂ω₂²/(ω₂² − ω²) (negative in the
gap, derived), and a finite chain's transmission decaying at the predicted
attenuation.
*Verification:* `LocallyResonantLattice.*`, `MetamaterialExamples.MassInMass*`.

### RES-META-007 — Resonant acoustic duct

A duct loaded periodically by Helmholtz side branches: Bloch dispersion from
the cell transfer matrix, the hybridization gap at the resonators and the
Bragg gap, the effective bulk modulus (negative above the resonance), and a
finite array's transmission against the attenuation.
*Verification:* `ResonatorDuct.*`, `MetamaterialExamples.ResonatorDuct*`.

### RES-META-008 — Split-ring resonators (circuit model)

A split-ring array as a quasi-static circuit (inductance of the ring, gap
and surface capacitance, ohmic loss): resonance, Lorentzian effective
permeability with its filling factor and the band where Re μ < 0, causality
(Kramers–Kronig) and passivity checks; labelled as an approximation that is
not compared with a full-wave calculation.
*Verification:* `SplitRing.*`.

### RES-META-009 — Thermal effective conductivity

A layered medium's effective conductivity tensor (series and parallel),
checked against the finite-volume conduction solver on the explicit layers.
*Verification:* `LayeredConductivity.*`.

### RES-META-010 — Validity of homogenization

Every effective result states its method, its validity (scale separation
a/λ or a/L, frequency range), whether it is frequency-dependent, and the
difference between the effective and the explicit model where both are
computed; effective values are reported only inside their validity.
*Verification:* `MetamaterialExamples.*`, `Retrieval.*`.

### RES-META-011 — Effective materials in the library and the coupled engine

An analysis's effective material can be saved into the project's library
(one undoable step) with provenance `homogenized`, frequency tables for
dispersive values, and its derivation in the source; the Coupled
Energy–Matter Engine uses it like any material, and a homogenized region
reproduces the explicit layered system within the stated tolerance.
*Verification:* `MetamaterialServices.*`.

### RES-META-012 — Examples, services and clients

Example projects for the mass-in-mass lattice, the resonator duct, the
layered anisotropic medium, the split-ring array and the layered thermal
medium; the analyses through `FunctionalStructureService`; effective
materials saved through the services, Python and MCP; the panel offers it.
*Verification:* `MetamaterialExamples.*`, `MetamaterialServices.*`, `python/tests/test_metamaterial_api.py`, `FunctionalPanel.*`.
