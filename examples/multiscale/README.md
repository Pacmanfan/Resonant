# Multiscale examples

Materials from atoms and microstructures to engineering values
(REQ-CHAT-20261126-020, `docs/materials/MULTISCALE_ARCHITECTURE.md`). Open one
and use **Panels > Material Scales**: select a representation, **Run** a
derivation, **Assign** it to a material, and trace it in **Lineage**. Or use
`MultiscaleService` from Python or the API. Values are illustrative; RVEs are
synthetic (generated, not measured).

| Project | Workflow | Limits |
|---|---|---|
| `a-crystalline-argon.resonant` | FCC argon with the Lennard-Jones force field → lattice statics (a = 5.25 Å, C11 4.40 GPa, C12 = C44 2.52 GPa) → E 4.32 GPa, ν 0.271, ρ 1834 kg/m³ → assign to "Solid argon (derived)" → the cantilever's first bending frequency (Functional Structures panel). Before the derivation the cantilever analysis fails: the material has no elastic data. NVE dynamics is a check: at 1e-14 s its drift is flagged, at 5e-15 s it is not | 0 K, classical, pair potential (Cauchy relation; real argon violates it); solid argon exists below 84 K |
| `b-polycrystal.resonant` | 40 Voronoi grains of uniaxial graphite (1700 / 8 W/(m K)): random c axes give about 900 W/(m K), near the self-consistent estimate (858); c axes within 15 degrees of z give 1470 along and 174 across | one RVE size; with realizations the spread (2.4 %) is reported, above the 2 % representativeness tolerance |
| `c-porous-composite.resonant` | alumina with 20 % spherical pores: 21.5 ± 0.05 W/(m K), near Maxwell-Garnett (21.8) and below the rule of mixtures (24.0); the same porosity as layers: 0.13 across, 24.0 along | no convection or radiation in the pores |
| `d-multiscale-metamaterial.resonant` | alumina with 30 % pores → static permittivity 6.43 → the porous layer of a layered dielectric (Functional Structures panel: eps_par, eps_perp) | quasi-static: pores far below the wavelength |
| `e-cross-scale-investigation.resonant` | a heated filled-epoxy pad (8 W): its local temperature from the coupled engine sets the RVE's constituents (temperature laws); iterating converges in 4 steps to 1.798 W/(m K) at 306.9 K (from a series-bound guess of 0.307); assign it and trace it back to the RVE | one temperature per region; illustrative temperature laws |
