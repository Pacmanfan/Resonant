---
id: exp.uncertainty
title: Uncertainty
category: Experiments and Hardware
order: 80
status: available
summary: Uncertainty budgets of measurements and calibrations, how components combine and expand, and what the numbers do not cover.
keywords: [uncertainty, budget, GUM, coverage factor, standard uncertainty, error]
related: [exp.calibration, exp.measurements, exp.correlation, sim.accuracy]
---

A measured value without its uncertainty cannot be compared with anything.
Resonant keeps an **uncertainty budget** with every data set and calibration
record, in the manner of the GUM: a list of standard-uncertainty components,
each with its source and its kind, combined under a stated assumption.

## A component

| Field | Meaning |
|---|---|
| source | sensor calibration, instrument accuracy, quantization, noise, position, orientation, clock, temperature, repeatability, material, model, numerical simulation, other |
| type | instrument specification, calibration, statistical (Type A), model, numerical simulation |
| relative | the standard uncertainty as a fraction of the value |
| absolute | the standard uncertainty in the quantity's unit |
| distribution | how the standard uncertainty was obtained: normal, rectangular, ... |
| degrees of freedom | 0 when not stated (taken as large) |
| description | what it is, in words |

The kinds are kept apart on purpose: an instrument specification, a
calibration, a statistical estimate, a model uncertainty and a
numerical-simulation uncertainty are different things.

## Combining and expanding

```math
u_c(x) = √( Σ (u_rel,i · x)² + Σ u_abs,j² )
U = k · u_c,   k = 2
u_q = q / √12
```

`u_c` is the combined standard uncertainty of a value `x`, from the relative
components `u_rel,i` and the absolute components `u_abs,j`; `U` the expanded
uncertainty with the coverage factor `k` (2: about 95 % for a normal
distribution); `u_q` the standard uncertainty of a quantization step `q`
(a rectangular distribution). The root-sum-square assumes that the components
are **uncorrelated**, and every budget says so.

## Where components come from

- **Raw data** carries the quantization of its sample format over the
  channel's range.
- **Calibration** adds the calibration record's budget; for a quotient of
  independent quantities the relative components add in quadrature.
- **H1 transfer functions** add their random error for `n_d` averaged
  segments:

```math
ε_r = √(1 − γ²) / (|γ| · √(2 n_d))
```

  where `γ²` is the coherence. Low coherence means a large random error.
- **Averaging** repetitions adds the standard error of the mean per sample
  (a statistical, repeatability component).
- **Spatial samples** (tone amplitudes at measured positions) carry the
  combined standard uncertainty of their value.

## In the application

On the **Measurements** tab, a data set's record shows a line such as
*Uncertainty: u_c = ... (standard), U = ... (k = 2); 3 component(s),
components assumed uncorrelated (root-sum-square)*. The panel evaluates the
budget for a value of 1 in the record's unit: relative components appear as
fractions, absolute ones in the unit. *no uncertainty information* means the
data set has no components. The HTML report lists the budgets in full.

A [comparison with a simulation](correlation.md) reports the fraction of
points whose difference lies within the expanded uncertainty when every
measured point states one; otherwise it says that agreement within
uncertainty was not assessed.

## Worked example

An illustration of the formulas with stated, hypothetical numbers. A 16-bit
channel of ±1 FS has a quantization step `q = 2 / 2¹⁶ ≈ 3.05 × 10⁻⁵ FS`, so
`u_q ≈ 8.8 × 10⁻⁶ FS` (floating-point sample formats, which the examples use,
record no quantization component). A microphone with a sensitivity of
0.0158 FS/Pa known to 0.5 % reads 0.2 Pa:

```math
u_cal = 0.005 · 0.2 Pa = 1.0 × 10⁻³ Pa
u_q = 8.8 × 10⁻⁶ FS / 0.0158 FS/Pa ≈ 5.6 × 10⁻⁴ Pa
u_c = √(1.0² + 0.56²) × 10⁻³ Pa ≈ 1.15 × 10⁻³ Pa,   U ≈ 2.3 × 10⁻³ Pa
```

In the automated tests the root-sum-square, the expansion and the
per-type combination agree with hand calculation to 10⁻¹², and a
calibrator's level uncertainty is carried into the calibrated result.

## Limitations

- Components are assumed uncorrelated; correlated components are not
  modelled.
- There is no Monte Carlo propagation; uncertainty follows each step by the
  rules above.
- The coverage factor is fixed by the budget (2 by default); degrees of
  freedom are recorded but do not change it.
- The budget covers the effects that were entered. Reflections, a sensor's
  frequency response outside its calibration, or a model that leaves out a
  mechanism are not uncertainty components unless someone states them.

## Related topics

[Calibration](calibration.md) · [Measurements](measurements.md) ·
[Simulation correlation](correlation.md) · [Numerical accuracy](../simulation/accuracy.md)
