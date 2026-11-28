# ADR-000232: Layered three-dimensional solids with explicit interfaces, in the existing structural model

**Status:** ACCEPTED
**Date:** 2026-11-28
**Requirements:** `docs/requirements/mechanics.md` RES-MECH-019..022
**Request:** REQ-CHAT-20261128-004 (Prompt 27, Part III)
**Builds on:** ADR-000205 (structural mechanics), ADR-000228 (functionally architected matter: the missing full-shell capability)

## Context

ADR-000228 recorded that layered bodies could be analysed only as layered plates
(lamination theory), beams and lumped cantilevers. There was no
three-dimensional model of a coated or layered body: curved, of varying
thickness, with material transitions. The matter architecture already declared
interface models (perfect, perfect-bonding, finite-stiffness, cohesive-zone,
frictional-contact, contact resistances), but only perfect and cohesive (as
bonded) interfaces reached a solver. No established finite-element library is
installed (deal.II, MFEM and FEniCS are absent, and installing them needs
administrator rights). The application has its own linear finite-element model
(`mechanics/StructuralModel`) and the static, modal, harmonic and transient
solvers built on it.

## Decision

1. **Extend the existing finite-element model, not a second one.**
   `StructuralModel` gains:
   - a quadratic 20-node serendipity hexahedron (`ElementKind::Solid20`,
     3 x 3 x 3 Gauss points). It represents curved geometry to second order
     and bends thin layers without the trilinear element's shear locking.
   - per-element elasticity matrices (`Constitutive`). An anisotropic, rotated
     or graded element carries its own `C` and density. An invalid matrix (not
     symmetric, not positive definite) is refused, never repaired.
   - zero-thickness interface elements: an 8 + 8-node spring layer between two
     coincident faces. Its normal and shear stiffnesses are scaled per Gauss
     point (debonded points and open contact points carry nothing). It also
     takes a general traction-jump tangent per point, for contact.

   The existing analyses (static, modal, harmonic, transient) work on these
   models unchanged.
2. **A layered-body builder** (`mechanics/LayeredSolid`). A reference surface
   (plane, cylinder, sphere, surface of revolution) carries layers built along
   its normal:
   - isotropic, orthotropic at an angle (axes following the surface) or graded
     (power law, Voigt mixture, element by element);
   - thickness constant or linear in u or v;
   - elements through every layer.

   Seams and poles are merged. Elements are kept right-handed whatever the
   surface's parameterization.
3. **No interface is assumed.** Each interface states its mechanical model:
   - perfect bonding (shared nodes: a declared idealization);
   - finite stiffness;
   - sliding (normal stiffness, no shear);
   - frictional contact (Coulomb with a penalty contact, no tension).

   It also states its thermal model (perfect, or a contact resistance).
   Further properties:
   - Debonded regions are traction-free, without contact; an interpenetration
     is reported.
   - Normal and shear strengths give the quadratic initiation criterion. There
     is no damage evolution, and a cohesive zone is refused.
   - A missing model refuses the analysis that needs it.
4. **Frictional contact** uses Newton's method:
   - Coulomb return mapping at every point (open, stick, or slip at mu p along
     the trial traction);
   - the consistent tangent, held symmetric by not differentiating the
     pressure (quasi-Newton), with a backtracking line search;
   - holonomic friction: total jump, no load history.

   An unconverged iteration is reported as such. A settled state beyond 1 % of
   the body's size is refused, because small-displacement theory no longer
   holds. An earlier secant and active-set scheme was replaced: it cycled near
   the Coulomb limit.
5. **Mechanisms are detected, not guessed.** An unheld body or a freely sliding
   layer shows as a vanishing LDLT pivot (below 10^-12 of the largest) and is
   refused. A thin, stiffly penalized body is ill-conditioned but well-posed;
   it is solved with iterative refinement and a loose residual check.
6. **Steady conduction on the same geometry.** It uses scalar 20-node elements,
   contact conductance 1/R between the faces of a resistance interface,
   temperature, flux and convection conditions, and the heat flow from the
   reactions.
7. **Matter integration through the capability system.** Two analyses,
   `layered-solid` (modal or static) and `layered-solid-thermal`, declare
   their supported material and interface models. `checkStack` refuses the
   rest with reasons:
   - cohesive zones;
   - segmented patterns;
   - in-plane orthotropic sets, which do not give the nine 3D constants.

   The surface, mesh, supports, loads and debonded regions come from the
   analysis options. Graded regions become sublayers. A `sliding` interface
   kind is added. They are reached through the existing matter service and the
   Matter panel.

## Consequences

- **Lower-fidelity references stay.** Lamination theory and the beam and plate
  models are kept as references, and the tests compare against them (a flat
  bilayer within 1.5 % of lamination theory; the 3D modes between the
  narrow-beam and wide-plate predictions). They are not called equivalent.
- **Validated against analytical solutions** (`LayeredSolid.*`):
  - Timoshenko cantilevers and beam modes;
  - Lamé cylinders and spheres, a full ring (seam merging), a revolved profile;
  - a compound cylinder with a spring interface;
  - series interface compliances (exact), sliding without shear, Coulomb stick
    and slip;
  - debonding bounded by bonded and sliding;
  - the exact initiation index;
  - a tapered beam, a graded profile's bending stiffness, orthotropic axes;
  - series and logarithmic conduction with contact resistances.
- **Limits:**
  - linear elasticity and small displacements;
  - no damage evolution;
  - no contact in debonded regions;
  - holonomic friction;
  - isotropic conduction;
  - at most 60000 elements.
