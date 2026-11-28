# Energy and Momentum Accounting

**Status:** the definitions and the rule are normative (RES-SFE-007,
ADR-000219). The coupled ledger and conservation audit are implemented
(REQ-CHAT-20261126-016, RES-CEME-009, ADR-000220: `src/core/multiphysics/Ledger`).
The unified accounting framework is current (REQ-CHAT-20261126-022,
RES-ACC-001..016, ADR-000226: `src/core/accounting`,
[ACCOUNTING_ARCHITECTURE.md](../architecture/ACCOUNTING_ARCHITECTURE.md)). It covers:

- energy for any system boundary;
- linear and angular momentum and forces, for lumped networks and the
  radiation force on layered bodies;
- entropy, where a model provides it;
- monitoring with honest statuses ([CONSERVATION_MONITORING.md](CONSERVATION_MONITORING.md)).

## The coupled ledger (implemented)

Each participant reports its external terms (input, boundary losses,
dissipation, storage); the engine adds one transfer record per exchange (source
and destination, mechanism, quantities, unit, operator, source and target
integrals, approximation) and the matching transfer-out and transfer-in
terms, so energy that moves between domains is counted once. The audit gives,
per participant and for the system, the residual

```text
participant:  input + transfer-in − boundary-out − dissipated − stored − transfer-out
system:       external input + (Σ target − Σ source over energy transfers) − boundary-out − dissipated − stored
```

and attributes what it knows: the transfer (mapping) error, the coupling error
(the change of the transferred power over the last iteration), and each
solver's own balance residual. Steady runs are accounted in watts; a run with
a transient participant in joules over its interval (steady participants'
powers multiplied by the interval). Informational terms (the elastic strain
energy of quasi-static linear thermoelasticity, which draws on no modeled
power) are reported and not summed. A wave participant whose absorbed power is
not passed on reports it as dissipated; when it heats another domain, it is a
transfer instead. `GetCoupledResult` returns the terms, transfers and audit.

```text
energy transfer and conservation accounting (the microwave-heated slab, steady, watts)

  external input            em participant                 transfer record                heat participant          boundary out
  I · A (incident) ──► [ reflected, transmitted: boundary-out ]                                                    
                       [ absorbed P_abs ] ── transfer-out ──► conservative, per reference ──► transfer-in ──► [ conduction ] ── faces: h (T − T∞) A
                                                              volume: source ∫ = target ∫                       stored: 0 (steady)
  audit:  em:   I·A − R − T − P_abs = solver residual (exact solution: rounding)
          heat: P_abs,in − Σ face losses = conduction balance residual
          system: I·A − R − T − Σ face losses + (target ∫ − source ∫)  →  relative residual vs. the tolerance (1e-6)
          attributed: transfer error, last-iteration coupling change, each solver's residual
```

