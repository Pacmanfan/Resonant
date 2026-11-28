# Resonant — Architecture Overview

**Status:** current. Decisions: ADR-000204 (stack, build systems),
ADR-000205 (layers, domains), ADR-000207 (acoustics), ADR-000208 (manipulators), ADR-000209 (full-wave solver), ADR-000210 (mechanics), ADR-000211 (electromagnetics), ADR-000212 (compute backends), ADR-000213 (experiments), ADR-000214 (optimization), ADR-000219 (Shape–Function–Energy direction), ADR-000220 (Coupled Energy–Matter Engine). Requirements: `docs/requirements/product.md`.

This document describes the architecture every milestone builds on. Each
subsystem gets its own document in this directory when it is implemented;
`README.md` here is the index.

## Layers

```text
src/main     the executable: main.cpp only
src/app      Qt Widgets application as a library — main window, dock panels
             (.ui forms in src/app/forms), controllers that turn user
             actions into core commands
src/render   OpenGL 4.5 Core rendering — shaders, meshes on the GPU, camera,
             grid, field display; knows core data, not the application
src/automation_api  the automation API's protobuf adapter (dispatcher, local
             endpoint); the only user of generated protobuf types; no Qt
src/automation_host the headless automation host resonant-automation
src/core     data model and physics — no Qt, no OpenGL; src/core/services is
             the application service layer above the domains
third_party  vendored GoogleTest and nlohmann-json
tests        GoogleTest suites: tests/core (no display needed),
             tests/app (run on an offscreen platform)
examples     reproducible example projects
```

A higher layer may use a lower one; never the reverse.

## Core modules (`src/core`)

| Module | Responsibility |
|---|---|
| `foundation` | stable ids, SI units and conversions, structured logging, result/error types |
| `scene` | scene graph: objects with id, name, type, local and world transform, visibility, parent/children, geometry and material references |
| `geometry` | parametric engineering geometry (primitives, later imported and generated shapes) and its tessellation into display meshes |
| `materials` | materials with per-domain property sets, provenance and validity ranges; the material library |
| `commands` | undoable commands and the undo/redo stack |
| `project` | the project document, JSON persistence, validation, schema versions and migration |
| `simulation` | simulation configuration, solver/domain identifiers, jobs (states, progress, cancellation), result metadata |
| `acoustics` | the acoustic domain: emitters, arrays, targets, probes, regions; medium; propagation models; field evaluation; focusing; inverse design; field jobs and results (`docs/acoustics/`) |
| `manipulators` | passive wave manipulators: design component, reflector / lens / waveguide / cavity geometry, analytical estimates, the reflector field model, rays, focus metrics, geometry optimization (`docs/manipulators/`) |
| `fdtd` | the full-wave acoustic solver: staggered-grid FDTD core, signals, spectra, simulation geometry, setup from a project, results (`docs/advanced_acoustics/`) |
| `solvers` | declarations of what each solver represents, and the assessment of a problem against them |
| `dsp` | signals over time and spectra, shared by every domain |
| `compute` | where numerical work runs: a deterministic thread pool, compute options and capability detection, the CUDA runtime queries, VOLK vector kernels; the solvers' CUDA kernels live beside them (`*Device.cu`) behind plain-data headers (`docs/compute/`) |
| `mechanics` | the mechanical domain: elastic materials, structural idealization, finite elements, eigensolver, static / modal / harmonic / transient analyses, actuators, sensors, targets, targeting, horns, periodic rods, measured data (`docs/mechanics/`) |
| `electromagnetics` | the electromagnetic domain: materials, sources, antennas (analytical models and estimates), impedance and matching, patterns, near fields, arrays (array factor, steering, grating lobes, tapers, synthesis), lines, guides and cavities, the Yee FDTD solver and its setup from a project, Touchstone data (`docs/electromagnetics/`) |
| `geometry/SolidModel` | simulation bodies (an inside test per geometry kind) shared by the acoustic and electromagnetic full-wave setups |
| `coupling` | the only code that uses two domains: sound radiated by vibrating plates; the registry of the optimization adapters |
| `experiments` | the domain-independent experiment framework: experiments, the hardware abstraction and simulated instruments, acquisition, calibration, measurement records and their HDF5 store, processing, spatial data, comparison, uncertainty, execution, output safety, reports, archives; the domains add the physical interpretation (`AcousticExperiments`, `MechanicalExperiments`, `RfExperiments`) (`docs/experiments/`) |
| `optimization` | the domain-independent optimization framework: problems, design variables and derived variables, objectives, constraints, the adapter interface, algorithms (sweeps, sampling, Nelder-Mead, L-BFGS-B, differential evolution, NSGA-II, Bayesian), runs, candidates, Pareto ranking, caching, checkpoints, jobs, sensitivity, surrogates, robust and multi-fidelity evaluation, linear inverse design, persistence; the domains provide adapters (`acoustic_optimization`, `mechanics/MechanicalOptimization` with `TopologyOptimization`, `electromagnetics/EmOptimization`), registered in `coupling/OptimizationRegistry` (`docs/optimization/`) |
| `geometry/MeshChecks` | mesh integrity, printability checks, STL and OBJ export |
| `services` | the application services used by the GUI, Python, the protobuf adapter and MCP: one implementation per operation, the session (revisions, events, sessions and permissions, audit, idempotency, jobs and results), batches and transactions (`docs/architecture/AUTOMATION_ARCHITECTURE.md`); no domain includes it |
| later physics domains | one module per domain, each with its own equations, property sets and solvers |

