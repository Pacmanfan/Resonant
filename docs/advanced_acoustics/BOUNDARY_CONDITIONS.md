# Boundary Conditions and Absorbing Layers

**Status:** current for REQ-CHAT-20261126-005.

Two different things are called boundaries: the **faces of the
computational domain**, and the **surfaces of objects** inside it.

## Faces of the domain

Each of the four (2D) or six (3D) faces has one of:

| Kind | Condition at the face | Implementation |
|---|---|---|
| Rigid | normal velocity = 0 | the face velocity is held at zero |
| Pressure release | pressure = 0 | a mirror cell of opposite pressure: the face velocity sees `2 p / dx` |
| Impedance | `p = Z v_n`, locally reacting | `rho dv/dt = -(Z v - p) / (dx/2)`, with `Z v` as the mean of old and new value |
| Absorbing | open space | a perfectly matched layer outside the region, backed by a rigid wall |

In the application one kind is chosen for all faces; the core accepts a
kind per face. A rigid wall is also the symmetry plane of a field that is
even across it. Periodic boundaries are not implemented.

Measured with a plane pulse at normal incidence (85 cells per wavelength):
rigid `R = +1` and pressure release `R = -1` within 1 %; impedance
`Z = 3 rho c` gives `0.5`, `rho c / 3` gives `-0.5`, `rho c` gives `0`, a
very large `Z` gives `+1`, all within 0.01.

The impedance boundary is first-order accurate and locally reacting: its
reflection at oblique incidence follows `(Z cos(theta) - rho c) /
(Z cos(theta) + rho c)`, so `Z = rho c` absorbs fully only at normal
incidence. For open space use the absorbing layers.

## Absorbing layers (PML)

A split-field perfectly matched layer for the first-order acoustic system:
in the layer the pressure is split into one part per direction, and each
part and its velocity component are damped along their own direction only,

```text
vx  <-  a (a vx - (dt / (rho dx)) dpx)          a = exp(-sigma_x dt / 2)
px  <-  a (a px - (K dt / dx) dvx)              p = px + py (+ pz)
```

with `sigma(d) = sigma_max (d / L)^2` rising from the inner edge of the
layer (`d` depth, `L` thickness) and `sigma_max = -3 c ln(R0) / (2 L)` for
a design reflection `R0 = 1e-6` at normal incidence. In the continuous
equations such a layer reflects nothing at any angle; the discrete layer
reflects a little because of the steps in `sigma`.

The layer is added **outside** the region (default 12 cells; 4 to 200), so
the region is not shortened. Material interfaces continue into it.

Measured:

| Test | Result |
|---|---|
| Plane pulse, normal incidence, 12 cells | reflected / incident below 0.2 % (−54 dB) |
| 24 cells | below 0.05 %; 8 cells reflects more than 12 |
| Point pulse in a 60-cell square domain with layers, against a domain too large for anything to return, at probes near a wall, near a corner and in the middle | difference below 1 % of the peak throughout |
| Energy left after the pulse has gone | below 1e-6 of its maximum |
| Same small domain with rigid walls | differs from open space by more than 50 % (the test can tell them apart) |

Limits: the layer is designed and tested for propagating waves. Evanescent
fields (very close to sources or to sub-wavelength structures) and waves at
grazing incidence are absorbed less; keep sources and structures a few
cells away from the layers. Split-field layers can grow slowly over very
long runs; none was seen in the validation runs (up to about 2 000 steps
with layers), longer runs with layers were not tested, and the energy
record would show a growth.

## How to tell a physical reflection from a boundary artefact

The energy record falls as sound leaves an open domain; change the layer
thickness and run again — a physical reflection does not move, a boundary
artefact does.

## Surfaces of objects

Cells of an object are one of:

* **Rigid** — no motion through any of its faces.
* **Pressure release** — pressure held at zero.
* **Material** — a fluid with the object's density and speed of sound
  (`MATERIAL_INTERACTIONS.md`).

A wave manipulator's boundary setting chooses among them: rigid, pressure
release, or "impedance", which here means the material itself (the wave
enters it — more than a locally reacting surface). An **ideal absorbing**
manipulator surface cannot be represented on the grid: it is simulated as
rigid and the preflight says so in capitals. A lens is always its
material. Other objects are their material if it has an acoustic density
and speed of sound, rigid otherwise.
