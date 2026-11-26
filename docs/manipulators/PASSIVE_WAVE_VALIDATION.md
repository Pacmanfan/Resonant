# Passive Wave Validation

**Status:** results of the run recorded for REQ-CHAT-20261126-004. Tests:
`tests/core/test_manipulators.cpp`, `tests/core/test_manipulator_examples.cpp`,
`tests/app/test_manipulators_ui.cpp`. Test medium: `c = 340 m/s`,
`ρ = 1.2 kg/m³`, `f = 34 kHz`, wavelength 10 mm.

## Geometry and focal relationships

| Check | Reference | Tolerance | Result |
|---|---|---|---|
| Parabolic surface elements | `z = r²/4f`; distance to focus = distance to directrix | 1e-14 m | pass |
| Parabolic normals | bisect the focal direction and the axis | 1e-12 | pass |
| Paraboloid area | closed form | sampled sum within 0.2 % | pass |
| Elliptical surface elements | distances to the foci add to `2a` | 1e-13 m | pass |
| Elliptical normals | bisect the two focal directions | 1e-12 | pass |
| Spherical cap | on the sphere; area `2πRh` within 0.2 % | — | pass |
| Trough | `z = x²/4f`; area = arc × length within 0.2 % | — | pass |
| Element count estimate | equals the generated count | exact | pass |

## Reflection direction (rays)

| Check | Reference | Tolerance | Result |
|---|---|---|---|
| Axis-parallel rays on a parabola | pass through the focus | 1e-13 m | pass |
| Rays from a parabola's focus | leave along the axis | 1e-12 | pass |
| Rays from one focus of an ellipse | pass through the other | 1e-13 m | pass |
| Spherical mirror | axis crossing `R − R/(2 cos θ)`; marginal rays closer; paraxial `R/2` | 1e-12 m | pass |
| Flat mirror | mirrored direction | 1e-13 | pass |
| Reflector moved and rotated in the scene | world-space rays still collimated | 1e-10 | pass |

## Analytical estimates

Mirror equation; interface reflection and transmission with energy
conservation `T = 1 − R²` (air to water: `R = 0.99944`, `T = 0.0011`);
thin-, thick- and plano-lens focal lengths; the diverging convex lens of a
fast material; refusal of intersecting lens surfaces; the tabulated zeros
of `J_m'` and of the spherical `j_n'` verified numerically to 1e-6; duct
cut-offs; box, cylinder and sphere resonances; Helmholtz and quarter-wave
formulas. All pass.

## Reflector field model

| Check | Reference | Tolerance | Result |
|---|---|---|---|
| Rigid plate 40 λ square, source 5 λ above | field of the mirror-image source, 3 points | 4 % of the image field | pass |
| Pressure-release plate | minus the image field | 5 % | pass |
| Behind a rigid plate | total field below 8 % of the incident field (shadow) | — | pass |
| Impedance equal to the medium's | no specular reflection | 6 % of the image field | pass |
| Very large impedance | rigid behaviour | 6 % | pass |
| All-sources evaluator | equals the per-source sum | 1e-9 relative | pass |
| Point on the surface | flagged invalid | — | pass |
| Parabola, source 2 m on axis | on-axis peak at the mirror-equation image (51.28 mm) | half a wavelength | pass |
| — focal gain there | more than 20 | — | pass |
| — lateral width at half peak | between 0.3 and 2 wavelengths | — | pass |
| Source at a parabola's focus | reflected beam amplitude at 0.6 m within 0.75 to 1.35 of that at 0.3 m (collimated); far smaller outside the aperture | — | pass |
| Ellipse, source at one focus | on-axis peak at the other focus | 0.75 wavelength | pass |
| — focal gain there | more than 8 | — | pass |

## Convergence

One point near a parabolic reflector's focus, surface resolution 2, 4, 8,
16 elements per wavelength: the difference from the 16-per-wavelength value
decreases at every refinement; 8 per wavelength is within 1 %, 4 within
5 %. (`ReflectorModel.ResultConvergesAsTheSurfaceResolutionIsRefined`.)

This is a convergence of the surface quadrature. It shows that the integral
is evaluated accurately, not that the Kirchhoff approximation itself is
accurate for a given geometry.

## Hybrid focusing, metrics, optimization

Hybrid: with model phases `|Σ H_i e^{jφ_i}| = Σ |H_i|` (1e-9 relative);
free-field phases in the presence of the reflector reach less than 90 % of
that; with a free-field model the method reduces to `k r`. Focus metrics on
a synthetic Gaussian field: peak, both widths (`2√(2 ln 2) σ`), displacement,
value at the intended point, sidelobe ratio, and a zero width for a peak cut
by the grid edge. Optimization: the focal length that the mirror equation
predicts, within 4 mm, with a more than threefold gain; constraints
respected; bad problems refused.

## Examples

A parabolic (peak at the mirror-equation image, gain above 20), B
elliptical (peak at the second focus, gain above 8), C lens (analytical
estimate; the lens is reported as not simulated), D hybrid (in-phase sum,
more than 1.5 times the array alone), E optimization (stored optimum equals
the optimizer's and beats the original more than threefold). Each file is
compared with what the engine builds.

## Limitations of this validation

* The reflector model is compared with closed-form limits (image source,
  mirror equation, ray optics), not with a full-wave solver or with
  measurements. Its error for wavelength-scale reflectors is not
  quantified.
* Mesh-refinement studies of a numerical solver do not apply: no numerical
  volume solver exists yet.
* Lenses, waveguides and cavities are validated as formulas only.
