# Acoustic Engine

**Status:** current for REQ-CHAT-20261126-003. Requirements:
`docs/requirements/acoustics.md`. Decision: ADR-000207.

The acoustic engine is `src/core/acoustics`. It depends on the scene,
materials-independent core modules and Eigen, and on nothing in the
rendering or application layers: it can be used from a program without a
user interface (the tests do exactly that).

## Parts

| File | Responsibility |
|---|---|
| `Conventions.h` | time convention, amplitude and source-strength conventions, reference pressures |
| `Medium.h` | the propagation medium; ideal-gas air, nominal water |
| `Components.h` | scene objects: emitter, array, target, probe, simulation region |
| `Propagation.h` | `PointSource`, `collectSources`, the `PropagationModel` interface, `FreeFieldModel` |
| `FieldEvaluator.h` | field quantities at arbitrary points |
| `Focusing.h` | phase and time-delay focusing, amplitude tapers |
| `InverseDesign.h` | propagation matrix and regularized least squares for point targets |
| `FieldGrid.h` | grids, preflight estimate, background field job, stored result, point reports and exports |
| `Settings.h` | project-level settings (medium, model options, frequency, memory limit) |

`ACOUSTIC_MODELS.md` has the equations; `ACOUSTIC_ARRAYS.md`,
`ACOUSTIC_FOCUSING.md` and `ACOUSTIC_INVERSE_DESIGN.md` the algorithms;
`ACOUSTIC_VALIDATION.md` the numerical evidence.

## Data flow

```text
scene (emitters, arrays)  --collectSources-->  PointSource list  ┐
project settings (medium, options, frequency)                    ├─> FieldEvaluator
PropagationModel (FreeFieldModel)                                ┘        │
                                                                          ├─> value at a probe / target
region object --gridOf--> GridFrame --estimateField--> allowed?           │
                                   └──────────── FieldJob (worker threads) ┴─> FieldResult (immutable)
```

A `FieldEvaluator` copies its sources and medium. A `FieldJob` holds an
evaluator, so a running or finished calculation is unaffected by later
edits, and a `FieldResult` keeps the sources, medium, frequency, grid,
assumptions and reproducibility metadata it was computed from.

## Interchangeable models

`PropagationModel` answers one question: the complex pressure a
unit-strength source produces at a point (and its gradient). The evaluator,
the inverse solver and the field job use only that interface, so a model
with reflections or a numerical solver can replace `FreeFieldModel` without
changing them. Each model reports its `id`, `version` and `assumptions`,
which are stored with every result and shown to the user.

Interfaces defined in this milestone, and what implements them:

| Interface | Implemented by |
|---|---|
| Analytical / point-source / directional-source propagation | `FreeFieldModel` with `Directivity` (omnidirectional, cardioid, baffled piston) |
| Array geometry and element excitation | `generateElements`, `collectSources` |
| Field evaluation | `FieldEvaluator`, `FieldJob` |
| Inverse field design | `solveInverse` |

A separate far-field **array-factor** routine is not provided: the field
of an array is always the exact sum of its elements' fields, which contains
the array factor as its far-field limit.

## Sampling and resources

`estimateField` reports points, memory, source evaluations, sample spacing
and samples per wavelength before anything is allocated, and refuses a grid
whose stored field would exceed the memory limit (1 GiB by default, a
project setting). It warns about aliased or excessive resolution and never
changes the resolution itself. A field stores 8 bytes per sample
(single-precision complex) plus one flag byte; point values (probes,
targets, the inverse solver) are computed in double precision.

`FieldJob` splits the grid into rows (planes) or layers (volumes) taken
from a shared counter by one thread per processor, reports progress, and
stops at the next row when cancelled. Threading does not change any value:
a test compares the threaded result with the single-threaded one bit for
bit.

## Point-report export formats

`pointReportsToCsv` — one header line, then one line per probe or target:

```text
name,x_m,y_m,z_m,pressure_real_Pa,pressure_imag_Pa,magnitude_Pa_peak,phase_rad,rms_Pa,spl_dB,
spl_reference_Pa,intensity_x_W_m2,intensity_y_W_m2,intensity_z_W_m2,intensity_exact,near_source
```

Commas, quotes and line breaks in a name are replaced by spaces.
`intensity_exact` is 0 when a source has a directivity and the gradient of
that directivity was neglected; `near_source` is 1 inside a source's radius,
where the point-source model is not valid.

`pointReportsToJson` — `{"format": "resonant-point-report", "version": 1,
"frequencyHz", "medium", "model", "timeConvention", "amplitudes": "peak",
"points": [...]}` with the same quantities per point, plus
`desiredPressure` for targets.

## What is stored in a project

Emitters, arrays (with per-element overrides), targets, probes and regions
are scene objects and are saved with the scene. The acoustic settings are in
`extensions.acoustics`. Each completed field run appends a record with its
full configuration and metadata to `extensions.acousticRuns`. The sampled
field itself is **not** saved; reopening a project and running the region
again reproduces it.

## Limits

* Free field only. Solid objects, materials assigned to them and boundaries
  have no effect on the calculated field, and the application says so with
  every result.
* One frequency per solution. Sources at another frequency are left out and
  counted.
* The medium is homogeneous and at rest.
* No isosurfaces; a volume is shown as up to three slices.
