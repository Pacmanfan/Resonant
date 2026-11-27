# Electromagnetic Waves, Antennas, Arrays and RF Propagation

**Owner of:** `RES-EM-001..024`
**Source:** REQ-CHAT-20261126-007 ("Prompt 11")
**Refines:** `product.md` RES-PRODUCT-001, -007, -009, -010, -012
**Decisions:** ADR-000205, ADR-000209, ADR-000211
**Documents:** `docs/electromagnetics/`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

Every requirement names the model a number comes from. Four kinds of
result exist and are never presented as one another: **analytical**
(closed-form solutions of Maxwell's equations for an assumed current or
geometry), **array factor** (element pattern times array factor, no
mutual coupling), **full-wave numerical** (the Yee solver) and **imported
measurement**. Design formulas are labelled *estimates*.

### RES-EM-001 — A separate electromagnetic domain

Electromagnetics is a core module (`src/core/electromagnetics`) with its
own equations, materials, sources, solvers and results, without Qt or
OpenGL and without dependence on the acoustic or mechanical modules.
Simulation bodies shared with the acoustic full-wave setup live in
`src/core/geometry` (`SolidModel`).
*Verification:* suite `layering`; `Em*` cases run without a GUI.

### RES-EM-002 — Conventions

SI units; time dependence `e^{+j omega t}`; phasors are peak values;
spherical angles theta from +Z and phi from +X in an object's local frame;
constants mu0 = 1.25663706212e-6 H/m (CODATA 2018), c, eps0 and eta0
derived from it. Every result states its frequency and model.
*Verification:* `EmFoundations.*`.

### RES-EM-003 — Electromagnetic materials

Relative permittivity, relative permeability, conductivity and loss
tangent, read from the material record at the frequency of use (tabulated
values are interpolated), with the validity range checked and the
provenance carried. Complex permittivity eps0 er (1 - j tan d) - j sigma /
omega; classification as good conductor (loss ratio > 100), lossy
dielectric or dielectric; wavenumber, impedance, skin depth and surface
resistance. Materials without electromagnetic data are reported, never
given invented values; example values are labelled illustrative.
*Verification:* `EmFoundations.ConstantsSineCosineIntegralsAndMaterials`.

### RES-EM-004 — Ideal sources

Electric dipole (current moment I l, A m), magnetic dipole (I A, A m^2)
and plane wave (peak |E|, V/m), with frequency, phase, delay and an
active flag; dipoles along local +Z, a plane wave travelling along local
+Z with E along local +X. Their exact fields are evaluated analytically.
*Verification:* `EmAntennas.HertzianDipoleRadiatesTheClassicalPowerAtEveryDistance`.

### RES-EM-005 — Antennas

Antenna objects are geometry plus an antenna component (frequency, peak
feed current, feed phase, delay, reference impedance). Dipole, monopole
(over an infinite ground plane) and small loop have analytical models;
patch, horn and helix give labelled design estimates only. The support
level is declared per kind and shown.
*Verification:* `EmExamples.A_*`, `EmExamples.B_*`, `EmUiTest.MenuAddsADipole*`.

### RES-EM-006 — Feed impedance and matching

Input impedance of dipoles and monopoles from the induced-EMF method
(sinusoidal current, Balanis 8-60), referred to the feed; small-loop
radiation resistance and inductance. Reflection coefficient, return loss,
VSWR and mismatch efficiency against the antenna's reference impedance.
Impedance sweeps over a frequency range. Points near anti-resonance,
where the model's feed current vanishes, are flagged invalid.
*Verification:* `EmAntennas.ThinDipoleImpedanceAndPatternMatchTheClassicalValues`, `EmAntennas.MatchingQuantities`.

### RES-EM-007 — Directivity, efficiency, gain and realized gain

Directivity from the model's pattern; radiation efficiency from the
assigned conductor's surface resistance; gain = directivity x efficiency;
realized gain = gain x mismatch efficiency. The three are always shown
separately. Polarization and far-field distance (largest of 2 D^2 /
lambda, 3 lambda and 3 D) are reported.
*Verification:* `EmExamples.A_HalfWaveDipoleImpedancePatternGainAndEstimates`.

### RES-EM-008 — Radiation patterns

Normalized far-field patterns over (theta, phi) for antennas of analytical
kinds and arrays, with the directivity integrated from the samples;
two-dimensional cuts (dB or linear) and a three-dimensional surface in the
viewport. Patterns are labelled normalized, not absolute.
*Verification:* `EmExamples.A_*`, `EmExamples.B_*`, `EmUiTest.ExampleA_*`.

### RES-EM-009 — Near field and far field

The analytical fields are the exact fields of the assumed currents at any
distance (near and far), not far-field approximations; the far-field
distance is reported; fields on field regions and at probe points; points
on a source or behind a ground plane are excluded, not given values.
*Verification:* `EmExamples.E_NearFieldOfTheDipoleCarriesExactlyItsRadiatedPower`, `EmUiTest.ExamplesEAndF_*`.

### RES-EM-010 — Parametric arrays

Linear, planar, circular and custom layouts; element patterns isotropic,
short dipole, half-wave dipole and cosine (half space); per-element
amplitude, phase, delay and on/off kept by element index.
*Verification:* `EmArrays.*`, `EmUiTest.ExamplesCAndD_*`.

### RES-EM-011 — Array-factor analysis

