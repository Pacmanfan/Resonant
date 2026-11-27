# Transmission Lines and Matching Networks

## Characteristic impedance (RES-EM-015)

| Line | Formula | Accuracy |
|---|---|---|
| Coaxial | Z0 = eta0 / (2 pi sqrt er) ln(b / a) | exact (TEM, perfect conductors) |
| Microstrip | Hammerstad-Jensen, with effective permittivity | about 1 % for 0.01 < w/h < 100 |
| Stripline | exact conformal map, zero-thickness strip: Z0 = 30 pi K(k') / (sqrt er K(k)) | exact for t = 0 |
| Coplanar waveguide | exact conformal map on an infinitely thick substrate | exact for its geometry |

Elliptic integrals K(k) by the arithmetic-geometric mean (1e-15).

The Lines tab of the Electromagnetics panel computes Z0, the effective
permittivity, the phase velocity and the wavelength on the line (and a
quarter wave) for given dimensions in mm.

## Loaded lines and matching

* Input impedance of a line of length l terminated in Z_L, with optional
  attenuation: Z_in = Z0 (Z_L + Z0 tanh(gamma l)) / (Z0 + Z_L tanh(gamma l)).
* Quarter-wave transformer Z_T = sqrt(Z0 R_L).
* L-networks (series reactance and shunt susceptance, both orientations)
  that match a complex load to a resistive source (Pozar 5.1); each
  solution is checked to give exactly the source resistance.

## Losses

* Conductor loss of a coaxial line from the surface resistance of both
  conductors.
* Dielectric loss of a TEM line: alpha_d = k tan d / 2.

These are core functions (`src/core/electromagnetics/Lines.h`) used by the
reports and available to the optimization milestone.
