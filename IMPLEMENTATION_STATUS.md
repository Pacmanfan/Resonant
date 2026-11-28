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
| Background jobs: states, progress, cancellation | Tested | listeners hear each job's changes in order; waiting returns after they were told (ADR-000231; the flaky cancellation test fixed) |
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
| Binary installer / distribution package | Tested | the self-contained Linux ZIP (REQ-CHAT-20261126-013, ADR-000218, `docs/release/LINUX_ZIP_DISTRIBUTION.md`); `make install` / `cmake --install` as before; no installer for other systems |

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
| Isotropic elastic materials (E, nu, rho, loss factor) with derived constants | Tested | anisotropic and frequency-dependent library data: Missing; plastic, damaging and viscoelastic states: see Dynamic material state |
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
| Layered 3D solids: 20-node hexahedra on plane, cylindrical, spherical or revolved surfaces; isotropic, orthotropic, graded layers; varying thickness (RES-MECH-019) | Tested | linear, small displacements; at most 60000 elements |
| Interfaces in 3D: perfect bonding (declared), springs, sliding, Coulomb contact (Newton), debonded regions, initiation index (RES-MECH-020) | Tested | no damage evolution or cohesive zones; no contact in debonded regions; holonomic friction |
| 3D steady conduction with contact resistances (RES-MECH-021) | Tested | isotropic layers, no radiation |
| Architected-matter stacks as 3D bodies: `layered-solid`, `layered-solid-thermal`, the Matter panel and service (RES-MECH-022) | Tested | surface, supports and loads from the options; in-plane orthotropic sets refused |
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

## Automation API — `docs/requirements/automation_api.md`, `docs/architecture/AUTOMATION_ARCHITECTURE.md`, `docs/reference/`

| Capability | State | Notes |
|---|---|---|
| Application service layer (project, scene, geometry, materials, emitters, arrays, simulation, jobs, results, optimization, workflows, transactions, application) with sessions, permissions, structured errors, dry runs, idempotent retries and an audit trail | Tested | Qt-free in `src/core/services`; layering-checked |
| Revisions counted from the scene and the undo history (also GUI commands, undo, redo, Python edits); optimistic concurrency with `CONFLICT` and the current revision | Tested | |
| One ordered event stream; subscriptions with snapshot, resumption and gap notices; filters | Tested | 10 000 events retained |
| Jobs through the API: acoustic field runs, workflows, optimizations; wait, cancel; field results as summaries with provenance, chunked slices with a stride and a limit, point samples | Tested | |
| Mechanical, electromagnetic and full-wave (FDTD) runs through the API | Missing | available in their panels and from Python; listed by `GetCapabilities` / `ListSolvers` as not startable through the API |
| Atomic batches and staged transactions (one undo step, base-revision conflicts) | Tested | project changes only: no rollback of files, jobs or hardware |
| proto3 contract `proto/resonant/v1` (16 services, 68 methods), generated at build time in qmake and CMake, descriptor set, compatibility check against a baseline, generated reference | Tested | optional: builds without protoc / libprotobuf |
| Local endpoint (Unix socket, same user, token) in the GUI and in the headless host `resonant-automation` | Tested | |
| gRPC transport | Tested | a grpcio bridge process (`python3 -m resonant_api.bridge`), loopback by default; C++ gRPC is not linked (libgrpc++ not installed here) |
| C++ gRPC transport | Not implemented | waits for the owner to install `libgrpc++-dev` and `protobuf-compiler-grpc` (REQ-CHAT-20261128-003, parked; `AUTOMATION_ARCHITECTURE.md`) |
| The GUI's operations through the services (create, rename, move, show / hide, reparent, parameters, materials, library, delete, duplicate, undo, redo, save, field runs); Tools > Automation Server (permissions, token, bridge, sessions) | Tested | domain panels that set a whole component state still push one command (counted like any change) |
| Python client `resonant_api` (socket, gRPC, in process) and in-process dispatch in the native module | Tested | `SubscribeEvents` needs a socket or gRPC |
| MCP adapter: tools from the contract by permission level, dry runs, its own MCP session | Tested | tested without an MCP client; `serve_stdio` uses the `mcp` package |
| Latency: GetProject median 0.12 ms over the local socket, 0.45 ms over gRPC through the bridge (this machine) | Measured | `AutomationApiTests.test_call_latency` |
| Authentication beyond the local token (TLS, users) | Missing | the bridge is unencrypted and loopback by default |


