# Accounting Audit: Energy, Momentum and Force Before Unified Accounting

**Status:** the audit behind REQ-CHAT-20261126-022 (RES-ACC-001, ADR-000226),
taken on 2026-11-28 before the accounting framework was added. It records
what was computed where. The framework that coordinates these calculations
is described in [ACCOUNTING_ARCHITECTURE.md](ACCOUNTING_ARCHITECTURE.md).

## What each module computed

| Module | Energy and power | Balance check | Force, momentum |
|---|---|---|---|
| acoustics | particle velocity, active intensity ½Re{p v*}, SPL (`FieldEvaluator`, `FieldGrid`) | none (the inverse-design residual is a field fit) | none |
| manipulators | horn radiated and mouth power, Helmholtz radiation and viscous resistance, Q, stored energy | `horn.powerBalance` (mouth / radiated − 1) | none |
| fdtd | total acoustic energy (kinetic + potential), sampled | instability detection only; the PML cells are inside the energy, no outflow term | none |
| electromagnetics | Poynting flux, absorbed power density ½ωε₀ε″\|E\|² (`LayeredMedia`); cavity W_e, W_m, wall and fill loss, Q; Yee grid energy; antenna radiation resistance and gain | 1 − R − T against the flux difference; Yee energy-decay fit | none: no Maxwell stress, Lorentz force or radiation pressure |
| mechanics | transient kinetic, strain, external work, dissipated (dissipated defined as the remainder); static compliance; von Mises stress; the material-state energies | plasticity bar equilibrium; damage, plasticity, viscoelasticity work = stored + dissipated | the cantilever's tip reaction, the bar's segment forces; `ForceInput` with an object id and a label only |
| thermal | face heat flows, source, stored (transient), the phase-change slab's sensible and latent heat | `balanceResidual` = source − stored − out | none |
| atomistic | molecular-dynamics kinetic, potential, total energy, temperature | energy drift | pair forces, the lattice's maximum force and pressure; total momentum zeroed at start, never monitored |
| multiphysics | the coupled ledger: input, boundary out, dissipated, stored, transfer records | per participant and system, with the transfer error | none: scalar energy only |
| coupling | the participants' terms; material-state run energies; evolution totals | the runs' work − stored − dissipated | thermal loads as `ForceInput` ("thermal load") |
| oscillators | supplied power, dissipation by mechanism, kinetic and potential (driven and transient) | transient max \|supplied − ΔE − dissipated\| | none |
| multiscale | — | Hill–Mandel energy consistency of the homogenization | — |
| dsp | Q, decay rate, bandwidth from ring-down and Lorentzian fits | — | — |

## Findings

1. **Duplication.**
   - ½Re(p u\*) is computed in acoustics and in the horn.
   - ½Re(E × H\*) is computed in `LayeredMedia` and `EmStudy`.
   - Strain energy is computed in five places: the mechanical transient, the
     damaged beam, the solid-column participant, damage and plasticity.
   - Q is computed in seven places: dsp, oscillators, cavity, Helmholtz,
     split ring, optical resonator and the mechanical experiments.

   None of these is wrong. The accounting framework does not add an eighth;
   it consumes them.
2. **Domain transfers.** Inter-domain energy transfers are explicit only in
   the coupled engine's transfer records, with their source and target
   integrals.
3. **Boundary fluxes.** Boundary fluxes are accounted by the coupled
   participants (reflected, transmitted, convection). The FDTD and Yee PMLs
   absorb without an outflow term.
4. **Numerical error.** Numerical error is measured per solver (the
   integrator's residual, the solvers' own balances), with no common status.
5. **Momentum, force and entropy.** Nothing was computed for torque, angular
   momentum or entropy. Momentum was never monitored, and radiation pressure
   and Maxwell stress were listed as planned couplings. Forces carried an
   object and a label, but no origin, pair or frame.

## Architecture map (before, with what the framework adds)

```text
 solvers (authoritative)                         adapters (coupling)            accounting (no physics)
 multiphysics Ledger + Audit  ─────────────────► coupledLedger, evolutionLedger ─┐
 oscillators integrate (+ element flows, new) ──► networkLedger, driven, Q, decay ┤
 electromagnetics LayeredSolution               ► radiationLedger ────────────────┼─► Ledger ─► evaluate(system) ─► Report
   + RadiationForce (Maxwell stress, Lorentz, new)                               │   (entities, exchanges,       (checks, breakdown,
 material-state runs (+ entropy, hysteron loss, new) ► materialStateLedger ──────┘    holdings, forces,           flows, forces,
                                                                                      missing, numerical,         histories)
 foundation Dimensions (new) ─── units of every family and of the quantity registry   contracts, checks)
```
