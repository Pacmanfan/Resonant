# Coupled Simulation Validation

**Status:** the reference cases below run in the core suite
(`tests/core/test_coupled_validation.cpp`, RES-CEME-013). Each checks a
physical result against a reference, not only that the run completes.

| Case | Assumptions and equations | Reference | Tolerance | Result |
|---|---|---|---|---|
| **EM → thermal** (`MicrowaveAbsorptionHeatsASlab`) | 10 mm slab, ε′ = 4, tan δ = 0.05, 2.45 GHz plane wave 20 kW/m², convection h = 20 W/(m² K) both faces; −kT″ = q(z) | the exact absorbed-power profile (flux differences of the transfer-matrix solution) integrated twice by quadrature (20 000 points) with both Robin conditions | 0.2 % of the temperature rise at every cell; absorbed = transferred = heat out to 1e-9 | pass |
| **Two-way feedback** (`TemperatureDependentLossConvergesToTheLumpedFixedPoint`) | 2 mm slab, k = 200 W/(m K) (Biot 2e-4), tan δ = 0.02 + 4e-4 (T − T₀), 100 kW/m² | the lumped fixed point 2h(T − T₀) = A(tan δ(T)) I solved by bisection with the uniform-slab solution | 0.1 % of the rise; converges; feedback hotter than one-way; relaxation 0.5 reaches the same point to 1e-5 K | pass |
| **Thermal → mechanical** (`UniformHeatingExpandsAndStressesTheColumn`, `ThermalLoads.*`) | uniform ΔT = 50 K; laterally constrained or free column | ε_zz = αΔT(1+ν)/(1−ν), σ_xx = σ_yy = −EαΔT/(1−ν), σ_zz = 0; free: u = αΔT L, σ = 0; fully constrained: σ = −EαΔT/(1−2ν); bars: αΔTL and −EαΔT | 1e-12 m, 1e-6 relative | pass |
| **E(T)** (`TemperatureDependentModulusScalesTheThermalStress`) | E(T) = E₀ − 3e7 (T − T₀) | σ = −E(T)αΔT/(1−ν) | 1e-6 relative | pass |
| **Geometry feedback** (`ExpansionFeedsBackIntoTheElectromagneticSolution`) | microwave heating → conduction → expansion (α exaggerated to 2e-4 1/K) → thicker slab → new wave solution, iterated | the final absorptance equals a standalone layered solution of the deformed slab; the thickness change equals the column's top displacement | 1e-9, 1e-12 m | pass |
| **Transient energy** (`TransientHeatingConservesEnergy`) | the EM → thermal case for 120 s from ambient | absorbed power × 120 s = stored + heat out | balanced to the default 1e-6 relative tolerance | pass |
| **Mechanical → acoustic** | a vibrating plate radiating into a half-space (Rayleigh integral) | the existing structural-radiation tests of REQ-CHAT-20261126-006 | as there | pass (outside the engine) |
| **Acoustic → mechanical** | a structure loaded by acoustic pressure | — | — | not implemented (model planned) |

Engine infrastructure is validated separately (`tests/core/test_multiphysics.cpp`):
transfers (integral preservation on random non-matching grids, exactness of
linear interpolation), material laws and extrapolation reporting, geometry
revisions, the physical-system document, execution order and cycles,
fixed-point iteration of a synthetic map (T* = 320 K), non-convergence,
failure, invalid values, cancellation, and the ledger's attribution of an
imbalance. The services (`test_multiphysics_services.cpp`) and the API end
to end (`python/tests/test_multiphysics_api.py`) are tested against real
runs; the example project runs to a converged, balanced result
(`TheMultiphysicsExampleConvergesWithABalancedLedger`).

## Deviations and limits

The layered reference systems are 1D: a full-wave 3D electromagnetic
participant (Yee) and 3D conduction would be validated against the same
cases in a 1D limit before being trusted in 3D. Material values in the
examples are illustrative. No coupled case has been compared with a
measurement yet (stage 12 of the research roadmap).
