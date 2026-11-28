# Resonance Spectroscopy

**Status:** current (RES-RSN-003): `src/core/dsp/Resonance`, `ResonanceService.IdentifyResonances`.

## Complex poles of a signal (matrix pencil)

A record x[n] = Σ A_k e^{(−α_k + jω_k) n Δt} is written as a Hankel matrix;
its dominant right singular vectors (relative threshold) give the pencil
whose eigenvalues are the poles z_k (Hua and Sarkar 1990). For each pole:
f = arg z/(2πΔt), α = −ln|z|/Δt, Q = ω/(2α), bandwidth α/π, amplitude and
phase by least squares. Overlapping modes (closer than three bandwidths)
are flagged. The uncertainty is the spread of the poles over sub-windows
(the record without its first or without its last tenth).

| Option | Meaning |
|---|---|
| minHz, maxHz | the band of interest (poles outside are discarded) |
| maxModes | the model order (two poles per real mode); a broadband excitation needs room for every excited mode |
| skipSamples | samples to leave out (the excitation pulse) |
| maxSamples | longer records are decimated (boxcar), the gain and delay of the boxcar undone per pole |

Refusals: fewer than three periods of minHz (insufficient duration), fewer
than 2.5 samples per period of maxHz (insufficient sampling), a record
without energy. A decay that the record cannot resolve (α T < 0.05) is
reported with a warning: its Q is a lower bound at best.

## Lorentzian fits of a spectrum

P(f) = A/(1 + ((f − f0)/γ)²) + B by Levenberg–Marquardt within ±4γ of a
peak: Q = f0/(2γ), uncertainties from the covariance. Fewer than five points
inside the linewidth is refused (insufficient frequency resolution). An
asymmetric residual (Fano-like line) and a second maximum the fit does not
explain (overlapping modes) are flagged; `fitResonancePeaks` fits every peak.

## Not every peak is a mode

A peak of a driven response may come from the source's spectrum, an
interference or a numerical artifact; the identification reports what the
data support (poles, fit quality, flags), and the analyses label a pole only
when it lies within 3 % of a model's mode.

## Validation (`tests/core/test_resonance_spectroscopy.cpp`)

Two overlapping damped modes recovered to 1e-6 in frequency and 1e-5 in Q;
with noise, Q within 5 % and its uncertainty reported; exact Lorentzians to
1e-8; Fano lines and overlapping peaks flagged; short, coarse and empty
records refused. Applied to the cavity's Yee ring-down (Q of a conducting
fill within 3 % of ωε/σ) and to simulated cantilever decays (within 0.5 %).
