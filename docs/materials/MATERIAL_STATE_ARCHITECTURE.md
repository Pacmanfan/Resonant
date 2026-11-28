# Material State Architecture

**Status:** the current material model is described first. Since
REQ-CHAT-20261126-016 (RES-CEME-003, ADR-000220) part of material state is
implemented: region laws (constant, linear in temperature, table) with
validity and provenance, the temperature state of a coupled run, revisions and
history, and the thermal property set; the rest of this page remains planned
(RES-SFE-008, ADR-000219).

## Implemented (the Coupled Energy–Matter Engine)

`src/core/multiphysics/MaterialState`: a physical system attaches laws to a
region's property (`"electromagnetic.lossTangent"`, `"mechanical.youngsModulus"`,
`"thermal.thermalConductivity"`, ...). The intrinsic value stays in the material
library; the state is a temperature field from the thermal participant
(relaxed updates, a revision per update, the last 20 snapshots kept). A
property at a material point is the law at the temperature interpolated
there; outside the law's validity the value at the nearest valid temperature
is used and a warning names the property, region, temperature and range. The
participants read the laws at their cells or elements whenever the material
revision changed. Distinguished as the request asks: intrinsic definition
(library), initial state (the reference temperature), current state (the
temperature field), its evolution (history), derived properties (law
values); effective (homogenized) properties remain planned.

```text
material state evolution in an iterative coupled run (revisions increase on every update)

library (intrinsic)        system law (derived)              state (current)                    use
 tan δ = 0.05      ──►  tan δ(T) = 0.05 + 2e-4 (T−T₀)   T₀ everywhere (revision 0)  ──►  EM sublayer properties
                         valid 273–450 K                       │ thermal solve
                                                               ▼
                                                  T₁(x) = T₀ + ω (T_solve − T₀)  (revision 1, snapshot)
                                                               │ EM prepares again: law at T₁(x), warning if outside validity
                                                               ▼
                                                  T₂(x) … until max |T_k − T_{k−1}| ≤ tolerance  →  result: state, history, revisions
```

## Today

`src/core/materials/Material.h`: a material has one property set per
physics domain (acoustic, mechanical, electromagnetic), so the domains'
constitutive data are never forced into one structure (ADR-000205). Each
property value records:

- its SI value and unit (from the domain's property definitions);
- its provenance: measured, manufacturer-supplied, literature-derived,
  theoretical, or illustrative;
- a validity range in frequency and temperature;
- optionally a table of values over frequency (linear interpolation,
  clamped at the ends).

What it cannot express: dependence on temperature, pressure, stress, strain
or fields; anisotropy (tensors); spatial variation inside an object;
history (plasticity, damage); phase; thermal properties; values computed by
a tool or homogenized from a structure.

## Planned: properties as functions of state

```text
Material ── per domain ──► PropertySet ── key ──► PropertyModel
                                                    ├─ Constant (today's value)
                                                    ├─ Table(state variables…)          measured data
                                                    ├─ Formula(name, parameters)         e.g. linear in T, Arrhenius, Debye
                                                    └─ Constitutive(domain model id)     anisotropic tensors, viscoelastic,
                                                                                         plastic, porous (domain code)
           each with: provenance, validity domain over its state variables, uncertainty
```

- **State variables** are declared, with units: temperature, pressure,
  stress and strain invariants, frequency, electric and magnetic field
  magnitude, composition, phase fraction, damage, a history variable.
- **Constitutive models stay in their domains.** An anisotropic stiffness
  tensor, a viscoelastic (Prony series) model, a plasticity law, a Debye or
  Drude–Lorentz permittivity, a porous-absorber model (Johnson–Champoux–Allard)
  are domain code; the material stores their parameters and declares which
  model reads them.
- **Validity is enforced:** evaluating outside the validity domain returns
  the value with a reported extrapolation (the run's warnings), or fails
  where the model forbids it. Never silent (RES-SFE-008).
- **Provenance gains** `Computed` (tool, version, inputs) and `Homogenized`
  (structure, model, frequency range), RES-SFE-009 and RES-SFE-005.

## Where state lives

The material library holds *laws*: how a property depends on state. The
*state itself* — a temperature field, a damage field, a phase field — is a
field over an object's material region, owned by a run (a coupled run, or a
single-domain run given a state field), stored with its results, never
written back into the library. A project can keep a named state (for example
"warm, 60 °C") as an input to later runs.

```text
library: steel  E(T) = E₀ (1 − β (T − T₀)),  valid 250–600 K, literature
run:     temperature field T(x) on the bracket's mesh (from a thermal participant)
solver:  evaluates E(T(x)) per element, reports min/max T and any extrapolation
```

## Kinds of materials to accommodate

| Kind | Needs | Domain |
|---|---|---|
| Linear isotropic | constants (today) | all |
| Anisotropic | tensors in a material frame tied to the object | mechanics, electromagnetics, acoustics (anisotropic fluids rarely) |
| Dispersive | frequency models with causality (Kramers–Kronig consistent: Debye, Lorentz) | electromagnetics, acoustics (porous) |
| Temperature-dependent | tables or formulas of T | all, through coupling |
| Viscoelastic | relaxation spectra | mechanics, acoustics |
| Plastic, damaging | history variables, evolution laws | mechanics |
| Porous | equivalent-fluid or Biot models | acoustics, mechanics |
| Composite, layered | homogenized properties or explicit structure | all ([metamaterials](METAMATERIALS_AND_ENGINEERED_STRUCTURES.md)) |
| Phase-changing | phase fraction, latent heat | thermal, mechanics |
| Evolving microstructure | state from a lower scale | through [multiscale](../physics/MULTISCALE_MODELING.md) |

## Compatibility

Existing projects load unchanged: a constant value is a `Constant` model;
frequency tables are `Table(frequency)`. The schema version and migration
of the project file (`src/core/project`) carry the change; the contract of
the automation API adds fields only.
