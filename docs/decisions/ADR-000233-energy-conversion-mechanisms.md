# ADR-000233: Energy-conversion mechanisms as stages of one accounting architecture

**Status:** ACCEPTED
**Date:** 2026-11-28
**Requirements:** `docs/requirements/energy_conversion.md` RES-CONV-001..006
**Request:** REQ-CHAT-20261128-005 (Prompt 27, Part II)
**Builds on:**
- ADR-000226 (unified energy accounting);
- ADR-000232 (layered 3D solids);
- ADR-000228 (functionally architected matter, whose mechanism registry listed thermoelectric, magnetostrictive and photovoltaic conversion as not modeled).

## Context

Before this decision, conversion was modelled in two places only:
- a lumped single-mode piezoelectric cantilever (`mechanics/PiezoelectricBeam`);
- the coupled engine's electromagnetic-to-thermal absorption.

The accounting framework balanced ledgers, but no conversion mechanism other
than the cantilever filled one. The request asks for:
- a unified conversion architecture across the mechanical, acoustic, thermal,
  electrical, electromagnetic, optical, elastic and dissipative forms;
- a general piezoelectric formulation;
- thermoelectric, magnetostrictive and photovoltaic models;
- a chain from acoustic excitation to an electrical load with independently
  verifiable stages.

## Decision

1. **One stage record, one ledger** (`conversion/ConversionStage`).
   - **Contents:** every mechanism reports its input, output, other inputs and
     outputs, stored and dissipated energy, coupling coefficients, boundary
     conditions, assumptions, validity limits, uncertainty, and the
     contributions it does not compute.
   - **Ledger:** a chain becomes an accounting ledger, one body per stage. Each
     stage's balance is checked by the existing conservation evaluation, and
     every join (output of one stage = input of the next) is checked too.
   - **Incomplete balances:** a stage with missing contributions is
     insufficient data, never verified.
2. **Piezoelectric continuum** (`mechanics/PiezoelectricSolid`).
   - **Elements:** coupled 20-node elements with a nodal potential on the
     layered bodies of ADR-000232.
   - **Material:** stress-charge constants from strain-charge data, rotated
     with the surface and the poling.
   - **Circuit:** electrodes on faces or segments, terminals (ground, driven,
     floating, resistive load).
   - **Analyses:** static; short- and open-circuit modal, the potential
     condensed exactly; harmonic base excitation with loss.
   - **Solution:** the electric unknowns are scaled to the mechanical ones, and
     accuracy is judged by the normwise backward error.
   - **Reference:** the lumped cantilever is kept as the reference model.
3. **Thermoelectric couples** (`thermal/Thermoelectric`).
   - one-dimensional legs with temperature-dependent alpha, rho, kappa;
     Joule, Peltier and Thomson terms;
   - a generator with its load or a cooler with its current;
   - the junction heats from the gradients and the power from the voltage, so
     the energy balance is an independent check;
   - the entropy generation reported, and refused if negative.
4. **Magnetostriction, linear only** (`mechanics/Magnetostrictive`).
   - piezomagnetic constants about a stated bias, with a dynamic-field limit;
   - a rod with a coil as an actuator and as a base-excited harvester;
   - stated as linear: no hysteresis, saturation or Delta-E effect.
5. **Photovoltaics by an equivalent circuit** (`conversion/Photovoltaic`).
   - single-diode model by Newton's method, cross-checked by the Lambert W
     closed form;
   - spectral photocurrent from absorption and collection;
   - Varshni band gap and I_0(T);
   - the incident light accounted for term by term (optics, thermalization,
     uncollected carriers, the voltage deficit, the circuit's losses).

   No semiconductor transport is solved, and nothing claims it.
6. **The chain** (`coupling/ConversionChains`). A plane wave in a duct closes on
   a piezoelectric plate, which drives a resistive load. The stages are:
   - the plane-wave relation against the plate's impedance;
   - the transducer's work, damping and terminal power;
   - the terminals against the load.

   Each balance comes from independent terms.
7. **Registry.** The mechanism registry marks the thermoelectric,
   magnetostrictive and photovoltaic effects `standalone`: a model exists, not
   yet an analysis of architectures.

## Consequences

- **Validation:**
  - **Photovoltaics:** the open-circuit voltage closed form, Lambert W
    against Newton, Green's fill factor, the stage residual, the temperature
    slope of V_oc.
  - **Thermoelectrics:** the constant-property closed forms (currents, heats,
    eta_max with ZT), the cooler's Delta T_max = Z T_c^2 / 2, second-order
    convergence of the energy residual with temperature-dependent legs, the
    Seebeck integral.
  - **Magnetostriction:** k^H / (1 - k^2) shorted stiffening, the blocked
    force, an exact harvester balance.
  - **Piezoelectric continuum:**
    - the free plate's -d33 V and -d31 V L / t;
    - the free capacitance;
    - the open-circuit -g33 p t, exactly;
    - the series bimorph 1.5 d31 V L^2 / t^2 within 3 %.
  - **Against the lumped reference:** the unimorph's short-circuit frequency
    within 3 %. Its coupling is within 8 % once the reference's static-shape
    overestimate is corrected analytically: about 22 %, a property of the
    lumped model, recorded. The harvester's power is within 30 %, with an
    exact balance.
  - **The chain:** verified at and off resonance.
- **Still planned:**
  - the coupled engine's piezoelectric participant;
  - thermoelectric, magnetostrictive and photovoltaic analyses of
    architectures;
  - semiconductor transport, ferroelectric switching, magnetostrictive
    hysteresis;
  - full-wave acoustic ledgers.