## Linux ZIP distribution — `docs/requirements/distribution.md`, `docs/release/`

| Capability | State | Notes |
|---|---|---|
| One command makes `dist/RESONANT-Linux-x86_64-<version>.zip` and `.sha256` from a Release CMake tree (`tools/release/build_linux_zip.sh`, target `package_linux_zip`) | Tested | a Debug tree is refused; the qmake build is not packaged |
| Bundled runtime by an explicit policy: Qt 6.4 with selected plugins, 78 libraries, Python 3.12 with NumPy, protobuf, grpcio; system libraries (C library, C++ runtime, OpenGL/EGL, X11/xcb, fonts, ALSA, zlib, expat) never bundled | Tested | an unclassified dependency fails the build |
| Relocatable: relative DT_RPATH on every ELF file, `bin/qt.conf`, no source, build or staging path in any file, no links | Tested | renamed, moved, path with spaces, through a link, read-only |
| Unified runtime paths (bundle, installation, build tree; user directories outside the installation) | Tested | `RuntimePathsTest.*` |
| Launcher `resonant` / `resonant.sh`: architecture, glibc, display and system-library checks with the packages to install, argument and exit-status pass-through, `--software-gl` | Tested | POSIX sh (dash and bash) |
| `--check-runtime`, `--diagnostics [--report]` (OpenGL 4.5 core context, Python, help, plugins, compute, package integrity; no private data, no telemetry) | Tested | OpenGL checked with Intel Mesa, NVIDIA and llvmpipe |
| Offline help in the ZIP, `help/index.html` with search for browsers, release build (no draft), link and asset validation | Tested | browser search checked with Node.js |
| Every example project opens from the package; every demonstration runs from it | Tested | 43 projects, 8 demonstrations, also in clean containers |
| Licenses: every bundled component in `tools/release/third_party.json`, `THIRD_PARTY_NOTICES.txt`, `licenses/` | Tested | the application's own license is not chosen yet (owner decision) |
| Validation of the final ZIP (41 checks) and clean-container tests (Ubuntu 24.04, Fedora 41, Fedora 42 offline; Ubuntu 22.04 and Debian 12 refused) | Tested | `docs/release/RELEASE_REPORT.md` |
| A real desktop session on another machine; native Wayland; AMD GPUs | Missing | not available here |
| Builds for older distributions (glibc < 2.38), other architectures | Missing | needs a build on an older baseline |

## Shape–Function–Energy direction — `docs/requirements/shape_function_energy.md`, `docs/vision/`, `docs/physics/`, `docs/materials/`, `docs/geometry/`

| Capability | State | Notes |
|---|---|---|
| The direction, its requirements (RES-SFE-001..012), the audit, the coupled multiphysics architecture and the twelve-stage research roadmap | Documented | ADR-000219; no code changed |
| Conventions shared by every domain (SI, `e^{+jωt}` peak phasors, provenance) | Tested | checked consistent by the audit |
| Single-domain energy records (acoustic FDTD, Yee, mechanical transient balance) | Tested | the first entries of the planned ledger |
| Single-domain resonances (acoustic cavity estimates, modal analysis, EM cavity modes, 1D Bloch band structure) | Tested | no shared resonance description, no Q from losses |
| One-way structure-to-sound coupling | Tested | outside the coupled engine (listed there as an external model) |
| Coupled Energy–Matter Engine, material state of temperature, thermal domain | Tested | see the section below (REQ-CHAT-20261126-016) |
| Material state of stress or fields; anisotropic tensors; laws in the material library | Partial | material-state fields (REQ-CHAT-20261126-021); anisotropic sets on architecture regions (REQ-CHAT-20261127-001); no tensors or laws in the library itself (REQ-CHAT-20261128-006) |
| Shared resonance description, Q, mode extraction, coupled resonators | Tested | resonance engineering (REQ-CHAT-20261126-018, `docs/requirements/resonance.md`) |
| Unit cells, lattices, 2D/3D Bloch analysis, effective properties | Partial | 1D lattices, layered and resonator cells, effective properties with validity (REQ-CHAT-20261126-019); 2D/3D Bloch of continua missing |
| Multiscale adapters to external tools | Partial | detection and requirements of external engines, built-in Lennard-Jones references (REQ-CHAT-20261126-020); no engine installed (REQ-CHAT-20261128-006) |
| Nonlinear, viscoelastic, plastic, damaging, phase-changing materials | Tested | see Dynamic material state (REQ-CHAT-20261126-021); finite strain, fracture and fatigue: Missing |
| Optical resonators and photonics | Missing | theoretical future (the electromagnetic domain is RF) |

