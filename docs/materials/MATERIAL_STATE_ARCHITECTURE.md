# Material State Architecture

**Status:** the current material model is described first; material state is
planned (RES-SFE-008, ADR-000219).

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
