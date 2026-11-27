# Electromagnetic Validation

Evidence for RES-EM-021. Every row is an automated test in the `core`
suite (`tests/core/test_electromagnetics.cpp`,
`tests/core/test_electromagnetics_scene.cpp`) or the `app` suite
(`tests/app/test_em_ui.cpp`); the tolerance is the one the test asserts.
Visual similarity is not used as evidence.

## Foundations and analytical models

| Check | Reference | Result / tolerance |
|---|---|---|
| c = 1 / sqrt(mu0 eps0), eta0 = sqrt(mu0/eps0) | definitions | 1e-3 m/s, 1e-6 ohm |
| Si, Ci at 1, pi, 2 pi, 5, 10 | mpmath / tables | 1e-12 |
| Skin depth, surface resistance, Fresnel at normal incidence | closed forms | 1e-10 / exact |
| Hertzian dipole power through spheres of 0.05, 0.3 and 20 lambda | eta pi / 3 abs(I l / lambda)^2 | 1e-5 relative at every radius |
| Half-wave dipole impedance | (eta0/4pi)(gamma + ln 2pi - Ci 2pi) + j(eta0/4pi) Si 2pi = 73.08 + j42.51 | 1e-9 ohm |
| Monopole | half the dipole of twice the height | 1e-9 |
| Directivity: half-wave, short, 1.25 lambda dipole | 1.6409, 1.5, about 3.3 | 1e-3, 1e-3, 0.05 |
| Thin-dipole far field vs pattern formula | Balanis 4-58 | 2e-3 |
| Near-field Poynting flux of a half-wave dipole vs 1/2 abs(I)^2 R_in | induced-EMF impedance | 1e-4 relative |
| Uniform 16-element array: beamwidth, first sidelobe, directivity | 0.886 x 2/N rad, -13.15 dB, N | 0.3 deg, 0.05 dB, 0.1 |
| Phase steering to (30, 45) deg; squint at 1.2 f; true-time delay | asin(0.5/1.2); no squint | 1 deg / 0.1 deg |
| Grating-lobe prediction | reciprocal lattice, array factor equal at the predicted direction | 1e-12 / 1e-9 |
| Dolph-Chebyshev 30 dB | equal sidelobes | 0.1 dB |
| Synthesized nulls | abs(AF) < 1e-4 of the main beam | met |
| Coax, microstrip, stripline, CPW impedances | closed forms, Pozar / Wadell values | 1e-9 .. 1 % |
| Loaded line, quarter-wave, L-networks | Z0^2/Z_L, Z_L, jZ0; exact match | 1e-9 |
| WR-90 cut-offs, circular-guide zeros, cavity resonance | closed forms, tables | 1 MHz / 1e-6 |
| TE10 power flux | closed-form power | 1e-6 relative |

## Yee solver

| Check | Reference | Result |
|---|---|---|
| Stability limit and boundary rules | CFL formula | larger steps and one-sided periodic faces refused |
| 2D line current at 20 cells per wavelength, 1 and 2 wavelengths away | (omega mu0 / 4) H0^(2)(k rho) | magnitude within 3 %, on the axis and the diagonal; phase within 0.1 rad |
| TE leakage in a TM run | zero | < 1e-9 relative |
| PEC cylinder (radius 0.5 lambda, source 1.5 lambda), 10 / 20 / 30 cells per lambda | exact series (40 terms) | error falls with refinement, < 6 % at 30 |
| Plane wave on er = 4 | Fresnel -1/3 | within 0.01 |
| Plane wave on er = 9, 40 and 80 cells per vacuum wavelength | Fresnel -1/2 | refinement reduces the error; within 0.01 at 80 |
| Absorbing layer alone (12 cells) | no reflection | < 2e-3 |
| PEC cavity 30 x 18 x 42 mm at 3 and 1.5 mm cells | first resonance | below the exact value (Yee dispersion), error falls with refinement, < 0.5 % at 1.5 mm; energy constant within 2 % after the pulse |
| 3D current element at 16 cells per lambda, 8 and 12 cells away | Hertzian dipole field | within 6 % |
| Example G from the scene: copper cylinder, 15 and 30 cells per lambda | exact series | 12.6 % and 4.7 % relative error |
| Half-wave dipole in 3D (16 cells per lambda) | thin-dipole field ratio at 45 deg / broadside | within 15 % |

## Example projects

`EmExamples.A_*` .. `G_*` check each example file against the stored
project and its calculation (`examples/README.md`).

## Known limits of the evidence

* No comparison with an external electromagnetic code has been run; all
  references are closed forms and series solutions computed in the tests.
* Thin-wire dipoles in the Yee solver use an effective radius of about
  0.23 cells, so their impedance is not compared with the analytical one.
* Mutual coupling in arrays is neither modelled nor validated.
