# Signal Processing Pipeline

RES-EXP-012, RES-EXP-015.

## Operations (`Processing.h`)

| Operation | Parameters | Result |
|---|---|---|
| segment | startSeconds, durationSeconds | a part of a time series |
| calibrate | calibrationId (default: the sensor's selected one) | physical units |
| spectrum | window | complex spectrum; a sinusoid A cos(2 pi f t + phi) reads A e^{j phi}; zero padding to a power of two, the resolution stated; I/Q two-sided |
| psd | segmentLength, overlap, window | Welch one-sided density, (unit)^2/Hz |
| transfer-function | reference, segmentLength, overlap, window | H1 = G_xy / G_xx with the coherence; both channels from the same run |
| impulse-response | - | inverse DFT of H1 (periodic in the segment length) |
| filter | band, lowHz, highHz, taps, window, compensateDelay | linear-phase windowed-sinc FIR; edges affected are stated |
| decimate | factor | low-pass at 0.8 of the new Nyquist frequency, then every n-th sample |
| resample | sampleRateHz | band-limited (windowed sinc) interpolation |
| cross-correlation / autocorrelation | reference, maxLagSeconds | normalized correlation over lag |
| delay | reference, maxLagSeconds | delay with its standard uncertainty |
| rms | - | root mean square (no window, no filter) |
| tone | frequencyHz, window | complex amplitude at one frequency (real or I/Q data) |
| average | - | mean of repetitions with the standard error per sample |
| integrate-frequency | times | divided by (j omega)^times: acceleration -> velocity -> displacement; DC set to 0 |
| scale | factor, quantity, reason | an inferred quantity (e.g. stress = E strain) with the model stated |

Chains (`runChain`) apply steps to the run's bindings in order: a step names
its `bindings` (default: every recorded binding, never the commanded
signals unless named) and its `reference`.

## Conventions

- One-sided densities: `G_xx = 2 <|X|^2> / (fs sum w^2)` (DC and Nyquist
  once); integrating over frequency gives the mean square (Parseval).
- Coherence `gamma^2 = |G_xy|^2 / (G_xx G_yy)`; the normalized random error
  of |H1| for n_d averages is `sqrt(1 - gamma^2) / (|gamma| sqrt(2 n_d))`
  (Bendat and Piersol); H1 is biased low by input noise and by a delay
  between the channels that is not small compared with the segment
  (about 1 - tau / T): use segments long compared with the delay.
- Windows: rectangular, Hann, Hamming, Blackman-Harris (4 term), flat top;
  coherent gain and noise bandwidth are recorded with the result.
- Delay: integer lag at the correlation peak, fraction from the weighted
  slope of the cross-spectrum phase of the aligned overlap.

## Reproducibility and honesty

- Raw data is never changed; every result is a new data set.
- A result's id is a hash of its sources' ids, the operation and its
  parameters: the same chain on the same data gives the same ids and values.
- Each step's description states every parameter it used; nothing is
  windowed, filtered, normalized, calibrated or converted unless a step says
  so.
- Data with gaps (NaN) is refused until a gap-free segment is chosen.

## Uncertainty

Budgets (`Uncertainty.h`) list components (source: sensor calibration,
instrument accuracy, quantization, noise, position, orientation, clock,
temperature, repeatability, material, model, numerical simulation; type:
instrument specification, calibration, statistical, model, numerical
simulation; relative and / or absolute standard uncertainty; distribution;
degrees of freedom), combine them by root-sum-square under the stated
assumption that they are uncorrelated, and expand them with a coverage
factor (k = 2). Raw records carry the quantization of their format;
calibration combines the calibration's budget; H1 adds its random error;
averaging adds the repeatability.
