---
id: em.impedance
title: Impedance
category: Electromagnetic Engineering
order: 70
status: available
summary: The feed impedance of dipoles, monopoles and small loops, its match to a reference impedance, and impedance sweeps over frequency.
keywords: [impedance, reactance, VSWR, return loss, matching, resonance, induced EMF]
related: [em.s-parameters, em.antennas, em.transmission-lines, opt.studio]
---

The feed impedance decides how much of a transmitter's power an antenna
accepts. Resonant calculates it analytically for dipoles, monopoles and
small loops, compares it with the antenna's **Reference impedance** and
sweeps it over frequency.

## The models

- **Dipole**: the induced-EMF formulas for a sinusoidal current give the
  impedance at the current maximum, which is referred to the feed by
  dividing by sin²(kl/2) (k = 2π/λ, l the length).
- **Monopole**: half the impedance of the dipole of twice its height, over
  an infinite perfect ground plane.
- **Small loop**: the radiation resistance R = 20π² (C/λ)⁴ (C the
  circumference) plus the inductive reactance of the loop.
- **Patch, horn, helix**: no impedance is calculated (design estimates
  only; the helix report estimates an input resistance of about 140 C/λ).

For a half-wave dipole the induced-EMF impedance is the textbook value

```math
Z = (η₀ / 4π) (γ + ln 2π − Ci 2π) + j (η₀ / 4π) Si 2π = 73.08 + j42.51 ohm
```

with η₀ = 376.73 ohm, γ Euler's constant and Si, Ci the sine and cosine
integrals (computed to 1e-12).

Near anti-resonance (a dipole close to a whole wavelength) the assumed
feed current vanishes and the impedance is marked **NOT VALID**.

## Matching quantities

Against the reference impedance Z₀ (default 50 ohm, set in the antenna's
properties):

```math
Γ = (Z − Z₀) / (Z + Z₀)                   reflection coefficient (= S11)
RL = −20 log10 |Γ|                        return loss, dB (positive)
VSWR = (1 + |Γ|) / (1 − |Γ|)
η_mismatch = 1 − |Γ|²                     share of the available power accepted
```

The antenna report lists all four; the realized gain uses the mismatch
efficiency ([Antennas](antennas.md)).

## Impedance sweeps

In the **Impedance** tab set **From** and **To** (MHz) and **Frequencies**
(default 500 - 1500 MHz, 201 points; stored in the project), choose
**Show**: *Resistance and reactance (ohm)*, *Return loss (dB)* or *VSWR*,
and press **Calculate Sweep**. Measured data imported as a Touchstone file
can be drawn beside the model with **Measured data**
([S-parameters](s-parameters.md)).

## Worked example

Open `examples/electromagnetics/a-half-wave-dipole.resonant` (149.9 mm,
wire radius 1 mm, 1 GHz) and select the dipole.

| Reference impedance | magnitude of Γ | Return loss | VSWR | Mismatch efficiency |
|---|---|---|---|---|
| 50 ohm | 0.371 | 8.60 dB | 2.18 | 86.2 % |
| 75 ohm | 0.276 | 11.18 dB | 1.76 | 92.4 % |

Calculate the sweep: the reactance passes through zero a little below
1 GHz, because a half-wavelength dipole is slightly too long for
resonance. The report's design estimate gives the thin-wire resonant length
for this wire radius. To shorten the dipole automatically, use the
Optimization Studio template *Electromagnetic: dipole length for zero
input reactance*
([The Optimization Studio](../optimization/studio.md)).

## Common mistakes

- **Comparing with a measurement without matching the reference.** Set
  the antenna's Reference impedance to the system impedance of the
  measurement (usually 50 ohm).
- **Reading the dipole model near a full wavelength.** It is NOT VALID
  there; a centre-fed full-wave dipole needs a computed current.
- **Expecting the feed region to matter.** The model has no feed gap,
  balun or feed line; the impedance is that of the ideal terminals.

## Limitations

- Assumed currents: no moment-method solution, so nearby objects and
  other antennas do not change the impedance (no mutual impedance).
- Matching networks are not part of the antenna model; the Lines tab gives
  line impedances ([Transmission lines](transmission-lines.md)).
- The impedance of the thin-wire dipole in the full-wave solver is not
  compared with the analytical one (its wire radius is set by the grid).
