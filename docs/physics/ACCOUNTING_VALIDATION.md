# Accounting Validation

**Status:** current (REQ-CHAT-20261126-022, RES-ACC-001..016). The request's benchmarks
(section 22), what each is checked against, and the tests.

| Benchmark | Checked against | Test |
|---|---|---|
| Closed mechanical system (22.1) | two gliders and a spring bumper, free: total momentum constant to 1e-12 while kinetic energy turns elastic and back; no external entity; the residual within the integrator's stated error | `AccountingNetworks.ClosedSystemConservesMomentumWhileEnergyChangesForm`, `AccountingExamples.CollisionAndFlywheels` |
| Work and dissipation (22.2) | a damped driven oscillator: applied work = stored + dissipated; the heat equals the integrator's own dissipation of the mechanism; the momentum the ground receives through the spring and damper | `AccountingNetworks.DampedDrivenWorkIsStoredOrDissipated` |
| Resonant buildup and decay (22.3) | Q = ω E / P by mechanism against the complex eigenvalue's Q (exact for one degree of freedom); E_ss (1 − e^(−γt))² while driven at resonance and decay at 2γ, within 2 % | `AccountingNetworks.ResonatorQualityAndBuildupDecay`, `AccountingExamples.ResonatorDrivenAndRingingDown` |
| Coupled resonators (22.4) | lossless and isolated: no net input or output for the whole; each resonator alone verified, the energy leaving one through the coupling equal to its loss; the same exchange internal to the whole and an output of one | `AccountingNetworks.SubsystemsOfCoupledResonators`, `AccountingServices.RunReclassifyCompareExportAndKeep` |
| Electromagnetic absorption (22.5) | a microwave-heated slab through the coupled engine: incident = reflected + transmitted + absorbed; the absorbed power transferred once to the heat; the wave alone and the heat alone each verified; momentum "insufficient data" | `AccountingCoupled.AbsorbedPowerMovesOnceFromTheWaveToTheHeat` |
| Momentum transfer (22.6) | a lossy layered slab, a copper film (2 I/c) and a matched absorber (≈ (1 + R) I/c): the vacuum Maxwell stress, the Lorentz volume force and (1 + R − T) I/c agree to 1e-7; the body's momentum balance; dielectric half-spaces and magnetic layers refused | `AccountingRadiation.*` |
| Dynamic material evolution (22.7) | J2 work stored (elastic, recoverable), locked (hardening) or dissipated, within the work quadrature's error; damage, viscoelasticity, the bar and the damaged cantilever; isothermal entropy production D/T > 0; the melting slab's energy (rounding) and entropy (production > 0, first-order residual); the Preisach loss within 2μ₀ Ms ΔH of the hysteron sum | `AccountingMaterialState.*`, `Hysteresis.LoopLossEqualsTheHysteronSum`, `PhaseChange.EntropyProductionIsNonNegative` |
| Numerical convergence (22.8) | the subsystem energy residual of coupled resonators falls more than 12× per halved step (fourth order) until rounding; Simpson's rule for the volume force more than 10× per doubled points; the phase change's entropy residual first order in the step; the Preisach error bound falling with the increment | `AccountingNetworks.ResidualConvergesWithTheStep`, `AccountingRadiation.LocalAndGlobalAbsorptionAgreeAndConvergeWithThePoints`, `PhaseChange.EntropyProductionIsNonNegative`, `AccountingMaterialState.HysteresisLossAgreesWithTheHysterons` |
| Rotation (section 9) | two flywheels on a torsion shaft, one driven by a torque for 0.5 s: the angular momentum gained equals the angular impulse, all of it in the wheels; torques are a family of their own | `AccountingNetworks.RotatingShaftAngularMomentumAndTorques` |
| Units and families | conversions, refused dimensions and affine scales, every registered unit understood; torque ≠ energy, entropy ≠ heat capacity, pressure ≠ momentum flux | `Dimensions.*`, `AccountingQuantities.*` |
| Statuses | missing data never verified, absent quantities not applicable, numerical error never dissipation, double counting refused, negative entropy production rejected | `AccountingConservation.*`, `AccountingLedger.*` |
| Clients | the service, Python, MCP and the panel | `AccountingServices.*`, `python/tests/test_accounting_api.py`, `AccountingPanelTest.*`, `AccountingExamples.EveryStudyRunsAndNothingExceeds` |

## What the accounting found in existing models

- **Scalar damage.** The dissipation was integrated as ½Eε²dD, which is
  first order. The residual exceeded every stated error, so the run was
  reported as unaccounted. The model now integrates the dissipation
  trapezoidally in κ, which is second order like the work.
- **Preisach.** A minor loop entered from a larger excursion is not closed
  on its first traversal, so its "loss per cycle" included a change of
  state. The runner now measures the second cycle, which return-point memory
  closes.

A small residual does not prove a model right. These benchmarks check each
model against an independent reference (a closed form, another method or an
eigenvalue), not only against itself.
