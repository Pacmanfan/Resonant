# Implementation Status

What Resonant can actually do today. Each line is in exactly one of these
states:

* **Tested** — implemented and covered by automated tests that pass.
* **Untested** — implemented, exercised by hand, not covered by an automated test.
* **Partial** — some of it works; what is missing is stated.
* **Missing** — not implemented.
* **Deferred** — deliberately left to a later milestone, with the reason.

Milestones and the requests that deliver them are in
`docs/architecture/roadmap.md`. Last updated for REQ-CHAT-20261126-009.

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
| Example material library (eighteen materials) | Tested | every value is illustrative, not authoritative |
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
| Geometry import from CAD or mesh files | Missing | no request covers it yet; it is part of V1 (RES-PRODUCT-015) |
| Windows and macOS builds | Untested | nothing platform-specific is used on purpose, but only Linux has been built and run |
| Binary installer / distribution package | Missing | `make install` / `cmake --install` install the application with its help, demonstrations, examples, plugins and Python package (REQ-CHAT-20261126-010) |

(Full-wave acoustics, wave manipulators, hardware and measurements, optimization,
Python scripting, the help system and demonstrations were delivered by the
milestones below.)

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

## Full-wave acoustic simulation — `docs/requirements/full_wave_acoustics.md`, `docs/advanced_acoustics/`

| Capability | State | Notes |
|---|---|---|
| Solver registry: declared capabilities and limitations of the three acoustic solvers | Tested | |
| Assessment of what each solver would leave out; no automatic selection | Tested | shown in the Full-Wave Solver panel |
| FDTD, 2D | Tested | staggered grid, second order; validated against closed forms |
| FDTD, 3D | Partial | implemented; validated for a point source in open space (amplitude, spreading, phase) and by a 3D example. Boundaries, materials and objects are tested in 2D only |
| Courant stability check before execution; failed run on a non-finite field | Tested | |
| Cells per wavelength and time-step fraction as settings; never lowered automatically | Tested | |
| Preflight: grid, time step, duration, resolution, dispersion error, memory, work, time estimate, objects, warnings | Tested | the time estimate is rough |
| Domain faces: rigid, pressure release, impedance, absorbing | Tested | one kind for all faces in the panel; per face in the core |
| Periodic and symmetry boundaries | Missing | a rigid wall is the symmetry plane of an even field |
| Absorbing layers (split-field PML), configurable thickness | Tested | below 0.2 % reflection at normal incidence with 12 cells; long runs with layers not tested |
| Per-cell density, speed of sound, attenuation; interfaces | Tested | reflection, transmission, speed, refraction, attenuation against closed forms |
| Solids as elastic bodies (shear waves) | Missing | solids are fluids with their density and speed of sound |
| Frequency-dependent attenuation, dispersion, anisotropy | Missing | |
| Moving media, flow, continuous property gradients | Missing | layered media can be built from objects |
| Simulation geometry from each geometry kind's definition | Tested | stepped at the cell size; thin shells thickened with a note |
| Mesh generation, refinement, mesh-quality tools; Gmsh | Missing | the solver uses a uniform grid |
| Signals: continuous, tone burst, Gaussian pulse, Ricker, sweep | Tested | one kind per run |
| Sampled signal | Partial | core and project file; no editor in the panel |
| True time delay of emitters | Tested | |
| Emitter directivity in full-wave runs | Missing | every emitter is a monopole; stated in the result |
| Recording: probe signals, snapshots, steady-state field, energy | Tested | |
| Particle-velocity fields, intensity, region-of-interest storage, full field histories | Missing | velocity is solver state but is not recorded |
| HDF5 or other binary result files; checkpointing and resumption | Missing | results live in memory for the session; probe signals export as CSV |
| Spectra of probe signals with resolution, window, aliasing limit | Tested | own radix-2 FFT |
| Resonance identification from response peaks | Tested | labelled as response peaks |
| Eigenmode analysis, mode shapes | Missing | |
| Steady-state field as an ordinary result: display, metrics, comparison, run record | Tested | |
| Recorded instants shown in the viewport as instantaneous pressure | Tested | slider in the Time Signals tab |
| Plots: pressure over time, spectrum magnitude and phase, energy over time | Tested | one curve at a time |
| Isosurfaces, volume rendering, arbitrary cross-sections, probe-to-probe comparison plots | Missing | |
| Background job with progress and cancellation; memory and work limits | Tested | |
| Threads, SIMD, GPU backends; backend selection | Partial | threads (bit-identical, 1.9x); optional CUDA backend implemented, not yet validated on a GPU (see Compute backends) |
| FEM, BEM, angular spectrum, pseudospectral solvers | Missing | |
| Nonlinear, thermoviscous, streaming, cavitation effects | Missing | requirements for each are listed in `ADVANCED_ACOUSTICS.md` |
| Example projects A (2D, 3D), B to G in `examples/advanced_acoustics` | Tested | |

