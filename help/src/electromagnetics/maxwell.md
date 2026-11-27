---
id: em.maxwell
title: Maxwell's equations
category: Electromagnetic Engineering
order: 10
status: available
summary: The equations behind every electromagnetic result, the phasor and sign conventions, the units and which model produced a number.
keywords: [maxwell, phasor, convention, permittivity, wavenumber, impedance, poynting]
related: [em.materials, em.solvers, em.near-far-field, ref.phase, ref.units]
---

All electromagnetic calculations in Resonant solve Maxwell's equations,
either exactly for an idealized source (the analytical models) or
numerically on a grid (the Yee solver). This page states the equations
and the conventions that every panel, report and exported file uses.

## The equations

In a linear, isotropic medium with permittivity ε, permeability μ and
conductivity σ:

```math
∇ × E = −μ ∂H/∂t
∇ × H = ε ∂E/∂t + σE + J
```

E is the electric field (V/m), H the magnetic field (A/m) and J the source
current density (A/m²). With the time dependence e^{+jωt} the phasor form
is

```math
∇ × E = −jωμ H
∇ × H = jω ε_c E + J,      ε_c = ε₀ εr (1 − j tan δ) − j σ / ω
```

where ω = 2πf, εr is the relative permittivity and tan δ the dielectric
loss tangent. A plane wave in the medium has

```math
k = ω √(μ ε_c),      η = √(μ / ε_c),      λ = 2π / Re k
```

the wavenumber k (its imaginary part is negative in a lossy medium), the
wave impedance η and the wavelength λ. The time-averaged power density is
the real part of the complex Poynting vector:

```math
S = ½ Re{E × H*}
```

## Conventions

| Item | Convention |
|---|---|
| Units | SI throughout: V/m, A/m, W/m², ohm, S/m, Hz, m, s |
| Time dependence | e^{+jωt}: a wave along +z is e^{−jkz}; an inductor's reactance is positive |
| Phasors | peak values: a feed current of 1 A means i(t) = 1 A · cos(ωt + phase); power is ½ Re{V I*} |
| Angles | θ from the object's local +Z, φ from local +X towards +Y; dipoles lie along local +Z |
| Levels | fields 20 log10, powers 10 log10; directivity and gain in dBi; return loss positive |

The constants are μ₀ = 1.25663706212 × 10⁻⁶ H/m (CODATA 2018),
c = 299 792 458 m/s, ε₀ = 1 / (μ₀ c²) and η₀ = √(μ₀/ε₀) = 376.730 ohm.
At 1 GHz the free-space wavelength is 299.8 mm and a half-wave dipole is
149.9 mm long. See [Phase conventions](../reference/phase.md) and
[Units](../reference/units.md) for the conventions shared with acoustics and
mechanics.

## Which model produced a number

Every report and result names its model:

| Kind | Used for | What it is not |
|---|---|---|
| Analytical | dipole, monopole and small-loop antennas, ideal sources, lines, guides, cavities | a computation of the current on a real antenna |
| Array factor | RF arrays | a model of coupling between elements |
| Full-wave numerical (Yee FDTD) | a field region with antennas, sources and bodies | exact: it has dispersion and staircase error |
| Design estimate | patch, horn and helical antennas | a calculated field or impedance |
| Measured | imported Touchstone data | merged with a model |

Field results from the analytical models and the Yee solver are absolute
(V/m for the stated feed current or source amplitude). Patterns are
normalized (|E| / max) and say so; the field display states whether its
colour scale is normalized or absolute.

## Working in the application

1. **Electromagnetics > Add ...** places an antenna, source, array or guide
   at the origin, selects it and shows it in the **Electromagnetics** panel.
2. The **Properties** panel edits the feed, the source, the array layout or
   the guide's frequency of interest; every edit is undoable.
3. The panel's report and its tabs (**Impedance**, **Pattern**, **Array**,
   **Fields**, **Lines**) give the analysis.

Start with [Antennas](antennas.md); the numerical solver is
described in [Electromagnetic solvers](solvers.md).

## Limitations

- Linear, isotropic, non-dispersive media at the frequency of use (values
  may be tabulated over frequency; Debye, Drude and Lorentz models are not
  implemented).
- No magnetic materials with loss, no anisotropic or nonlinear media.
