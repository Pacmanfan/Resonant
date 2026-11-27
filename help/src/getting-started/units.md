---
id: gs.units
title: Units and coordinate systems
category: Getting Started
order: 70
status: available
summary: SI units inside, your choice of length unit on screen, the right-handed Z-up world, local object frames, rotation order and the phase convention.
keywords: [units, SI, millimetres, coordinates, axes, rotation, phase convention]
related: [ref.units, ref.coordinates, ref.phase, concepts.geometry, ui.property-editors]
---

Every number in Resonant has a unit. Internally, in the project file and in
every solver, values are **SI**: metres, seconds, hertz, kilograms,
pascals, radians. The user interface converts only at one boundary: what
you see and type can be in another unit, what is stored is SI.

## The display length unit

**Tools > Display Length Unit** chooses how lengths are shown and typed:
*Metres (m)*, *Centimetres (cm)*, *Millimetres (mm)* or *Inches (in)*. It
applies to positions in the Properties panel, geometry parameters, array
spacings, region sizes and the other length fields of the editors; each
field shows the unit next to it. Changing it never changes the scene, only
how it is displayed. Your choice is remembered between sessions; it is not
part of the project.

```math
1 cm = 0.01 m     1 mm = 0.001 m     1 in = 0.0254 m
```

A few fields always use a fixed unit, written next to them (for example the
acoustic *Minimum distance* in mm and the electromagnetic line dimensions
in mm).

## Units shown in the panels

| Quantity | Shown as | Stored as |
|---|---|---|
| Rotation, phase, steering angles | degrees (deg) | radians |
| Acoustic and mechanical frequency | Hz | Hz |
| Electromagnetic frequency | MHz | Hz |
| Acoustic time delay | microseconds (us) | s |
| Electromagnetic time delay and simulated time | ns | s |
| Mechanical time delay; mechanical and acoustic full-wave simulated time | ms | s |
| Medium temperature | degC | K |
| Pressure | Pa, peak amplitude | Pa |
| Sound pressure level | dB re 20 µPa in air, re 1 µPa in water | derived from the pressure |
| Force, membrane tension | N, N/m | N, N/m |
| Wanted vibration displacement | micrometres (um) | m |
| Material properties | SI, in the *Value (SI)* column | SI |

## World axes

The world is **right-handed** with **Z up**: X horizontal, Y depth, Z
vertical. The ground grid lies in the XY plane at Z = 0. The axes are drawn
in red (X), green (Y) and blue (Z, upwards only); the brighter half of the
X and Y axes is the positive side.

## Object frames and transforms

Every object has a **local frame**. Its transform, edited in
**Properties > Transform (relative to parent)**, places that frame in its
parent (a group, or the world):

- **Position:** the origin of the local frame, in the display length unit.
- **Rotation:** angles about X, Y and Z in degrees, applied in that order about the fixed parent axes (intrinsic Z-Y-X Euler angles).
- **Scale:** a factor along each local axis, applied before the rotation.

```math
R = R_z(γ) · R_y(β) · R_x(α)
x_parent = p + R · (S · x_local)
x_world = M_parent · x_parent
```

`α`, `β`, `γ` are the rotations about X, Y and Z, `S` the scale, `p` the
position and `M_parent` the world transform of the parent group.
Rotations compose with the parent's, so rotating a group turns its
contents with it.

Many objects use their local axes to mean something:

| Object | Local convention |
|---|---|
| Box, plane, disc, cylinder, cone, ellipsoid | centred on the origin; plane and disc lie in local XY; cylinder and cone along local Z |
| Acoustic emitter, array element | radiates along local +Z; directivity angle measured from it |
| Linear / planar acoustic or RF array | elements along local X / in local XY, centred |
| Field plane | *Plane XY*, *Plane XZ* or *Plane YZ* of its local frame, centred on the object |
| Dipole and monopole antenna, electric dipole source | along local +Z, fed at the origin |
| Plane-wave source | travels along local +Z with E along local +X |
| Mechanical actuator, sensor, vibration target | act or measure along local +Z |
| Parabolic reflector | axis along local +Z, focus at (0, 0, f) |

## Amplitudes and phase

All harmonic results use the time convention `e^{+jωt}` with **peak**
amplitudes:

```math
p(t) = Re{ P · e^{jωt} },    ω = 2π f,    p_rms = |P| / √2
```

A positive phase leads; a time delay `τ` multiplies a source by
`e^{−jωτ}`. An outgoing wave varies as `e^{−jkr} / r` with `k = ω / c`.
Electromagnetic angles: θ from local +Z, φ from local +X towards +Y.
Details: [Phase conventions](../reference/phase.md).

## Common mistakes

- **Treating Y as up.** Z is up; *Top* (Ctrl+1) looks down the Z axis.
- **Typing in the wrong unit.** With metres displayed, `5` is 5 m, not 5 mm.
- **Reading radians in a project file as degrees.** Files store radians: a rotation of 90 degrees is `1.5708`.
- **Expecting a different rotation order.** X is applied first, then Y, then Z, all about the parent's fixed axes.
- **Comparing peak and RMS values.** Pressures are peak amplitudes unless the column says RMS.

Reference tables: [Units](../reference/units.md) and
[Coordinate conventions](../reference/coordinates.md).
