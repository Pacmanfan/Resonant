# Laboratory Validation

**Status:** current (REQ-CHAT-20261126-023, RES-LAB-018). The request's benchmarks
(section 24) run on synthetic data with known ground truth. Each is labelled
synthetic and verifies the method; none is physical evidence.

| Benchmark | Ground truth and check | Test |
|---|---|---|
| Resonator (24.1) | m = 0.5 kg, k = 2·10⁵ N/m, c = 6 N s/m with 1 % noise: each inside its 95 % interval and identified; a second run validates within uncertainty; validation on the first run refused | `LaboratoryCalibration.AResonatorIsCalibratedAndValidatedOnAnotherRun`, `LaboratoryPanelTest.*` |
| Frequency response (24.2) | a 2 % stiffer prediction on another grid: the peak offset −2 %, the overlap only, a spectral similarity below 1, inconsistent; the same model consistent | `LaboratoryComparison.FrequencyResponsesAreAlignedOnTheirOverlap` |
| Parameter calibration (24.3) | the mass–spring–damper recovered within 4 standard errors and its intervals; y = a b x not identifiable (the combination named), y = a x + b identified; bounds, priors, joint and unweighted fits | `ParameterEstimation.*` |
| Spatial field (24.4) | a field x + 2y on a grid, measured in a frame shifted and turned by 90°: a residual below 10⁻¹²; a point outside counted; different frames without a transform refused | `LaboratoryComparison.FieldsAfterAFrameTransform` |
| Thermal (24.5) | the mid-plane temperature of a heated sheet through the coupled evolution: the handbook conductivity inconsistent, k = 0.50 W/(m K) recovered, validated on a second run, applied as a fitted material revision | `LaboratoryIntegration.ThermalRunThroughTheCoupledEngine` |
| Conservation (24.6) | a heated block: consistent within the expanded measurement uncertainty; a 20 J shortfall exceeded; an unmeasured radiation pathway is insufficient data; without uncertainties not evaluated | `LaboratoryEnergy.*`, `LaboratoryIntegration.PropertiesDesignRevisionsAndEnergyBalances` |
| Invalid datasets (24.7) | missing units, malformed rows (with the row), NaN and infinities, non-monotonic and duplicate times, inconsistent sampling, missing channels, unknown units, missing metadata and frames, empty and unreadable files: each reported; partial imports recorded | `LaboratoryImport.InvalidDatasetsAreReportedAndNothingIsDiscarded`, `LaboratoryServices.ImportsNeedASavedProjectAndInvalidOnesAreRecorded` |
| Calibration and validation (24.8) | the dielectric sheet (ε′ = 4.2, tan δ = 0.02 recovered; validated on a repeat), the steel coupon (yield 260 MPa, hardening 2.5 GPa; validated on a repeat), the resonator | `LaboratoryExamples.RingDownDielectricAndCoupon`, `LaboratoryCalibration.*` |
| Resonance (13) | two natural modes, a mains-interference line, an excitation peak classified with their evidence; nothing called a mode without phase; a ring-down's 440 Hz and Q 220 | `ExperimentalResonance.*`, `LaboratoryExamples.RingDownDielectricAndCoupon` |
| Integration (14–18) | a Helmholtz resonator's analysis against a measured response; retrieved effective permittivity inside and outside the homogenization regime; a library property with its provenance; a design revision's evaluation, the optimization records unchanged | `LaboratoryIntegration.*`, `LaboratoryModels.*` |
| Uncertainty (10) | distributions, Welch–Satterthwaite, Student t (2.776 at 4 degrees of freedom), stated correlations with justification, linear against Monte Carlo (the bias of exp) | `LaboratoryUncertainty.*` |

The datasets in `examples/laboratory/data` came from closed forms (the FRFs, the
ring-down, the coupon, the energy measurements) or from Resonant's own models. The
thermal runs come from the coupled evolution, and the S11 from the layered model.
Each was generated once with the parameters stated in its header and seeded noise.
