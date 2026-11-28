# Example Projects

Projects that open in the current version of Resonant. A test
(`Examples.EveryExampleProjectLoads`) loads each of them, so they cannot
silently go stale.

| Project | Shows | Assumptions and limits |
|---|---|---|
| `foundation/primitives.resonant` | the seven primitive shapes, a group with its own transform, material assignments | geometry only: no simulation is attached. Materials are the illustrative example library. |
| `acoustics/a-two-source-interference.resonant` | two coherent emitters 3 wavelengths apart, a field plane, one probe where the paths are equal and one where they differ by half a wavelength | free field, omnidirectional sources, 40 kHz in ideal-gas air at 20 degC. Change the emitters' positions to change the separation. |
| `acoustics/b-linear-array.resonant` | a 16-element linear array at half-wavelength spacing, a field plane, probes on axis and 30 degrees off | as above; unsteered and unfocused |
| `acoustics/c-focused-planar-array.resonant` | an 8 x 8 planar array at 10 mm pitch with phase focusing applied for a target at (10, 0, 100) mm, and a field plane through the focus | as above; the stored per-element phases are valid at 40 kHz only |
| `acoustics/d-multiple-target-points.resonant` | the same array driven by an inverse-design solution for three targets: 60 Pa, 40 Pa at +90 degrees, and a null | as above; amplitude and phase control, no amplitude limit |
| `manipulators/a-parabolic-reflector.resonant` | a rigid parabolic reflector (f = 50 mm, aperture 200 mm), an emitter 1 m away on its axis, a field plane around the focus | reflector model (Kirchhoff approximation, one reflection; not full-wave). The peak is at the mirror-equation image, 2.6 mm beyond the geometric focus. |
| `manipulators/b-elliptical-reflector.resonant` | half a prolate spheroid, an emitter at one focus and a target at the other | as above |
| `manipulators/c-acoustic-lens.resonant` | a biconcave acrylic lens in water with its estimated focal length | **analytical estimate only**: the simulated field is the emitter's free field; the lens is not simulated. Material values are illustrative. |
| `manipulators/d-hybrid-focusing.resonant` | a 6 x 6 array facing a parabolic reflector, with phases that align direct and reflected sound at a target | reflector model; phases valid at 40 kHz only |
| `manipulators/e-geometry-optimization.resonant` | a reflector with f = 40 mm and, hidden beside it, the design the optimizer found for a target at 70 mm | reflector model; show one reflector and hide the other to compare (a hidden reflector is not simulated) |
| `advanced_acoustics/a-free-field-pulse-2d.resonant` | a Gaussian pulse spreading from an emitter, two probes | **full-wave (FDTD), 2D**: the emitter is a line source; 12 cells per wavelength; absorbing layers |
| `advanced_acoustics/a-free-field-pulse-3d.resonant` | the same in a 34 mm cube | full-wave, 3D; 10 cells per wavelength, 8-cell absorbing layers |
| `advanced_acoustics/b-reflection.resonant` | an emitter above a rigid plate: standing wave, shadow behind | full-wave, 2D, continuous signal |
| `advanced_acoustics/c-diffraction.resonant` | a rigid plate with a 5 mm slit; probes on the axis and in the geometric shadow | full-wave, 2D; the analytical solvers would ignore the plate |
| `advanced_acoustics/d-material-interface.resonant` | a 100 kHz pulse in water entering an acrylic block | full-wave, 2D; acrylic as a fluid (no shear waves); illustrative material values |
| `advanced_acoustics/e-resonant-cavity.resonant` | a rigid 40 x 30 mm cavity excited by a broadband pulse; the probe's spectrum shows its resonances | full-wave, 2D, 30 cells per wavelength, rigid domain, 10 ms |
| `advanced_acoustics/f-passive-focusing.resonant` | a parabolic reflector focusing an emitter 150 mm away | full-wave, 2D: the reflector is a trough of that cross-section |
| `advanced_acoustics/g-hybrid-focusing.resonant` | an 8-element array focused through a parabolic reflector, phases obtained from the full-wave solver by reciprocity | full-wave, 2D; phases valid at 40 kHz |
| `mechanics/a-vibrating-beam.resonant` | an aluminium cantilever 500 x 30 x 5 mm | Euler-Bernoulli beam; run modal analysis (Mechanics menu) |
| `mechanics/b-vibrating-plate.resonant` | a simply supported aluminium plate 400 x 300 x 2 mm | Kirchhoff plate; modes show nodal lines with the signed out-of-plane colouring |
| `mechanics/c-forced-resonance.resonant` | the cantilever with an ideal force at the tip and two accelerometers; sweep 5 - 500 Hz | loss factor 0.02; the force is applied as set (no shaker model) |
| `mechanics/d-transient-vibration.resonant` | a clamped steel plate struck by a Ricker force pulse | Newmark integration, light Rayleigh damping |
| `mechanics/e-wave-propagation.resonant` | a free steel rod 2 m long, axial force pulse, two sensors | rod theory; the pulse returns from the free end |
| `mechanics/f-vibration-concentration.resonant` | four actuators on a plate, one 10 um target and two quiet points at 150 Hz, with solved inputs | harmonic, loss factor 0.01 |
| `mechanics/g-phononic-rod.resonant` | a steel rod of 8 cells of two diameters | longitudinal waves; band structure from the Mechanics panel |
| `mechanics/h-stepped-horn.resonant` | a stepped titanium horn 20 / 10 mm | half-wave axial mode with displacement gain 4 |
| `electromagnetics/a-half-wave-dipole.resonant` | a copper half-wave dipole at 1 GHz | analytical: 73.08 + j42.51 ohm, directivity 1.64, gain and realized gain against 50 ohm |
| `electromagnetics/b-quarter-wave-monopole.resonant` | a quarter-wave monopole at 1 GHz | analytical, infinite ground plane by images: half the impedance, twice the directivity |
| `electromagnetics/c-linear-array.resonant` | 16 half-wave dipoles, half-wave spacing, Dolph-Chebyshev 30 dB | array factor (no coupling): -30 dB sidelobes |
| `electromagnetics/d-phased-array-steering.resonant` | an 8 x 8 planar array steered to (30, 45) deg | array factor; widen the spacing to see a grating-lobe warning |
| `electromagnetics/e-near-field.resonant` | the dipole with an XZ field plane of two wavelengths | analytical exact near field; Fields tab: evaluate, probe |
| `electromagnetics/f-rectangular-waveguide.resonant` | a WR-90 guide at 10 GHz | closed-form modes (TE10 cut-off 6.557 GHz) and the TE10 cross-section |
| `electromagnetics/g-full-wave-scattering.resonant` | a copper cylinder (radius lambda / 2) lit by a line source, 5 x 5 wavelength plane | full-wave Yee FDTD, 2D TM, 30 cells per wavelength; checked against the exact series |

