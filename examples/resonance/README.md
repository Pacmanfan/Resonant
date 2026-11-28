# Resonance examples

Resonant structures, energy storage and coupled oscillators
(REQ-CHAT-20261126-018, `docs/physics/RESONANCE_ARCHITECTURE.md`). Open one
and use the **Functional Structures** panel, or `FunctionalStructureService`
from Python or the API. Material values are the example library's unless stated.

| Project | Structure and analysis | Parameters | Assumptions and limits |
|---|---|---|---|
| `a-helmholtz-resonator.resonant` | a 98 cm^3 printed cavity with a 10 mm neck: f0, end corrections, radiation and viscous Q, pressure gain; lumped or plane-wave model | cavity length and radius, neck radius and length; volume (derived) | lumped elements; no thermal loss in the cavity |
| `b-coupled-cantilevers.resonant` | two aluminium cantilevers 200 x 20 x 2 mm joined at the tips by a 10 N/m spring: splitting, energy exchange, detuning by length | the two lengths, the coupling stiffness, the loss factor | first bending modes as tip-referred oscillators; a massless spring |
| `c-coupled-helmholtz.resonant` | an open Helmholtz resonator joined by a 6 mm connector to a closed one: two coupled acoustic modes and the cavity pressures | cavity 2's length, the connector's radius and length | lumped network; losses at resonator 1's frequency |
| `d-heated-cavity.resonant` | a PTFE-filled copper cavity driven at its cold resonance with 1 W through the Coupled Energy-Matter Engine: heating, falling permittivity and expanding walls detune it | the cavity's a and d | lumped thermal model (uniform temperature, 0.05 W/K); illustrative permittivity law and copper expansion |
