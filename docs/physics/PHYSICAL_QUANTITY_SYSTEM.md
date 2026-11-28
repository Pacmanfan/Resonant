# Physical Quantity System

**Status:** implemented (RES-CEME-002, ADR-000220): `src/core/multiphysics/Quantity`, `FieldData`.

Every quantity that crosses between domains is typed. A descriptor names:

| Field | Meaning |
|---|---|
| `id` | e.g. `absorbed_power_density`, `heat_source_density`, `temperature`, `displacement`, `stress` |
| `unit` | the SI unit (`W/m^3`, `K`, `m`, `Pa`) |
| `rank` | scalar, vector (3 components), tensor (6: xx, yy, zz, xy, yz, zx) |
| `representation` | real or complex (phasors, `e^{+jωt}`) |
| `meaning` | instantaneous, peak, RMS, time-averaged, integrated, static |
| `domain` | the domain that defines it (electromagnetic, thermal, mechanical, acoustic, general) |
| `conservative` | a density whose integral transfers must preserve (power, heat, force, mass, charge) |

Field data add the support (an axis-aligned cell grid in world meters, frame
`world`), the time character (steady, harmonic at a frequency, an instant),
the producing participant and solver, and the material and geometry revisions
it was computed from.

## The registry

Energy and power: `energy` (J), `power` (W), `energy_density` (J/m³),
`power_density`, `absorbed_power_density` (EM, time-averaged), `acoustic_absorbed_power_density`,
`heat_source_density` (steady), `heat_flux`, `poynting_flux`, `acoustic_intensity`.
Fields: `temperature`, `electric_field`, `magnetic_field`, `electric_flux_density`,
`magnetic_flux_density`, `current_density`, `charge_density`, `acoustic_pressure`,
`particle_velocity`, `displacement`, `velocity`, `surface_normal_velocity`,
`strain`, `thermal_strain`, `stress`, `pressure`, `force_density`, `surface_traction`,
`momentum_density`. Material state: `mass_density`, `relative_permittivity`,
`electrical_conductivity`, `thermal_conductivity`, `damage`, `phase_fraction`.

## Compatibility and conversions

Two descriptors are compatible when id, unit, rank, representation and
meaning agree. Anything else needs a conversion a coupling model declares:
`em.absorption-to-heat` turns the time-averaged `absorbed_power_density` of a
harmonic solution into the steady `heat_source_density` of conduction, which
is valid because the wave period is many orders of magnitude shorter than the
thermal time constant. The conversion is recorded with every transfer
(source and target quantity). Field names in the contract carry their units
(`max_temperature_change_kelvin`), and physical-system settings use SI values
with the unit in the key where it could be ambiguous (`frequencyHz`).

## Values that are refused

A field with a value that is not finite is never passed on: the run ends
with the stage that produced it (`Multiphysics.OrchestratorStopsOnFailureInvalidValuesAndCancellation`).

## Dimensions and accounting families

`foundation/Dimensions` turns a unit expression into SI dimension exponents
(kg, m, s, K, A, mol, cd) and a factor; conversions between different
dimensions, and of affine temperature scales, are refused. Every unit of the
registry above parses (`Dimensions.EveryRegisteredQuantityHasAKnownUnit`).
Dimension is not identity: the accounting framework's families
(`accounting/Quantities`, REQ-CHAT-20261126-022, RES-ACC-002, -003) keep
torque apart from energy, entropy from heat capacity and pressure from
momentum flux, though their dimensions agree, and mark which are conserved
(energy, linear and angular momentum, mass, charge; not force, torque or
power).

