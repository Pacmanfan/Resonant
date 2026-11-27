# Electromagnetic Physics in Resonant

Resonant's electromagnetic domain designs antennas, builds and steers
arrays, calculates fields near and far from radiators, analyses lines,
guides and cavities, and solves Maxwell's equations numerically with the
Yee method. It lives in `src/core/electromagnetics` (no Qt, no OpenGL, no
dependence on acoustics or mechanics) and appears in the application as the
**Electromagnetics** menu and panel. Requirements: `RES-EM-001..024`
(`docs/requirements/electromagnetics.md`); decision: ADR-000211.

## Which model produced a number

| Kind | Where | What it is | What it is not |
|---|---|---|---|
| **Analytical** | antennas (dipole, monopole, small loop), ideal sources, lines, guides, cavities | closed-form solutions of Maxwell's equations for an assumed current or an ideal geometry | a computation of the current on a real antenna |
| **Array factor** | RF arrays | element pattern x array factor | coupling between elements |
| **Full-wave numerical** | Yee FDTD on a field region | a discretized solution of Maxwell's equations with materials and conductors | exact: it has dispersion and staircase error, quantified in the validation |
| **Design estimate** | patch, horn, helix | textbook formulas (resonant length, aperture gain, Kraus) | a calculated field or impedance |
| **Measured** | imported Touchstone data | data from a file, shown beside the model | mixed with the model |

Every result and report names its kind; full-wave field results are
labelled "full-wave numerical (Yee FDTD)" and analytical ones "analytical".

## The documents

* `MAXWELL_EQUATIONS.md` — equations, conventions, units, phasors.
* `ELECTROMAGNETIC_MATERIALS.md` — permittivity, permeability, conductivity, loss tangent, classification.
* `ANTENNA_ENGINEERING.md` — antenna objects, models, estimates; directivity, gain, realized gain.
* `RF_IMPEDANCE_AND_S_PARAMETERS.md` — feed impedance, matching quantities, sweeps, Touchstone files.
* `NEAR_FIELD_FAR_FIELD.md` — exact near fields, far-field distance, field regions and probes.
* `ANTENNA_ARRAYS.md` — array objects and layouts.
* `ARRAY_FACTOR_AND_BEAMFORMING.md` — array factor, steering, grating lobes, tapers, synthesis.
* `TRANSMISSION_LINES.md` — line impedances, loaded lines, transformers, L-networks, losses.
* `WAVEGUIDES_AND_CAVITIES.md` — guide modes, propagation, cavities, guide objects.
* `ELECTROMAGNETIC_SOLVERS.md` — the Yee solver and its setup from a scene.
* `ELECTROMAGNETIC_VALIDATION.md` — the evidence.
* `PERFORMANCE_AND_GPU.md` — cost, limits and the compute backend.

## Working in the application

1. **Electromagnetics > Add ...** places an antenna, source, array or guide
   at the origin, selects it and shows it in the Electromagnetics panel.
   Antennas are sized for 1 GHz (a half-wave dipole, a quarter-wave
   monopole) and wire antennas get copper — visible defaults you can change.
2. The **Properties** panel edits the feed (frequency, peak current, phase,
   delay, reference impedance), the source, the array layout and each
   element, or the guide's frequency of interest. Every edit is undoable.
3. The panel's report shows impedance, matching, directivity, efficiency,
   gain and realized gain with the model named. The tabs give impedance
   sweeps (with measured data), patterns (cuts and a 3D surface), array
   steering, tapering and synthesis, fields on regions and probes, the
   full-wave solver, and a line calculator.
4. Fields appear on the viewport's field plane, by default as a level in dB
   over 40 dB; the viewport shows one field at a time, the one chosen last.

Examples: `examples/electromagnetics/` (`a-` .. `g-`), described in
`examples/README.md`.
