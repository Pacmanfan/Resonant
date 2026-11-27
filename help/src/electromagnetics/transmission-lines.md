---
id: em.transmission-lines
title: Transmission lines
category: Electromagnetic Engineering
order: 90
status: available
summary: Characteristic impedance, effective permittivity and wavelength of coaxial, microstrip, stripline and coplanar lines in the Lines tab.
keywords: [transmission line, characteristic impedance, coaxial, microstrip, stripline, coplanar, quarter wave]
related: [em.impedance, em.waveguides, em.s-parameters, em.materials]
---

A transmission line carries a signal between a source and a load. Its
characteristic impedance Z₀ and the wavelength on it decide how it
transforms a load and how long a quarter-wave section must be. The
**Lines** tab of the Electromagnetics panel is a calculator for four common
lines; it needs no scene object.

## Using the Lines tab

1. Choose the **Line**. The text box names the two dimensions for it.
2. Enter **Dimension 1** and **Dimension 2** (mm), the **Relative
   permittivity** of the dielectric and the **Frequency** (MHz).
3. Press **Calculate Line**.

| Line | Dimension 1 | Dimension 2 | Model |
|---|---|---|---|
| Coaxial line (exact) | Inner conductor radius | Outer conductor inner radius | exact TEM, perfect conductors |
| Microstrip (Hammerstad-Jensen) | Strip width | Substrate height | about 1 % for 0.01 < w/h < 100 |
| Stripline (exact, zero-thickness strip) | Strip width | Ground plane spacing | exact conformal map for a strip of zero thickness |
| Coplanar waveguide (exact, infinite substrate) | Centre strip width | Gap | exact conformal map on an infinitely thick substrate |

The result gives Z₀ (ohm), the effective permittivity, the phase velocity
as a fraction of c, the wavelength on the line at the frequency and a
quarter of it, and names the model.

## The formulas

```math
Z₀ = (η₀ / (2π √εr)) · ln(b / a)                 coaxial line, radii a < b
Z₀ = 30π · K(k′) / (√εr · K(k))                   stripline (elliptic integrals K)
v_p = c / √ε_eff,      λ_line = v_p / f          phase velocity, wavelength on the line
```

η₀ = 376.73 ohm is the impedance of free space and ε_eff the effective
permittivity (equal to εr for the coaxial line and stripline; between 1
and εr for microstrip and coplanar lines, whose field is partly in air).
The elliptic integrals are computed by the arithmetic-geometric mean
(1e-15).

## Worked example

Choose *Coaxial line (exact)*, enter 0.45 mm and 1.47 mm (the dimensions
of the object **Electromagnetics > Add Coaxial Line** creates), frequency
1000 MHz:

| Relative permittivity | Z₀ | Phase velocity | Wavelength | Quarter wave |
|---|---|---|---|---|
| 1 (air) | 70.98 ohm | 1.000 c | 299.79 mm | 74.95 mm |
| 2.1 (PTFE-like) | 48.98 ohm | 0.690 c | 206.88 mm | 51.72 mm |

## The coaxial line object

**Electromagnetics > Add Coaxial Line** places a coaxial section (inner
radius 0.45 mm, outer radius 1.47 mm, 50 mm long) along local Z. Its
report gives the TEM characteristic impedance of its filling (vacuum
without a material) and the cut-off of the first higher mode, TE11, from
the approximation k_c = 2 / (a + b): 49.7 GHz for the empty line. Above
that frequency the line is no longer single-mode. See
[Waveguides](waveguides.md) for the other guide objects.

## Calculations that are not in the panel

The engine also implements, and its tests validate (1e-9), the following
line calculations, but neither the Lines tab nor the Python API offers
them yet. Use the formulas by hand until they are exposed:

```math
Z_in = Z₀ · (Z_L + Z₀ tanh(γl)) / (Z₀ + Z_L tanh(γl))     input impedance of a loaded line
Z_T = √(Z₀ R_L)                                          quarter-wave transformer
α_d = k tan δ / 2                                        dielectric loss of a TEM line (Np/m)
```

Z_L is the load, l the line length, γ = α + jβ the propagation constant,
R_L a resistive load and tan δ the dielectric's loss tangent.
L-network matching (series reactance and shunt susceptance) and the
conductor loss of a coaxial line are likewise engine-only.

## Common mistakes

- **Diameters instead of radii.** The coaxial dimensions are radii; the
  inner radius must be smaller than the outer one, or the calculation is
  refused.
- **Microstrip outside its range.** Very wide or very narrow strips
  (w/h outside 0.01 - 100) leave the formula's accuracy.
- **Expecting frequency dispersion.** The microstrip result is the
  quasi-static value; the frequency only sets the wavelength.

## Limitations

- Perfect conductors and lossless dielectrics in Z₀; strip thickness zero
  for the stripline; no surface roughness.
- No circuit simulation of lines with stubs or networks.