## Coupled Energy–Matter Engine — `docs/requirements/coupled_engine.md`, `docs/architecture/COUPLED_ENGINE_ARCHITECTURE.md`

| Capability | State | Notes |
|---|---|---|
| Typed physical quantities, field data on cell grids, conservative and interpolating transfer with reports | Tested | `src/core/multiphysics`; mesh-to-mesh mapping and local frames are planned |
| Material state: region laws (constant, linear, table) of temperature with validity, extrapolation warnings, revisions, history | Tested | laws live in the physical system, not yet in the material library |
| Geometry state: reference and current layer thicknesses, revisions, stale-solution reporting | Tested | layered systems only |
| Physical systems in the project (undoable, saved, validated), the coupling-model registry with real maturity | Tested | `layered-slab` systems only; scene-object systems are planned |
| One-way, sequential and iterative coupling with relaxation; failure, invalid-value and cancellation handling; jobs and provenance | Tested | no multirate time-dependent coupling, no checkpoint resume |
| Energy ledger: per-participant and system residuals, transfer records, attribution | Tested | momentum and force accounting are planned |
| Thermal domain: finite-volume conduction, steady and transient (implicit Euler), thermal property set | Tested | `src/core/thermal`; no GUI panel; the example library has no thermal values |
| Layered-media plane-wave solution (exact, lossy layers, absorbed power) | Tested | normal incidence |
| Thermal loads and thermal stress in the finite-element model (solids, frames) | Tested | plates and membranes refused |
| Couplings EM → heat, temperature → EM, expansion, temperature → modulus, deformation → geometry | Tested | validated against analytical or independent references in 1D (`docs/physics/COUPLED_SIMULATION_VALIDATION.md`) |
| `MultiphysicsService` (8 methods), Python, MCP, capabilities | Tested | `tests/core/test_multiphysics_services.cpp`, `python/tests/test_multiphysics_api.py` |
| A physical-system panel, ledger and coupling-graph views in the GUI | Missing | planned (`docs/architecture/COUPLED_ENGINE_ROADMAP.md`) |
| Yee, FDTD, finite-element plates and 3D conduction as participants | Missing | planned |
| Acoustic ↔ structure, Lorentz, radiation-pressure, magnetic-loss, piezoelectric, acoustic-heating and fluid couplings | Missing | registered as planned; validation refuses them |

## Functional structures — `docs/requirements/functional_structures.md`, `docs/architecture/FUNCTIONAL_STRUCTURE_ARCHITECTURE.md`

| Capability | State | Notes |
|---|---|---|
| Functional structures as project resources (undoable, saved, validated) with bodies, regions, boundaries, analysis, objectives, constraints | Tested | `src/core/functional`; created through Python, the API or example projects |
| Bound, free and derived parameters; changes as one undoable step through the geometry kinds' rules; optimistic concurrency | Tested | rotations and material choices are not structure parameters yet |
| Content-keyed staleness of results (geometry, material, definition, discretization), stale-on-completion, the simulation-geometry cache | Tested | |
| Microwave cavity: modes, fields, stored energy, wall and fill Q, bandwidth, port response, sensitivity, thermal detuning; Yee full-wave frequencies and fill decay | Tested | rectangular cavities; lumped port; Yee walls perfectly conducting |
| Two-mirror optical resonator (paraxial): stability, waist, spots, spacing, finesse, Q, clipping | Tested | no misalignment, no diffraction calculation |
| Acoustic horn (Webster, baffled piston) and the `horn.acoustic` geometry kind | Tested | plane waves, lossless walls |
| Mechanical resonator (finite elements, closed form) | Tested | reuses the mechanics studies |
| Photonic two-layer cell (exact Bloch, finite stack, homogenized index) and the periodic rod | Tested | 1D, normal incidence |
| Coupled slab through the Coupled Energy–Matter Engine with geometry from the scene | Tested | layered systems |
| Sweeps, sensitivities, mode tracking, objectives, constraints (manufacturing), explained candidates; optimization through the existing framework | Tested | no adjoints, no topology optimization of structures |
| `FunctionalStructureService` (14 operations), Python, MCP, capabilities | Tested | `tests/core/test_functional_services.cpp`, `python/tests/test_functional_api.py` |
| Functional Structures panel (parameters, analysis, metrics, modes, staleness, curves, sweep) | Tested | `tests/app/test_functional_panel.cpp`; no structure editor, no viewport overlays of functional results |
| CAD import, OpenCASCADE | Missing | no CAD kernel in Resonant (open V1 item RES-PRODUCT-015) |
| Force and momentum metrics, coupled resonators, cylindrical cavities, 2D/3D lattices | Missing | planned (`docs/architecture/ENERGY_MATTER_RESEARCH_ROADMAP.md`) |

