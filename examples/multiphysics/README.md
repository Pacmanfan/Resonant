# Multiphysics examples

Physical systems of the Coupled Energy–Matter Engine (REQ-CHAT-20261126-016,
`docs/architecture/COUPLED_ENGINE_ARCHITECTURE.md`). A project stores its
systems in the extension `multiphysics`; run one with
`MultiphysicsService.StartCoupledSimulation` (Python: `resonant_api`), and
read the result with `GetCoupledResult`. There is no panel for them yet.

| Project | Shows | Assumptions and limits |
|---|---|---|
| `a-microwave-heated-slab.resonant` | a 10 mm lossy dielectric slab in a 2.45 GHz plane wave of 20 kW/m^2, cooled by convection; its loss tangent rises with temperature, it expands, and the thicker slab changes the wave solution, iterated to a fixed point with an energy ledger | layered plane wave at normal incidence; 1D conduction (lateral faces adiabatic); linear thermoelasticity in the reference configuration; every material value and the loss law are illustrative |
