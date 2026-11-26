# Resonant — Roadmap

**Status:** current. Product requirements: `docs/requirements/product.md`.
The implementation status of each milestone is kept in
`IMPLEMENTATION_STATUS.md` at the repository root once code exists.

| Milestone | Request | Delivers | Product requirements |
|---|---|---|---|
| M0 Vision and architecture | REQ-CHAT-20261126-001 | product requirements, architecture, decisions, this roadmap | RES-PRODUCT-001..016 (recorded) |
| M1 Application foundation (V0.1) | REQ-CHAT-20261126-002 | application shell, 3D viewport, scene graph, primitives, materials, project save/load, undo/redo, job infrastructure, logging | RES-PRODUCT-002, -007, -012, -013 |
| M2 Acoustic physics engine | REQ-CHAT-20261126-003 | acoustic emitters and arrays, analytical propagation, focusing, multi-target inverse design, field regions, field visualization, probes | RES-PRODUCT-003, -004, -008, -010, -012 |
| M3 Passive wave manipulation | REQ-CHAT-20261126-004 | wave manipulators (reflectors, lenses, waveguides, cavities), ray-based reflector model, hybrid focusing, geometry optimization | RES-PRODUCT-005, -006, -010 |
| M4 Full-wave numerical solvers | REQ-CHAT-20261126-005 | solver framework, validated FDTD, boundaries, material interfaces, transient sources, frequency analysis | RES-PRODUCT-001, -006, -008 |
| Later | — | geometry import from CAD formats, Python scripting interface, mechanical and electromagnetic domains, hardware and measurements, GPU backends | RES-PRODUCT-001, -002, -009, -015 |

V1 (RES-PRODUCT-015) is reached when M1 and M2 are complete and the two
remaining V1 items — geometry import and the initial Python scripting
interface — have been delivered by requests of their own. Neither is part of
a submitted request yet.
