# Transient and Broadband Signals

**Status:** current for REQ-CHAT-20261126-005. Code: `src/core/dsp/Waveform.h` (shared by every domain since REQ-CHAT-20261126-006).

A signal is a dimensionless function `w(t)`, zero before `t = 0`, of peak
magnitude about one. An emitter multiplies it by its amplitude and shifts
it by its time delay.

| Kind | Definition | Highest significant frequency |
|---|---|---|
| Continuous | `ramp(t) cos(2 pi f t + phi)`, raised-cosine ramp over 4 cycles | `f` |
| Tone burst | whole number of cycles under a Hann envelope | `f (1 + 2 / cycles)` |
| Gaussian pulse | `exp(-tau^2 / (2 s^2)) cos(2 pi f tau + phi)`, −6 dB bandwidth `b f`, `s = sqrt(2 ln 2) / (pi b f)`, started 4 `s` before its peak | `f (1 + b)` |
| Ricker wavelet | `(1 − 2a) exp(−a)`, `a = (pi f tau)^2`; zero mean, broadband | `2.5 f` |
| Frequency sweep | linear from `f` to `f_end` over a duration, 10 % cosine tapers | the larger of the two |
| Sampled | recorded samples, linearly interpolated | the Nyquist frequency of the record |

Any finite signal can repeat with a period. The highest significant
frequency sets the grid: a broadband signal costs more than its centre
frequency suggests.

## What comes from the emitter and what from the signal

* Amplitude and **time delay**: always the emitter's own.
* Continuous, tone burst, Gaussian pulse: the carrier **frequency and
  phase** are each emitter's own, so an array with per-element phases or
  frequencies works as set.
* Ricker wavelet, sweep, sampled signal: used as defined for every
  emitter; emitter frequency and phase do not apply (the result says so).

One signal kind applies to all emitters of a run. Per-emitter kinds are
not implemented. A sampled signal can be set through the core and the
project file; the panel has no editor for it.

## True time delay

A delay shifts the whole signal. With a Ricker pulse, delaying the source
by 120 steps reproduces the probe signal shifted by exactly 120 steps
(1e-9 relative); from a scene, a 100 µs emitter delay moves the pulse's
arrival by 100 µs within 1 µs with its amplitude unchanged. No
frequency-dependent phase is used anywhere in the time-domain solver.

Time-delay focusing of an array (`docs/acoustics/ACOUSTIC_FOCUSING.md`)
therefore focuses a pulse; phase focusing focuses only the frequency it
was calculated for.

## Results of a transient run

Probe signals, snapshots, energy. There is no steady-state field: the
viewport shows a recorded instant, labelled as instantaneous pressure, and
the Time Signals tab steps through the instants.
