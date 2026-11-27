# Python API

**Requirements:** RES-PY-001..006 (`docs/requirements/automation.md`).
**Decision:** ADR-000215. **User help:** topics `py.api`, `ref.api` (generated).

## Shape

```text
python/resonant/            the package (pure Python, documented, stable: API 1.0)
    __init__.py             API_VERSION, CORE_VERSION, Error, Project, ObjectRef, require()
    _native.py              finds the native module
    project, scene, geometry, materials, acoustics, mechanics, electromagnetics,
    simulation, optimization, experiments, measurements, automation,
    analysis, reporting, visualization, io, plugins, app, worker
src/python/ResonantModule.cpp   the native module _resonant (pybind11) over resonant_core
```

The package is the public surface; `_resonant` is an implementation detail.
Physics is never reimplemented in Python: every calculation calls the C++
core (fields, FDTD, modal and harmonic analysis, antenna models, optimization,
workflow actions). Python adds conveniences only (NumPy arrays, CSV / JSON /
HTML, plots).

## Objects and lifetimes

- `Project` owns a core `Project` in a shared holder with a recursive mutex.
  Copies (`project.copy()`) are deep.
- `ObjectRef` holds the holder and the object's **id**, and looks the object
  up on every access. It never dangles: after removal `valid` is False and any
  use raises `resonant.Error`. A reference keeps its project alive.
- Component data cross as dicts (`obj.component` is a copy;
  `obj.set_component(dict)` merges a patch and validates it through the
  component's own JSON reader, so invalid values are refused with the core's
  message).

## Results

| Call | Returns |
|---|---|
| `acoustics.field(project, region)` | dict: `pressure` complex ndarray `[w][v][u]`, grid origin / axes / counts, `max_pressure`, model id, assumptions |
| `acoustics.pressure(project, points)` | complex ndarray, one value per point |
| `simulation.fdtd(project, region, progress, cancel)` | dict: steady-state field, plan, assessment |
| `mechanics.modal / harmonic` | frequencies (Hz), effective masses, responses |
| `electromagnetics.antenna / pattern / pattern_metrics` | impedance, directivity; `magnitude` on a θ, φ grid; directivity, beamwidth, sidelobes |
| `optimization.run` | the run record (candidates, best, Pareto, termination), recorded in the project unless `record=False` |
| `automation.run` | the workflow run record (steps, outputs, status) |

## Errors, threads, cancellation

- Every core failure raises `resonant.Error` with the core's message.
- Long calls release the GIL; progress and cancellation callbacks reacquire it
  (`CallbackContext`). Each project serializes its own calls with its mutex;
  independent projects run in parallel threads (tested).
- `simulation.Background(fn, *args)` runs a call in a thread with `wait`,
  `cancel` and `progress` (callbacks are passed only to functions that take
  them).

## Versioning

`API_VERSION` is major.minor ("1.0"). Minor versions only add; a major version
may change or remove and is announced. `require(major, minor)` fails with a
clear message. The native module states the API it implements; the package
refuses a module of another major version.

## Tests

`python/tests/` (unittest): imports, lifetimes, versioning, project and scene
operations, every domain, results, optimization with promotion, workflows,
experiments, errors, cancellation, threads, the worker protocol, the example
plugins, every example script, the generated API reference. They run from the
core suite (`tests/core/test_python_api.cpp`, against the module of the same
build tree) and are skipped, with the reason, when the module is not built.