Array factor sum w_n exp(j k r.r_n - j omega tau_n) times the element
pattern; main lobe, directivity, half-power beamwidths, highest sidelobe
and other maxima within 1 dB. Always labelled as the array-factor model
without mutual coupling.
*Verification:* `EmArrays.UniformLinearArrayHasTheTextbookDirectivityBeamwidthAndSidelobes`, `EmExamples.C_*`.

### RES-EM-012 — Beam steering

Phase steering and true-time-delay steering to a direction; phase
steering squints with frequency, time delay does not.
*Verification:* `EmArrays.PhaseSteeringPointsTheBeamAndSquintsAtOtherFrequenciesTimeDelayDoesNot`, `EmExamples.D_*`.

### RES-EM-013 — Grating lobes

Grating lobes predicted from the reciprocal lattice for linear and
rectangular lattices and reported as warnings; other near-equal maxima
reported for all layouts.
*Verification:* `EmArrays.GratingLobesArePredictedAndFound`, `EmUiTest.ExamplesCAndD_*`.

### RES-EM-014 — Tapers and beam synthesis

Dolph-Chebyshev amplitudes for a sidelobe level; least-squares synthesis
(Tikhonov regularization, optional amplitude limit) of weights for wanted
beams and nulls, with the achieved levels reported; results applied to
the array only on request and undoably.
*Verification:* `EmArrays.ChebyshevWeightsGiveEqualSidelobesAndSynthesisPlacesNulls`.

### RES-EM-015 — Transmission lines and matching networks

Characteristic impedance and effective permittivity of coaxial line,
microstrip (Hammerstad-Jensen), stripline and coplanar waveguide (exact
elliptic-integral forms); input impedance of a loaded line; quarter-wave
transformers; L-network matches; conductor and dielectric losses.
*Verification:* `EmLines.*`, `EmUiTest.LineCalculatorGivesTheCoaxialImpedance`.

### RES-EM-016 — Waveguides and cavities

Modes and cut-offs of rectangular, circular and parallel-plate guides;
propagation constant, guide wavelength, wave impedance, phase and group
velocity, evanescent decay; rectangular cavity resonances; the TE10 field.
Guide and cavity objects (rectangular, circular, coaxial, cavity) report
their modes at a frequency of interest; walls are perfect conductors and
the filling is the object's material.
*Verification:* `EmGuides.*`, `EmExamples.F_*`.

### RES-EM-017 — A full-wave numerical solver

The Yee finite-difference time-domain method: three-dimensional, or
two-dimensional TM on a plane; split-field perfectly matched layer;
perfect-conductor, periodic boundaries; materials with permittivity,
permeability and conductivity; good conductors as perfect conductors;
thin wires as zeroed edges; the stability limit enforced; dipoles,
monopoles and electric-dipole sources from a scene; a plan with grid,
memory, steps and estimated time before running; refusal (never silent
coarsening) when limits are exceeded; a background job that can be
cancelled; the steady-state field extracted into an ordinary field result.
*Verification:* `EmYee.*`, `EmExamples.G_*`, `EmFullWaveScene.*`, `EmUiTest.ExampleG_*`.

### RES-EM-018 — Field visualization

Ex, Ey, Ez, |E|, Hx, Hy, Hz, |H| and power density |Re S| on field regions,
as magnitude, phase, instantaneous value (animated) or level in dB over a
40 dB range; normalized to the result or on an absolute scale, always
stated; results labelled analytical or full-wave numerical; visualization
separate from the solvers.
*Verification:* `EmUiTest.ExamplesEAndF_*`, `EmUiTest.ExampleG_*`.

### RES-EM-019 — Measured data

One-port Touchstone (.s1p; RI, MA, DB; Hz to GHz) import into the project
and export of calculated impedance sweeps; measured data shown beside, and
labelled apart from, the model. No RF hardware is driven.
*Verification:* `EmScene.TouchstoneRoundTripSettingsAndPersistence`, `EmUiTest.ExampleA_*`.

### RES-EM-020 — Reproducibility

Every field result carries its solver id and version, configuration
(grid, time step, settings, represented objects, warnings, backend),
assumptions and project id; every run is recorded in the project;
electromagnetic settings persist in the project file.
*Verification:* `EmScene.TouchstoneRoundTripSettingsAndPersistence`, `EmUiTest.ExampleG_*`.

### RES-EM-021 — Validation

Analytical models and the Yee solver are validated against closed forms
and independent series solutions, with refinement studies for the
numerical solver; documented in `docs/electromagnetics/ELECTROMAGNETIC_VALIDATION.md`.
*Verification:* suite `core` (`tests/core/test_electromagnetics.cpp`, `test_electromagnetics_scene.cpp`).

### RES-EM-022 — Example projects

Seven example projects (`examples/electromagnetics/`): half-wave dipole,
quarter-wave monopole, Chebyshev linear array, steered planar array, near
field, WR-90 waveguide, and full-wave scattering by a copper cylinder with
a refinement study; each checked by a test against its calculation.
*Verification:* `EmExamples.*`.

### RES-EM-023 — Optimization interfaces

Electromagnetic inverse design available now: resonant dipole length,
Chebyshev tapers, least-squares beam and null synthesis, L-network and
quarter-wave matches; each states its model. General optimization of
geometry is the subject of the optimization milestone (REQ-CHAT-20261126-009).
*Verification:* `EmAntennas.*`, `EmArrays.*`, `EmLines.*`.

### RES-EM-024 — Compute backend

A correct CPU reference first: double precision, one thread, the backend
recorded in every full-wave result. No GPU backend is claimed.
*Verification:* metadata checked in `EmUiTest.ExampleG_*`; `docs/electromagnetics/PERFORMANCE_AND_GPU.md`.
