# Acoustic Horns

**Status:** current (REQ-CHAT-20261126-017, RES-FUNC-008):
`src/core/manipulators/Horn`, the geometry kind `horn.acoustic`, the analysis `acoustic.horn`.

## Geometry

`horn.acoustic`: an axisymmetric shell along Z, throat at −L/2, mouth at
+L/2; parameters throat radius, mouth radius, length, profile (0 conical,
1 exponential, 2 hyperbolic, 3 uniform), hyperbolic flare T in (0, 1] and
wall thickness. The full-wave solvers see the same shell (`geometry/SolidModel`).

| Profile | Radius |
|---|---|
| conical | r_t + (r_m − r_t) x/L |
| exponential | r_t (r_m/r_t)^{x/L}, area S_t e^{mx} with m = 2 ln(r_m/r_t)/L |
| hyperbolic (hypex) | r_t (cosh(x/x₀) + T sinh(x/x₀)), x₀ from the mouth radius |
| uniform | r_t |

## Model (`acoustic.horn.webster`)

The Webster horn equation dp/dx = −jωρU/S, dU/dx = −jωS p/(ρc²), integrated
from the mouth to the throat by fourth-order Runge–Kutta steps; the mouth is
loaded by the radiation impedance of a circular piston in an infinite rigid
baffle, Z = (ρc/S)[1 − J₁(2ka)/(ka) + j H₁(2ka)/(ka)] (H₁ by quadrature).
Results: the throat impedance and its normalization Z S_t/ρc (impedance
transformation), the reflection against a throat-sized duct and the power
transmission 1 − |Γ|², pressure, particle velocity, intensity and power along
the axis, the radiated power for a throat velocity, the mouth's directivity
factor (ka)²/(1 − J₁(2ka)/ka), directivity index and −6 dB half angle, the
on-axis pressure at 1 m, the flare cut-off m c/4π, and the response over the
frequency range (resistance peaks reveal mouth resonances).

## Validation

A uniform duct against the transmission-line formula (1e-9), an exponential
horn with its outgoing-wave load against the infinite-horn impedance (1e-8;
reactive below cut-off), a conical horn against the spherical-wave impedance
(1e-7), power equal at throat, mouth and every sample (1e-8), fourth-order
convergence, and the piston impedance against its power series and limits.

## Limits

Plane waves along the axis: accurate while the local ka is below about 1.8
(a warning above) and the flare is gradual; lossless walls (no viscous or
thermal boundary layers); a baffled mouth (an unbaffled horn radiates less at
low frequency); the far-field pressure assumes a uniform mouth velocity.
