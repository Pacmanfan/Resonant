# Helmholtz Resonators

**Status:** current (RES-RSN-006): `src/core/manipulators/Helmholtz`, the geometry kind
`resonator.helmholtz`, the analyses `acoustic.helmholtz` and `acoustic.coupled-helmholtz`.

## Geometry

`resonator.helmholtz`: a closed cylindrical cavity (radius, length) centred
on the origin with a neck (radius, length) along +Z and a wall thickness;
the full-wave solvers see the same walls (`geometry/SolidModel`).

## Lumped model (`acoustic.helmholtz.lumped`)

| Quantity | Expression |
|---|---|
| resonance | f₀ = (c/2π)√(S/(V L_eff)) |
| effective length | L + outer correction (8a/3π flanged, 0.6133a unflanged) + inner correction 0.8488a(1 − 1.25a/R) (Ingard) |
| radiation | R_rad = ρck²/2π (flanged; /4π unflanged) |
| viscous | R_visc = (2L_eff/πa³)√(ωρμ/2) (boundary layer of the neck) |
| Q | Q_i = ω₀M/R_i, 1/Q = 1/Q_rad + 1/Q_visc; bandwidth f₀/Q |
| response | the series impedance R + j(ωM − 1/ωC); cavity pressure per incident pressure (blocked pressure 2p_i at a baffled mouth) |
| stored energy | ½C|p_c|² per incident pascal² at resonance |

Validity is reported: k times the cavity's size and k L_eff below about 0.5,
the neck narrow against the cavity; thermal losses in the cavity are not
modeled.

## Plane-wave stepped duct (`acoustic.helmholtz.stepped-duct`)

The neck and the cavity as uniform ducts (exact transfer matrices), the back
rigid, the mouth loaded by the baffled-piston radiation impedance. It has
no inner end correction and no viscous loss; it is the check of the lumped
model: their resonances agree to within the distributed corrections
O((k size)²), which vanish as the cavity shortens, and the duct resonates
where the lumped model with the outer end correction 8a/3π does — not
where the bare neck length would put it. The end correction is therefore
the piston's radiation reactance.

## Coupled Helmholtz resonators

An open resonator joined through a connector (both ends inner-corrected) to
a closed one: a two-coordinate network ([coupled resonators](../physics/COUPLED_RESONATORS.md));
its frequencies match the closed-form quadratic of the 2 × 2 problem.

## Validation

`tests/core/test_helmholtz.cpp`, `tests/core/test_resonance_examples.cpp`
(`ResonanceExamples.Helmholtz…`, `…CoupledHelmholtz…`).
