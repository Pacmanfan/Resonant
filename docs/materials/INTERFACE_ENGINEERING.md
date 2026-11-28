# Interface Engineering

**Status:** current (REQ-CHAT-20261127-001, RES-FAM-009, ADR-000228).

An interface is not merely a geometric boundary, and no interface is assumed
perfect: an interface without a model for the physics an analysis reads makes
that analysis refuse the stack.

## Models

| Model | Physics | Parameters | Used by |
|---|---|---|---|
| perfect | all | - | every analysis |
| perfect-bonding | mechanical | - | lamination, modes, the cantilever, the 3D layered solid (shared nodes) |
| finite-stiffness | mechanical | normalStiffness, shearStiffness (N/m^3) | the 3D layered solid (spring layer); refused by the others |
| cohesive-zone | mechanical | strength (Pa), fractureEnergy (J/m^2) | bonded below the strength; delamination screening |
| frictional-contact | mechanical | frictionCoefficient (penalties normalStiffness, shearStiffness optional) | the 3D layered solid (Coulomb, Newton with return mapping); refused by the others |
| sliding | mechanical | normalStiffness (N/m^3) | the 3D layered solid (no shear); refused by the others |
| thermal-contact-resistance | thermal | resistance (m^2 K/W) | through-thickness conduction; the coupled engine; the 3D layered conduction |
| electrical-contact-resistance | electrical | resistance (ohm m^2) | through-thickness electrical; refused by the layered solver |
| dielectric | electrical | surfaceChargeDensity (C/m^2) | recorded; refused |
| charge-transfer | electrical | barrierHeight (V) | recorded; refused |
| diffusive | chemical | diffusionLength (m) | recorded |
| reactive | chemical | reactionLayerThickness (m) | recorded |

An interface may carry several models, one per physics. `modelFor(physics)`
returns the model of that physics, or `perfect`, or nothing. Adhesion energy,
residual stress, defects, chemical compatibility and preparation are recorded
too. Every parameter has a unit, a provenance and an uncertainty.

## Derived quantities

The `interfaces` analysis reports, each with its formula:
- **Acoustic transmission at normal incidence:** `4 Z1 Z2 / (Z1 + Z2)^2`, from
  the acoustic density and sound speed, or else from the longitudinal wave of
  an isotropic solid (the basis is named).
- **Thermal mismatch stress:** for a temperature change, the free laminate's
  ply stresses next to the interface (lamination theory), and whether they
  exceed a cohesive strength.
- **Temperature jump** `q R` under a stated heat flux, and the **voltage
  drop** `J R` under a current density.

The comparison with a cohesive strength is a screening. Lamination theory gives
in-plane ply stresses. The interfacial shear and peel stresses concentrate at
free edges and are higher, so a value below the strength does not exclude
delamination.

## In the solvers

`thermal::ConductionProblem::contactResistance` holds a series resistance per
internal plane of the grid. The coupled engine's thermal participant fills it
from its domain settings `interfaces`, which `stackToPhysicalSystem` writes from
the architecture. It is verified against the series formula and the jump
`q (w1 / 2 k1 + w2 / 2 k2 + R)` between the adjacent cells
(`ThermalContact.*`, `MatterCoupling.TheBarrierStackRunsInTheCoupledEngine`).

The three-dimensional layered solid (`mechanics/LayeredSolid`,
`MatterAnalyses` `layered-solid`, `layered-solid-thermal`; RES-MECH-019..022)
represents perfect bonding, finite stiffness, sliding and frictional contact
mechanically, and perfect contact or a contact resistance thermally. Optional
model parameters `normalStrength` and `shearStrength` give the quadratic
initiation index at every interface point. Debonded regions are given as
analysis options. A cohesive zone is refused: there is no damage evolution
(`../mechanics/LAYERED_SOLIDS.md`).

## Optimization and measurement

Interface parameters are design variables of the `matter` adapter
(`matter/interface/<id>/<model>/<key>`). They are also parameters of the
laboratory model `matter.stack-metric`, so a contact resistance can be
calibrated from measured temperature drops and validated on another set
(`MatterCoupling.InterfaceResistanceCalibratedFromMeasurements`).
