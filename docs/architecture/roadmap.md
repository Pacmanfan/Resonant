# Resonant — Roadmap

**Status:** current. Product requirements: `docs/requirements/product.md`.
The implementation status of each milestone is kept in
`IMPLEMENTATION_STATUS.md` at the repository root once code exists.

| Milestone | Request | Delivers | Product requirements |
|---|---|---|---|
| M0 Vision and architecture | REQ-CHAT-20261126-001 | product requirements, architecture, decisions, this roadmap | RES-PRODUCT-001..016 (recorded) |
| M1 Application foundation (V0.1) | REQ-CHAT-20261126-002 | application shell, 3D viewport, scene graph, primitives, materials, project save/load, undo/redo, job infrastructure, logging | RES-PRODUCT-002, -007, -012, -013 |
| M2 Acoustic physics engine | REQ-CHAT-20261126-003 | acoustic emitters and arrays, analytical propagation, focusing, multi-target inverse design, field regions, field visualization, probes | RES-PRODUCT-003, -004, -008, -010, -012 |
| M3 Passive wave manipulation | REQ-CHAT-20261126-004 | wave manipulators (reflectors, lenses, waveguides, cavities), Kirchhoff reflector field model, ray paths, hybrid focusing, geometry optimization | RES-PRODUCT-005, -006, -010 |
| M4 Full-wave numerical solvers | REQ-CHAT-20261126-005 | solver framework, validated FDTD, boundaries, material interfaces, transient sources, frequency analysis | RES-PRODUCT-001, -006, -008 |
| M5 Mechanical waves | REQ-CHAT-20261126-006 | mechanical domain: structural models, modal analysis, forced and transient response | RES-PRODUCT-001 |
| M6 Electromagnetic waves | REQ-CHAT-20261126-007 | electromagnetic domain: antennas, arrays, array factor, RF analysis, a full-wave method | RES-PRODUCT-001, -003 |
| M7 Experiments and hardware | REQ-CHAT-20261126-008 | experiment framework, simulated devices, acquisition, calibration, correlation with simulation | RES-PRODUCT-009 |
| M8 Optimization | REQ-CHAT-20261126-009 | optimization framework, inverse design, geometry optimization across domains | RES-PRODUCT-004, -010 |
| M9 Python, help, demonstrations | REQ-CHAT-20261126-010 | Python API and scripting, automation, plugins, HTML help, demonstration system | RES-PRODUCT-015 |
| M10 Automation API | REQ-CHAT-20261126-012 | application services, protobuf contract, local endpoint and gRPC bridge, events, jobs, transactions, Python client, MCP adapter | RES-PRODUCT-015 |
| M11 Linux ZIP distribution | REQ-CHAT-20261126-013 | a self-contained, relocatable x86_64 Linux ZIP with its own Qt and Python, launcher, diagnostics, offline help, licenses, and validation in clean containers | RES-PRODUCT-015 |
| M12 Shape–Function–Energy architecture | REQ-CHAT-20261126-015 | the direction, its requirements (RES-SFE-001..012), the coupled multiphysics architecture, the audit, and the twelve-stage research roadmap (`ENERGY_MATTER_RESEARCH_ROADMAP.md`) whose stages the queued requests REQ-CHAT-20261126-016..023 and REQ-CHAT-20261127-001 address | RES-PRODUCT-006, -013, -016 |
| M13 Coupled Energy–Matter Engine | REQ-CHAT-20261126-016 | the engine core (quantities, field data, transfers, ledger, material and geometry state, physical systems, orchestration), a thermal domain, layered-media electromagnetics, thermal loads, validated EM → thermal → mechanical → geometry coupling, services, contract, Python and MCP (`COUPLED_ENGINE_ROADMAP.md`) | RES-PRODUCT-001, -006, -016 |
| M14 Functional structures | REQ-CHAT-20261126-017 | geometry-driven engineering: structures over scene objects, named parameters, content-keyed staleness, cavity (closed form and Yee), optical resonator, acoustic horn, mechanical resonator, photonic cell and coupled-slab analyses, sweeps, sensitivities, explained optimization, services, contract, Python, MCP and a panel (`FUNCTIONAL_STRUCTURE_ARCHITECTURE.md`) | RES-PRODUCT-004, -005, -010, -016 |
| M15 Resonance engineering | REQ-CHAT-20261126-018 | one resonance description (nature, Q by mechanism), spectroscopy (matrix pencil, Lorentzian fits), lumped oscillator networks with an energy ledger, coupled resonators (splitting, exchange, avoided crossings), Helmholtz resonators, cantilevers as resonators, Duffing and Mathieu models, a resonant cavity in the coupled engine, ResonanceService (`docs/physics/RESONANCE_ARCHITECTURE.md`) | RES-PRODUCT-001, -006, -016 |
| Later | — | geometry import from CAD formats, GPU backends | RES-PRODUCT-002, -014 |

V1 (RES-PRODUCT-015) is reached when M1 and M2 are complete and the two
remaining V1 items have been delivered: the initial Python scripting
interface (part of M9) and geometry import, which no submitted request
covers yet.
