# ADR-000223: Metamaterials as functional structures with effective properties derived by domain methods, a homogenized provenance, and effective materials saved into the library

**Status:** ACCEPTED
**Date:** 2026-11-27
**Type:** architecture
**Requirements:** `docs/requirements/metamaterials.md` RES-META-001..012
**Request:** REQ-CHAT-20261126-019
**Builds on:** ADR-000221 (functional structures), ADR-000222 (resonance, lumped networks), ADR-000220 (coupled engine), ADR-000219 (effective properties derived)

## Context

Resonant already had 1D photonic cells (exact Bloch, quasi-static
homogenization), a periodic elastic rod, lumped oscillator networks, a
Helmholtz model and a finite-volume conduction solver. Missing: retrieval of
effective parameters, anisotropic homogenization checked at oblique
incidence, resonant (locally resonant) lattices, effective-property
provenance, and a way to use an effective material elsewhere.

## Decisions

1. **No separate metamaterial resource.** A metamaterial is a functional
   structure whose analysis (category `effective-medium`) computes effective
   properties; its unit cell is parameterized like any structure.
2. **Domain methods, no universal calculator:** S-parameter retrieval and
   oblique layered stacks in electromagnetics, a split-ring circuit model in
   electromagnetics, the mass-in-mass lattice in mechanics, the resonator duct
   in manipulators (acoustics), layered conductivity in thermal; numerical
   Bloch eigenproblems of lumped cells in the oscillators module.
3. **An effective-material record** (`functional/EffectiveMaterial`): the
   properties (constant or frequency tables), method, kind (homogenized,
   retrieved, equivalent model), validity and derivation keys; an analysis
   returns it only inside its validity.
4. **Provenance `homogenized`** joins the material provenances (project files,
   contract enum value 6, the materials dialog); a saved effective material
   carries the derivation in its source.
5. **Saving is a service operation** (`FunctionalStructureService.CreateEffectiveMaterial`,
   undoable) used by the panel, Python and MCP; the coupled engine uses the
   saved material through the ordinary material interfaces.

## Consequences

- No full-wave split-ring or 3D lattice computation (no periodic plane-wave
  port in the Yee solver); the split ring is a labelled circuit model.
- 2D/3D Bloch of continua (plane-wave or finite-element) remains planned.
- Existing projects are unchanged; the provenance addition is additive.
