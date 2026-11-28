# Accounting Architecture

**Status:** current (REQ-CHAT-20261126-022, RES-ACC-001..016, ADR-000226). The audit
that motivated it is [ACCOUNTING_AUDIT.md](ACCOUNTING_AUDIT.md). The physics of each
pathway is in [ENERGY_AND_MOMENTUM_ACCOUNTING.md](../physics/ENERGY_AND_MOMENTUM_ACCOUNTING.md)
and [FORCE_AND_MOMENTUM.md](../physics/FORCE_AND_MOMENTUM.md), the monitoring in
[CONSERVATION_MONITORING.md](../physics/CONSERVATION_MONITORING.md), and the
benchmarks in [ACCOUNTING_VALIDATION.md](../physics/ACCOUNTING_VALIDATION.md).

The framework coordinates and verifies; it never computes a solver's physics again.

## Layers

| Layer | Module | Holds |
|---|---|---|
| foundation | `foundation/Dimensions` | SI dimension exponents of unit expressions, factors, checked conversions |
| infrastructure (no domains) | `accounting/Quantities` | the families (energy, power, force, torque, linear and angular momentum, entropy, heat capacity, pressure, momentum flux …) with dimensions and a conserved flag; typed quantities with context |
| | `accounting/AccountingLedger` | entities, holdings, exchanges, generations, force records, missing contributions, numerical errors, scale contracts, consistency checks; JSON |
| | `accounting/ControlVolume` | accounting systems (members), the classification of an exchange (internal, input, output, dissipated) |
| | `accounting/Conservation` | the registry of conservation laws and consistency relations; `evaluate(ledger, system)` → report |
| domains | `electromagnetics/RadiationForce`, `oscillators` element flows, `thermal/PhaseChange` entropy, `electromagnetics/Hysteresis` hysteron loss | the new physical outputs, each in its domain |
| coupling | `coupling/AccountingAdapters`, `coupling/AccountingStudies` | ledgers from the solvers' outputs; studies, reclassification, comparison, queries, export, records |
| services | `AccountingService` | studies and records in the project extension `accounting`, runs as jobs, the coupled engine's results carrying their accounting |
| clients | `proto/resonant/v1/accounting`, Python, MCP, the Accounting panel | 17 RPCs; the panel's flows, checks, breakdown, forces and histories |

`accounting` is domain-independent: `tools/check_layering.py` forbids its
including any domain or coupling.

## The ledger and the boundary

```text
ledger (what the calculation reported)                     system (what is being accounted)
 entities   body:r1  element:coupling  body:r2  heat(sink)   members {body:r1, element:k1}
 exchange   work:r1->coupling   body:r1 -> element:coupling   ───►  output (from a member to a non-member)
 the same exchange for {body:r1, element:k1, element:coupling, body:r2, element:k2}  ───►  internal
```

An exchange's role is decided by membership, every time a report is made.
No input or internal flag is ever stored. Sources, sinks and the
environment are never members:

- **Sources** give, for example the incident wave or the applied force.
- **Sinks** receive dissipation as heat.
- **The environment** receives outputs, for example the reflected wave or
  the ground.

The basis is either an interval or a rate:

- **Interval:** amounts in J and kg·m/s over [t0, t1], and holdings are
  states at the two ends.
- **Rate** (steady states): W and kg·m/s². Holdings are storage rates, and a
  steady state's stored energy is an informational quantity.

A material point's ledger is per m³ (`per`).

## Persistence and reproducibility

The project extension `accounting` holds:

- **Studies:** the source, the systems and the tolerances.
- **Records:** the study, the ledger with its histories thinned to 200
  samples, the systems, the tolerances, the status, the convergence rows and
  the result they came from.

A record's reports are evaluated again from its ledger, so nothing derived
is stored twice. A coupled run's result (MultiphysicsService) carries its
own ledger and whole-system report.

## Interfaces

- **Service and RPCs:** `AccountingService` exposes the families and unit
  conversion, the conservation models and source kinds, studies (set and
  delete), runs, results, reports for any system, comparisons, queries,
  exports (JSON, CSV, Markdown) and records.
- **Queries:** a query returns one facet's quantities with their full
  context: id, family, role, value or vector, unit, domains, entities,
  interval, method, provenance and uncertainty. The facets are energy,
  momentum, angular momentum, entropy, forces, torques, transfers, missing
  and residuals.
- **MCP:** 17 tools. Reads are read-level, studies and records are
  modify-level, and runs are execute-level.
- **The Accounting panel** (**Panels > Accounting**) shows a study and its
  system, either the study's own or members you tick. It has five tabs:
  - **Flows:** the bands are as wide as their amounts, with the value and
    unit on each.
  - **Conservation:** each check's status, residual, relative residual,
    tolerance and why.
  - **Breakdown:** the categories, with the exchanges and holdings under them.
  - **Forces:** each force's origin, target, vector, method and pair.
  - **Over time:** the balances' histories and the residuals' histories.

  You can also save a record and export the accounting.
