# Coupled Multiphysics Architecture

**Status:** the direction (ADR-000219, RES-SFE-006, RES-SFE-007, RES-SFE-012).
Its first implementation, for layered systems, is described in
[the Coupled Energy–Matter Engine architecture](COUPLED_ENGINE_ARCHITECTURE.md)
(REQ-CHAT-20261126-016, ADR-000220): the building blocks below exist as
`multiphysics/Quantity`, `FieldData`, `Transfer`, `Orchestrator` (participants,
scheduler), `Ledger` and `coupling/CouplingModels`; the microwave-heating
example below runs in one dimension (`examples/multiphysics`). The one-way
plate radiation (`coupling/StructuralRadiation`) is not yet a participant pair. The physics of coupling is in
[coupled energy–matter](../physics/COUPLED_ENERGY_MATTER.md); the gaps this
fills are in [the audit](SHAPE_FUNCTION_ENERGY_AUDIT.md).

## Principle: coordinate, do not replace

The Coupled Energy–Matter Engine connects the existing solvers; it is not a
new solver. Each domain keeps its equations, discretization, time stepping
and numerical method. The engine owns only what lies *between* them: which
quantity goes where, how it is transferred, in what order, until when, and
what the energy books say.

```text
                    ┌─────────────────── src/core/coupling ───────────────────┐
                    │                                                         │
                    │   CouplingGraph ──► CouplingScheduler ──► EnergyLedger  │
                    │        │                  │                     ▲       │
                    │        ▼                  ▼                     │       │
                    │   Exchange(quantity, from, to, TransferOperator)│       │
                    └────────┬──────────────────┬─────────────────────┼───────┘
                             │                  │                     │
                CouplingParticipant   CouplingParticipant   CouplingParticipant
                   (adapter)              (adapter)             (adapter)
                       │                      │                     │
             electromagnetics/Yee     thermal/               mechanics/FE
             acoustics/FDTD ...       its own grid          its own mesh
```

Layering stays as ADR-000205 and ADR-000210 set it: domains never include
each other or `coupling`; `coupling` includes the domains; the services
(`src/core/services`) start coupled runs as jobs; `tools/check_layering.py`
enforces it.

## Building blocks

| Block | Responsibility |
|---|---|
| `PhysicalQuantity` | a registered name with unit, tensor rank (scalar, vector, symmetric tensor), support kind (volume, surface, points) and time character (static, harmonic at a frequency with the `e^{+jωt}` peak convention, transient) |
| `Support` and `FieldData` | where values live (a structured grid, mesh nodes or cells, a surface, points), its geometry in world meters, and the samples |
| `TransferOperator` | maps `FieldData` between supports: nearest, linear interpolation, L2 projection, conservative remapping (integral preserved), surface-to-volume and back; each declares what it conserves and its error behavior |
| `CouplingParticipant` | adapter around a solver: quantities read and written, `prepare`, `solve` or `advance(dt)`, `state` / `restore` for iterations, energy terms for the ledger, provenance |
| `CouplingGraph` | participants and directed exchanges, validated for units, supports, time characters and cycles |
| `CouplingScheduler` | runs the graph: one-way chains, sequential (staggered) two-way, iterative (fixed point with relaxation, or Aitken / quasi-Newton later) until stated tolerances, time-dependent with sub-cycling when the participants' time steps differ |
| `EnergyLedger` | stored energy, inputs, boundary losses, dissipation and transfers per participant and exchange, residual per step ([accounting](../physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md)) |
| `CoupledResult` | the participants' results, the exchanged fields at their last state, the ledger, convergence history, and the provenance of every solver and operator |

## Coupling modes

| Mode | Use | Scheduler |
|---|---|---|
| One-way | the receiver does not affect the source (plate → sound in air, field → heat when heating does not detune) | run the source, transfer, run the receiver |
| Sequential two-way | slow feedback (thermal drift of a resonator) | alternate solves; one pass per load step |
| Iterative | strong steady-state feedback (heating that shifts a resonance into or out of the drive) | repeat until each exchanged quantity and the ledger residual are within tolerance; report divergence |
| Time-dependent | transient interaction (acoustic radiation force on a moving membrane) | common synchronization times, sub-cycling, predictor–corrector for stability; time-lag errors stated |
| Multi-domain | chains and loops of more than two (electromagnetic → thermal → mechanical → electromagnetic) | the graph, ordered by the scheduler |

## Different resolutions and times

Participants keep their own grids and steps. Transfers state their support
mapping; a harmonic quantity crossing to a time-domain participant carries
its frequency and the phasor convention; averaging over a period
(time-averaged power ½ Re{E·J*}, for example) is an explicit operator, not an
implicit conversion (RES-SFE-012). When a transferred quantity must be
conserved (power, force, heat), the operator is conservative and the ledger
checks it.

## Example: microwave heating that detunes a cavity

```text
 Yee (EM, harmonic or transient) ──absorbed power density q(x)──► thermal
      ▲                                                                │
      │ changed geometry / permittivity(T)                  temperature T(x)
      │                                                                ▼
 geometry update ◄──── displacement u(x) ──── mechanics FE (thermal expansion)
```

Exchanges: `heat_source_density` [W/m³] (time-averaged, conservative
projection from the Yee grid to the thermal mesh), `temperature` [K]
(interpolation to the FE mesh and to the Yee grid for ε(T)),
`displacement` [m] (to a geometry update, then a new Yee setup). Ledger:
electromagnetic input, absorbed power, heat stored and conducted out,
strain energy, residual. The run reports the cavity's frequency shift with
its convergence history, never a converged value without one.

## Interfaces outside the core

- **Services and API:** a coupling setup is a project extension; starting a
  coupled run is a job like any other (ADR-000216); results expose the
  ledger and convergence. Contract additions are new messages in `proto/`
  (additions only within v1).
- **Python:** coupling setups and results in the `resonant` package, the
  same objects as the services.
- **Visualization:** exchanged fields are ordinary fields with units, shown
  by the existing layers and the planned multi-quantity probe.
- **Storage:** exchanged fields and state fields can be large: HDF5 (already
  used by experiments, ADR-000213) next to the project, referenced from it.

## What is reused

`StructuralRadiation` becomes the first `CouplingParticipant` pair
(mechanics harmonic result → acoustic field), with its existing tests as
the regression; the solvers' setups (`FdtdSetup`, the Yee setup, the FE
model builders) become participant `prepare` steps; the jobs, compute
backends and provenance records are used as they are.

## Not part of this architecture

A monolithic multiphysics solver; coupling logic inside a domain; implicit
unit or convention conversions; claims of conservation without a ledger
residual.
