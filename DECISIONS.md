# Architectural Decisions

Index of the decisions that shape Resonant. The decision records themselves
are in `docs/decisions/` (one file per decision, manifest
`docs/decisions/decisions.txt`); this file is a reading guide, not a second
source of truth. Records numbered below 000204 concern the
development-control system, not the application.

| Record | Decision | Milestone |
|---|---|---|
| [ADR-000204](docs/decisions/ADR-000204-technology-stack-and-dual-build-system.md) | C++20, Qt 6 Widgets, OpenGL 4.5 Core, Eigen, nlohmann-json; CMake and qmake build the same sources; GoogleTest vendored; user interfaces are `.ui` forms | M0 |
| [ADR-000205](docs/decisions/ADR-000205-layered-architecture-and-physics-domains.md) | Layers core / render / app with a Qt-free core; physics domains as separate modules; engineering geometry distinct from meshes; SI units, Z-up, radians, Z-Y-X Euler angles; results carry their model and assumptions; jobs off the UI thread; CPU reference before GPU | M0 |
| [ADR-000206](docs/decisions/ADR-000206-application-foundation-design.md) | One editable state per object and immutable domain components; geometry kinds as registry data; commands on one stack; a document controller; draw items instead of scene objects in the renderer; project format 1; the application as a library plus an executable; globbed sources; moc kept away from standard headers; display units in the application | M1 |
| [ADR-000207](docs/decisions/ADR-000207-acoustic-engine-design.md) | Acoustic conventions (`e^{+jωt}`, peak amplitudes), source strength as pressure at 1 m, explicit medium, one propagation interface, near-source clamping, intensity from particle velocity, arrays as one object with overrides, phase and delay focusing as distinct settings, SVD inverse design, results as snapshots, display from stored data | M2 |

## Deviations from requests

Recorded in the decision that caused them:

* qmake is supported in addition to CMake (ADR-000204), on the operator's instruction.
* GoogleTest is vendored rather than fetched or installed (ADR-000204).
* The deviations of the V0.1 foundation from its request are listed at the end of ADR-000206.
* The deviations of the acoustic engine from its request are listed at the end of ADR-000207.
