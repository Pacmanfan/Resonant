# Near Field and Far Field

## Exact fields at any distance (RES-EM-009)

The analytical field models are not far-field approximations:

* **Thin dipole:** the closed-form E and H of a sinusoidal current filament
  (the "three spherical waves" form of E_z and E_rho and the exact H_phi),
  valid at every point off the wire.
* **Current element (Hertzian dipole):** the complete 1/r, 1/r^2 and 1/r^3
  terms.
* **Small loop / magnetic dipole:** the dual of the current element.
* **Monopole:** the dipole of twice the height by images, above the ground
  plane only; points below it are excluded.
* **Plane wave:** E along local X, travelling along local +Z.

Points on a wire or a source, where the model is singular, are **excluded**
(not drawn and not reported), not given an arbitrary value.

The validation integrates the Poynting vector of the near field over a
sphere 0.8 lambda around a half-wave dipole and obtains 1/2 |I|^2 R_in
from the impedance calculation to 1e-4 (`EmExamples.E_*`); the far-field
wave impedance |E| / |H| tends to eta0.

## Far-field distance

Reported for each antenna: the largest of 2 D^2 / lambda, 3 lambda and
3 D. Patterns (`ANTENNA_ENGINEERING.md`) are far-field quantities and are
normalized.

## Fields on regions and probes

The Fields tab evaluates the analytical fields of every active antenna,
source and dipole-element array on a field region (the same region
objects as acoustics: Acoustics > Add Field Plane / Volume) at the
frequency of the first active radiator; radiators at other frequencies are
left out and reported. Arrays of isotropic or cosine elements have no
field model and are refused. **Probe** gives E and H (magnitude and phase
per component), |E|, |H| and the power density at a world point.

Shown components: Ex, Ey, Ez, |E|, Hx, Hy, Hz, |H|, power density
|Re{E x H*}| / 2; as magnitude, phase, instantaneous value (animated) or
level in dB (default, 40 dB range); normalized or absolute (RES-EM-018).