## Resonance engineering — `docs/requirements/resonance.md`, `docs/physics/RESONANCE_ARCHITECTURE.md`

| Capability | State | Notes |
|---|---|---|
| One resonance description: nature (natural, driven-peak, identified), decay, bandwidth, Q by loss mechanism | Tested | every functional analysis; in the contract (`FunctionalResult.modes`) and the panel |
| Spectroscopy: matrix-pencil poles of signals, Lorentzian fits of spectra, uncertainty, overlap and asymmetry flags, refusals | Tested | `src/core/dsp/Resonance`; `ResonanceService.IdentifyResonances` |
| Lumped oscillator networks: complex modes, driven response, transient integration with an energy ledger, coupled-mode theory as an approximation | Tested | `src/core/oscillators` |
| Coupled cantilevers and coupled Helmholtz resonators: splitting, hybrid modes, localization, avoided crossings, energy exchange | Tested | lumped; individual resonators from their domain models |
| Helmholtz resonator: end corrections, radiation and viscous Q, impedance, pressure gain; plane-wave stepped duct; `resonator.helmholtz` | Tested | no thermal loss in the cavity |
| Cantilever as a resonator: modes, tip modal mass and stiffness, Q, frequency response, ring-down | Tested | Euler–Bernoulli |
| Cavity ring-down: complex poles of the Yee record | Tested | |
| Duffing (backbone, bistability, hysteresis) and Mathieu (Floquet tongue) | Tested | single lumped oscillators |
| Resonant cavity in the Coupled Energy–Matter Engine (`rectangular-cavity`, `em.cavity.mode`, `thermal.lumped`, `thermal.uniform-expansion`) | Tested | lumped thermal model |
| Q against extraction (β/(1+β)) as a tradeoff | Tested | through sweeps and objectives |
| Coupled resonators as one finite-element model, Q of open radiating structures, fluid resonances beyond lumped acoustics, nonlinear field solvers | Missing | planned |

## Metamaterials and effective properties — `docs/requirements/metamaterials.md`, `docs/materials/METAMATERIAL_ARCHITECTURE.md`

| Capability | State | Notes |
|---|---|---|
| Effective-medium analyses as functional structures, constituents / unit cell / periodic / effective / equivalent kept apart | Tested | category `effective-medium`; `details.representations` |
| Bloch–Floquet analysis of lumped cells (numerical, phase-shifted links) | Tested | `src/core/oscillators/BlochLattice`; against monatomic, diatomic and mass-in-mass closed forms |
| Locally resonant mass-in-mass lattice: gap, negative effective mass, finite-chain attenuation, long-wave speed | Tested | cell masses from geometry in the example |
| Duct with Helmholtz resonators: Bloch dispersion, hybridization and Bragg gaps, effective bulk modulus, finite-array attenuation | Tested | plane waves; lumped resonators |
| Layered media at oblique incidence (TE, TM, lossy), uniaxial effective slab, oblique Bloch waves | Tested | against the layered-media solver, Airy and Brewster |
| S-parameter retrieval: branch by continuity, passivity, conditioning, locality and antiresonance flags | Tested | negative-index slab retrieved |
| Split-ring arrays: circuit model, Lorentzian permeability, negative band, Kramers–Kronig and passivity | Tested | an approximation; no full-wave check |
| Layered thermal conductivity tensor against finite volumes | Tested | |
| Validity with every effective value; no effective material outside it | Tested | `docs/physics/HOMOGENIZATION_AND_VALIDITY.md` |
| Effective materials saved into the library (provenance `homogenized`, undoable, stale results refused) and used by the coupled engine | Tested | `CreateEffectiveMaterial`; Python, MCP, the panel's Save effective material |
| 2D/3D Bloch of continua, full-wave split rings, anisotropic (tensor) library materials, architected and graded lattices, nonlinear or time-varying metamaterials | Missing | planned |