## Mechanical waves and structural vibration — `docs/requirements/mechanics.md`, `docs/mechanics/`

| Capability | State | Notes |
|---|---|---|
| Separate mechanical module; signals shared through `src/core/dsp`; coupling module | Tested | |
| Isotropic elastic materials (E, nu, rho, loss factor) with derived constants | Tested | anisotropic, viscoelastic, frequency- and temperature-dependent behaviour: Missing |
| Material library: stainless steel, brass, titanium, ABS, PLA, nylon, polycarbonate, alumina added | Tested | illustrative values; composites not included (need orthotropy) |
| Structural idealization chosen by the user, explained, with warnings | Tested | boxes, cylinders, planes, horns, periodic rods; other geometry refused |
| Euler-Bernoulli 3D frame element | Tested | shear deformation and rotary inertia neglected (stated) |
| Kirchhoff plate (ACM rectangle) | Tested | out-of-plane bending only; thin plates |
| Pre-tensioned membrane | Tested | frequencies validated; no stress |
| Trilinear hexahedral solid | Partial | exact in tension; too stiff in bending unless finely meshed (measured and stated) |
| Shells, multi-member frames, trusses, layered and composite structures | Missing | |
| Mesh generation for arbitrary geometry; Gmsh | Missing | structured meshes only |
| Static analysis with stresses | Tested | beam, plate, solid; refuses structures that are not held |
| Modal analysis: frequencies, shapes, effective mass, residuals | Tested | dense and shift-invert Lanczos |
| Harmonic analysis: direct and modal superposition, Rayleigh / loss factor / modal damping | Tested | |
| Transient analysis: Newmark, actuator signals and delays, sensors, snapshots, energy | Tested | stress and strain histories not recorded |
| Actuator, sensor, vibration-target objects; property editor; markers | Tested | prescribed displacement / velocity and physical actuator response models: Missing |
| Deformed display with separate visual magnification, outline, animation, colour by quantity | Tested | strain not displayed |
| Frequency-response plots with mode markers; transient plots; mode table | Tested | |
| Measured frequency responses from CSV, stored and compared | Tested | no hardware acquisition |
| Vibration targeting (actuator inputs by regularized least squares), applied undoably | Tested | structural parameter optimization: request 009 |
| Horn geometry (conical, exponential, stepped) | Tested | analysed as a member of varying section |
| Periodic rod: longitudinal Bloch band structure, stop bands, finite-rod attenuation | Tested | 2D/3D phononic crystals, locally resonant metamaterials: Missing |
| One-way sound radiation from plates and membranes (Rayleigh) | Tested | two-way fluid-structure interaction: Missing |
| Threads, SIMD, GPU backends | Missing | single-threaded sparse direct solvers (not accelerated by decision: ADR-000212) |
| Example projects A to H in `examples/mechanics` | Tested | |

## Electromagnetic waves, antennas and RF — `docs/requirements/electromagnetics.md`, `docs/electromagnetics/`

