# Coupling Architecture and the Initial Coupling Models

**Status:** per model below (RES-CEME-006, RES-CEME-013, ADR-000220). The
registry is `src/core/coupling/CouplingModels`; `ListCouplingModels` returns it.

## Kinds of coupling

| Kind | What the engine does after the source participant solves |
|---|---|
| field-transfer | takes the source's quantity, applies the model's declared conversion, maps it to the target's grid (conservative or interpolating) and gives it to the target; energy densities become ledger transfers |
| state-update | puts the source's temperature into the material state (relaxed); participants read their laws at the new state when they next prepare |
| geometry-update | applies the source's geometry changes to the current geometry (relaxed); participants that work in the current geometry rebuild |

Strategies: **one-way** (one pass, no feedback), **sequential** (fixed
passes), **iterative** (until the state changes of a pass are below the
tolerances, with relaxation), all from one orchestrator
([execution](COUPLED_SIMULATION_EXECUTION.md)). Fully time-dependent
multirate coupling is planned.

## Electromagnetic ↔ thermal

| | |
|---|---|
| Models | `em.absorption-to-heat` (**validated**), `thermal.temperature-to-em-properties` (**validated**), `em.magnetic-loss-to-heat` (planned) |
| Equations | q = ½ ω ε₀ ε″_eff |E|², ε″_eff = ε′ tan δ + σ/(ωε₀) (period average) as the source of ρc_p ∂T/∂t = ∇·(k∇T) + q; ε′(T), tan δ(T), σ(T) from region laws; magnetic loss ½ ω μ₀ μ″ |H|² |
| Inputs / outputs | EM: frequency, intensity, ε, tan δ, σ per sublayer → absorbed power density; thermal: k, ρ, c_p, faces → temperature |
| Numerics | exact layered solution; finite volumes; power transferred conservatively per reference volume (exact); Picard iteration with relaxation for the feedback |
| Conservation | absorbed power = heat source = heat out (steady) or stored + out (transient), to rounding |
| Benchmarks | `CoupledValidation.MicrowaveAbsorptionHeatsASlab`, `.TemperatureDependentLossConvergesToTheLumpedFixedPoint`, `.TransientHeatingConservesEnergy` |
| Limitations | plane wave at normal incidence on layers; slow thermal time scale; no temperature dependence unless a law is given |

## Thermal ↔ mechanical

| | |
|---|---|
| Models | `thermal.expansion` (**validated**), `thermal.temperature-to-mechanical-properties` (**validated**) |
| Equations | ε_th = α (T − T_ref) I, σ = D (ε − ε_th); E(T), ν(T) from laws; thermal damping and thermoelastic heating not modeled |
| Inputs / outputs | temperature per element → displacement, stress, deformed thicknesses |
| Numerics | trilinear hexahedra, 2×2×2 Gauss thermal loads, reference configuration |
| Conservation | quasi-static linear thermoelasticity draws no modeled energy: strain energy is informational |
| Benchmarks | `ThermalLoads.*`, `CoupledValidation.UniformHeatingExpandsAndStressesTheColumn`, `.TemperatureDependentModulusScalesTheThermalStress` |
| Limitations | small strain; uncoupled (mechanical work does not heat); solid columns and frames only |

## Mechanical → geometry → electromagnetic

| | |
|---|---|
| Diagram | `heat ──T──► solid ──u_z(top) − u_z(bottom)──► GeometryState.current (revision++) ──► em rebuilds its sublayers with the new thicknesses`; conduction and mechanics stay in the reference configuration |
| Model | `mechanics.deformation-to-geometry` (**validated**) |
| Equation | d_i = d_i,ref + u_z(top of i) − u_z(bottom of i) |
| Benchmark | `CoupledValidation.ExpansionFeedsBackIntoTheElectromagneticSolution` (the final wave solution equals a standalone solution of the deformed slab to 1e-9) |
| Limitations | layer thicknesses only; no remeshing of general shapes |

## Acoustic ↔ mechanical

| | |
|---|---|
| Models | `mechanics.structure-to-acoustic.rayleigh` (**external**: implemented and validated in the mechanics panel, `coupling/StructuralRadiation`, not yet a participant pair), `acoustic.pressure-to-structure` (planned) |
| Equations | Rayleigh integral p(x) = (jωρ/2π) Σ v_e A_e e^{−jkR}/R (baffled); traction t = −p n, two-way with added mass and radiation damping |
| Benchmarks | the existing structural-radiation tests (REQ-CHAT-20261126-006); planned: a baffled piston, a plate in water |
| Limitations | one-way only; infinite rigid baffle |

## Electromagnetic ↔ mechanical

Models `em.lorentz-force`, `em.radiation-pressure`, `em.piezoelectric`
(planned). Equations: f = ρE + J×B (period average ½Re(J×B*)); F = −∮ T·n dA
with the Maxwell stress tensor; strain-charge piezoelectric equations S = s^E T + dᵗE,
D = dT + ε^T E. Benchmarks planned: the force on a conductor in a field,
radiation pressure on a mirror (2I/c), a piezoelectric disk resonance.

## Acoustic ↔ thermal

Models `acoustic.absorption-to-heat`, `thermal.temperature-to-acoustic-properties`
(planned). Equations: q = 2αI for a plane wave; c(T) = √(γRT/M) for an ideal
gas. Benchmarks planned: absorber heating; sound speed drift in a heated duct.

## Fluid ↔ mechanical

Model `fluid.structure-interaction` (planned): Resonant has no
fluid-dynamics domain. Navier–Stokes with moving boundaries, traction and
velocity continuity at the interface.

## Adding a coupling

A coupling model (registry entry with equations and status), the
participants' provide/accept of its quantities, a reference test, and the
status raised to validated only when that test compares the physical result
with a reference.