## Rendering and application (`src/render`, `src/app`)

| Part | Responsibility |
|---|---|
| `render/Camera` | orbit camera mathematics and pick rays; no OpenGL calls |
| `render/ShaderLibrary`, `render/GpuMesh` | shader programs with reported build errors; meshes and line buffers on the GPU |
| `render/RenderLayer`, `render/Layers` | a frame is a sequence of layers: grid and axes, lit meshes, marker lines; new visualizations are new layers |
| `render/FieldLayer` | draws a complex field on a grid (plane or volume slices) from a texture; quantity (magnitude, phase, instantaneous, relative intensity, dB), scale and animation phase are uniforms |
| `render/Renderer` | lifecycle (initialize, resize, render, shutdown), requires OpenGL 4.5 Core |
| `app/DocumentController` | the project, the command stack, the selection and the jobs; the only path from the user interface to the core |
| `app/SceneRenderBridge` | scene objects to draw items; cached display meshes; picking |
| `app/ViewportWidget` | the OpenGL widget: navigation and selection input |
| `app/SceneTreePanel`, `app/PropertiesPanel`, `app/AnalysisPanel`, `app/MaterialsDialog` | the panels and the material editor |
| `app/AcousticController`, `app/AcousticsPanel`, `app/AcousticEditors`, `app/FieldAnalysisPanel`, `app/AcousticMarkers` | the acoustic user interface: creating objects, focusing, inverse design, running and displaying fields, analysis |
| `app/ManipulatorController`, `app/ManipulatorEditor` | adding manipulators, their property editor, focal markers and ray paths, reflector optimization |
| `app/FdtdController`, `app/FdtdPanel`, `app/TimeSignalPanel`, `app/PlotWidget` | the full-wave solver's settings, preflight, runs, recorded signals and spectra |
| `app/MechanicsController`, `app/MechanicsPanel`, `app/MechanicalEditor`, `app/MechanicsMarkers`, `render/ScalarMeshLayer` | mechanical objects, analyses, results, deformed display |
| `app/EmController`, `app/EmPanel`, `app/EmEditor`, `app/EmMarkers` | electromagnetic objects, reports, sweeps, patterns (3D through `ScalarMeshLayer`), arrays, fields, full-wave runs, measured data |
| `app/DisplayUnits` | the single boundary between displayed units and SI |
| `app/OptimizationController`, `app/OptimizationPanel` | the Optimization Studio: problems, background runs, candidates, re-evaluation, inverse design, sensitivity, promotion as an undoable revision, export, topology |
| `app/MainWindow` | wires the actions of `forms/MainWindow.ui` to the controller |

## Data flow

```text
user action -> controller (app) -> command (core) -> scene / materials change
                                                        |
                         change notification -----------+
                              |                         |
                        panels refresh           renderer rebuilds what changed

simulation configuration + scene snapshot -> job (worker thread) -> result
result (data + metadata) -> analysis panels, field renderer
```

A simulation works on a snapshot of its inputs, and a result keeps the
configuration that produced it, so editing the scene afterwards never
changes a historical result.

## Conventions

* SI units internally; display units are converted in the user interface.
* World axes: X horizontal, Y depth, Z vertical; right-handed.
* Angles in radians internally; rotation as intrinsic Z-Y-X Euler angles.
* Ids are stable, unique and persisted; names are for people.
* Harmonic time dependence `e^{+jωt}` with peak amplitudes in every domain
  and every coupling (RES-SFE-012); other physics conventions are owned by
  each domain's document.

## Direction: Shape–Function–Energy

An object's function is computed from its geometry, materials, excitation
and environment by a physical model, never attached to its type
(RES-SFE-001, `docs/vision/SHAPE_FUNCTION_ENERGY.md`). Interactions between
domains go through the Coupled Energy–Matter Engine, which coordinates the
solvers with explicit exchanged quantities, transfer operators and energy
accounting, without replacing them (ADR-000219,
`COUPLED_MULTIPHYSICS_ARCHITECTURE.md`). Its domain-independent core is
`src/core/multiphysics` (it includes no domain); participants and coupling
models live in `src/core/coupling`; the thermal domain is `src/core/thermal`
(ADR-000220, `COUPLED_ENGINE_ARCHITECTURE.md`; implemented for layered
systems). The audit of the
current architecture against this direction is
`SHAPE_FUNCTION_ENERGY_AUDIT.md`; the plan is `ENERGY_MATTER_RESEARCH_ROADMAP.md`.

## Build

CMake (`CMakeLists.txt`) and qmake (`project.pro`) build the same targets;
see ADR-000204 and `BUILDING.md`.