| Capability | State | Notes |
|---|---|---|
| Separate electromagnetic module; conventions `e^{+jωt}`, peak phasors, SI | Tested | |
| Electromagnetic materials: er, mur, sigma, loss tangent, classification, skin depth | Tested | example values illustrative; dispersive models (Debye, Drude, Lorentz) Missing |
| Ideal sources: electric dipole, magnetic dipole, plane wave | Tested | |
| Antennas: dipole, monopole (infinite ground), small loop — analytical | Tested | thin-wire, assumed current; moment-method wire solver Missing |
| Antennas: patch, horn, helix — design estimates only | Partial | no field, pattern or impedance calculated |
| Feed impedance (induced EMF), matching, sweeps, VSWR, return loss | Tested | |
| Directivity, radiation efficiency (conductor loss), gain, realized gain | Tested | dielectric and ground losses Missing |
| Patterns: 2D cuts and 3D surface | Tested | |
| Exact near fields, probes, fields on regions, far-field distance | Tested | |
| Arrays: linear, planar, circular, custom; per-element overrides; markers | Tested | mutual coupling Missing (stated everywhere) |
| Array factor analysis, phase / true-time-delay steering, squint | Tested | |
| Grating-lobe prediction and warnings | Tested | regular lattices; other layouts via near-equal maxima |
| Dolph-Chebyshev tapers; least-squares beam and null synthesis | Tested | |
| Lines: coax, microstrip, stripline, CPW; loaded lines, transformers, L-networks, losses | Tested | |
| Guides and cavities: modes, propagation, TE10 field; guide objects | Tested | waveguide fields other than TE10 Missing |
| Yee FDTD full-wave solver (2D TM, 3D; PML; PEC; materials; thin wires) | Tested | validated against series solutions with refinement |
| Full-wave setup from a scene: dipoles, monopoles, current elements, bodies | Tested | loops, patches, horns, helices, plane waves, arrays not represented |
| Field display: components, magnitude / phase / instantaneous / dB, normalized or absolute | Tested | the viewport shows one field at a time |
| Touchstone one-port import and export; measured vs calculated | Tested | multi-port Missing; no instrument control |
| Reproducibility metadata and run records | Tested | fields kept in memory, not in the project file |
| Electromagnetic optimization | Partial | synthesis, tapers, resonant length, matching; general optimization: request 009 |
| Threads, SIMD, GPU backends | Partial | Yee solver, analytical fields and patterns threaded (bit-identical, Yee 4.4x); optional CUDA Yee backend implemented, not yet validated on a GPU |
| Example projects A to G in `examples/electromagnetics` | Tested | |

## Compute backends — `docs/requirements/compute.md`, `docs/compute/`

| Capability | State | Notes |
|---|---|---|
| `src/core/compute`: thread pool, compute options, capability detection | Tested | chunked scheduling for hybrid CPUs |
| Threaded acoustic FDTD and Yee solvers, bit-identical to one thread | Tested | 1.9x and 4.4x on 16 threads (memory bandwidth limits the acoustic solver) |
| Threaded analytical EM fields and pattern sampling | Tested | bit-identical |
| CUDA backend for both full-wave solvers (double precision, no FMA) | Partial | implemented and compiled (sm_75..sm_120 + PTX); GPU comparison tests skipped here because the GPU's memory was taken by another program: NOT validated yet |
| Backend choice in the panels, plan "Runs on", refusal without a usable device, backend in results | Tested | never a silent fall-back |
| VOLK vector kernels with plain fallbacks | Tested | used for field display magnitudes and complex field components |
| Optional VOLK / CUDA in CMake and qmake; static CUDA runtime | Tested | builds without either work (stubs) |
| Compute capabilities in Diagnostics and the startup log | Tested | |
| Benchmark in the perf lane | Tested | figures in `docs/compute/ACCELERATION_AUDIT.md` |
| Vendor-neutral GPU backend; single precision on the GPU | Missing | the stepper interface allows them |
| Mechanics solvers accelerated | Missing | by decision (ADR-000212) |

## Experimental hardware, measurement and calibration — `docs/requirements/experiments.md`, `docs/experiments/`

