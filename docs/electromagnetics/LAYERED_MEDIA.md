# Layered Media

**Status:** current (REQ-CHAT-20261126-016, RES-CEME-011):
`src/core/electromagnetics/LayeredMedia`. Used by the Coupled Energy–Matter
Engine as the electromagnetic participant of layered systems.

A plane wave at normal incidence on a stack of homogeneous layers, solved
exactly: in each layer E = A e^{−jkz} + B e^{+jkz}, tangential E and H
continuous at every interface (transfer matrices
[[cos kd, jη sin kd], [j sin kd / η, cos kd]]). Time dependence `e^{+jωt}`,
peak amplitudes; a lossy layer has ε_r = ε′ − jε″_eff with
ε″_eff = ε′ tan δ + σ/(ωε₀); μ_r real. The incidence and exit half-spaces must
be lossless.

Results: r, t, reflectance, transmittance, absorptance; E, H and the
time-averaged Poynting flux ½Re{E H*} at any depth; the power absorbed in
[z₁, z₂] as S(z₁) − S(z₂) (exact); the absorbed power density ½ωε₀ε″|E|².

## Validation (`tests/core/test_layered_media.cpp`)

Fresnel coefficients of one interface (r = −1/3 for n = 2); a half-wave slab
is transparent and a quarter-wave coating matches (R < 1e-12); the
integral of the loss density equals the flux difference (1e-5 with 20 000
samples) and R + T + A = 1; a thick lossy slab attenuates at −Im(k); active
media and lossy half-spaces are refused.

## Limits

Normal incidence, isotropic layers, no dispersion model (values at the
given frequency), no magnetic loss; very thick, very lossy layers can
overflow (split them).
