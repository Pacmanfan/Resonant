# Resonant — Architecture Overview

**Status:** current. Decisions: ADR-000204 (stack, build systems),
ADR-000205 (layers, domains). Requirements: `docs/requirements/product.md`.

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
src/core     data model and physics — no Qt, no OpenGL
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
| later physics domains | one module per domain, each with its own equations, property sets and solvers |

## Rendering and application (`src/render`, `src/app`)

| Part | Responsibility |
|---|---|
| `render/Camera` | orbit camera mathematics and pick rays; no OpenGL calls |
| `render/ShaderLibrary`, `render/GpuMesh` | shader programs with reported build errors; meshes and line buffers on the GPU |
| `render/RenderLayer`, `render/Layers` | a frame is a sequence of layers: grid and axes, lit meshes, marker lines; new visualizations are new layers |
| `render/FieldLayer` | draws a complex field on a grid (plane or volume slices) from a texture; quantity, scale and animation phase are uniforms |
| `render/Renderer` | lifecycle (initialize, resize, render, shutdown), requires OpenGL 4.5 Core |
| `app/DocumentController` | the project, the command stack, the selection and the jobs; the only path from the user interface to the core |
| `app/SceneRenderBridge` | scene objects to draw items; cached display meshes; picking |
| `app/ViewportWidget` | the OpenGL widget: navigation and selection input |
| `app/SceneTreePanel`, `app/PropertiesPanel`, `app/AnalysisPanel`, `app/MaterialsDialog` | the panels and the material editor |
| `app/AcousticController`, `app/AcousticsPanel`, `app/AcousticEditors`, `app/FieldAnalysisPanel`, `app/AcousticMarkers` | the acoustic user interface: creating objects, focusing, inverse design, running and displaying fields, analysis |
| `app/DisplayUnits` | the single boundary between displayed units and SI |
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
* Harmonic time convention and other physics conventions are owned by each
  domain's document.

## Build

CMake (`CMakeLists.txt`) and qmake (`project.pro`) build the same targets;
see ADR-000204 and `BUILDING.md`.
