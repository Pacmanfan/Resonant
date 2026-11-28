# Optical Resonators

**Status:** current (REQ-CHAT-20261126-017, RES-FUNC-007):
`src/core/electromagnetics/OpticalResonator`, the analysis `optics.resonator.two-mirror`.

**This is a paraxial model, not a full electromagnetic solution.** Scalar
Hermite–Gaussian beams between two thin spherical (or plane) mirrors aligned
on one axis, in a lossless medium of index n (Kogelnik and Li 1966; Siegman,
*Lasers*, ch. 19–21). Valid while the waist is many wavelengths (a warning
below ten) and the mirrors are aligned.

| Quantity | Expression |
|---|---|
| stability | g_i = 1 − L/R_i; stable 0 < g₁g₂ < 1; marginal at 0 and 1 (confocal: the symmetric mode is reported, degenerate) ; unstable otherwise (no Gaussian mode) |
| waist | w₀² = (λL/nπ) √(g₁g₂(1 − g₁g₂)/(g₁ + g₂ − 2g₁g₂)²) (symmetric: (λL/nπ)√((1+g)/4(1−g))) |
| waist position | z₁ = L g₂(1 − g₁)/(g₁ + g₂ − 2g₁g₂) from mirror 1 |
| spot sizes | w₁² = (λL/nπ)√(g₂/(g₁(1 − g₁g₂))), and w₂ alike |
| spectrum | FSR = c/2nL; f_qmn = FSR (q + (m + n + 1) arccos(±√(g₁g₂))/π) |
| losses | clipping per bounce e^{−2a²/w²}; survival S = R₁′R₂′(1 − extra loss) |
| finesse, linewidth | F = π S^{1/4}/(1 − √S); Δν = FSR/F; τ = 1/(2πΔν); Q = ν/Δν |
| transmission | on resonance T₁T₂/(1 − √S)² (lossless coatings, T = 1 − R) |
| stored energy | 2nL/c per watt circulating |

In a structure, the mirrors are `reflector.spherical` (curvature radius,
aperture) or `reflector.flat` objects; the separation is the distance between
them (a position parameter); reflectivities and the wavelength are free
parameters named by the analysis settings.

## Validation

ABCD self-consistency of the beam parameter after one round trip (1e-12);
confocal closed forms; the free spectral range and linewidth of a plane
Fabry–Perot against the exact layered-media solution of two quarter-wave
mirrors 50 wavelengths apart (0.5 %, 3 %).

## Limits

No misalignment or tilt; no diffraction calculation (clipping is estimated
from the Gaussian profile and warned when the aperture is under 1.5 beam
radii); no gain medium; no thermal lensing; no polarization.
