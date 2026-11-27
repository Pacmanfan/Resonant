# Domain Optimization Adapters

Each adapter is bound to a design snapshot and a configuration and states its
variables, quantities, models and limitations (`Capabilities`). Quantity
definitions: `OBJECTIVE_FUNCTIONS.md`.

## Acoustic (`acoustic`, `src/core/acoustic_optimization`)

Configuration: `focus` (target id), `region` (field region id: background,
energy and peak quantities; needed by the full-wave model), `focalRadius`,
`lineHalfLength`, `lineSamples`, `regionSamples`, `lineAxis`, `targetSpacing`.

| Model | Fidelity | Notes |
|---|---|---|
| `acoustic.freefield.analytical` | analytical | point-source superposition; objects do not affect the field |
| `acoustic.reflector.kirchhoff` | reduced | free field + one Kirchhoff reflection per reflector (surface elements per wavelength from the acoustic settings) |
| `acoustic.fdtd.staggered` | high fidelity | the full-wave solver on the configured region with the project's FDTD settings (a plane region is a 2D problem); continuous signals only; refused with the solver's reason when the plan is refused |

Bindings: `acoustics/frequency` (every source follows it), `acoustics/medium/temperature`
(ideal-gas air only); emitters `object/<id>/emitter/{amplitude,phase,delay}`;
arrays `object/<id>/array/{amplitude,spacingX,spacingY,radius,countX,countY}`
and `object/<id>/array/element/<i>/{amplitude,phase,delay,active}`;
manipulators: geometry, position, material; positions of emitters and arrays.
Linear inverse: free-field and reflector models.

Templates: phase focus of an array (L-BFGS-B), several targets (amplitudes and
phases), reflector geometry for a focus (sweep, Kirchhoff), focal pressure
against sidelobes (NSGA-II).

Limits: lenses and waveguides only in the full-wave model; region and line
quantities are sampled; the free-field model ignores the scene.

## Mechanical (`mechanical`, `src/core/mechanics/MechanicalOptimization`)

Configuration: `structure` (default the first), `modes` (6), `frequency`
(harmonic, default the mechanics settings' target frequency),
`avoidFrequency`, `analyses` (`modal`, `harmonic`, `static`; default modal,
and harmonic and static when there are actuators).

| Model | Fidelity | Notes |
|---|---|---|
| `mechanical.analytical` | analytical | Euler-Bernoulli bending of box and cylinder beams (clamped, pinned, free ends; both principal planes) and simply supported Kirchhoff box plates; frequencies and mass only; unavailable otherwise, with the reason |
| `mechanical.fe.coarse` | reduced | the finite-element study with elements twice the study's size |
| `mechanical.fe` | high fidelity | the finite-element study as configured (its idealization, elements, damping) |

Bindings: structure geometry, `object/<id>/structure/thickness`,
`object/<id>/structure/support/<k>` (free, pinned, clamped), `object/<id>/material`
(library materials with mechanical properties), `object/<id>/material/{youngsModulusScale,densityScale}`
(HYPOTHETICAL: a copy of the material with its own id and a theoretical
provenance; every evaluation notes it), `mechanics/damping/eta`,
`mechanics/frequency`; actuators `object/<id>/actuator/{amplitude,phase,delay,active}`
and positions. Linear inverse: the actuators' transfer to the vibration targets.

Templates: tune the first natural frequency (Nelder-Mead on the analytical
model where it exists, validated with finite elements), suppress vibration at
targets (actuator amplitudes and phases, Nelder-Mead).

Limits: linear, small displacements; free-free structures show rigid-body
modes near 0 Hz in the finite-element frequencies; the analytical model has
no forced response or stress.

## Electromagnetic (`electromagnetic`, `src/core/electromagnetics/EmOptimization`)

Configuration: `object` (array or antenna; default the first array, else the
first antenna), `directions` ([{label, thetaDeg, phiDeg, levelDb?, null?}] in
the object's local frame; the first is the target), `resolutionDeg`,
`distance` (exact-field model, default 100 wavelengths).

| Model | Fidelity | Notes |
|---|---|---|
| `em.arrayfactor` | analytical | element pattern x array factor; identical isolated elements |
| `em.dipoles.exact` | reduced | superposed exact fields of the elements' assumed sinusoidal currents at a finite distance (dipole elements only); no mutual coupling |
| `em.antenna.analytical` | analytical | induced-EMF impedance and the assumed current's pattern (dipole, monopole, small loop) |

Bindings: arrays `object/<id>/array/{frequency,spacingX,spacingY,radius,countX,amplitude}`,
`object/<id>/array/element/<i>/{amplitude,phase,delay,active}`, position;
antennas: geometry, material, `object/<id>/antenna/{frequency,referenceImpedance}`.
Linear inverse: array factor toward directions with wanted levels and nulls.

Templates: steer an array (phases, L-BFGS-B), directivity against sidelobes
(amplitudes, NSGA-II), keep the beam and place a null (hard constraint,
Nelder-Mead), dipole length for zero reactance.

Limits: no mutual coupling in either array model; the Yee solver does not
represent arrays; patch, horn and helix antennas have design-formula
estimates only and no analytical model here.
