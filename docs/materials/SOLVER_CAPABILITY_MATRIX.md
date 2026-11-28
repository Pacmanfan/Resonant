# Solver Capability Matrix

**Status:** current (REQ-CHAT-20261127-001, RES-FAM-010). The declarations are in
`coupling/MatterAnalyses` (`matterCapabilities()`, also through
`MatterService.GetMatterCapabilities`). A stack is checked against the declaration
before it runs. What a declaration does not cover is refused with the reason. A
field that a solver's input could hold is not thereby supported.

| Analysis | Solver | Physics | Material models | Interfaces | Patterns | Required properties |
|---|---|---|---|---|---|---|
| laminate | lamination theory (`mechanics/Laminate`) | mechanical | isotropic, orthotropic in plane, graded, composition | perfect, perfect-bonding, cohesive (bonded) | segmented (counted whole) | E, nu, rho |
| modes | lamination + closed-form modes | mechanical | as laminate | as laminate | segmented (counted whole) | E, nu, rho |
| harvester | lumped cantilever (`mechanics/PiezoelectricBeam`) | electromechanical | isotropic, orthotropic in plane, piezoelectric, graded (passive layers), composition | as laminate | segmented electrodes | E, nu, rho; d31, eps33T, poling |
| thermal | series conduction | thermal | isotropic, principal conductivities, graded, composition | perfect, thermal contact resistance | - | k |
| electrical | series conduction and capacitance | electrical | isotropic, graded, composition | perfect, electrical contact resistance | segmented (per covered area) | sigma, eps_r |
| electromagnetic | `em::LayeredSolution` | electromagnetic | isotropic, graded, composition | perfect | - | eps_r, tan delta, sigma, mu_r |
| coupled-engine | the layered slab of the engine | thermal, electromagnetic | isotropic, through-thickness conductivity, graded, composition | perfect, thermal contact resistance | - | k, rho, c |

Every analysis also declares its dimensionality, scales, couplings,
nonlinearities, limitations, inputs and outputs. The main limitations:

| Analysis | Limitations |
|---|---|
| laminate | thin plies in plane stress; no transverse shear; no edge stresses; linear |
| modes | clamped-free Euler–Bernoulli beams or simply supported specially orthotropic plates; slender, low modes |
| harvester | the first mode (Rayleigh); a resistive load; equipotential electrodes; no graded transduction layer |
| thermal | steady, one-dimensional, no spreading or radiation |
| electrical | ohmic layers; ideal insulators at zero conductivity |
| electromagnetic | normal incidence; laterally infinite isotropic layers |
| coupled-engine | the slab geometry; electromagnetic interfaces perfect; anisotropy only through the thickness |

What none of them represents: finite-stiffness, frictional, dielectric,
charge-transfer, diffusive and reactive interfaces; out-of-plane orientations
of orthotropic layers; periodic and inclusion patterns; shells and solids of
layered bodies.