## Multiscale material modeling — `docs/requirements/multiscale.md`, `docs/materials/MULTISCALE_ARCHITECTURE.md`

| Capability | State | Notes |
|---|---|---|
| Representations per scale (electronic … continuum) with revisions, provenance, validation, model cards; relationships | Tested | project extension `multiscale` |
| Material lineage: derivation records, uncertainty, provenances computed / database / fitted / surrogate; save and reload; staleness | Tested | `docs/materials/MATERIAL_LINEAGE.md` |
| Voxel RVEs: layered, spheres, pores, Voronoi grains with random or aligned orientations, imported voxels | Tested | seeded; labelled synthetic or imported |
| Computational homogenization of the steady potential problem (thermal, electrical, static permittivity) with periodic, linear, flux and mixed boundaries | Tested | against exact, Maxwell-Garnett, Hashin-Shtrikman and self-consistent references |
| RVE size studies, realizations, uncertainty, representativeness | Tested | |
| Crystals, XYZ and POSCAR import, force-field records with applicability | Tested | CIF and PDB not supported |
| Lennard-Jones lattice statics (elastic constants, stability, cutoff sensitivity) and NVE dynamics | Tested | reference implementations for pair potentials |
| Derivation pipeline: applicability, reliability, assignment to library materials (one undo step) | Tested | `MultiscaleService` |
| Local states of coupled results; iterative region-RVE coupling | Tested | one temperature per region |
| Derived materials in the coupled engine, functional structures, metamaterial analyses | Tested | |
| Material Scales panel, Python, MCP | Tested | |
| External engines (LAMMPS, OpenMM, Quantum ESPRESSO, CP2K, GPAW, phonopy) | Missing | listed with detection and requirements; none installed or integrated |
| Electronic-structure and molecular calculations | Missing | representations only |
| Elastic RVE homogenization, FE2, concurrent coupling, uncertainty propagated through chains | Missing | planned |

## Dynamic material state — `docs/requirements/material_state.md`, `docs/materials/DYNAMIC_MATERIAL_STATE.md`

| Capability | State | Notes |
|---|---|---|
| State variables with unit and class (reversible, irreversible, history), fields with summaries, model cards | Tested | `src/core/materialstate` |
| State records in the project (extension `material-state`), save and reload, undo | Tested | |
| J2 plasticity: isotropic and kinematic hardening, radial return, consistent tangent, energy split; uniaxial point; indeterminate bar | Tested | small strain |
| Scalar damage (exponential, power law); a cantilever accumulating damage with its modes from the damaged stiffness | Tested | also the functional analysis `mechanics.damaged-cantilever` |
| Standard linear solid: exact relaxation, storage and loss moduli | Tested | one branch (no Prony series) |
| Phase change by the enthalpy method: latent heat, phase properties, energy balance, Stefan front, refused unstable steps | Tested | one-dimensional slab |
| Preisach hysteresis: loops, return-point memory, congruent minor loops, loss per cycle, core power | Tested | scalar, rate-independent |
| Time-stepped coupled evolution: temperature-dependent properties per step, thermo-mechanical stress and thickness, energy per step | Tested | the coupled solvers carry temperature laws only |
| Commit of an evolved state to a library material: provenance computed, derivation, lineage, staleness of dependents | Tested | |
| Catalogue naming the models that are not implemented | Tested | |
| MaterialStateService, Python, MCP, Material State panel, examples a–f | Tested | |
| Crack propagation, fatigue, finite-strain plasticity, hyperelasticity | Missing | listed as not implemented |
| Nonlinear dielectrics, nonlinear optics, nonlinear acoustics, reaction kinetics | Missing | listed as not implemented |

## Unified accounting — `docs/requirements/accounting.md`, `docs/architecture/ACCOUNTING_ARCHITECTURE.md`

