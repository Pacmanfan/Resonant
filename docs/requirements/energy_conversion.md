# Energy Conversion Mechanisms

**Owner of:** `RES-CONV-001..006`
**Source:** REQ-CHAT-20261128-005 (Prompt 27, Part II, "Multiphysics and Energy Conversion")
**Refines:** `accounting.md` (the unified ledger), `mechanics.md` RES-MECH-019..022 (layered solids)
**Decisions:** ADR-000233 (builds on ADR-000226, ADR-000232)
**Documents:** `docs/physics/ENERGY_CONVERSION.md`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

### RES-CONV-001 — Conversion stages in one ledger

Every conversion mechanism reports a stage. It holds:
- the input and output energy (or power) and their forms; any other input or
  output;
- the stored and dissipated energy;
- the coupling coefficients, boundary conditions, assumptions, validity limits
  and uncertainty;
- the contributions the model does not compute.

A chain of stages becomes an accounting ledger. Each stage's balance and each
join between stages are checked, and energy never appears or disappears
through an undocumented operation. A stage with missing contributions is
reported as insufficient data.
*Verification:* `EnergyConversion.AChainIsCheckedStageByStageAndAtEveryJoin`.

### RES-CONV-002 — Piezoelectric continua

Piezoelectric layers of layered bodies are modelled in three dimensions:
- **Constitutive relations:** the stress-charge form, anisotropic, converted
  from strain-charge data, rotated with the surface and the poling.
- **Circuit:** electrodes on faces or segments, wired to grounded, driven,
  floating or resistive-load terminals.
- **Analyses:** static (converse and direct effects); short- and open-circuit
  modes; harmonic base excitation with a loss factor and electrical loads,
  with its energy balance.

The lumped cantilever is kept as the reference model. Invalid constants are
refused.
*Verification:* `PiezoelectricSolid.AFreePlateUnderVoltageStrainsByItsD33AndD31`, `-AnOpenCircuitPlateUnderPressureShowsMinusG33PT`, `-ASeriesBimorphBendsByThreeHalvesD31VLSquaredOverTSquared`, `-TheUnimorphsModesAgreeWithTheLumpedReference`, `-AHarvesterAtResonanceBalancesItsPowers`, `-WhatCannotBeWiredIsRefused`.

### RES-CONV-003 — Thermoelectric conversion

A thermoelectric couple models:
- temperature-dependent Seebeck coefficient, resistivity and conductivity;
- Joule heating, the Peltier effect and the Thomson effect.

It runs as a generator with its load or as a cooler with its current. It
reports the junction heats, the electrical power, the efficiency or COP and
the entropy generated. The energy balance is an independent check, and a
negative entropy generation is refused.
*Verification:* `EnergyConversion.AThermoelectricGeneratorMatchesTheConstantPropertyClosedForm`, `-AThermoelectricCoolerReachesHalfZTSquaredWithoutLoad`, `-TemperatureDependentLegsConserveEnergyAndConverge`.

### RES-CONV-004 — Magnetostrictive conversion

A magnetostrictive rod in a coil is modelled by linear piezomagnetic constants
about a stated bias, with a stated dynamic-field limit. It works as an
actuator (free elongation, blocked force) and as a harvester (coil, resistive
load, energy balance). The model is labelled linear. Hysteresis, saturation
and the Delta-E effect are not modelled, and inconsistent constants are
refused.
*Verification:* `EnergyConversion.AMagnetostrictiveRodIsLinearAboutItsBias`.

### RES-CONV-005 — Photovoltaic conversion

A photovoltaic cell is modelled by the single-diode equivalent circuit, solved
by Newton's method and cross-checked by the Lambert W closed form. The
photocurrent comes from the spectrum, the absorption and the collection
fraction. The band gap and saturation current depend on temperature. The
model reports the maximum power point, the fill factor and the efficiency.

The incident light is accounted for term by term:
- **optics:** reflected and transmitted light;
- **absorption:** thermalization and uncollected carriers;
- **circuit:** the voltage deficit and the diode, shunt and series losses.

No semiconductor transport is solved.
*Verification:* `EnergyConversion.LambertWSolvesItsDefiningEquation`, `-TheDiodeEquationAgreesWithItsClosedForms`, `-TheIncidentLightIsAccountedForTermByTerm`, `-AWarmerCellLosesOpenCircuitVoltage`.

### RES-CONV-006 — A cross-domain chain

The energy of an acoustic wave is followed to an electrical load:
1. a plane wave in a duct, closed by a piezoelectric plate;
2. the plate's mechanical work;
3. its piezoelectric conversion;
4. the terminals, then the load.

Every stage's balance comes from independent terms, and every join is
checked.
*Verification:* `PiezoelectricSolid.AnAcousticWaveIsFollowedToTheElectricalLoad`.
