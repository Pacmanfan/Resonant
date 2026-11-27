# Mechanical Focusing, Actuators and Wave Manipulation

**Status:** current for REQ-CHAT-20261126-006.

## Actuator

A scene object (`mechanical.actuator`) with: excitation (point force at the
nearest node, or a force spread evenly over the nodes within a radius),
amplitude (N), phase, time delay, a signal over time, active state, the
structure it acts on (or the nearest), and optionally the name of a
physical device. The force acts along the actuator's local +Z axis.

**The drive and the force are distinct, and only the force is modelled.**
A named device (shaker, piezo stack, voice coil) is recorded, and the
study notes that its response is not modelled: the force is applied as
set. Prescribed displacement and velocity excitations are not implemented.

An actuator is attached when it lies within `2 x element size + half the
section or thickness` of the structure; otherwise a named actuator is
reported as not attached. A force component the idealization cannot carry
(in-plane forces on a plate or membrane) is reported with its fraction.

## Sensors and vibration targets

`mechanical.sensor`: records the response along its local +Z axis, with
calibration fields for measured data. `mechanical.target`: a wanted peak
displacement and phase along its +Z axis, a weight, or a quiet point
(wanted zero).

## Targeting (`Targeting.h`)

At one frequency the structure is linear: `t = H f`, with
`H[i][j] = s_i^T D(omega)^-1 p_j` — target selector, dynamic stiffness,
actuator load pattern — calculated with one factorization of `D`. The
complex forces minimize

```text
|| W (H f - t_wanted) ||^2 + (lambda sigma_max)^2 || f ||^2
```

(weighted, Tikhonov-regularized least squares by SVD). The result lists
each actuator's amplitude and phase, the achieved value at every target,
the relative residual and the singular values, and notes when there are
more targets than actuators or the transfer matrix is nearly singular.
Nothing guarantees a wanted pattern is reachable. "Apply" writes the
inputs into the actuators as one undo step.

Example F: four actuators on a simply supported aluminium plate, one target
at 10 um and two quiet points, at 150 Hz with loss factor 0.01: the stored
inputs give 10 um within 1 % and the quiet points below 1 % of it; driven
plainly (1 N in phase) the quiet points move more than 10 % of the target.

Structural-parameter optimization (changing the geometry to meet targets)
is the subject of request 009.

## Mechanical wave manipulators

Geometry kinds of the mechanical module:

* `rod.horn` — velocity transformer: conical, exponential or stepped from
  an input radius to an output radius. Analysed as a beam with a section
  per element. A stepped horn of two quarter-wave halves resonates at
  `c / (2L)` and amplifies the end displacement by the area ratio; the
  test reproduces both (0.5 % and 2 %) for a 20 / 10 mm titanium horn.
* `rod.periodic` — `PHONONIC_STRUCTURES.md`.

Not present as structural objects: resonant plates and membranes as
dedicated designs (a plate or membrane structure serves), impedance
transformers other than horns, isolation mounts, locally resonant
metamaterials.