| Capability | State | Notes |
|---|---|---|
| Domain-independent experiment framework; drivers in `resonant_hardware`; layering rules | Tested | `tools/check_layering.py` |
| Experiment definition, templates (10), validation without hardware, persistence in the project | Tested | run-time states not saved |
| Hardware abstraction, capabilities, statuses, asynchronous discovery | Tested | |
| Simulated instruments (audio interface, vibration DAQ, RF receiver, signal generator, VNA, positioner) with fault injection | Tested | data marked generated, not measured |
| Asynchronous acquisition: bounded queue, backpressure, gaps as NaN, clipping, timestamps, device errors, live view | Tested | |
| Measurement records and the HDF5 / binary store next to the project (exclusive, read-only) | Tested | HDF5 1.10 here |
| Calibration records, append-only library, frequency-dependent application, procedures | Tested | calibrator, comparison, channel-to-channel, actuator, ultrasonic and RF arrays |
| Processing: 17 explicit operations, deterministic ids, reproduction from archives | Tested | Welch, H1 with coherence, filters, resampling, delay |
| Spatial scans and plane reconstruction with coverage flags | Tested | inverse-distance weighting only |
| Comparison with simulation (generic and per domain) | Tested | acoustic free field, antenna S11, modes, patterns; relative only on request |
| Uncertainty budgets carried through calibration and processing | Tested | uncorrelated components only |
| Execution engine (13 steps, states), background job, cancellation | Tested | |
| Output authorization, safety limits, latched emergency stop | Tested | never saved; checked per output block |
| HTML reports, experiment archives | Tested | |
| Acoustic, mechanical and RF workflows; N-port Touchstone 1.1 / 2.0 | Tested | on simulated devices |
| ALSA capture and playback | Partial | tested on the ALSA `null` device only; no physical interface was used |
| SoapySDR | Partial | discovery and capabilities; streaming not implemented (no SDR to test) |
| Experimental Hardware dock, Experiments menu, viewport markers, `--run-experiment` / `--authorize-output` | Tested | |
| Level triggering; motorized positioners other than the simulated one; SCPI / VISA, UHD, libiio, PortAudio drivers | Missing | the abstraction admits them (ADR-000213) |
| Correlated uncertainty, Monte Carlo propagation | Missing | |

## Optimization, inverse design and automated engineering — `docs/requirements/optimization.md`, `docs/optimization/`

| Capability | State | Notes |
|---|---|---|
| Domain-independent framework (problems, variables, objectives, constraints, runs, candidates); adapters registered in `coupling`; layering rule | Tested | `tools/check_layering.py` |
| Design variables: continuous, stepped, integer, categorical; locks, activation, groups; derived variables by expression | Tested | expressions have no units |
| Objectives (minimize, maximize, target, thresholds) with normalization; documented quantities of three adapters | Tested | |
| Hard and soft constraints on quantities and expressions; violations reported; feasibility | Tested | |
| Sweeps, random and Latin-hypercube sampling, Nelder-Mead, L-BFGS-B (finite differences), differential evolution, NSGA-II, Bayesian optimization; recommendation | Tested | SQP, CMA-ES, Powell: Missing |
| Pareto fronts, priorities after the run, no single best for several objectives | Tested | |
| Finite-difference gradients with reliability checks; local sensitivities, rankings, heat map, response curves, correlations | Tested | exact / adjoint gradients: Missing |
| Linear inverse source design: Tikhonov, power penalty, minimum norm, bounded weights, sparse excitation; classification | Tested | acoustic, mechanical, electromagnetic response matrices |
| Geometry optimization with regeneration, joint validation of parameters, invalid designs never evaluated | Tested | |
| Passive focusing judged by the Kirchhoff and full-wave models | Tested | lenses and waveguides only in the full-wave model |
| Material selection from the library; hypothetical property scalings labelled | Tested | |
| Multi-fidelity: model levels, recorded per candidate, re-evaluation as linked candidates, validation status | Tested | experimental level not an optimization model yet |
| Surrogates (quadratic, RBF, Gaussian process) with training and leave-one-out errors; active learning | Tested | isotropic length scale |
| Robust optimization: uniform / normal uncertain parameters with required sources, common Latin-hypercube samples, mean / mean + k sigma / worst case, constraint probability | Tested | independent parameters only |
| Jobs: background runs, live candidates, parallel evaluations with a limit, time and memory limits, cancellation, checkpoints, restart by replay, exact caching | Tested | one optimization job at a time in the Studio |
| Candidate comparison; promotion as one undoable revision with lineage; candidate and run export (JSON, CSV, STL, OBJ with mesh checks) | Tested | STEP: Missing |
| Topology optimization: 2D SIMP compliance proof of concept, filter, cleanup, extrusion | Tested | not acoustic or electromagnetic; not a finished design |
| Optimization Studio dock, Optimization menu, `--run-optimization` | Tested | |
| Seven example projects (A-G) run by the tests | Tested | `examples/optimization/` |
| Python workflows | Tested | the `resonant.optimization` API (REQ-CHAT-20261126-010) and the versioned JSON / CSV exchange |
| Mutual coupling in array optimization; Yee full-wave evaluation of arrays | Missing | both array models assume isolated elements |

