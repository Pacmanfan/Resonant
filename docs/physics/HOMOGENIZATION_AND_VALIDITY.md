# Homogenization and Validity

**Status:** current (REQ-CHAT-20261126-019). **Requirement:** RES-META-010.

An effective property describes a structure only where its organization is
fine against the scale on which the fields vary. Every effective-medium result
states:

| Item | Where |
|---|---|
| the method (model id) | `effectiveMaterial.method`, the analysis's model |
| the validity range (Hz; both 0 for steady) | `validFromHz`, `validToHz` |
| the scale ratio at the top of the range (cell / wavelength, or cell / length) | `scaleRatio` |
| the condition in words | `validity` |
| whether values are frequency-dependent | each property's `frequencyTable` |
| the comparison with the explicit model | `derivation.comparison` and the metrics |
| the limitations | `limitations` |

## Ranges used

| Analysis | Valid where | Outside |
|---|---|---|
| layered dielectric | period ≤ `maxScaleRatio` (0.1) of the wavelength in the denser layer | the explicit/effective difference is reported up to three times the limit (`compare.maxDifferenceBeyond*`) |
| resonator duct | below half the resonators' frequency (positive, slowly varying modulus) | the dispersive and negative modulus are curves and metrics |
| split-ring array | below 0.8 f₀, with the cell ≤ `maxScaleRatio` (0.2) of the wavelength at f₀ | no effective material is reported, with a warning |
| layered thermal | lengths much larger than the period; transients slower than one period's diffusion time | — |
| mass-in-mass | the lattice itself is lumped; its effective mass is reported over the whole range | not stored |

## How the validity is demonstrated

- **Scale separation:** the explicit stack approaches the uniaxial slab as
  (a/λ)² when the same slab is cut into finer periods
  (`ObliqueLayers.HomogenizationConvergesAsThePeriodShrinks`: the error falls
  by 4 per halving).
- **Frequency dependence:** the duct's effective sound speed is a frequency
  table; the split ring's permeability is Lorentzian.
- **Spatial dispersion:** the retrieved μ of a layered slab rises above 1 as
  (ka)² and a lossy one shows Im μ > 0 (antiresonance) — reported, not hidden.
- **Retrieval stability:** the branch is tracked by phase continuity from a
  frequency where |n| k₀ d < π; ill-conditioned points are flagged.
- **Full-structure comparison:** every analysis compares its effective
  description with an explicit model of the structure (finite chain, finite
  array, explicit layers, finite volumes; `docs/physics/METAMATERIAL_VALIDATION.md`).
