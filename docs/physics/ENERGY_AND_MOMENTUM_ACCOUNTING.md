# Energy and Momentum Accounting

**Status:** the definitions and the rule are normative (RES-SFE-007,
ADR-000219); single-domain energy records exist (below); the coupled
ledger is planned ([architecture](../architecture/COUPLED_MULTIPHYSICS_ARCHITECTURE.md)).

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
| Heat (planned) | ρ c_p T (relative to a reference) | −k ∇T | — |

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

## Reporting

A ledger entry names: the participant, the term, its definition (one of the
above, or the model's own), units, sign convention (positive into the
participant), sampling times, and the quadrature used. The coupled result
prints the per-step and cumulative residual and the tolerance it was
judged against.
