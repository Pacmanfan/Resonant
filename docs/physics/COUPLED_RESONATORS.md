# Coupled Resonators, Energy Exchange and Nonlinear Resonance

**Status:** current (RES-RSN-004, -005, -009, -010): `src/core/oscillators`,
`mechanics/LumpedResonators`, `manipulators/Helmholtz`, the analyses
`mechanics.coupled-cantilevers` and `acoustic.coupled-helmholtz`.

## Lumped networks

M ẍ + C ẋ + K x = f(t), assembled by a domain:

| Domain | Coordinates | M | K | C |
|---|---|---|---|---|
| two cantilevers joined at the tips by a spring | tip displacements | tip-referred modal masses (¼ of each beam for mode 1) | ω₁² m of each beam + the coupling spring | structural loss factor as η k/ω per beam |
| an open Helmholtz resonator and a closed one joined by a connector | volume displacements through neck 1 and the connector | inertances ρL_eff/S | (q₁+q_c)²/2C₁ + q_c²/2C₂ | radiation and viscous resistances (at resonator 1's frequency) |

Results: complex natural modes (Im s, −Re s, Q, shapes, participation,
in-phase/anti-phase), Q by mechanism (modal), the driven response (supplied,
dissipated by mechanism, stored), and time integration (fourth-order
Runge–Kutta) with the energy ledger.

## Splitting, hybridization, localization, avoided crossing

For identical resonators f_in-phase = √(k/m)/2π, f_anti-phase = √((k + 2k_c)/m)/2π.
Detuned, the modes localize in one resonator each (participation) and never
cross: a sweep of the detuning shows an avoided crossing whose minimum gap is
the coupling splitting (`ResonanceExamples.CoupledCantilevers…`, the sweep's
`avoidedCrossings`).

## Coupled-mode theory, labelled as an approximation

κ = k_c/(2√(m₁m₂)√(ω₁ω₂)); frequencies ω̄ ± √(Δ²/4 + κ²); maximum transfer
4κ²/(4κ² + Δ²). It is reported next to the full network solution
(`coupled.cmtSplitting`, `coupled.cmtTransfer`), never instead of it.

## Energy exchange

Releasing resonator 1 alone: E₂(t) rises to its first maximum at half a beat
period 1/(2 Δf) (full transfer for identical lossless resonators, partial
when detuned or damped). Metrics: transfer fraction, transfer time, beat
period, remaining energy; curves E₁(t), E₂(t). The coupling spring's energy
is its own term, so nothing is counted twice; the ledger residual is
reported.

## Buildup and decay

Under a resonant drive the amplitude grows as 1 − e^{−t/τ}, τ = 2Q/ω; freely
the energy decays as e^{−ωt/Q} (`LumpedNetwork.BuildupAndDecay…`).

## Nonlinear resonance (Duffing)

m ẍ + c ẋ + k x + k₃x³ = F cos ωt: backbone ω_b(A) = √((k + ¾k₃A²)/m)
(harmonic balance) verified by time integration; forced response with up to
three amplitudes (the middle one unstable) — bistability and hysteresis, the
branch reached depending on where the integration starts.

## Parametric resonance (Mathieu)

ẍ + 2ζω₀ẋ + ω₀²(1 + h cos Ωt)x = 0: stability from the Floquet multipliers of
one pump period; the first tongue around Ω = 2ω₀ (edges a = 1 ± q − q²/8 with
a = 4ω₀²/Ω², q = ah/2), closed by damping below h ≈ 4ζ. This is pumping of a
parameter, distinct from an external drive.

## Limits

Lumped: the coupling is a massless spring or a connector; the individual
resonators' parameters come from their models (Euler–Bernoulli, lumped
Helmholtz); losses evaluated at one frequency; nonlinear models are single
oscillators.
