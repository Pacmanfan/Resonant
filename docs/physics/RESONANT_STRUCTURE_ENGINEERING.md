# Resonant Structure Engineering

**Status:** implemented for the analyses below (RES-FUNC-006, -007, -009, -010, -011), extended by
REQ-CHAT-20261126-018 ([resonance architecture](RESONANCE_ARCHITECTURE.md)): every mode now states
its nature (natural, driven-peak, identified), its decay, bandwidth and Q by loss mechanism; Helmholtz
resonators, cantilevers as resonators, coupled resonators, spectroscopy and a resonator in the coupled
engine were added.

Resonance is computed per domain and reported in one form: modes with a
label (when the model can label them), a frequency and a Q (0 when not
computed, infinity for a lossless model), plus metrics.

| Structure | Modes | Q, bandwidth | Tuning by geometry |
|---|---|---|---|
| Microwave cavity | TE/TM mnp, labelled, with field distributions | wall Q (surface resistance), fill Q (loss tangent, conductivity), unloaded, external and loaded Q; bandwidth f₀/Q_L; port response | `cavity.dfda`, `…dfdb`, `…dfdd` analytically; sweeps of a, b, d; the operating temperature through the wall expansion |
| Optical resonator | TEM_m0,q near the wavelength, labelled, with degeneracy | finesse, linewidth, photon lifetime, Q from mirror reflectivity, clipping and extra loss | separation (a mirror position), curvature; stability lost past g₁g₂ = 1 |
| Mechanical resonator | finite-element modes (unlabelled), effective masses | not computed (damping is the mechanics settings' and not part of the modal metrics) | length and thickness; d ln f / d ln L = −2 for bending |
| Photonic and phononic cells | band gaps as "modes" (centre, edges) | — (lossless) | layer thicknesses, cell lengths |

Behaviour is reported as established by the model: `resonant` (cavities,
resonators, structures), `broadband` (a horn without resistance peaks, a cell
without gaps in the range), `mixed` (stop and pass bands; a horn whose mouth
reflections cause resonances), `non-resonant` (an unstable optical
resonator), `coupled`. No structure is assumed resonant.

## Mode tracking

Across a sweep, modes are matched by label when every point's modes are
labelled (the cavity's TE101, the optical TEM00 q); a crossing of labelled
modes is reported, not hidden. Unlabelled modes (finite elements) are matched
by continuity; when a second candidate lies within 2 % of the nearest, the
correspondence is flagged ambiguous at that point
(`FunctionalStructures.ModeTrackingFollowsLabelsAndFlagsAmbiguity`).

## Resonance and coupled physics

Heating changes a cavity's dimensions (one-way through the walls' expansion
coefficient) and so its frequencies (`cavity.thermalShift`). Coupled
feedback (heating that detunes and changes the absorbed power) runs through
the coupled engine; in this work that is the coupled slab, whose absorptance
depends on its standing-wave pattern (`f-coupled-slab`).

## Not covered

(Coupled resonators and mode splitting are now analyses: [coupled resonators](COUPLED_RESONATORS.md).)
Q from radiation of
open structures, cylindrical and spherical cavities in the cavity analysis
(their closed-form frequencies exist in `manipulators/Analytics` for
acoustics), Q of mechanical modes from material loss factors.
