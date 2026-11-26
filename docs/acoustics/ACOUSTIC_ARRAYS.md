# Acoustic Arrays

**Status:** current for REQ-CHAT-20261126-003.

An **acoustic array** is one scene object holding a parametric layout and
the default settings of its elements. Its elements are generated from those
parameters every time they are needed; they are not separate scene objects.
The array's transform places and orients the whole array.

## Layouts (local frame of the array)

| Layout | Elements | Parameters |
|---|---|---|
| Linear | along X, centred | count X, spacing X |
| Rectangular planar | in the XY plane, centred; index `ix + countX·iy` | count X, count Y, spacing X, spacing Y (independent) |
| Circular | a ring in the XY plane | count (per ring), radius |
| Cylindrical | rings stacked along Z, centred | count per ring, rings, radius, ring spacing |
| Spherical | points on a sphere | count, radius |
| Custom | manually placed | a list of positions |

Elements face along +Z (axial), away from the axis or centre (outward) or
towards it (inward).

**Spherical layout:** a Fibonacci lattice. It is nearly uniform but not
exactly equal-area, has no symmetry, and covers the whole sphere (no cap or
opening can be specified yet).

## Element defaults and per-element overrides

The array stores one set of element defaults (frequency, amplitude, phase,
delay, directivity, radius) and a table of **overrides by element index**:
amplitude, phase, delay, frequency and active state, each optional.

* An element's settings are the defaults with its override applied.
* Changing a layout parameter (count, spacing, radius, layout) regenerates
  positions and keeps the override table.
* Changing a default changes every element that does not override it.
* An override whose index is beyond the current element count is kept and
  ignored; it applies again if the array grows back.
* "Clear per-element settings" removes the table; it is undoable.

Overrides are keyed by index, so after a change of layout or counts an
override stays with its number, not with a position in space.

Focusing and inverse design write their results into the override table
(phases or delays, and amplitudes when a taper or an inverse solution sets
them).

## In the user interface

The array editor shows the parameters, the element defaults and a table of
up to 512 elements in which a row's amplitude, phase, delay and active state
can be edited; for a custom layout the positions are editable too and the
count sets how many elements exist.
