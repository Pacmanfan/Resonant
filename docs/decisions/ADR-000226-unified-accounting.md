# ADR-000226: Unified accounting: dimensions and quantity families, a ledger classified by system boundaries, conservation and consistency checks, adapters on authoritative solver outputs

**Status:** ACCEPTED
**Date:** 2026-11-28
**Type:** architecture
**Requirements:** `docs/requirements/accounting.md` RES-ACC-001..016
**Request:** REQ-CHAT-20261126-022
**Builds on:** ADR-000220 (coupled ledger), ADR-000222 (resonance), ADR-000224 (lineage), ADR-000225 (material state)

## Context

Energy was balanced per coupled run (`multiphysics/Ledger`, steady watts or
joules over an interval), per lumped network (supplied = stored +
dissipated) and per material-state model, each with its own residual. There
was no momentum or force accounting, no notion of a chosen system boundary,
no dimensional analysis beyond display units, and no common status for a
check that could not be performed.

## Decisions

1. **Dimensions in foundation.** `foundation/Dimensions` parses SI unit
   expressions into dimension exponents and a factor; it is used to convert
   display units and to verify the registered quantities' units.
2. **A domain-independent `accounting` module** (no physics, no domains):
   quantity families with dimensions and a conserved flag; entities;
   exchanges and holdings with histories; force records with pairs;
   accounting systems (members); a registry of conservation laws and
   consistency relations; evaluation of a ledger for a system giving checks
   with statuses, a breakdown and a flow graph. The same ledger is evaluated
   for any system: internal or boundary is decided by membership, never
   stored.
3. **Adapters in coupling** convert authoritative outputs: the coupled
   engine's terms and transfer records (and the steps of an evolution), the
   lumped-network integrator (which now optionally records each element's
   work and impulse on each coordinate, with the same Runge–Kutta stages),
   the layered electromagnetic solution (radiation force by the vacuum
   Maxwell stress and the volume Lorentz force, `electromagnetics/RadiationForce`),
   and the material-state run documents. No adapter recomputes a solver's
   physics.
4. **Rate and interval bases.** A steady ledger is in rates (W, kg·m/s²); a
   transient one in amounts over its interval (J, kg·m/s). Stored energy of
   a steady state (for Q) is an informational quantity, not a holding.
5. **Honest statuses.** Declared missing contributions make a check
   "insufficient data"; dissipation is only what a model reports; the
   residual is compared with the stated numerical errors.
6. **AccountingService** with studies and records in the project extension
   `accounting`, a contract, Python and MCP clients, and the Accounting panel.

## Consequences

- Momentum is accounted where a model supplies it: lumped networks
  (translational and rotational) and the layered radiation force. The
  coupled participants report no momentum, so the coupled system's momentum
  check is "insufficient data".
- Entropy is evaluated for the phase-change slab and for isothermal
  dissipation; other solvers provide no entropy and are "not evaluated".
- Local conservation is checked where a volume integral can be compared
  with boundary fluxes (the layered absorption); elsewhere only global checks
  exist and are labelled global.

## Deviations from the request

- No acoustic, fluid or FDTD momentum accounting (no solver computes
  radiation force or momentum flux there); listed as missing.
- Moving control volumes, open systems with mass flow, and charge and mass
  balances are registered models without data ("not applicable").
- Spatial accounting views are limited to the layered profiles (absorbed
  power density, force density); no 3D field accounting.
