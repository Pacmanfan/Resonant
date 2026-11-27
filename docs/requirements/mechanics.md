# Mechanical Waves, Structural Vibration and Elasticity

**Owner of:** `RES-MECH-001..018`
**Source:** REQ-CHAT-20261126-006 ("Prompt 10")
**Refines:** `product.md` RES-PRODUCT-001, -007, -009, -010, -012
**Decisions:** ADR-000205, ADR-000210
**Documents:** `docs/mechanics/`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-MECH-001 — A separate mechanical domain

Mechanics is a core module (`src/core/mechanics`) with its own equations,
materials, boundary conditions and solvers, without Qt or OpenGL and
without dependence on the acoustic module. Shared signal definitions live
in `src/core/dsp`; coupling between domains in `src/core/coupling`.
*Verification:* suite `layering`; `Mechanical*` cases run without a GUI.

### RES-MECH-002 — Elastic material model with provenance

Isotropic linear elasticity from density, Young's modulus and Poisson's
ratio (optional loss factor), with derived constants; missing values are
named; provenance is carried into results. The example library includes
the common engineering metals, polymers and a ceramic, labelled
illustrative; materials that need anisotropic models are not offered as
isotropic composites.
*Verification:* `MechanicalMaterial.*`, `MaterialLibrary` count test, `MechanicalExamples.A_*`.

### RES-MECH-003 — Explicit structural idealization

An object becomes a beam, plate, membrane or solid only by the user's
choice; the representation is explained, its assumptions stated, and
stretched assumptions warned about; geometry without an idealization is
refused with the reason.
*Verification:* `MechanicalStudy.IdealizationsFollowTheGeometryAndExplainThemselves`,
`MechanicsUiTest.ObjectsBecomeStructures*`.

### RES-MECH-004 — Element formulations

Euler-Bernoulli 3D frame, Kirchhoff ACM plate, bilinear membrane and
trilinear hexahedral solid elements with consistent mass, sparse assembly
and supports appropriate to each.
*Verification:* `MechanicalFrame.*`, `MechanicalPlate.*`, `MechanicalMembrane.*`, `MechanicalSolid.*`.

### RES-MECH-005 — Static analysis

Linear static deformation under actuator forces, refusal of structures
that are not held, stresses where the formulation yields them.
*Verification:* `MechanicalFrame.Cantilever*`, `MechanicalFrame.AStructureWithoutSupports*`,
`MechanicalSolid.PatchTest*`, `MechanicsUiTest.TransientAndStaticResults`.

### RES-MECH-006 — Modal analysis

Natural frequencies, mass-normalized mode shapes, effective mass fractions
and residuals from the generalized eigenproblem, by a dense solver or
shift-invert Lanczos; rigid-body modes handled.
*Verification:* `MechanicalFrame.BendingFrequencies*`, `MechanicalEigensolver.*`,
`MechanicalExamples.A_*`, `MechanicalExamples.B_*`.

### RES-MECH-007 — Forced response and damping

Harmonic sweeps by direct solution or modal superposition with Rayleigh,
loss-factor or modal damping, the model always stated, invalid
combinations refused.
*Verification:* `MechanicalHarmonic.*`, `MechanicalExamples.C_*`.

### RES-MECH-008 — Transient analysis

Newmark average-acceleration integration with actuator signals and true
delays, sensor and snapshot recording, energy balance, stated accuracy.
*Verification:* `MechanicalTransient.*`, `MechanicalExamples.D_*`.

### RES-MECH-009 — Mechanical field quantities

Displacement, velocity and acceleration; stresses for beams, plates and
solids; energies; vector components and magnitudes distinguished; a
quantity a result does not have is not offered.
*Verification:* `MechanicsUiTest.ModalResultIsShown*`, `MechanicsUiTest.TransientAndStaticResults`.

### RES-MECH-010 — Mechanical wave propagation

Longitudinal and flexural waves propagate with their theoretical speeds
and dispersion and reflect at supports.
*Verification:* `MechanicalTransient.LongitudinalPulse*`,
`MechanicalTransient.FlexuralWaves*`, `MechanicalExamples.E_*`.

### RES-MECH-011 — Actuator, sensor and vibration target objects

Actuators (point or distributed force, amplitude, phase, delay, signal,
active state, target structure, named device whose response is not
modelled), sensors (direction, calibration fields) and targets (wanted
displacement, quiet points) are scene objects, attached with reported
distances and lost force components.
*Verification:* `MechanicalStudy.Attachments*`, `MechanicalPersistence.*`,
`MechanicsUiTest.ObjectsBecomeStructures*`.

### RES-MECH-012 — Vibration targeting

Actuator inputs for wanted displacements and quiet points at a frequency by
regularized least squares, with residual and achieved values reported,
applied as one undoable step.
*Verification:* `MechanicalExamples.F_*`, `MechanicsUiTest.TargetingSolvesAppliesAndUndoes`.

### RES-MECH-013 — Mechanical wave manipulators

Horn geometry analysed as a member of varying section, reproducing the
stepped horn's resonance and amplification.
*Verification:* `MechanicalExamples.H_*`.

### RES-MECH-014 — Periodic structures

Longitudinal Bloch band structure of a periodic rod validated against the
dispersion relation, and attenuation of a finite periodic rod inside its
stop band.
*Verification:* `MechanicalExamples.G_*`, `MechanicsUiTest.BandStructure*`.

### RES-MECH-015 — One-way acoustic coupling

Sound radiated by a vibrating plate or membrane into the acoustic domain by
the Rayleigh integral, with its assumptions stated.
*Verification:* `MechanicalCoupling.*`, `MechanicsUiTest.BandStructureAndSoundRadiatedByAPlate`.

### RES-MECH-016 — Measured data

Measured frequency responses imported from CSV with sensor metadata,
stored in the project and compared with simulation; no hardware acquisition.
*Verification:* `MechanicalMeasurement.*`, `MechanicsUiTest.FrequencyResponseIsPlotted*`.

### RES-MECH-017 — Display separates magnification from physics

Deformed results are drawn with a visual magnification reported separately
from the physical displacement (or stated arbitrary for modes), with the
undeformed outline, animation and display changes without recalculation.
*Verification:* `MechanicsUiTest.ModalResultIsShown*`, `MechanicalExamples.A_*`.

### RES-MECH-018 — Resources, reproducibility, validation and examples

Preflight of mesh size and memory with refusal instead of coarsening;
background jobs with progress and cancellation; results with complete
metadata; validation recorded in `docs/mechanics/MECHANICAL_VALIDATION.md`;
eight example projects generated and checked by the tests.
*Verification:* `MechanicalStudy.*`, `MechanicalExamples.*`.
