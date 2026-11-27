---
id: acoustics.materials
title: Acoustic materials
category: Acoustic Engineering
order: 20
status: available
summary: The propagation medium of a simulation and the acoustic properties of object materials, and which solver uses which.
keywords: [medium, air, water, density, speed of sound, attenuation, impedance]
related: [concepts.materials, acoustics.physics, acoustics.fdtd, acoustics.lenses]
---

Two different things are called "material" in acoustics, and Resonant keeps
them apart:

- the **propagation medium**: the fluid the sound travels through, chosen
  once for the whole project in the **Acoustics** panel;
- the **material of an object**: assigned to a scene object with
  **Materials > Assign to Selection**, and used only by the solvers that let
  objects take part.

The medium is never taken from the material of an object, and assigning
water to a box does not put the scene under water.

## The propagation medium

Open the **Acoustics** panel (**View > Panels > Acoustics**), tab
**Medium**.

| Field | Meaning |
|---|---|
| **Medium** | *Air - ideal gas model*, *Water - nominal values* or *Custom* |
| **Temperature** | degC; sets density and speed of sound of the air model |
| **Density** | kg/m³; editable for *Custom* only |
| **Speed of sound** | m/s; editable for *Custom* only |
| **Attenuation** | Np/m, one amplitude coefficient at the simulation frequency (0 by default) |
| **Impedance** | ρc, shown for information |

The air model is theoretical dry air at 101 325 Pa:

```math
c = √(γ R T),      ρ = p₀ / (R T)
γ = 1.4,   R = 287.05 J/(kg·K),   p₀ = 101 325 Pa
```

`T` is the absolute temperature (K). At 20 °C this gives `c` = 343.23 m/s and
`ρ` = 1.204 kg/m³. Humidity and absorption are not modelled. The water
values (998 kg/m³, 1482 m/s) are fixed illustrative values; temperature,
salinity and pressure do not change them. The text under the fields states
the basis of the values in use.

Attenuation is applied by the analytical models only when **Apply
attenuation** is checked (Field model group), as `e^(−αr)`.

## Acoustic properties of a material

**Materials > Material Library...** (Ctrl+M) lists the project's materials.
Choose **Physics domain: Acoustic** to see:

| Property | Unit | Used by |
|---|---|---|
| Density | kg/m³ | full-wave solver, reflector impedance, lens estimate |
| Speed of sound | m/s | full-wave solver, reflector impedance, lens estimate |
| Attenuation coefficient | Np/m | full-wave solver (optional) |
| Absorption coefficient | (none) | stored only; no solver uses it |
| Reference temperature | K | stored with the values |

Each value carries its provenance. The example library's values are marked
*illustrative*: nominal figures for trying the application, not
authoritative data. Replace them with measured or cited values before
engineering use.

## Which solver uses what

| Solver | Medium | Object materials |
|---|---|---|
| Analytical free field | yes | ignored (objects do not affect the field) |
| Free field + reflectors | yes | a reflector set to *Impedance of the assigned material* uses Z = ρc of its material |
| Full-wave (FDTD) | yes, as material 0 | every object with density and speed of sound becomes a fluid region of that material; objects without them are rigid |

In the full-wave solver **solids are fluids** with their density and
longitudinal speed of sound: shear waves, mode conversion and structural
resonance are not represented. For stiff objects in air this matters little
(almost everything is reflected); for solids in water it does. A material's
attenuation `α` becomes the loss rate `γ = 2αc`, the same at every
frequency.

## Values in the example library

| Material | Density (kg/m³) | Speed of sound (m/s) |
|---|---|---|
| Air | 1.204 | 343 |
| Water | 998 | 1482 |
| Acrylic | 1180 | 2730 |
| Aluminum | 2700 | 6320 |
| Steel | 7850 | 5900 |
| Glass | 2500 | 5600 |
| Rubber | 1100 | 1600 |
| Concrete | 2400 | 3700 |

The library also has copper, wood (along the grain), stainless steel, brass,
titanium alloy, ABS, PLA, nylon, polycarbonate and alumina ceramic. For
solids the speed is the longitudinal bulk-wave speed.

## Worked example: water into acrylic

`examples/advanced_acoustics/d-material-interface.resonant` sends a 100 kHz
Gaussian pulse through water (*Water - nominal values*) into an acrylic
block whose face is at z = 0.

```math
Z₁ = 998 · 1482 = 1.48 × 10⁶ Pa·s/m,      Z₂ = 1180 · 2730 = 3.22 × 10⁶ Pa·s/m
R = 0.371,      T = 1.371
```

Run it with **Simulation > Run Full-Wave Simulation** (F6). The **Time
Signals** tab shows the reflected pulse at the probe in the water and the
transmitted pulse at the probes 20 mm and 70 mm into the acrylic; the
validation finds the speed in acrylic within 3 % and the reflected amplitude
as the normal-incidence coefficient predicts.

## Common mistakes

- Expecting an object's material to change an analytical result: the free
  field model ignores every object.
- Leaving a lens or block without acoustic density or speed of sound: the
  full-wave solver then simulates it as rigid and says so in its preflight.
- Taking the illustrative library values as data.

## Limits

One homogeneous medium at rest; no field of material properties (a gradient
must be built from layers of objects); no frequency-dependent, dispersive or
anisotropic materials; up to 254 materials in one full-wave run. See
[Materials](../core-concepts/materials.md) for editing and assigning materials.
