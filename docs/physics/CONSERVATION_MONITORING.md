# Conservation Monitoring

**Status:** current (REQ-CHAT-20261126-022, RES-ACC-010..015, ADR-000226),
`accounting/Conservation`.

## The registry

| Id | Kind | Equation |
|---|---|---|
| `energy.total` | conservation | ΔE_stored = E_in − E_out + E_generated − E_dissipated |
| `momentum.linear` | conservation | ΔP = P_in − P_out + I_external (per component) |
| `momentum.angular` | conservation | ΔL = L_in − L_out + H_external |
| `entropy.second-law` | conservation | ΔS = S_in − S_out + S_produced, with S_produced ≥ 0 |
| `mass`, `charge` | conservation | registered; no current model reports them |
| `resonance.quality-factor` | consistency | Q_m = ω₀ ⟨E⟩ / ⟨P_m⟩ against the modal Q |
| `resonance.buildup-decay` | consistency | E_ss (1 − e^(−γt))² while driven; decay at 2γ |
| `radiation.force-methods` | consistency | Maxwell stress = Lorentz volume force = (1 + R − T) I / c |
| `absorption.local-global` | consistency (local) | ∫ ½ωε₀ε″\|E\|² dz = S(0) − S(d) |
| `hysteresis.loop-loss` | consistency | μ₀∮H dM = Σ over the switched relays of 2μ₀ Ms w (α − β) |

Each model names its sources, sinks, fluxes, required data, method and
assumptions (`ListConservationModels`).

## A check

A check reports the following:

- the initial and final holdings, the input (fluxes), the impulse, the
  output, the internal transfers, the generation, the dissipation and the
  change;
- the residual;
- a relative residual, only when the scale is above the absolute floor;
- the tolerance and the warning threshold, the stated numerical error and
  the attribution;
- any missing contributions, notes, and the residual's history when every
  contribution has one.

## Statuses

| Status | When |
|---|---|
| verified | relative residual ≤ tolerance (or the residual is below the floor when there is no meaningful scale) |
| warning | above the tolerance, within the warning threshold; or verified but with a possible double counting |
| exceeded | above the warning threshold; or negative entropy production |
| insufficient data | a declared contribution is missing, whatever the residual |
| not evaluated | double counting across scales (an effective description and the representation it replaces both hold the quantity); no energy reported |
| not applicable | the quantity is reported nowhere (momentum of a heat problem); a consistency relation whose assumptions do not hold (Q of a heavily damped mode) |

A green status is never shown for a check that was not performed.

## Numerical error and physical loss

Dissipation is only what a model reports as dissipated, so no residual is
ever moved into it. The residual is compared with the numerical errors the
sources state:

- the integrator's own energy residual;
- the transfer mapping (target minus source integrals);
- a solver's balance;
- the work quadrature (Richardson);
- Simpson's rule at half the points;
- the field-increment bound of a hysteresis loop.

A residual within those errors is attributed to them. A larger one is
"unaccounted": a missing contribution, a model error, or a numerical error
nobody stated.

## Refinement, time, space

- **Refinement.** A transient network study with `refine` runs again at half
  the step and reports each check's worst residual at both steps, their
  ratio and the observed order.
- **Histories.** The ledgers carry sampled histories, so every check has a
  residual history, and the panel plots it.
- **Local checks.** `absorption.local-global` is labelled `local` (a volume
  integral against boundary fluxes). Every other check is global, and no
  local conservation is claimed from it.

## Double counting

- **The same mechanism twice.** Two exchanges carrying the same mechanism
  between the same entities are flagged. So is a destination given a share
  that its source's calculation already includes (`includesDestination`).
- **Scale contracts.** A region using an effective material records where
  the effective values came from, and that the source's energy is not
  represented again. A system holding energy in both is not evaluated.
