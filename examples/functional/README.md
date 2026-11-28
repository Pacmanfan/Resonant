# Functional structure examples

Engineered objects whose behavior an analysis computes (REQ-CHAT-20261126-017,
`docs/architecture/FUNCTIONAL_STRUCTURE_ARCHITECTURE.md`). Each project holds
one structure in the extension `functional`; open it and use the **Functional
Structures** panel (Panels menu), or `FunctionalStructureService` from Python
or the API. Every metric is computed by the analysis's model; material values
are the illustrative example library unless stated.

| Project | Structure and analysis | Parameters | Assumptions and limits |
|---|---|---|---|
| `a-microwave-cavity.resonant` | a copper WR-90 cavity 30 mm long, PTFE-filled, critically coupled: modes, fields, wall and fill Q, bandwidth, port response, thermal detuning; closed form or full-wave (Yee) | a, b, d (the cavity), temperature (free), aspect (derived) | perturbational wall loss; lumped port; Yee walls perfectly conducting; illustrative PTFE and copper expansion values |
| `b-optical-resonator.resonant` | two 1-inch spherical mirrors (R = 0.5 m) 0.25 m apart at 1064 nm: stability, waist, mode spacing, finesse, Q | L (mirror 2's position), R1, R2, reflectivity, wavelength; g1, g2 (derived) | paraxial Gaussian beams; aligned mirrors; mirror orientation not checked |
| `c-acoustic-horn.resonant` | an exponential horn, 2-inch throat to 300 mm mouth, 0.5 m: impedance transformation, transmission, radiated power, directivity | length, mouth, profile, wall (manufacturing constraint: 2 mm for injection molding), frequency | Webster plane waves, lossless walls, baffled mouth |
| `d-mechanical-resonator.resonant` | an aluminium cantilever 500 x 30 x 5 mm: natural frequencies against Euler-Bernoulli, tuning to 25 Hz | length, thickness, width; slenderness (derived) | linear elasticity; finite elements or closed form |
| `e-photonic-unit-cell.resonant` | a quarter-wave alumina / polycarbonate cell at 10 GHz: band gap, ten-cell transmission, homogenized index | d1, d2 (the layer boxes' heights); period (derived) | normal incidence, lossless layers |
| `f-coupled-slab.resonant` | the microwave-heated slab of the coupled engine with its thickness from the box: absorptance, temperature, stress, energy balance | thickness (the box height) | the coupled engine's layered models; illustrative material and loss law |
