# Antenna Engineering

## Antenna objects (RES-EM-005)

An antenna is a scene object with an antenna geometry kind and an antenna
component: frequency, **peak feed current**, feed phase, time delay,
reference impedance (default 50 ohm) and an active flag. Wire antennas
are drawn with their real wire radius; the feed is at the local origin.

| Kind | Model | Support |
|---|---|---|
| Dipole (`antenna.dipole`) | thin wire, sinusoidal current; exact fields; induced-EMF impedance | analytical |
| Monopole (`antenna.monopole`) | dipole of twice the height by image theory over an **infinite** perfect ground plane | analytical |
| Small loop (`antenna.loop`) | magnetic dipole; R = 20 pi^2 (C / lambda)^4 and the loop inductance | analytical while C < 0.3 lambda |
| Patch (`antenna.patch`) | transmission-line model with Hammerstad fringing | estimate (resonant frequency) |
| Horn (`antenna.horn`) | optimum-horn aperture gain, feed-guide cut-off | estimate |
| Helix (`antenna.helix`) | Kraus' axial-mode formulas (known to overestimate) | estimate |

The dipole model is accurate for thin wires up to about one wavelength;
it assumes the current and does not compute it (that needs the deferred
moment-method solver, ADR-000211). The finite ground plane drawn for a
monopole is not part of its analytical model; the report says
"INFINITE ground plane".

## Directivity, efficiency, gain, realized gain (RES-EM-007)

* **Directivity** D: from the model's pattern (1.641 for a half-wave
  dipole, 1.5 for a short one, twice the dipole's for a monopole).
* **Radiation efficiency**: R_rad / (R_rad + R_loss) with R_loss from the
  assigned conductor's surface resistance Rs / (2 pi a) per unit length,
  integrated over the sinusoidal current and referred to the feed (half of
  it for a monopole; C Rs / (2 pi a) for a loop); 1 without a conductor.
* **Gain** G = efficiency x D.
* **Realized gain** = G x (1 - |Gamma|^2) against the reference impedance.

The report shows all four and the far-field distance (the largest of
2 D^2 / lambda, 3 lambda and 3 D, D the largest dimension). Polarization:
linear along the wire for dipoles and monopoles, azimuthal for loops.

## Design aids

* Resonant length of a dipole (reactance zero) by bisection on the
  induced-EMF reactance, reported as an estimate for the wire radius.
* Patch: resonant frequency of the length from the substrate permittivity.
* Horn and helix: gain and input-resistance estimates, labelled.

## In the application

Electromagnetics > Add Dipole / Monopole / Small Loop / Patch / Horn /
Helical Antenna. The panel's report and the Impedance and Pattern tabs
show the analysis; the Properties panel edits the feed.
