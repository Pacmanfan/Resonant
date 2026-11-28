# State Evolution of Coupled Systems

**Status:** current (REQ-CHAT-20261126-021, RES-DMS-002, -009, -010, -012,
ADR-000225). `src/core/coupling/StateEvolution`.

A physical system with a thermal domain evolves in time steps. Each step is
one run of the Coupled Energy–Matter Engine
([COUPLED_ENGINE_ARCHITECTURE.md](../architecture/COUPLED_ENGINE_ARCHITECTURE.md)),
and its transient thermal participant starts from the previous step's
temperature field:

```text
T⁰ = ambient (or the system's initial state)
for step k = 1..N  (Δt = duration / N):
    system.thermal.settings.transient = {duration: Δt, steps: substeps, snapshots: 1, initialTemperature: T^{k-1}}
    run the coupled engine: its scheme (iterative or one-way) converges the step:
        EM / mechanics read the region laws at the step's temperature (temperature-dependent conductivity, loss, modulus…)
    commit: T^k, region means, law values, stress, thickness, the step's energy audit
    a step that fails is reported and ends the run (the committed steps stay)
```

- **Properties per step.** Region laws are evaluated at the step's
  temperature field, and the evaluated values are recorded per step. These
  values can be committed to a material.
- **Thermo-mechanical evolution.** With a mechanical domain and
  `thermal.expansion`, the stress and the thermal strain follow the
  temperature. The geometry updates where `mechanics.deformation-to-geometry`
  carries the deformed thickness to an electromagnetic domain.
  `examples/materialstate/b-thermomechanical-expansion` therefore adds a
  negligible probe wave of 1 nW/m² for that purpose.
- **Energy.** Each step's audit gives the input, the stored energy and the
  outflow. The run's account sums the steps, and its residual is at solver
  tolerance.
- **Convergence.** The change in the end temperature shrinks with each
  halving of the step. First order is expected, and `StateEvolution.*`
  checks a reduction of at least 30 % per halving.

**Not covered:**

- The coupled solvers carry no state variables of other models, such as
  plastic strain or damage. Those models run in their own studies.
- Moving boundaries (melt fronts) are not available inside the coupled
  slab.
- Adaptive time steps are not available.
