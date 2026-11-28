# Mechanical Physics

**Status:** current for REQ-CHAT-20261126-006. Requirements:
`docs/requirements/mechanics.md`. Decision: ADR-000210.

The mechanical domain is a separate module (`src/core/mechanics`) with its
own equations, materials, boundary conditions and solvers. It shares only
generic infrastructure with the acoustic domain: scene, geometry, units,
material records, jobs, results, rendering, and the signal definitions of
`src/core/dsp`. One-way coupling to acoustics lives in `src/core/coupling`.

| Document | Contents |
|---|---|
| `STRUCTURAL_MECHANICS.md` | idealizations, element formulations, supports, meshes, statics |
| `LAYERED_SOLIDS.md` | layered 3D solids on curved surfaces, interfaces (springs, sliding, friction, debonding, initiation), 3D conduction, validation |
| `MECHANICAL_MATERIALS.md` | the elastic material model and the material library |
| `MODAL_ANALYSIS.md` | the eigenvalue problem, the solvers, mode display |
| `FORCED_RESPONSE.md` | harmonic analysis, damping models, frequency-response functions, measured data |
| `TRANSIENT_VIBRATION.md` | Newmark time integration, signals, energy, recording |
| `MECHANICAL_WAVE_PROPAGATION.md` | longitudinal, flexural and membrane waves; reflection; dispersion |
| `MECHANICAL_FOCUSING.md` | actuators, vibration targets, actuator-input targeting, horns |
| `PHONONIC_STRUCTURES.md` | periodic rods, Bloch analysis, stop bands |
| `FLUID_STRUCTURE_COUPLING.md` | sound radiated by plates (one-way) |
| `MECHANICAL_VALIDATION.md` | every numerical check with its tolerance and measured error |

## Governing equations

Linear elastodynamics, small displacements and strains:

```text
rho d2u/dt2 = div sigma + f,      sigma = C : epsilon,      epsilon = (grad u + grad u^T) / 2
```

isotropic: `sigma = lambda tr(epsilon) I + 2 G epsilon`. Discretized by
finite elements into

```text
M a + C v + K u = f(t)
```

with consistent mass `M`, stiffness `K`, and damping `C` or a complex
stiffness. Bulk elastic waves (`c_P`, `c_S`), rod waves (`sqrt(E / rho)`),
bending waves of beams and plates (dispersive) and membrane waves
(`sqrt(T / rho h)`) are different waves of different structural theories;
each idealization states which it represents. The acoustic FDTD solver is
not used for solids.

## What exists

| | |
|---|---|
| Idealizations | Euler-Bernoulli beam (3D frame element), Kirchhoff plate (ACM rectangle), pre-tensioned membrane (bilinear), 3D solid (trilinear hexahedron) |
| Analyses | static; modal (natural frequencies, mode shapes, effective mass); harmonic sweep (direct or modal superposition); transient (Newmark) |
| Damping | Rayleigh, structural loss factor (material or global), modal ratio |
| Results | displacement, velocity, acceleration; stresses for beams, plates, solids; energies |
| Objects | structure component, actuator (point or distributed force), sensor, vibration target |
| Design | actuator-input targeting; stepped / conical / exponential horns; periodic rods with Bloch band structure |
| Coupling | one-way radiation of plates and membranes into the acoustic domain |
| Data | measured frequency responses from CSV, compared with simulation |

## What does not exist

Shell elements, curved or connected structures (frames of several members,
trusses, assemblies), layered and composite structures, orthotropic and
anisotropic materials, viscoelasticity, frequency- or temperature-dependent
properties, plasticity, contact, fracture, prestress other than membrane
tension, geometric nonlinearity, prescribed displacement or velocity
excitation, physical actuator response models, mesh generation for arbitrary
geometry (structured meshes of boxes, cylinders, planes and rods only),
two-way fluid-structure interaction, hardware acquisition, 2D/3D phononic
crystals and locally resonant metamaterials, GPU and threaded backends.