## Python, automation, plugins, help and demonstrations — `docs/requirements/automation.md`, `docs/python/`, `docs/automation/`, `docs/plugins/`, `docs/help/`, `docs/demos/`

| Capability | State | Notes |
|---|---|---|
| Python API 1.0: pybind11 module over the core, documented package by domain, versioning, id-based object references | Tested | built when pybind11 and the Python headers are found; skipped with the reason otherwise |
| Python: scene construction (every object type), settings, acoustic fields and pressures, FDTD, modal and harmonic analyses, antennas, patterns, steering, tapers, optimization with promotion, workflows, experiment validation; NumPy results | Tested | experiments are run only from the Experiments panel (outputs need authorization) |
| Errors as `resonant.Error`, progress and cancellation callbacks, GIL released, parallel projects in threads | Tested | |
| Python console (worker process): multi-line input, highlighting, completion, history, inspection, Stop, crash isolation, `app.apply` as one undoable step | Tested | the console works on a copy of the project |
| Script editor: line numbers, indentation, completion, run / run selection / stop, error navigation, templates, examples, scripts in the project | Tested | no debugger |
| Seven example scripts run by the tests | Tested | `python/examples/` |
| Workflow engine (`resonant.workflow` v1): parameters, conditions, repetition, dependencies, failure behaviour, cancellation, jobs on snapshots, history | Tested | steps run one after another |
| Workflow actions for every domain; five templates; Workflows dialog; `--run-workflow` | Tested | |
| Plugins: manifests, discovery, validation, dependencies, explicit enabling, states and reasons; workflow-pack, Python and native kinds; Plugins dialog; Tools > Extensions | Tested | not sandboxed (stated); native plugins can crash the application |
| Example plugins (geometry generator, analysis, importer, workflow pack) and deliberately failing test plugins | Tested | the importer's bundled data is synthetic |
| Offline in-app help: 120 topics in 12 categories with statuses, navigation, ranked search with highlighting, images and readable equations, missing-topic handling | Tested | QTextBrowser (no web engine): simple styling |
| Help build (both build systems): validation of links, anchors, related topics, images, screenshot manifest, statuses; navigation and search index; installed with the application; `--check-help` | Tested | the development build uses `--draft` for not-yet-captured screenshots |
| Context help (F1) from a central topic registry; Help menu; help buttons; failure messages mapped to troubleshooting | Tested | |
| 22 screenshots captured from the running application by Demo Studio sequences, with annotated copies | Tested | `--capture-screenshots help/screenshots/manifest.json` |
| Generated Python API reference; documentation coverage report | Tested | `tools/help/generate_api_reference.py`, `tools/help/coverage.py` |
| Demo Studio: profiles, application-level step sequences, event-based waiting, time limits, failure handling, stop, restart, run manifests; `--run-demo` | Tested | |
| Presenter mode: clean layout, presenter bar with instructions, notes, progress, pause / resume / next / restart / exit | Tested | |
| Demonstrations A-F and H run by the tests from a clean state | Tested | |
| Demonstration G (experimental correlation) | Partial | imports labelled synthetic S11 data and compares it with a dipole model; no calibration step; no live acquisition (every bundled experiment drives outputs, which a demonstration never authorizes) |
| RF_IQ_Analyst reference audit | Done | `docs/reference/ANALYST_REFERENCE_AUDIT.md`; the reference was not modified |
| Video recording, narration audio | Missing | by design: Resonant prepares fixed-size, scripted, reproducible runs for external recording |

