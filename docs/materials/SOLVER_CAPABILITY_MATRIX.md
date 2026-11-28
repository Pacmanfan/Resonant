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
| layered-solid | 3D layered solid, 20-node hexahedra (`mechanics/LayeredSolid`) | mechanical | isotropic, graded (sublayers), composition | perfect, perfect-bonding, finite-stiffness, sliding, frictional-contact | - | E, nu, rho |
| layered-solid-thermal | 3D layered conduction, 20-node hexahedra | thermal | isotropic, graded (sublayers), composition | perfect, thermal contact resistance | - | k |

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
| layered-solid | linear, small displacements; isotropic layers (an in-plane orthotropic set lacks the 3D constants); no damage evolution; debonded regions without contact; holonomic friction |
| layered-solid-thermal | steady, isotropic, no radiation; faces without a condition insulated |

What none of them represents: cohesive damage evolution, dielectric,
charge-transfer, diffusive and reactive interfaces; out-of-plane orientations
of orthotropic layers; periodic and inclusion patterns. Finite-stiffness,
sliding and frictional interfaces, and curved layered solids, are represented
by the 3D layered solid only (REQ-CHAT-20261128-004).