Run **Acoustics > Run Field Simulation** (F5) after opening an acoustic
example: the sampled field is not stored in the project file. The acoustic
examples are generated by the engine (`tests/core/test_acoustic_examples.cpp`)
and compared with it on every test run; the manipulator examples likewise
(`tests/core/test_manipulator_examples.cpp`).

The full-wave examples are run with **Simulation > Run Full-Wave
Simulation** (F6); their solver settings are stored in the project. They
are generated and checked by `tests/core/test_fdtd_scene.cpp`.

The mechanics examples are run from the Mechanics menu or panel; their
analysis settings are stored in the project. They are generated and checked
by `tests/core/test_mechanics_scene.cpp`.

The electromagnetics examples are analysed from the Electromagnetics menu
or panel (example G with **Run Full-Wave** in the Fields tab); their
settings are stored in the project. They are generated and checked by
`tests/core/test_electromagnetics_scene.cpp`.

## Experiments

| Project | Shows | Assumptions and limits |
|---|---|---|
| `experiments/acoustic_experiments.resonant` | speaker characterization (H1, calibrated to Pa/FS), field mapping at 2 kHz, focusing validation with two delayed loudspeakers, ultrasonic element characterization | **simulated devices**: generated data; the microphone calibration is the simulated model's sensitivity |
| `experiments/mechanical_experiments.resonant` | modal test (accelerance FRFs, resonances), vibration characterization at 120 Hz, shaker calibration | simulated vibration DAQ and modal structure; nominal sensitivities as calibrations |
| `experiments/rf_experiments.resonant` | antenna S11 with a VNA, array channel calibration, radiation pattern from an angular scan | simulated RF devices; transmission only with authorization |
| `experiments/rlc_antenna_and_line.s2p` | a two-port Touchstone file | **synthetic**, from the simulated VNA's model: not a measurement |

Every experiment with an output needs an explicit authorization before it
runs (Run > Authorize Output...), and a saved project (raw data is stored
next to it). They are generated and checked by
`tests/core/test_experiment_examples.cpp`.

## Optimization

