# Resonance Architecture

**Status:** implemented (REQ-CHAT-20261126-018, RES-RSN-001..014, ADR-000222).
Resonance is computed by domain models and reported in one form; there is
no resonance subsystem beside the functional analyses
([functional structures](../architecture/FUNCTIONAL_STRUCTURE_ARCHITECTURE.md)).

```text
 structure + materials + excitation + environment + boundaries
                │
   domain model (cavity, optical resonator, Helmholtz, cantilever, coupled network, coupled engine)
                │
   modes: label · nature (natural | driven-peak | identified) · frequency · decay · Q · Q by mechanism · bandwidth
   metrics: stored, supplied, dissipated, transferred, extracted energy where the model computes them
   curves: responses, ring-downs, energy exchange
                │
   studies: sweeps (mode tracking, avoided crossings) · sensitivities · objectives · optimization
```

## Modules

| Module | Content | Layering |
|---|---|---|
| `dsp/Resonance` | spectroscopy: matrix-pencil poles, Lorentzian fits ([page](RESONANCE_SPECTROSCOPY.md)) | signal processing, no domain |
| `oscillators/LumpedNetwork`, `Nonlinear` | lumped networks, coupled modes, energy ledger, Duffing, Mathieu ([page](COUPLED_RESONATORS.md)) | infrastructure (layering rule), no domain |
| `electromagnetics/CavityResonator` | cavity modes, losses, port, Yee model and its ring-down record | electromagnetic domain |
| `manipulators/Helmholtz` | Helmholtz resonator, stepped duct, coupled Helmholtz network ([page](../manipulators/HELMHOLTZ_RESONATORS.md)) | acoustic domain |
| `mechanics/LumpedResonators` | cantilever modes as oscillators, coupled cantilevers | mechanical domain |
| `coupling/ResonanceAnalyses`, `FunctionalAnalyses` | the analyses | coupling |
| `coupling/CavityParticipants` | the cavity in the Coupled Energy–Matter Engine | coupling |
| `services/ResonanceServices` | identification of user data | services |

## Natural modes, driven peaks, identified resonances

| Nature | Where it comes from | Example |
|---|---|---|
| natural | an eigenproblem or closed form (complex frequency: Im s, −Re s) | TE101, Helmholtz f0, the in-phase and anti-phase modes of two cantilevers |
| driven-peak | the maximum of a response to an excitation | the Helmholtz cavity's pressure-gain peak (it lies off f0 with damping) |
| identified | poles of a simulated or measured signal | the cavity's Yee ring-down, a cantilever's simulated decay |

They are reported apart (`LumpedNetwork.DrivenPeaksDifferFromNaturalFrequencies…`):
a damped displacement peak lies at f0 √(1 − 2ζ²), below the natural frequency.

## Energy storage, supply, dissipation, transfer, extraction

| Quantity | Where |
|---|---|
| stored | cavity W_e + W_m (per peak field), Helmholtz ½C|p|² per incident pascal², cantilever kinetic energy per tip amplitude², network kinetic + potential |
| supplied | network ∫f·v dt; driven response ½Re(F*V); the coupled engine's input terms |
| dissipated | per mechanism: cavity wall and fill, Helmholtz radiation and viscous, structural dampers, network dampers |
| transferred | between coupled resonators (time integration, energy exchange metrics); between domains (the coupled engine's transfers) |
| extracted | the cavity port: extraction efficiency β/(1+β) |
| remaining after excitation | ring-down records and the energy-exchange curves |

The network ledger attributes the coupling spring's energy to the coupling
and checks supplied = Δstored + dissipated at every sample
(`LumpedNetwork.CouplingEnergyIsCountedOnceInTheLedger`); a field amplitude
is never reported as stored energy.

## Damping and losses

Every model names its mechanisms and combines them as 1/Q = Σ 1/Q_i. A
single arbitrary damping parameter is not offered: the cantilever's loss
factor is the material's structural damping, the Helmholtz losses are the
radiation of its mouth and the viscous boundary layer of its neck, the
cavity's are its walls, its fill and its port.

## Q and bandwidth

Q = 2π (stored)/(lost per cycle) = ω/(2α) for a mode decaying as e^{−αt};
Q = f0/Δf holds for an isolated, lightly damped (Lorentzian) resonance — the
spectroscopy flags overlapping and asymmetric lines where it does not.

## Frequency and time domain

| Frequency domain | Time domain |
|---|---|
| eigenmodes, driven response, impedance (Helmholtz), port reflection (cavity), transfer functions (networks) | ring-down (cavity Yee, cantilever), buildup under a drive, beating and energy exchange (networks), nonlinear free and forced response (Duffing) |

## Nonlinear and parametric resonance

Lumped models only ([coupled resonators](COUPLED_RESONATORS.md)): Duffing backbone and
forced response (bistability, hysteresis), Mathieu stability by Floquet
multipliers. Linear sweeps never claim nonlinear behavior.

## Coupled physics

The cavity in the Coupled Energy–Matter Engine
([architecture](../architecture/COUPLED_ENGINE_ARCHITECTURE.md)): driven at its
cold resonance, heated by its losses, its fill's permittivity falling and its
walls expanding, the resonance moves away from the drive; the engine
iterates to the fixed point with its ledger (`CavityHeating.*`,
`examples/resonance/d-heated-cavity.resonant`).

## Workflow

Open a structure (examples in `examples/resonance` and `examples/functional`),
analyze it (Functional Structures panel, Python or the API), read its
resonances (nature, Q by mechanism), sweep a dimension to tune it (mode
tracking and avoided crossings), compare models (closed form against a
full-wave or plane-wave model), set objectives and optimize; identify
resonances of your own signals with `ResonanceService.IdentifyResonances`.

## Not covered

Q from radiation of open structures in general, fluid resonances beyond
lumped acoustics, full-field nonlinear solvers, coupled resonators built as
a single finite-element model, optical coupled cavities.
