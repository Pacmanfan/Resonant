# Implementation Status

What Resonant can actually do today. Each line is in exactly one of these
states:

* **Tested** — implemented and covered by automated tests that pass.
* **Untested** — implemented, exercised by hand, not covered by an automated test.
* **Partial** — some of it works; what is missing is stated.
* **Missing** — not implemented.
* **Deferred** — deliberately left to a later milestone, with the reason.

Milestones and the requests that deliver them are in
`docs/architecture/roadmap.md`. Last updated for REQ-CHAT-20261126-002.

## Application foundation (V0.1) — `docs/requirements/application_foundation.md`

| Capability | State | Notes |
|---|---|---|
| Build with CMake (Debug, Release) | Tested | suite `cmake` builds the tree and runs CTest |
| Build with qmake | Tested | the development-control system's build |
| Core library independent of Qt and OpenGL | Tested | suite `layering` |
| Main window, menus, toolbars, dockable panels | Tested | every action is connected |
| Workspace layout, view options and display unit persist | Tested | |
| 3D viewport, OpenGL 4.5 Core | Tested | offscreen pixel tests and the real window on Mesa llvmpipe (software renderer); not yet run on a hardware GPU driver |
| Camera orbit, pan, zoom, frame, standard views | Tested | |
| Ground grid and axes | Tested | the grid spacing follows the camera distance |
| Lit geometry, selection highlight | Tested | one headlight plus ambient; no shadows, no material colours |
| Selection by clicking in the viewport | Tested | CPU ray cast against display meshes |
| Message instead of a silent fallback when OpenGL 4.5 is unavailable | Untested | the code path exists; no machine without OpenGL 4.5 was available to run it |
| Scene graph: ids, names, types, transforms, visibility, hierarchy | Tested | |
| Primitives: box, sphere, cylinder, cone, plane, disc, ellipsoid | Tested | parameters validated; display meshes checked against areas and bounds |
| Create, select, rename, hide/show, duplicate, delete | Tested | |
| Move, rotate, scale | Tested | numerically in the Properties panel only |
| Move, rotate, scale by dragging handles in the viewport | Missing | |
| Groups; moving an object to the top level | Tested | |
| Moving an object into a group from the user interface | Missing | the command exists and is tested; there is no menu entry or drag-and-drop for it yet |
| Undo / redo of every scene and material edit | Tested | |
| SI units internally; display length unit m / cm / mm / in; angles in degrees | Tested | |
| Material model: per-domain property sets, provenance, validity range, frequency tables | Tested | |
| Example material library (ten materials) | Tested | every value is illustrative, not authoritative |
| Material browser / editor, assignment to objects | Tested | frequency tables can be stored and loaded but not edited in the dialog |
| Editing temperature validity ranges and frequency tables in the dialog | Missing | |
| Project save / load (JSON), Save As, New | Tested | atomic save |
| Validation of project files, clear errors, current project kept on failure | Tested | |
| Unsaved-changes detection and prompt | Tested | |
| Schema version check, migration chain, refusal of newer files | Tested | no migration step exists yet because format 1 is the first |
| External / binary assets beside the project file | Deferred | no large data exists before the simulation milestones; the format reserves `extensions` and a later version adds referenced binary files |
| Simulation configuration and result-metadata records | Tested | data structures only; no solver uses them yet |
| Background jobs: states, progress, cancellation | Tested | |
| Demonstrator job | Tested | performs no calculation; it only exercises the job system |
| Structured logging, Console panel, Diagnostics panel | Tested | |
| Example project `examples/foundation/primitives.resonant` | Tested | loaded by a test |

## Not part of any completed milestone

| Capability | State | Notes |
|---|---|---|
| Any physics solver (acoustic, mechanical, electromagnetic) | Missing | requests 003 to 007 |
| Emitters, arrays, targets, probes, simulation regions | Missing | request 003 |
| Field visualization | Missing | request 003 |
| Wave manipulators, reflectors, lenses | Missing | request 004 |
| Geometry import from CAD or mesh files | Missing | no request covers it yet; it is part of V1 (RES-PRODUCT-015) |
| Python scripting | Missing | request 010 |
| Hardware and measurements | Missing | request 008 |
| Optimization | Missing | request 009 |
| Help system, demonstrations | Missing | request 010 |
| Windows and macOS builds | Untested | nothing platform-specific is used on purpose, but only Linux has been built and run |
| Installer / packaging | Missing | |