Each project holds a scene and an optimization problem (Optimization Studio,
or `resonant <project> --run-optimization first`). They are generated and
checked by `tests/core/test_optimization_examples.cpp`, which runs every
problem with the real physics.

| Project | Shows | Assumptions and limits |
|---|---|---|
| `optimization/a-acoustic-focal-array.resonant` | eight elements: phases and amplitudes for the largest pressure at an off-axis focus, against the uniform excitation | free field; the optimum is the coherent sum, reached to 0.1 %; a field plane allows full-wave re-evaluation (2D) |
| `optimization/b-acoustic-reflector.resonant` | a parabolic reflector's focal length swept for a target 80 mm from the vertex; candidates compared | Kirchhoff model (one reflection); optimum within 5 % of the mirror equation |
| `optimization/c-mechanical-vibration-suppression.resonant` | two actuators on a plate at 120 Hz: a quiet point and a point with 2 um motion | finite elements, loss factor 0.02; the linear inverse solution is available |
| `optimization/d-mechanical-resonator.resonant` | a cantilever tuned to 25 Hz by length, stock thickness and library material, at most 250 g | screened with Euler-Bernoulli, validated with finite elements; illustrative material values |
| `optimization/e-electromagnetic-beam-steering.resonant` | a 4 x 4 planar array steered to (30, 45) degrees by its phases | array factor, isotropic elements, no coupling |
| `optimization/f-sidelobe-reduction.resonant` | a symmetric taper (derived variables) of a 10-element array: directivity against sidelobes, Pareto | array factor; compare with Dolph-Chebyshev |
| `optimization/g-robust-design.resonant` | a taper for the worst directivity toward 30 degrees over 0.9 - 1.1 GHz, against the 1 GHz optimum | the band is a stated requirement, sampled at eight common frequencies |
| `optimization/read_run.py` | reads an exported run record (standard library; `--plot` with NumPy and matplotlib) | |
| `multiphysics/a-microwave-heated-slab.resonant` | a physical system: a lossy slab heated by a 2.45 GHz plane wave, its loss rising with temperature, its expansion feeding back into the wave solution, iterated with an energy ledger (`multiphysics/README.md`) | layered 1D models of the Coupled Energy–Matter Engine; illustrative material values; run through the automation API (no panel yet) |
| `functional/a-microwave-cavity.resonant` … `functional/f-coupled-slab.resonant` | six functional structures: a microwave cavity, a two-mirror optical resonator, an acoustic horn, a mechanical resonator, a photonic unit cell and a coupled slab, each with parameters, objectives and constraints (`functional/README.md`) | the models of each analysis; open the Functional Structures panel |
| `resonance/a-helmholtz-resonator.resonant` … `resonance/d-heated-cavity.resonant` | a Helmholtz resonator, coupled cantilevers, coupled Helmholtz resonators and a heated cavity through the coupled engine (`resonance/README.md`) | lumped and closed-form models; open the Functional Structures panel |
| `metamaterials/a-mass-in-mass-lattice.resonant` … `metamaterials/e-layered-thermal.resonant` | a locally resonant lattice, a duct with Helmholtz resonators, a layered dielectric, a split-ring array and a copper-epoxy laminate: effective properties derived from the structure, with their validity (`metamaterials/README.md`) | one-dimensional and circuit models; open the Functional Structures panel; effective materials can be saved into the library |
| `multiscale/a-crystalline-argon.resonant` … `multiscale/e-cross-scale-investigation.resonant` | an argon crystal to a cantilever, a graphite polycrystal, porous alumina, a porous dielectric in a layered metamaterial, and a heated pad whose RVE is iterated at its temperature (`multiscale/README.md`) | Lennard-Jones and voxel-RVE models; open the Material Scales panel; derived values keep their lineage |
| `materialstate/a-evolving-conductor.resonant` … `materialstate/f-hysteretic-core.resonant` | an evolving conductor, a constrained heated plate, a plastic steel bar, a damaged cantilever whose frequencies fall, a melting paraffin wall and a hysteretic ferrite core (`materialstate/README.md`) | constitutive models with state and time-stepped evolutions; open the Material State panel; a run's record can be committed to a material with its lineage |
| `accounting/a-closed-collision.resonant` … `accounting/h-torsional-flywheels.resonant` | a collision, a damped driven oscillator, a resonator's Q and ring-down, coupled resonators, microwave absorption through the coupled engine, radiation pressure, the energy of evolving materials and torsional flywheels (`accounting/README.md`) | where the energy and momentum go, for any system boundary; open the Accounting panel |
