---
id: py.api
title: The Python API
category: Python and Automation
order: 20
status: available
summary: Build, configure, simulate, optimize, automate and report from Python with the resonant package.
keywords: [python, api, scripting, project, scene, simulation, numpy]
related: [ref.api, py.console, py.examples, py.workflows]
---

The `resonant` package is the documented, stable way to use Resonant from Python
(API version 1.0). It calls the same C++ models as the application: a field
calculated in Python is the field the application shows.

## The pieces

| Module | For |
|---|---|
| `resonant.Project`, `resonant.project` | new, open, save, copy, summary |
| `resonant.scene` | add and find objects: geometry, emitters, arrays, targets, regions, reflectors, structures, actuators, sensors, antennas, antenna arrays |
| `resonant.materials`, `resonant.geometry` | the material library, geometry kinds and their parameters |
| `resonant.acoustics` | frequency, fields on regions, pressure at points, focusing |
| `resonant.simulation` | the full-wave (FDTD) solver, background runs |
| `resonant.mechanics` | modal and harmonic analyses |
| `resonant.electromagnetics` | antenna analysis, patterns, pattern metrics, steering, tapers |
| `resonant.optimization` | templates, runs, best and Pareto candidates, promotion |
| `resonant.experiments`, `resonant.measurements` | experiment definitions, validation, measurement comparison |
| `resonant.automation` | workflow templates, actions, running workflows |
| `resonant.analysis`, `resonant.reporting`, `resonant.io` | peaks, widths, CSV / JSON / HTML, plots |

Every function is listed in the [Python API reference](../reference/api.md).

## A first script

```python
import resonant

p = resonant.Project("Focus")
array = resonant.scene.add_array(p, "Array", count=16, spacing=0.0043)
target = resonant.scene.add_target(p, "Focus", position=(0, 0, 0.06))
resonant.acoustics.focus(p, array, target)            # phases for the target
print(abs(resonant.acoustics.pressure(p, [(0, 0, 0.06)])[0]), "Pa")
p.save("focus.resonant")
```

## Objects are references

`add_*` and `project.object("name")` return an `ObjectRef`. It refers to the object
by id and looks it up on every use, so it never points at freed memory: after
`p.remove(obj.id)`, `obj.valid` is False and using it raises `resonant.Error`.
`obj.component` returns a copy of the domain data; change it with
`obj.set_component({...})`, which is validated (a negative amplitude is refused).

## Results

Fields are dictionaries with NumPy arrays: `field["pressure"]` is complex, shaped
`[w][v][u]` along the region's axes. Mechanical results give `frequencies` (Hz);
patterns give `magnitude` on a θ, φ grid. Units are SI throughout ([Units](../reference/units.md)).

## Errors

Everything the core refuses raises `resonant.Error` with the core's own message,
for example "the full-wave solver refuses: no active emitter lies in the plane of
the region". Catch it like any exception.

## Long calculations

Long functions accept `progress=lambda fraction, message: ...` and
`cancel=lambda: bool` and release Python's lock while they run, so other threads
keep working. `resonant.simulation.Background(function, *args)` runs one in a thread
with `wait()`, `cancel()` and `progress`. Each project has its own lock: use one
project per thread for parallel studies.

## In the application

In the [console](console.md) and the [script editor](editor.md), `project`
is a copy of the open project and `app.apply(project, "what")` hands your changes
back as one undoable step.