| Capability | State | Notes |
|---|---|---|
| Dimensional analysis of unit expressions, checked conversions | Tested | `foundation/Dimensions`; affine temperature scales refused |
| Quantity families with dimensions and a conserved flag; typed quantities with context | Tested | torque ≠ energy, entropy ≠ heat capacity, pressure ≠ momentum flux |
| Ledger: entities, holdings, exchanges, generations, forces, missing data, numerical errors, scale contracts; JSON | Tested | rate and interval bases, per-volume normalization |
| Systems and control volumes: an exchange classified internal, input, output or dissipated by membership | Tested | the same record for every boundary |
| Conservation and consistency registry; checks with honest statuses, attribution to stated numerical errors, histories | Tested | `docs/physics/CONSERVATION_MONITORING.md` |
| Coupled engine: terms and transfers as a ledger; every coupled result carries its accounting; evolutions step by step | Tested | momentum declared missing (the participants report none) |
| Lumped networks: element work and impulses (the integrator's stages), momentum, forces with pairs; rotational networks with torques | Tested | |
| Driven resonators: Q = ω E / P by mechanism against the modal Q; buildup and decay | Tested | light damping only |
| Radiation force on layered bodies: vacuum Maxwell stress, Lorentz volume force, (1 + R − T) I / c; local and global absorption | Tested | vacuum half-spaces, non-magnetic layers |
| Material-state energies and entropy (J2, damage, viscoelasticity, the damaged cantilever, phase change, Preisach against the hysterons) | Tested | |
| Refinement studies; scale contracts of effective materials; double-counting detection | Tested | |
| AccountingService, Python, MCP, Accounting panel, examples a–h | Tested | |
| Momentum of the coupled participants, acoustic radiation force, full-wave Maxwell stress, MD momentum | Missing | reported as missing or not applicable |
| Moving control volumes, mass and charge balances, open systems with mass flow | Missing | registered models without data |
| 3D spatial accounting views | Missing | the layered profiles only |

## Energy conversion — `docs/requirements/energy_conversion.md`, `docs/physics/ENERGY_CONVERSION.md`

| Capability | State | Notes |
|---|---|---|
| Conversion stages and chains in one ledger: each stage's balance and every join checked; missing terms reported (RES-CONV-001) | Tested | |
| 3D piezoelectric continuum: stress-charge constants rotated with surface and poling, electrodes and terminals, static, short- and open-circuit modes, harmonic with loads and a balance (RES-CONV-002) | Tested | no dielectric loss, ferroelectric switching or electrode stiffness; the engine's piezoelectric participant planned |
| Thermoelectric couples: alpha, rho, kappa of T; Joule, Peltier, Thomson; generator and cooler; entropy (RES-CONV-003) | Tested | 1D legs, imposed junction temperatures; not yet an analysis of architectures |
| Magnetostrictive rods: linear piezomagnetic actuator and coil harvester (RES-CONV-004) | Tested | linear about a bias only: no hysteresis, saturation or Delta-E effect |
| Photovoltaic cells: single-diode circuit (Newton and Lambert W), spectral photocurrent, temperature, a term-by-term light budget (RES-CONV-005) | Tested | no semiconductor transport; a scaled black-body spectrum unless one is given |
| Acoustic to electrical chain: a duct plane wave, a piezoelectric plate, a load (RES-CONV-006) | Tested | 1D plane wave; nothing behind the plate |

## Experimental laboratory — `docs/requirements/laboratory.md`, `docs/experiments/LABORATORY_ARCHITECTURE.md`

| Capability | State | Notes |
|---|---|---|
| Studies: objective, hypothesis, specimen, model links with revisions and staleness, measured deviations, dataset roles | Tested | a dataset cannot be both calibration and validation |
| Imports of CSV, TSV, JSON and WAV into raw records with an import report; the original kept under its SHA-256 | Tested | Touchstone through the Experiments workspace; needs a saved project |
| Units converted to SI and recorded; degC to K; synthetic data labelled | Tested | `docs/experiments/DATA_IMPORT.md` |
| Comparisons: scalars (E_n), frequency responses, transients, fields after a frame transform | Tested | both uncertainties combined; different frames refused |
| Error budgets with distributions, stated correlations, Welch–Satterthwaite, Student t; linear and Monte Carlo propagation | Tested | |
| Parameter estimation: weighted Levenberg–Marquardt, bounds, Gaussian priors, joint fits, covariance, intervals, identifiability | Tested | `docs/experiments/MODEL_CALIBRATION.md` |
| Predictions from lumped resonators, layered electromagnetics, the coupled evolution, J2, functional metrics, library properties, design revisions | Tested | only physical parameters may be calibrated |
| Calibration as a job, kept explicitly; validation on independent data; application as a new material or material-state revision | Tested | validation on calibration data refused; not-identifiable results not applied |
| Measured resonances classified with evidence; ring-downs | Tested | nothing called a mode without phase |
| Experimental energy balances as measured accounting ledgers | Tested | judged against the expanded measurement uncertainty |
| Evidence summaries and reports (Markdown, HTML) | Tested | deterministic; change nothing |
| LaboratoryService, Python, MCP, Laboratory panel, examples a–g | Tested | |
| NetCDF, TDMS, external HDF5 and image imports | Missing | |
| Bayesian sampling of posteriors | Missing | maximum a posteriori with Gaussian priors only |
| New instrument protocol adapters (VISA, SCPI, serial); instrument control through the automation API | Missing | acquisition stays in the Experiments workspace |

## Functionally architected matter — `docs/requirements/architected_matter.md`, `docs/materials/ARCHITECTED_MATTER.md`

| Capability | State | Notes |
|---|---|---|
| Architectures: regions (composition, phases, microstructure, orientation, anisotropic and piezoelectric sets), stacks, interfaces, fields, functions, adaptive class, processes, records; validation; persistence without loss | Tested | extension `matter`; projects without one unchanged |
| Property resolution (field, region value, composition by its method, library) and effective-property estimators with bounds | Tested | no unqualified average; missing properties reported |
| Interfaces with models and derived quantities (acoustic transmission, mismatch stress, temperature jump) | Tested | `docs/materials/INTERFACE_ENGINEERING.md` |
| Solver capability declarations and compatibility checks | Tested | `docs/materials/SOLVER_CAPABILITY_MATRIX.md` |
| Lamination theory (ABD, moduli, neutral surface, thermal curvature, ply stresses); beam and plate modes coated and bare | Tested | Kirchhoff, perfect bonding |
| Piezoelectric cantilever (unimorph, bimorph; series, parallel; electrode span; orientation; energy balance in the accounting framework) | Tested | lumped, first mode |
| Through-thickness conduction and electrical analysis with contact resistances; the layered electromagnetic solver on stacks | Tested | |
| Contact resistance in the thermal conduction problem and the coupled engine; stacks as physical systems with laws | Tested | |
| Optimizer adapter `matter`: thickness, angle, coverage, material choice, interface parameters; manufacturing constrained or rejected; Pareto | Tested | an optimized design is not a validated one |
| Manufacturing: process kinds, stated constraints, residual stress at the process temperature, records | Tested | `docs/materials/MANUFACTURING_AWARENESS.md` |
| Exploration (findings apart from hypotheses), mechanisms and cascades, transformation pathways with ledgers | Tested | deterministic; changes nothing |
| Laboratory link and the `matter.stack-metric` model; MatterService, Python, MCP, Matter panel, examples a–h | Tested | |
| Shell or solid finite-element models of layered bodies (the bell) | Missing | coated beams and plates only |
| Thermoelectric, magnetostrictive, photovoltaic, triboelectric, induction and chemical conversion | Missing | listed as mechanisms, not modeled |
| Finite-stiffness, frictional, diffusive, reactive and charge-transfer interfaces in the solvers | Missing | recorded and refused |
| Fields in two and three dimensions; CAD export; 3D field rendering; editing in the panel | Missing | through the service and files today |

## Release readiness (milestone 27) — `docs/requirements/release_readiness.md`, `docs/architecture/MILESTONE_27_GAP_ANALYSIS.md`

| Capability | State | Notes |
|---|---|---|
| Baseline gap analysis and the phase plan (REQ-CHAT-20261128-001..011) | Tested | the status classes V, L, P, X, N per capability |
| Licensing decision document | Partial | prepared; the owner's decision is not yet recorded (`docs/release/LICENSING_DECISION.md`) |
| Decision-number integrity (ADR-000217 recorded as unused) | Tested | numbers are never reused |
| The deployment process and a concrete package lane (validate, clean-machine) | Partial | the process change approved by the owner (ADR-000230); the lane itself runs on a built ZIP in REQ-CHAT-20261128-010 |
| Demonstration readiness audit (electric bell, energy-focusing metamaterial, resonant pendulum) | Tested | the audit; the scenarios depend on phases 3-10 (`docs/demos/DEMONSTRATION_READINESS.md`) |

