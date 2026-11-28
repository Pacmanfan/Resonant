# ADR-000222: Resonance engineering — a shared resonance description on the functional analyses, domain-independent spectroscopy and lumped oscillator networks, domain builders, and a resonant cavity in the coupled engine

**Status:** ACCEPTED
**Date:** 2026-11-27
**Type:** architecture
**Requirements:** `docs/requirements/resonance.md` RES-RSN-001..014
**Request:** REQ-CHAT-20261126-018
**Builds on:** ADR-000221 (functional structures), ADR-000220 (coupled engine), ADR-000214 (optimization)

## Context

Functional structures (ADR-000221) already compute modes, Q and sweeps for a
cavity, an optical resonator, a cantilever and periodic cells. Missing: a
common description of resonances (their nature, loss breakdown, energies),
identification of resonances from signals and spectra, models of coupled
resonators and energy exchange, a Helmholtz resonator, nonlinear and
parametric resonance, and a resonator in the coupled engine (which supports
layered slabs only).

## Decisions

1. **One description, on the existing modes.** `functional::Mode` gains its
   nature, decay rate, bandwidth and Q per loss mechanism; every analysis
   fills what its model computes. No resonance registry separate from the
   functional analyses.
2. **Spectroscopy is signal processing.** Lorentzian fitting and the matrix
   pencil method live in `src/core/dsp/Resonance` (domain-independent), with
   explicit refusals for insufficient resolution or duration.
3. **Lumped oscillator networks are a domain-independent module**
   (`src/core/oscillators`): matrices M, C (by mechanism), K; complex modes,
   driven response, time integration with an energy ledger, coupled-mode
   theory as a labelled approximation, Duffing and Mathieu models. The
   domains build networks from their physics (`mechanics/LumpedResonators`,
   `manipulators/Helmholtz`); the layering rule treats the module as
   infrastructure.
4. **Helmholtz resonators in the manipulators module** with a geometry kind
   `resonator.helmholtz` and its full-wave solid model; the lumped model is
   verified against an exact stepped-duct transfer-matrix model.
5. **A resonant cavity in the coupled engine.** A geometry kind
   `rectangular-cavity` for physical systems with region dimensions; two
   participants (`em.cavity.mode`: resonance, port, wall and fill loss from
   `electromagnetics/CavityResonator`; `thermal.lumped`: heat capacity and
   conductance) and a coupling model `thermal.uniform-expansion`
   (geometry update); the existing absorption-to-heat and
   temperature-to-electromagnetic-properties models apply unchanged. The
   engine's orchestration is unchanged.
6. **Services.** `ResonanceService.IdentifyResonances` for user data; new
   analyses through `FunctionalStructureService`; modes added to the
   contract's functional result (additions only).

## Consequences

- Coupled resonator models are lumped: the individual resonators' parameters
  come from their domain models; a full finite-element model of two beams
  joined by a spring is not built.
- No resonance of fluids beyond lumped acoustics; no nonlinear full-field
  solver; parametric and Duffing models are lumped.
- The heated cavity is a lumped thermal model (uniform temperature).
