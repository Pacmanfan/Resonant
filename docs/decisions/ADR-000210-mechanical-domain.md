# ADR-000210: Mechanical domain — own finite-element core, explicit idealizations, own eigensolver, Newmark integration, one-way coupling module

**Status:** ACCEPTED
**Date:** 2026-11-26
**Type:** architecture
**Requirements:** `docs/requirements/mechanics.md` RES-MECH-001..018
**Request:** REQ-CHAT-20261126-006
**Builds on:** ADR-000205, ADR-000207, ADR-000209

## Decisions

1. **A finite-element core written for this module,** on Eigen's sparse
   matrices, rather than an external structural code: no such library is
   installed or vendorable under the project's dependency rule, and the
   element set needed (beam, plate, membrane, hexahedron) is small and fully
   validated against closed forms.
2. **Explicit idealization.** A structure component on a geometry object
   states beam, plate, membrane or solid; nothing is converted
   automatically. Structured meshes only (boxes, cylinders, planes, rods).
3. **Element choices:** 3D Euler-Bernoulli frame; ACM rectangular Kirchhoff
   plate (simple, converges on rectangular meshes); bilinear membrane;
   fully integrated trilinear hexahedron, with its bending stiffness stated
   instead of hidden. Consistent mass throughout.
4. **Eigensolver:** dense for small problems, shift-invert Lanczos with full
   re-orthogonalization for large ones, a negative shift for free
   structures, and a backward error per pair.
5. **Damping models are explicit and limited to where they are valid:**
   Rayleigh everywhere, loss factor only in the frequency domain, modal
   ratio only with modal superposition.
6. **Newmark average acceleration** for transients: unconditionally stable,
   energy-conserving, period error stated.
7. **Signals moved to `src/core/dsp`** (namespace `resonant::dsp`;
   `signals` collides with a Qt macro) so that both domains use them without
   depending on each other.
8. **Coupling is its own module** (`src/core/coupling`), the only code that
   includes both domains; one-way Rayleigh radiation first.
9. **Results keep the study they came from** (model, attachments,
   assumptions) and complete metadata; the display builds a deformed surface
   in a new render layer and replaces the object's own mesh while shown.
10. **Plan, then refuse:** oversized meshes are refused before they are built.

## Deviations from the request

* No shells, frames of several members, trusses, layered or composite
  structures, orthotropic or anisotropic materials, viscoelasticity,
  frequency-dependent moduli, prestress other than membrane tension.
* No prescribed displacement or velocity excitation; no physical actuator
  response models.
* No mesh generation for arbitrary geometry, no Gmsh, no mesh refinement
  tools beyond the element size.
* No spectral elements, reduced-order models, BEM radiation, external
  solvers, GPU, OpenMP or SIMD.
* Stress and strain histories in transients are not recorded; strain is not
  displayed.
* Mechanical focusing: actuator-input targeting only; structural parameter
  optimization is request 009.
* Phononic analysis: 1D longitudinal Bloch bands of periodic rods only.
* Coupling: one-way, plates and membranes in a baffle only.
* No comparison with an independent finite-element code.

## Rejected

* Reusing the acoustic FDTD grid for solids — a scalar pressure scheme has
  no shear waves and no structural boundary conditions.
* Mindlin plate elements in this milestone — more robust for thick plates
  but need locking treatment (MITC); the Kirchhoff ACM element is exact
  enough for thin plates and simpler to validate.
* Spectra (Lanczos library) — not installed; a self-contained solver with
  stated residuals instead.
