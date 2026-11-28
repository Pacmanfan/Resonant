# Energy Transformation by Geometry

**Status:** the analyses below are implemented (RES-FUNC-005..010, RES-FUNC-013);
force and momentum metrics are not.

Geometry transforms energy only through the physics of a domain; each
category of the request is computed by a domain model, never by a universal
equation.

| Category | What the geometry does | Analysis and metrics | Not covered |
|---|---|---|---|
| Propagation and transmission | a flaring duct transforms impedance; layers and lattices pass or stop bands | `acoustic.horn`: throat impedance, transmission, power along the axis (intensity, particle velocity), cut-off; `em.periodic.layers`: Bloch wavenumber, transmission; `mechanics.periodic-rod`: bands | optical and mechanical waveguides as structures |
| Reflection and redirection | impedance steps reflect; periodic steps reflect in phase | `em.periodic.layers`: stop bands, stack reflection; horn throat reflection; existing reflector models (Kirchhoff, manipulators) | scattering patterns of structures |
| Focusing and concentration | a horn mouth directs power; reflectors focus (existing manipulators) | horn directivity index and −6 dB angle; mirror spot sizes and waist of the optical resonator | focal metrics of structures (the manipulators' focus metrics are separate) |
| Confinement and storage | walls and mirrors confine standing waves | `em.cavity.rectangular`: modes, stored energy, Q, bandwidth; `optics.resonator.two-mirror`: finesse, linewidth, photon lifetime, stored energy per circulating watt; `mechanics.modal`: natural frequencies, modal mass | coupled-resonator networks |
| Dissipation and conversion | lossy walls and fills absorb; absorption heats (coupled) | cavity wall and fill loss (Q); `multiphysics.coupled-slab`: absorbed power, temperature, stress with the ledger | acoustic absorption of structures |
| Force and momentum | — | not computed: no momentum accounting yet (REQ-CHAT-20261126-022) | radiation pressure, Maxwell stress, acoustic radiation force |

## Distinguishing amplitude, intensity, power and energy

Metrics state their quantity and unit: field amplitudes (`|E|` in V/m, `|p|`
in Pa), intensity (W/m²), power (W), stored energy (J, for a stated
amplitude). A horn raises the radiated power for a given throat velocity by
raising the throat resistance — it does not create energy: the power entering
the throat equals the power leaving the mouth (`horn.powerBalance`: below 1e-6
in the example, set by the integration step). A cavity's stored energy is that of one mode at a stated peak field;
a resonator's stored energy is per watt circulating, distinct from the power
supplied by a source (`optics.transmission` relates them on resonance).

## Accounting across domains

Energy that moves between domains is accounted by the coupled engine's
ledger (ADR-000220): the coupled-slab analysis reports `coupled.balanced` and
`coupled.relativeResidual`. Single-domain analyses state their own balances
(the horn's power balance, the cavity's energy equipartition).
