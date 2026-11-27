# Frequency Analysis

**Status:** current for REQ-CHAT-20261126-005. Code: `fdtd/Spectrum.h`.

The spectrum of a probe's recorded pressure, by a radix-2 FFT written for
this purpose (no FFT library is used; none was installed, and probe
records are short).

## What the record can tell

| Quantity | Value | Meaning |
|---|---|---|
| Sampling rate | `1 / dt` | the solver's time step |
| Nyquist frequency | `1 / (2 dt)` | content above it would alias; the solver's own stability limit keeps physical content far below |
| Observation time `T` | steps × `dt` | |
| **Resolution** | `1 / T` (rectangular), `2 / T` (Hann) | the smallest separation of two components that can be told apart |
| Point spacing | `1 / (N_padded dt)` | spacing of the plotted points |

The record is zero-padded fourfold before the transform: more points on
the same curve. **Padding does not improve the resolution**, and the
resolution shown never depends on it. To resolve finer, simulate longer.
A test confirms that two components 0.8 / T apart appear as one peak and
separate when the record is ten times longer.

## Windows and leakage

A record that does not hold a whole number of periods leaks into
neighbouring frequencies. Rectangular window: narrowest main lobe,
sidelobes from −13 dB. Hann (default): main lobe twice as wide, sidelobes
from −31 dB falling quickly. For a decaying response that has died out
within the record either is fine; for a response still ringing at the end,
Hann.

## Scaling

A steady sinusoid `A cos(2 pi f t + phi)` filling the record reads
`A e^{j phi}` at its frequency (the window's gain is divided out). For a
transient the values depend on the record length: ratios between probes
and between frequencies are meaningful, absolute values are not spectral
densities.

## Validity in frequency

The grid was resolved for the signal's highest significant frequency. The
spectrum is plotted up to 1.5 times that; above the resolved frequency the
content is numerical (dispersion grows quickly) and the panel says so.

## In the application

Time Signals tab: pressure over time, spectrum magnitude, spectrum phase,
with the record's description (samples, rate, duration, window,
resolution, point spacing, aliasing limit), and the response peaks marked
(`MODAL_ANALYSIS.md`). Probe signals export as CSV.

## Not implemented

Transfer functions between two probes, frequency sweeps as a series of
steady-state runs, spectra of whole fields (only the steady-state field at
one frequency is extracted during a run), spectrograms.
