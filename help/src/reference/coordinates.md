---
id: ref.coordinates
title: Coordinate conventions
category: Technical Reference
order: 20
status: available
summary: The world axes, how object positions and rotations are defined, and the local axes that emitters, arrays, antennas, regions and structures use.
keywords: [coordinates, axes, rotation, Euler angles, local frame, spherical angles]
related: [gs.units, ref.units, ref.phase, ui.viewport, concepts.regions]
---

Resonant uses one **right-handed** world frame: **X** horizontal, **Y**
depth and **Z** vertical (up). In the viewport the X axis is drawn red, Y
green and Z blue, and the ground grid lies in the XY plane at `z = 0`.
**Analysis > Diagnostics > Conventions** states the axes and the rotation
convention, and every project file records them in its `units` block.

## Standard views

| View | Shortcut | Camera |
|---|---|---|
| **View > Top (looking down Z)** | Ctrl+1 | looks along −Z onto the XY plane |
| **View > Front (looking along +Y)** | Ctrl+2 | looks along +Y onto the XZ plane |
| **View > Right (looking along -X)** | Ctrl+3 | looks along −X onto the YZ plane |

## Position, rotation and scale

The **Transform (relative to parent)** group of the Properties panel holds
**Position**, **Rotation** and **Scale**. They are relative to the parent
group, or to the world for an object at the top level. An object's local
coordinates map to its parent's by

```math
x_parent = T · R · S · x_local
R = R_z(γ) · R_y(β) · R_x(α)
```

- `S` scales each local axis by its **Scale** factor, before the rotation.
- `R` is the rotation: **Rotation** `(α, β, γ)` in degrees about X, Y and Z.
  The point is rotated about the fixed parent axes first about X, then
  about Y, then about Z (intrinsic Z-Y-X Euler angles). The project file
  stores the angles in radians as `[x, y, z]`.
- `T` moves the result by **Position** (in the display length unit; stored
  in metres).

**Worked example.** An emitter radiates along its local +Z axis. With
**Rotation** (0, 90, 0) deg it radiates along world +X; with (−90, 0, 0)
deg along world +Y. Rotations are not commutative: (90, 90, 0) is not the
same as (0, 90, 90).

## Local axes of each kind of object

| Object | Local convention |
|---|---|
| acoustic emitter | radiates along local +Z; directivity angle θ is measured from +Z |
| linear array | elements along local X, centred on the origin |
| planar and circular arrays | elements in the local XY plane, centred on the origin |
| cylindrical array | rings in the XY plane, stacked along local Z |
| antennas and RF arrays | spherical angles θ from local +Z, φ from local +X towards +Y |
| field region | centred on the object and turned with it; see below |
| mechanical actuator, sensor, vibration target | force, measured response and wanted displacement along local +Z |
| beam | along the **Axis** chosen (*Automatic*, *Local X*, *Local Y*, *Local Z*); cylinders and rods along local Z |
| plate, membrane | a plane: its own XY plane; a box: normal to the **Axis** chosen, or across its thinnest dimension with *Automatic* |

## Field regions and their sample axes

A field region's **Kind** is *Plane XY*, *Plane XZ*, *Plane YZ* or
*Volume*, in the region's own frame, so a rotated region samples an
arbitrary plane. **Size X**, **Size Y** and **Size Z** are its extent along
its local axes (the unused one is ignored for a plane). Samples are spaced
evenly from edge to edge, the edges included.

| Kind | U axis | V axis | W axis |
|---|---|---|---|
| Plane XY | local X | local Y | - |
| Plane XZ | local X | local Z | - |
| Plane YZ | local Y | local Z | - |
| Volume | local X | local Y | local Z |

For the full-wave solvers the region is the computational domain. A plane
region gives a **two-dimensional** problem: nothing varies along the
plane's normal, emitters become line sources and objects infinitely long
cylinders with the cross-section the plane cuts.

## Radiation pattern cuts

The **Cut** list of the **Pattern** tab uses the antenna's or array's local
frame: *Elevation, phi = 0 (XZ plane)*, *Elevation, phi = 90 deg (YZ plane)*
and *Azimuth, theta = 90 deg (XY plane)*. Steering directions (**Steer to
theta**, **Steer to phi**) and the beams and nulls of beam synthesis use the
same angles.

## Mechanical supports

The **Support** fields name the ends, edges or faces in the structure's
local frame: *Start of the axis* and *End of the axis* for a beam,
*Edge at −axis 1* to *Edge at +axis 2* for a plate, *Face −X* to *Face +Z*
for a solid. A membrane is held at every edge.

## Experiments

Sensor and probe positions in an experiment are poses: a position in metres,
a unit direction and the name of its frame (`scene` by default), with a
stated position and orientation uncertainty. The **Probe** of the
Electromagnetics **Fields** tab takes world coordinates.

## Related topics

- [Units and coordinate systems](../getting-started/units.md)
- [Units](units.md)
- [Phase conventions](phase.md)
- [Viewport controls](../ui/viewport.md)
