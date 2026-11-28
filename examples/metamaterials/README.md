# Metamaterial examples

Organized structures whose effective behavior an analysis derives
(REQ-CHAT-20261126-019, `docs/materials/METAMATERIAL_ARCHITECTURE.md`). Open one
and use the **Functional Structures** panel (Analyze, then **Save effective
material** where the analysis reports one), or `FunctionalStructureService`
from Python or the API. Material values are illustrative.

| Project | Structure and analysis | Parameters | Assumptions and limits |
|---|---|---|---|
| `a-mass-in-mass-lattice.resonant` | 20 mm steel blocks carrying 8 mm lead cores on a soft coating: a gap at 298–353 Hz, far below the chain's zone edge (1.8 kHz), where the cell's effective mass is negative; a 30-cell chain decays at the Bloch rate | block size (the period), core radius; link and coating stiffness, coating loss; the masses derived from the geometry | lumped one-dimensional lattice; the dynamic mass is reported, not stored |
| `b-resonator-duct.resonant` | a 50 mm duct with a 63 cm^3 Helmholtz resonator every 50 mm: sound slows to 268 m/s below the 466 Hz resonance, the array blocks 459–594 Hz (negative bulk modulus), a Bragg gap opens at 3.43 kHz | cavity length, neck radius, spacing, duct radius | plane waves; lumped resonators; effective sound speed stored up to half the resonance |
| `c-layered-anisotropy.resonant` | ten periods of 1 mm alumina and 2 mm PTFE: eps_par 4.4, eps_perp 2.82; the uniaxial slab against the explicit layers at 45 degrees (TE, TM) and by retrieval | the two thicknesses, the angle; alumina fraction (derived) | valid up to 3.3 GHz (period a tenth of the wavelength in alumina); one principal component per saved material |
| `d-split-ring-array.resonant` | copper split rings (2 mm radius, 50 um gap) at 5.5 mm pitch on a 10.2 substrate: a magnetic resonance at 7.4 GHz, Re mu < 0 up to 9.7 GHz | ring radius, gap, strip width, pitch, substrate permittivity | **a quasi-static circuit model**, not full-wave; the stored permeability covers 0.05–0.8 f0 |
| `e-layered-thermal.resonant` | a copper-epoxy laminate, 0.5 / 0.1 mm: 333 W/(m K) along, 1.2 W/(m K) across; checked by finite volumes; with an explicit coupled-engine system (`sys-laminate-explicit`) to compare a region of the saved effective material against | the two thicknesses | steady conduction, perfect contact; isotropic library values |
