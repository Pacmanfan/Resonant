# Objective Functions

An objective names a quantity of the problem's adapter and what is wanted of
it. Its **score** (to minimize, dimensionless) uses the user's normalization
`scale` (in the quantity's unit; never guessed):

| Sense | Score |
|---|---|
| minimize | v / scale |
| maximize | -v / scale |
| target | \|v - target\| / scale |
| at most (threshold) | max(0, v - target) / scale |
| at least (threshold) | max(0, target - v) / scale |

One objective: the score is minimized. Several: Pareto methods compare the
scores separately; single-objective methods minimize sum(weight x score)
(+ soft-constraint penalties), and the run still reports its Pareto front and
names no single best candidate. The same number has no common meaning across
domains, so scores are only combined on the user's weights.

## Acoustic quantities (`acoustic` adapter)

Complex peak pressure p (Pa), focus x_f = the configured focus target (else
the first target that is not a suppression).

| Key | Unit | Definition |
|---|---|---|
| focus.pressure | Pa | \|p(x_f)\| |
| focus.pressureDb | dB | 20 log10(\|p(x_f)\| / 1 Pa), dB re 1 Pa (peak) |
| focus.phase | rad | arg p(x_f) |
| focus.gain | - | \|p(x_f)\| / \|p_free(x_f)\|: the model's field over the free field of the same sources |
| targets.error | - | sqrt(sum q_m \|p_m - d_m\|^2 / sum q_m \|d_m\|^2) over every target sample; when every target is a suppression: sqrt(sum q_m \|p_m\|^2 / sum q_m), Pa |
| targets.magnitudeError | - | sqrt(sum q_m (\|p_m\| - \|d_m\|)^2 / sum q_m \|d_m\|^2) |
| targets.phaseErrorRms | rad | rms of the wrapped arg p_m - arg d_m over non-suppression targets |
| targets.minPressure | Pa | min \|p_m\| over non-suppression samples (several focal points) |
| suppression.maxPressure | Pa | max \|p_m\| over suppression samples |
| focus.contrastDb | dB | 10 log10(\|p(x_f)\|^2 / mean \|p\|^2 over region samples farther than the focal radius) |
| focus.energyOutside | - | sum \|p\|^2 over region samples beyond the focal radius / sum over all region samples |
| focus.peakOffset | m | distance from x_f to the region sample of largest \|p\| (where the field actually focuses) |
| region.peakPressure | Pa | max \|p\| over region samples outside the near-source limit |
| focus.width | m | full width of \|p\| >= peak / 2 around the peak nearest x_f on the focal line |
| focus.sidelobeDb | dB | 20 log10(max \|p\| outside the main lobe / peak) on the focal line; the main lobe ends at the first minimum on each side; -200 when nothing lies outside |
| sources.effort | Pa^2 | sum \|A_i\|^2 of the source strengths (Pa at 1 m); proportional to drive power for identical sources |
| sources.maxAmplitude | Pa | max \|A_i\| |
| sources.clearance | m | min over pairs \|r_i - r_j\| - (a_i + a_j); negative = overlapping sources |
| manipulators.minThickness | m | smallest thickness / wallThickness / centerThickness parameter of a manipulator |
| manipulators.maxExtent | m | largest bounding-box side of a manipulator |

Target samples: point targets as they are; line, plane, surface and volume
targets sampled at half a wavelength in their local frame (at most 400
samples each, each carrying weight / count). Focal radius: one wavelength
unless configured. Focal line: through x_f along the region's first axis (or
X), +/- four wavelengths, 241 samples. Region quantities: the configured
region, at most 41 samples per axis (the full-wave model uses its own grid,
sub-sampled). Sampled quantities depend on their sampling, which is part of
their definition.

## Mechanical quantities (`mechanical` adapter)

| Key | Unit | Definition |
|---|---|---|
| modal.f1 .. modal.fK | Hz | the K lowest undamped natural frequencies (K = configuration `modes`, 6) |
| modal.clearance | Hz | min_i \|f_i - f_avoid\| (f_avoid: configuration, default the harmonic frequency) |
| modal.massFractionZ1 .. Z<K> | - | effective modal mass along world Z / total mass of the free dofs (finite elements) |
| mass | kg | total mass (analytical: density x volume) |
| vibration.targetError | - | sqrt(sum w \|u_t - d_t\|^2 / sum w \|d_t\|^2) over vibration targets at the harmonic frequency (m when all are suppressions) |
| vibration.suppressionMax | m | max \|u\| over suppression targets |
| vibration.targetMin | m | min \|u\| over targets that are not suppressions |
| vibration.sensorMax | m | max \|u\| over sensors along their axis |
| response/<object id> | m | \|u\| at that sensor or target |
| static.maxDisplacement, static.maxVonMises, static.stressConcentration, static.compliance | m, Pa, -, J | the actuator forces as static loads; concentration = max / mean element von Mises stress |
| actuators.effort | N^2 | sum of the squared force amplitudes of the active actuators |

## Electromagnetic quantities (`electromagnetic` adapter)

Arrays: radiation intensity U(r^) of the model, sampled on the sphere in the
object's local frame (theta from +Z, phi from +X; 1 degree for the array
factor, 2 degrees for exact fields, configurable).

| Key | Unit | Definition |
|---|---|---|
| pattern.directivityDbi | dBi | 10 log10(4 pi U_max / integral of U) |
| pattern.levelDb/<label> | dB | 10 log10(U(direction) / U_max), exact in that direction |
| pattern.gainDbi/<label> | dBi | directivity + level toward the direction (idealized: no losses) |
| pattern.sidelobeDb | dB | 10 log10(max U outside the main lobe / U_max); the main lobe = the samples reachable from the maximum without climbing (a fan beam is one lobe; a grating lobe is not part of it) |
| pattern.beamwidthThetaDeg / PhiDeg | deg | half-power width along the meridian through the main lobe / the great circle across it (none for a fan along it) |
| pattern.mainLobeErrorDeg | deg | angle between the main lobe (of equal maxima, the one nearest the target) and the first configured direction |
| array.effort, array.maxWeight | - | sum \|w_n\|^2, max \|w_n\| of the active elements |
| array.minSpacingWavelengths | - | smallest element distance / wavelength (grating lobes) |

Antennas (dipole, monopole, small loop; induced EMF): `antenna.inputResistance`,
`antenna.inputReactance` (ohm), `antenna.reflectionMagnitude` \|Gamma\|,
`antenna.returnLossDb`, `antenna.vswr`, `antenna.directivityDbi`,
`antenna.gainDbi` (directivity x radiation efficiency), `antenna.realizedGainDbi`
(gain x (1 - \|Gamma\|^2)), `antenna.efficiency`.
