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
| [ADR-000208](docs/decisions/ADR-000208-wave-manipulators-and-reflector-model.md) | Manipulators as scene objects with design data, parametric geometry and a separate simulation representation; support levels; a Kirchhoff reflector model as a selectable propagation model; the field model as a recorded project setting; rays as a geometric aid; minimal optimization interfaces; constraints as checks | M3 |
| [ADR-000209](docs/decisions/ADR-000209-full-wave-fdtd-solver.md) | Staggered-grid FDTD as the first full-wave solver, validated as a scene-independent core; split-field PML; calibrated soft sources; 2D as a stated different problem; simulation geometry from definitions; solids as fluids; steady state as an ordinary field result; a solver registry that assesses and never selects; refuse rather than degrade | M4 |
| [ADR-000210](docs/decisions/ADR-000210-mechanical-domain.md) | Mechanical domain with an own finite-element core; explicit idealizations; frame, ACM plate, membrane and hexahedral elements; Lanczos eigensolver with residuals; explicit damping models; Newmark transients; shared dsp module; separate coupling module | M5 |
| [ADR-000211](docs/decisions/ADR-000211-electromagnetic-domain.md) | Electromagnetic domain: analytical thin-wire antennas and induced-EMF impedance; arrays as an array-factor model without coupling; an own Yee FDTD solver (split-field PML, PEC, thin wires, 2D TM); setup separate from numerics with refusal instead of coarsening; simulation bodies shared in the geometry module; guide objects; one-port Touchstone; a dB field view | M6 |
| [ADR-000212](docs/decisions/ADR-000212-compute-backends.md) | Compute backends: a domain-independent compute module; deterministic CPU threads (bit-identical, ordered reductions) with chunked scheduling; a stepper interface per full-wave solver; CUDA as an optional, statically linked backend with the reference arithmetic (no FMA) and refusal instead of fall-back; VOLK only for single-precision data | — |

## Deviations from requests

Recorded in the decision that caused them:

* qmake is supported in addition to CMake (ADR-000204), on the operator's instruction.
* GoogleTest is vendored rather than fetched or installed (ADR-000204).
* The deviations of the V0.1 foundation from its request are listed at the end of ADR-000206.
* The deviations of the acoustic engine from its request are listed at the end of ADR-000207.
* The deviations of passive wave manipulation from its request are listed in ADR-000208.
* The deviations of the full-wave solver milestone from its request are listed in ADR-000209.
* The deviations of the mechanical milestone from its request are listed in ADR-000210.
* The deviations of the electromagnetic milestone from its request are listed in ADR-000211 and `IMPLEMENTATION_STATUS.md`.
* The deviations of the compute-backend work from its request are listed in ADR-000212.
