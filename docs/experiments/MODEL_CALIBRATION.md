# Model Calibration, Identifiability and Validation

**Status:** current (REQ-CHAT-20261126-023, RES-LAB-007..010),
`laboratory/ParameterEstimation`, `coupling/LaboratoryStudies`.

## Estimation

The cost is the sum over the observations of `((y_i − f_i(p)) / σ_i)²`, plus a
prior term `((p − μ) / σ_p)²` for each parameter that has a prior:

- **Algorithm:** Levenberg–Marquardt on the parameters scaled by their initial
  values, with forward differences. A difference falls back to the other side at
  a bound or a model limit, and a step is projected onto the bounds.
- **Several datasets** are fitted jointly.
- **Unweighted fits:** without stated uncertainties the fit is unweighted, and
  the covariance is scaled by the residual variance. The record says so.
- **Statistics:**
  - the covariance `(JᵀJ)⁺`;
  - standard errors, and intervals from the Student t quantile at the degrees of
    freedom;
  - the correlation matrix and the reduced χ²;
  - a reduced χ² above 3 is flagged as model-form trouble or understated
    uncertainties.

## Identifiability

The scaled Jacobian's singular values and the parameters' correlations and relative
errors classify each parameter:

| Class | When |
|---|---|
| identified | relative standard error ≤ 10 % |
| poorly identified | relative error ≤ 100 %, or a correlation above 0.99 with more than 1 % |
| not identifiable | it loads the weakest direction of a near-singular Jacobian (ratio < 10⁻⁶), or its relative error exceeds 100 % |

Correlations above 0.95 are named in the notes. The weakest direction is reported,
for example "the data hardly change along +0.71 a −0.71 b". Two examples from the
benchmarks:

- **Thermal sheet:** the conductivity of a thin, weakly cooled sheet (Biot
  number about 0.1) is not identifiable from its mid-plane temperature. The
  example therefore uses a 40 mm sheet with h = 50 W/(m² K), a Biot number near
  2.
- **Dielectric sheet:** with the thickness freed as well as the permittivity,
  the data determine the electrical length, not each of the two.

## The records

1. **Calibration (a job).** Its record holds:
   - the model and configuration, the original values and the estimates;
   - the datasets and the axis datasets;
   - the method and options;
   - the residuals, the covariance and the identifiability;
   - the operating range of the data, and the date;
   - its status: calibrated, poorly identified, not identifiable or not
     converged.

   **Accept** keeps it in the project.
2. **Validation.** It runs on other datasets: an overlap is refused as "not
   independent". It reports:
   - the RMS of the normalized residuals for the calibrated and the original
     values;
   - the fraction of points within twice the combined standard uncertainty.
     The combined uncertainty includes the measurement and the prediction's
     parameter uncertainty, `g C gᵀ`.
   - a verdict: validated within uncertainty when that fraction reaches the
     normal coverage less three binomial standard deviations. A correct model
     at 160 points lies above about 0.905, not always above 0.95.
   - datasets outside the calibrated range, reported as extrapolations;
   - a warning when the calibration was not identifiable.
3. **Application.** Applying is explicit, and the original is never changed:
   - A thermal calibration creates a material copy whose values are *fitted*,
     with their standard uncertainty and the calibration in their source.
   - A J2 calibration creates a copy of the material-state study.
   - Other models keep the values in the record only.

   Not-identifiable calibrations are not applied.
