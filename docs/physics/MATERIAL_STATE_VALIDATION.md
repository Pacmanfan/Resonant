# Material State Validation

**Status:** current (REQ-CHAT-20261126-021, RES-DMS-004..014). What each model is checked
against, and the test that checks it.

| Model | Checked against | Test |
|---|---|---|
| J2, uniaxial | the bilinear closed form `σ = σy + E Et/(E+Et)(ε − εy)`; elastic unloading and residual strain | `Plasticity.ElasticThenHardeningAgainstTheBilinearLaw`, `.UnloadingIsElasticAndLeavesResidualStrain` |
| J2, kinematic | the reverse yield at `σmax − 2σy` (Bauschinger) | `Plasticity.KinematicHardeningShiftsTheReverseYield` |
| J2 tangent | central finite differences of the update | `Plasticity.TheConsistentTangentMatchesFiniteDifferences` |
| J2 energy | work = elastic + hardening + dissipated over cycles | `Plasticity.WorkIsStoredOrDissipated` |
| J2 bar | the analytic limit load, load redistribution and residual forces in balance after unloading | `Plasticity.AnIndeterminateBarRedistributesAndKeepsResidualStress` |
| Damage | the law curves, irreversibility, secant unloading, energy split | `Damage.*` |
| Damaged cantilever | Euler–Bernoulli frequencies (β·L = 1.875, 4.694, …) undamaged; energy balance converging at second order in the step; modes from the damaged stiffness | `DamagedBeam.*` |
| Standard linear solid | the exact relaxation `E∞ + E1 e^{−t/τ}`; loss per cycle `π E'' ε0²` | `Viscoelasticity.*` |
| Phase change | the Neumann (one-phase Stefan) front; the energy balance of sensible and latent heat; solidification releasing the latent heat; the refused unstable step | `PhaseChange.*` |
| Preisach | the uniform-density first-order reversal curve and major-loop area in closed form; return-point memory; congruent minor loops; no loss from the reversible part | `Hysteresis.*` |
| Evolution | uniform heating against its closed form step by step; constrained thermal stress `−E α ΔT/(1−ν)` at every step; temperature-dependent loss; convergence with the step; the energy of the steps summing to the run | `StateEvolution.*` |
| Catalogue | each implemented model runs its presets; the models that are not implemented are refused | `MaterialStateModel.*` |
| Services | studies, jobs, records through save and reload, undo, commit with lineage and staleness | `MaterialStateServices.*` |
| Examples | an evolving conductor (conductivity following the temperature), a constrained plate (thickening, compressive stress), plasticity, damage and the frequency drop through the functional framework, a melting wall (within 2 % of Stefan), a core with loop losses | `MaterialStateExamples.*` |
| Clients | the API and MCP levels | `python/tests/test_material_state_api.py`, `MaterialStatePanelTest.*` |
