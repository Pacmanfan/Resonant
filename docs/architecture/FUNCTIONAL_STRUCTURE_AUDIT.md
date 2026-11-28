# Functional Structure Audit

The audit before functional structures (REQ-CHAT-20261126-017,
`docs/requirements/functional_structures.md`, ADR-000221), from the source at
commit `2073ee2` (after the Coupled Energy–Matter Engine, ADR-000220).

| Area | Found | Reused as | Missing (added here) |
|---|---|---|---|
| Geometry representation, CAD import | parametric geometry kinds (`geometry/Geometry`, primitives, manipulators, mechanics, antennas) with validated parameters; tessellation for the viewport; no CAD kernel, no OpenCASCADE, no import (an open V1 item, RES-PRODUCT-015) | the engineering geometry | CAD import stays open; a `horn.acoustic` kind |
| Geometry editing and parameterization | `GeometryService.updateParameters` (undoable, revisions), the optimization framework's scene bindings (`object/<id>/geometry/<key>`, `…/position/<axis>`), derived variables by `optimization/Expression` | parameter bindings and expressions | named parameters spanning objects, free and derived parameters of a structure |
| Geometry-to-mesh | each solver builds its own discretization (FDTD and Yee grids from `geometry/SolidModel`, finite-element meshes from `mechanics/Meshing`) | simulation geometry | a cache keyed by content (`SimulationGeometryCache`) |
| Material regions, boundaries | one material per object; component settings (supports, guide settings) | material and boundary references by role | roles of regions and boundaries in a structure |
| Solvers | acoustic analytical, Kirchhoff, FDTD; mechanical FE (static, modal, harmonic, transient); EM analytical antennas, lines, guides, cavity modes (lossless), Yee; layered media; heat conduction; the coupled engine | the analyses' models | wall-loss Q, port response, an optical resonator, the Webster horn and piston radiation impedance, 1D photonic cells |
| Resonance and modal analysis | cavity modes (closed form), FE modal analysis with effective masses, Yee cavity resonances (a test), periodic-rod Bloch analysis, acoustic cavity estimates | the cavity, mechanical and rod analyses | Q and bandwidth, mode tracking across sweeps |
| Field visualization | scalar layers, probes, plots (`app/PlotWidget`) | the panel's curves | — |
| Energy and momentum accounting | single-domain energy records; the coupled ledger | coupled analyses report the ledger's balance | momentum and force (REQ-CHAT-20261126-022) |
| CEME | physical systems (layered slabs) in the project, regions optionally linked to objects | the coupled-slab analysis | thickness of linked regions from the objects |
| Optimization | adapters, algorithms (sweep, LHS, Nelder–Mead, L-BFGS, DE, NSGA-II, Bayesian), sensitivities, Pareto, run records | candidates for structures (adapter `functional`) | the adapter, objectives and constraints of structures |
| Serialization | project extensions | extension `functional` | — |
| GUI | dock panels with controllers over the services | a new dock panel | the panel |
| Automation | services, contract (16 services), Python, MCP | the same procedure | `FunctionalStructureService` |

## Revision tracking found

The session counts revisions of the project and of each object
(`Session::objectRevision`), but they are not stored with results and do not
survive save and load. Results therefore record content keys instead
(ADR-000221, decision 3).

## Plan followed

1. A domain-independent module `src/core/functional` (structure, parameters, fingerprints, studies) with the analysis registry interface.
2. New physics in the domains: `electromagnetics/CavityResonator`, `OpticalResonator`, `PeriodicLayers`, `manipulators/Horn` and the `horn.acoustic` kind.
3. Analyses adapting the domains, and the optimization adapter, in `src/core/coupling`.
4. Services, contract, Python, MCP; a panel; six example projects validated in tests.
