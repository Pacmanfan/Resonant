# Implementation Status

What Resonant can actually do today. Each line is in exactly one of these
states:

* **Tested** — implemented and covered by automated tests that pass.
* **Untested** — implemented, exercised by hand, not covered by an automated test.
* **Partial** — some of it works; what is missing is stated.
* **Missing** — not implemented.
* **Deferred** — deliberately left to a later milestone, with the reason.

Milestones and the requests that deliver them are in
`docs/architecture/roadmap.md`. Last updated for REQ-CHAT-20261126-004.

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

## Acoustic engine — `docs/requirements/acoustics.md`, `docs/acoustics/`

| Capability | State | Notes |
|---|---|---|
| Acoustic emitter object (frequency, amplitude, phase, delay, directivity, active, radius) | Tested | radiates along local +Z |
| Directivity: omnidirectional, cardioid family, baffled circular piston | Tested | the piston pattern is a far-field formula used at all distances |
| Measured transducer response, phase response, impedance, calibration | Missing | an emitter can be marked as a physical transducer and name a profile; nothing is modelled from it |
| Waveform other than a sinusoid | Missing | the field is a single-frequency harmonic solution; the waveform field is a label |
| Arrays: linear, planar, circular, cylindrical, spherical, custom | Tested | spherical is a full-sphere Fibonacci lattice |
| Per-element overrides that survive layout changes | Tested | keyed by element index |
| Selecting one array element in the viewport | Missing | elements are edited in the array editor's table (first 512) |
| Propagation medium: ideal-gas air, nominal water, custom | Tested | temperature dependence for air only; attenuation is one user-entered coefficient |
| Analytical free-field propagation, superposition | Tested | see `docs/acoustics/ACOUSTIC_VALIDATION.md` |
| Reflection, transmission, refraction, diffraction, scattering | Missing | objects do not affect the field; every result states this |
| Pressure, magnitude, phase, RMS, SPL | Tested | |
| Active intensity from particle velocity | Tested | exact for omnidirectional sources, approximate with directivity (flagged) |
| Focusing by phase and by true time delay; tapers | Tested | |
| Target points, weights, suppression | Tested | |
| Line, plane, surface, volume targets | Partial | stored in the data model; no solver uses them, and they are reported as unused |
| Inverse design for multiple point targets (regularized least squares) | Tested | amplitude and phase control at one frequency |
| Phase-only inverse design | Missing | |
| Field regions: planes XY / XZ / YZ, rotated planes, volumes | Tested | |
| Preflight estimate, memory limit, resolution warnings | Tested | |
| Multi-threaded, cancellable field job with progress | Tested | |
| Field maps and volume slices: magnitude, phase, instantaneous, relative intensity | Tested | |
| Animation of instantaneous pressure from the stored field | Tested | |
| Isosurfaces, volume rendering | Missing | |
| Colour scale: normalized or absolute, stated in words | Tested | no colour-bar legend is drawn in the viewport yet |
| Field analysis panel, probes, CSV / JSON export | Tested | |
| Result metadata, results as snapshots, several results per session | Tested | |
| Side-by-side or difference view of two results | Missing | results are compared by switching between them |
| Saving sampled fields in the project | Deferred | the project stores configuration and run records; fields are recomputed (binary storage comes with the numerical-solver milestone) |
| Example projects A to D | Tested | |

## Not part of any completed milestone

| Capability | State | Notes |
|---|---|---|
| Numerical (full-wave) acoustic solvers | Missing | request 005 |
| Mechanical and electromagnetic solvers | Missing | requests 006 and 007 |
| Wave manipulators, reflectors, lenses | Missing | request 004 |
| Geometry import from CAD or mesh files | Missing | no request covers it yet; it is part of V1 (RES-PRODUCT-015) |
| Python scripting | Missing | request 010 |
| Hardware and measurements | Missing | request 008 |
| Optimization | Missing | request 009 |
| Help system, demonstrations | Missing | request 010 |
| Windows and macOS builds | Untested | nothing platform-specific is used on purpose, but only Linux has been built and run |
| Installer / packaging | Missing | |

## Passive wave manipulation — `docs/requirements/wave_manipulators.md`, `docs/manipulators/`

| Capability | State | Notes |
|---|---|---|
| Manipulator object: category, boundary condition, intended focus and source, notes | Tested | undoable, saved in the project |
| Reflector geometry: parabolic, elliptical, spherical, cylindrical trough, flat | Tested | generated from the equations; characteristic points shown |
| Freeform reflectors, extruded 2D ellipse | Missing | |
| Lens geometry (spherical surfaces) | Tested | geometry only in simulations |
| Lens estimate: acoustic refractive index, thick-lens focal length, interface transmission | Tested | paraxial, fluid-like lens; an estimate, not a field result |
| Field through a lens | Missing | needs a full-wave solver (request 005) |
| Waveguide geometry (circular straight / tapered, rectangular) and cut-off estimates | Tested | rigid walls; entry cross-section only for a taper |
| Curved or branching waveguides; field inside a waveguide | Missing | |
| Cavity geometry (box, cylinder, sphere) and resonance estimates; Helmholtz and quarter-wave formulas | Tested | rigid, lossless; the two resonator formulas are functions without a geometry kind |
| Cavity mode shapes, frequency response | Missing | |
| Slit plate | Tested | geometry only; no diffraction field |
| Periodic, layered, metamaterial structures | Missing | |
| Reflector field model (Kirchhoff approximation), four boundary behaviours | Tested | one reflection; no mutual shadowing; not full-wave; validated against closed-form limits only |
| Particle velocity and intensity with the reflector model | Missing | shown as not calculated |
| Surface resolution setting, preflight with surface elements, convergence | Tested | |
| Ray paths for the selected reflector | Tested | geometric aid only |
| Focus metrics: peak, location, widths, displacement, sidelobe ratio, focal gain | Tested | energy-based efficiency is not reported |
| Result comparison table | Tested | results of the current session |
| Hybrid focusing (phase) and inverse design with the reflector model | Tested | time-delay focusing compensates direct paths only, and says so |
| Geometry optimization interfaces; one-parameter reflector optimization in the editor | Tested | local search; general framework is request 009 |
| Manufacturing constraint checks | Tested | wall thickness, largest dimension, curvature radius, feature size; in the core, used by the optimizer; no editor for them |
| Example projects A to E in `examples/manipulators` | Tested | |