Discrepancy sources the request names, and how they appear: physical
dissipation (dissipated terms), external work and boundary transfer (input
and boundary-out terms), solver error (solver residuals), interpolation error
(transfer records), coupling error (the last iteration's change), incomplete
accounting (a residual left after these).

## The unified ledger (current)

The coupled ledger above is one source of the unified accounting ledger (`coupling/AccountingAdapters`):

- **Participants become entities.** Inputs come from `outside:sources`,
  boundary outputs go to `outside:environment`, and dissipation goes to
  `outside:heat`.
- **Transfer records become exchanges.** Each has its target integral as the
  amount and its source integral as `sourceAmount`. An internal transfer is
  counted once. The difference between the two integrals is a stated
  numerical error, never a source.
- **The audit's solver residuals** are the stated numerical errors.
- **Momentum is declared missing.** The participants report none, so a
  coupled system's momentum check is "insufficient data".

Every stored coupled result carries this ledger and its whole-system report
(`accounting` in `GetCoupledResult`). The same accounting is available for
lumped networks, time-stepped evolutions, layered radiation and the
material-state models. The definitions used by each are in
[FORCE_AND_MOMENTUM.md](FORCE_AND_MOMENTUM.md) and
[ACCOUNTING_VALIDATION.md](ACCOUNTING_VALIDATION.md).

## The rule

A result that claims a balance shows its terms and its residual:

```text
d/dt E_stored  =  P_in  −  P_out,boundary  −  P_dissipated  −  Σ P_transferred,k      (per participant)
residual(t)    =  E_stored(t) − E_stored(0) − ∫ (P_in − P_out − P_diss − Σ P_tr) dt
```

Conservation is claimed only when the residual is computed and below a
stated tolerance relative to the energy that flowed. Moving a quantity
between solvers does not make a result conservative by itself: the transfer
operator must conserve the transferred power (a conservative projection),
and the ledger shows that it did.

## Definitions (SI, peak-amplitude phasors with `e^{+jωt}`)

| Domain | Energy density | Power flux | Time-averaged (harmonic) |
|---|---|---|---|
| Acoustics (linear, fluid at rest) | w = p²/(2ρc²) + ½ ρ|v|² | I = p v | ⟨w⟩ = |P|²/(4ρc²) + ρ|V|²/4, ⟨I⟩ = ½ Re{P V*} |
| Electromagnetics (linear, non-dispersive) | w = ½ ε|E|² + ½ μ|H|² | S = E × H | ⟨w⟩ = ¼ ε|E|² + ¼ μ|H|², ⟨S⟩ = ½ Re{E × H*} |
| Elastic solids | w = ½ ρ|u̇|² + ½ σ:ε | −σ·u̇ | ⟨w⟩ = ¼ ρω²|U|² + ¼ Re{σ:ε*} |
| Heat (`thermal/HeatConduction`) | ρ c_p T (relative to a reference) | −k ∇T | — |

Dissipation: acoustic attenuation and viscous losses; ½ σ|E|² and
½ ωε₀ε″|E|² in electromagnetics; damping forces (Rayleigh damping in the
mechanical solvers) in solids. Dispersive materials need the dispersive
energy expressions; until such materials exist, the non-dispersive ones
apply and say so.

## What is recorded today

| Solver | Record | Meaning and limits |
|---|---|---|
| Acoustic FDTD (`fdtd/FdtdGrid::energy`, `FdtdRun`) | energy of the whole grid over time (J; J/m in 2D) | includes the absorbing layers; constant to rounding in a closed lossless domain once sources end |
| Yee (`electromagnetics/Yee::energy`) | electromagnetic energy over time | an estimate: E and H are half a step apart, so it ripples by about ω·dt; it does not grow in a stable closed lossless domain |
| Mechanical transient (`mechanics/Analyses`, `TransientSolution`) | kinetic, strain, external work and dissipated energy over time | the complete single-domain ledger: kinetic + strain + dissipated − external work is the residual |

These are the first ledger entries: a coupled run will collect them per
participant and add the transfer terms.

## Momentum and force

Where a function is a force (levitation, radiation pressure on a membrane,
optical or electromagnetic forces), momentum is accounted for like energy:

| Domain | Momentum density | Momentum flux / stress |
|---|---|---|
| Acoustics | ρ₀ v (first order); the radiation force is second order | the time-averaged momentum flux: radiation stress ⟨ρ₀ v v⟩ + (⟨p²⟩/(2ρ₀c²) − ½ρ₀⟨|v|²⟩) I, integrated over a surface enclosing the object |
| Electromagnetics (vacuum) | g = S/c² | Maxwell stress tensor T = ε₀ E E + μ₀⁻¹ B B − ½ (ε₀|E|² + μ₀⁻¹|B|²) I |
| Solids | ρ u̇ | the Cauchy stress σ |

A force result states the surface it integrates over, the order of the
approximation (radiation force is a second-order quantity computed from
first-order fields) and the medium it assumes.

Current: the vacuum Maxwell stress at the control surfaces of a layered body,
and the volume Lorentz force on its induced currents
(`electromagnetics/RadiationForce`, [FORCE_AND_MOMENTUM.md](FORCE_AND_MOMENTUM.md)).
Lumped networks give their bodies' momentum and their elements' forces. The
acoustic radiation force and the Maxwell stress of the full-wave solvers are
not implemented.

## Reporting

A ledger entry names: the participant, the term, its definition (one of the
above, or the model's own), units, sign convention (positive into the
participant), sampling times, and the quadrature used. The coupled result
prints the per-step and cumulative residual and the tolerance it was
judged against.
