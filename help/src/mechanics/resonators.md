---
id: mechanics.resonators
title: Resonators
category: Mechanical Engineering
order: 80
status: available
summary: Horns that transform vibration velocity, periodic rods with stop bands, and plates and membranes used as resonant structures.
keywords: [horn, velocity transformer, periodic rod, phononic, band gap, Bloch, resonance]
related: [mechanics.modal, mechanics.waves, mechanics.forced-response, mechanics.optimization, concepts.manipulators]
---

Resonant has two mechanical wave-manipulating objects: the **horn**, a
member of varying section that resonates and amplifies motion, and the
**periodic rod**, a one-dimensional phononic structure with stop bands.
Plates and membranes made structural also serve as resonators. All are
ordinary structures analysed with the Mechanics panel.

## Horns

**Mechanics > Add Horn** adds a stepped steel horn, idealized as a free
beam. Its geometry, in the Properties panel:

| Parameter | Meaning |
|---|---|
| Input radius (-Z end) | radius at the driven end |
| Output radius (+Z end) | radius at the working end |
| Length (Z) | total length |
| Profile (0 conical, 1 exponential, 2 stepped) | how the radius changes |

The horn is analysed as one beam with the section of each element's
midpoint, so the impedance ρ c A changes along it and waves reflect where
the section changes. A stepped horn of two quarter-wave halves resonates
in its half-wave axial mode and amplifies the end displacement by the
area ratio:

```math
f = c / (2L),      u_out / u_in = −(r_in / r_out)²
```

c = √(E/ρ) is the rod wave speed, L the length and r the end radii; the
minus sign says the ends move in opposite directions.

**Worked example.** Open `examples/mechanics/h-stepped-horn.resonant`: a
titanium-alloy horn 250 mm long, 20 mm to 10 mm radius. Calculate 16 modes
(the first six are rigid-body modes at 0 Hz). The axial half-wave mode is
at c / (2L) = 5073 / 0.5 = 10.15 kHz and its shape shows a displacement
gain of 4 with the ends in opposition (validated within 0.5 % in
frequency and 2 % in gain).

## Periodic rods

**Mechanics > Add Periodic Rod** adds a steel rod of alternating segments:
**Radius of segment A**, **Length of segment A**, **Radius of segment B**,
**Length of segment B** and **Cells**. As a structure it is a beam.

In the **Periodic Rod and Sound** tab, select the rod in the Structure
list and press **Band Structure of the Selected Periodic Rod**. The unit
cell (24 rod elements per segment) is solved with the Bloch condition

```math
u(x + a) = e^{jqa} u(x),      a = l_A + l_B
cos(qa) = cos(k l_A) cos(k l_B) − ½ (Z_A/Z_B + Z_B/Z_A) sin(k l_A) sin(k l_B)
```

for wavenumbers qa/π from 0 to 1. k = ω/c is the wavenumber in the
material and Z ∝ A the segment impedance. The plot shows the longitudinal
bands in kHz; the note lists the **stop bands**, frequencies where no
longitudinal wave propagates (where the right side of the second line
exceeds 1 in magnitude).

**Worked example.** `examples/mechanics/g-phononic-rod.resonant` has 8
cells of steel, 10 mm / 4 mm radius, 60 mm / 40 mm long. The band
structure gives stop bands of 12 428 - 36 627 Hz and 44 266 - 58 608 Hz;
the dispersion relation gives 12 428 - 36 612 Hz and 44 232 - 58 563 Hz
(within 0.1 %). The first gap contains the Bragg frequency c / (2a) =
25.2 kHz. Then run **Calculate Response** (1 - 45 kHz, loss factor 0.002):
in the middle of the first stop band the response at the far end is below
1/1000 of the pass-band response below it. A band gap is never claimed from
a low response alone; the Bloch analysis is the evidence.

Equal segment lengths close every second gap; the example uses unequal
lengths for that reason.

## Plates and membranes as resonators

There is no dedicated resonant-plate or resonant-membrane object: make a
box or plane a plate or membrane and use [Modal analysis](modal.md)
and [Forced response](forced-response.md). A membrane's
frequencies follow from its tension:

```math
f_mn = (c / 2) · √((m / a)² + (n / b)²),      c = √(T / (ρ h))
```

## Common mistakes

- **Reading rigid-body modes as resonances.** Horns and rods are free; skip
  the modes near 0 Hz.
- **Expecting bending gaps.** The band structure is longitudinal only.
- **Coarse elements on a periodic rod.** Choose an element size that divides
  the segment lengths so segment boundaries fall on element boundaries.

## Limitations

- 1D rod theory: lateral inertia of the segments is neglected, which
  matters when the radius is not small against the wavelength.
- Bending and torsional band structures, 2D and 3D phononic crystals and
  locally resonant metamaterials are not implemented.
- Impedance transformers other than horns, isolation mounts and resonant
  plate or membrane design objects are not implemented.
- To tune a resonator automatically, see
  [Mechanical optimization](optimization.md).
