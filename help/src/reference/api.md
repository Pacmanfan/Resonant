---
id: ref.api
title: Python API reference
category: Technical Reference
order: 80
status: available
summary: Every module, function and class of the resonant Python package (API 1.0), generated from the package itself.
keywords: [python, api, reference, functions, modules, resonant]
related: [py.api, py.console, py.editor, ref.plugin-api, py.examples]
---

This page is generated from the `resonant` package by `tools/help/generate_api_reference.py`;
the documentation strings in the code are its source. For how to use the API see
[The Python API](../python/api.md); for runnable scripts see [Example scripts](../python/examples.md).

API version **1.0**. `resonant.require(1, 0)` fails with `resonant.Error` when the installed
API is older or of another major version. Every error the core reports is raised as `resonant.Error`.

## Classes

### Project

A Resonant project: scene, materials, settings and results records.

| Member | Description |
|---|---|
| `__init__` | A new, empty project with the standard material library. |
| `add` | Add an object from a description dict (type, name, kind, parameters, component, position, rotation, material, id, parent). |
| `add_material` | Add a material (a dict in the project file's material format). |
| `copy` | An independent deep copy. |
| `from_json` | A project from its JSON text (as to_json() writes it). |
| `id` (property) | The project's stable id. |
| `load` | Open a .resonant project file (migrated to the current format). |
| `material` | A material by id or name, as a dict. |
| `materials` | Every material of the project's library, as dicts. |
| `name` (property) | The project's name. |
| `object` | An object by id or name. |
| `objects` | Every scene object, in pre-order. |
| `remove` | Remove an object (by id or name) with its children. |
| `save` | Save to a .resonant project file. |
| `set_settings` | Merge a dict into a domain's settings (validated). |
| `settings` | Project settings of a domain (acoustics, mechanics, electromagnetics, fdtd) as a dict. |
| `to_json` | The project as JSON text (the project file format). |

### ObjectRef

A scene object, referred to by id (it never dangles: a removed object raises).

| Member | Description |
|---|---|
| `__init__` | Initialize self.  See help(type(self)) for accurate signature. |
| `children` (property) | The child objects, in order. |
| `component` (property) | The domain data as a dict (a copy). |
| `component_type` (property) | The domain component type (acoustic.array, em.antenna, ...), or empty. |
| `geometry_kind` (property) | The geometry kind id (box, sphere, reflector.parabolic, ...), or empty. |
| `id` (property) | The stable object id (str). |
| `material` (property) | The assigned material id (set: an id or name of the library). |
| `name` (property) | The object's name (shown in the scene tree). |
| `parameters` (property) | Geometry parameters (SI) |
| `parent` (property) | The parent object (None at the top level). |
| `position` (property) | Local position, m |
| `rotation` (property) | Local Z-Y-X Euler angles [x, y, z], rad |
| `set_component` | Merge a dict into the domain data (validated). |
| `set_parameters` | Set geometry parameters together (validated by the geometry kind). |
| `type` (property) | What the object is: geometry, group, or its component type. |
| `valid` (property) | False once the object was removed from its project. |
| `visible` (property) | Shown in the viewport (hiding does not switch a source off). |
| `world_position` (property) | Position in world coordinates, m (through the parents). |

## Modules

### resonant.project

Projects: create, open, save, inspect.

| Function | Description |
|---|---|
| `new(name: str = 'Untitled')` | A new project with the example material library and an empty scene. |
| `open(path: str)` | Open a project file (.resonant). |
| `save(project, path: str) -> None` |  |
| `summary(project) -> dict` | What the project holds: objects by type, materials, settings present. |

### resonant.scene

Scene construction. Every helper returns an ObjectRef; ``add(project, dict)`` is the general form (see ``resonant.native_types()`` for the types).

| Function | Description |
|---|---|
| `add(project, description: dict)` |  |
| `add_geometry(project, name, kind='box', parameters=None, position=None, rotation=None, material=None, object_id=None)` |  |
| `add_emitter(project, name, frequency=40000.0, amplitude=1.0, phase=0.0, position=None, rotation=None, object_id=None, **settings)` |  |
| `add_array(project, name, count=8, spacing=0.0043, layout='linear', frequency=40000.0, rows=1, position=None, rotation=None, object_id=None, **settings)` |  |
| `add_target(project, name, position=(0.0, 0.0, 0.1), amplitude=1.0, suppress=False, object_id=None)` |  |
| `add_region(project, name, size=(0.1, 0.01, 0.1), samples=(61, 61), kind='planeXZ', position=None, object_id=None)` |  |
| `add_reflector(project, name, kind='reflector.parabolic', parameters=None, position=None, rotation=None, material='mat-aluminum', object_id=None)` |  |
| `add_structure(project, name, kind='box', parameters=None, idealization='beam', supports=None, position=None, material='mat-aluminum', object_id=None)` |  |
| `add_actuator(project, name, position, amplitude=1.0, phase=0.0, object_id=None)` |  |
| `add_sensor(project, name, position, object_id=None)` |  |
| `add_antenna(project, name, kind='antenna.dipole', parameters=None, frequency=1000000000.0, position=None, rotation=None, material='mat-copper', object_id=None)` |  |
| `add_em_array(project, name, count=8, spacing=0.15, frequency=1000000000.0, pattern='halfWaveDipole', layout='linear', rows=1, position=None, object_id=None)` |  |
| `find(project, reference)` |  |
| `remove(project, obj) -> None` |  |
| `types()` | The object types ``add`` accepts. |

### resonant.geometry

Geometry kinds and their parameters.

| Function | Description |
|---|---|
| `kinds(category: str = None)` |  |
| `kind(kind_id: str) -> dict` |  |

### resonant.materials

Materials of a project's library. Values carry their provenance; the example library is illustrative, not authoritative.

| Function | Description |
|---|---|
| `list_materials(project, domain: str = None)` |  |
| `get(project, reference: str) -> dict` |  |
| `add(project, material: dict) -> None` |  |

### resonant.acoustics

Acoustic fields, focusing and settings (analytical free field or the Kirchhoff reflector model; the full-wave solver is in ``simulation``).

| Function | Description |
|---|---|
| `settings(project) -> dict` |  |
| `set_frequency(project, frequency: float) -> None` |  |
| `field(project, region, model: str = 'auto') -> dict` | Complex pressure on a region: dict with 'pressure' (complex ndarray [w][v][u], Pa), the grid ('origin', 'axis_u', 'axis_v', 'axis_w', 'shape'), 'max_pressure', 'assumptions'. |
| `pressure(project, points, model: str = 'auto')` | Complex pressure (Pa) at points [(x, y, z), ...] as a NumPy array. |
| `focus(project, array, target=None, point=None, control: str = 'phase') -> dict` | Focus an array on a target object or a point by phase or time delay (changes the array). |

### resonant.mechanics

Structural analyses (finite elements).

| Function | Description |
|---|---|
| `modal(project, structure=None, modes: int = 6) -> dict` | Natural frequencies: {'frequencies': [Hz, ...], 'method': ...}. |
| `harmonic(project, structure=None, f_min: float = 10.0, f_max: float = 1000.0, points: int = 200) -> dict` | Forced response at the structure's sensors and targets: peak frequency and displacement per point. |

### resonant.electromagnetics

Antennas and arrays: impedance, patterns, steering, tapers.

| Function | Description |
|---|---|
| `antenna(project, antenna) -> dict` | Induced-EMF impedance, return loss, directivity and gain of an analytical antenna. |
| `pattern(project, obj, resolution_deg: float = 2.0) -> dict` | Normalized far-field pattern: {'magnitude': ndarray [theta][phi], 'step_deg', 'directivity', 'model'}. |
| `pattern_metrics(project, obj, resolution_deg: float = 1.0) -> dict` | Directivity (dBi), main lobe, sidelobe level and beamwidth. |
| `steer(project, array, theta_deg: float, phi_deg: float, method: str = 'phase') -> dict` |  |
| `taper(project, array, sidelobe_db: float = 30.0) -> dict` | Dolph-Chebyshev amplitudes for a linear array. |

### resonant.simulation

Long-running simulations and background execution.

| Function | Description |
|---|---|
| `fdtd(project, region, progress=None, cancel=None) -> dict` | Run the full-wave FDTD solver on a region (the project's FDTD settings). ``progress(fraction, message)`` is called during the run; ``cancel()`` returning True stops it. |
| `Background(function, *args, **kwargs)` | Runs ``function(*args, **kwargs)`` in a thread (with ``progress=`` and ``cancel=`` callables when the function accepts them). |

### resonant.optimization

Optimization: problems (dicts in the 'resonant.optimization.problem' format), runs (run records), candidates and promotion.

| Function | Description |
|---|---|
| `templates()` |  |
| `problem_from_template(project, template_id: str) -> dict` |  |
| `run(project, problem: dict, progress=None, cancel=None, record: bool = True) -> dict` | Run a problem; returns the run record. The design is never changed by a run. |
| `best(run_record: dict)` | The best feasible candidate of a single-objective run, or None. |
| `pareto(run_record: dict)` | The non-dominated feasible candidates (several objectives). |
| `candidates(run_record: dict, feasible_only: bool = False)` |  |
| `promote(project, run_record: dict, candidate) -> None` | Write a candidate into the design (recorded as a revision in the project). |

### resonant.experiments

Experiment definitions (sensors, actuators, acquisition, calibration references). Running an experiment with physical outputs is done in the application, where outputs need an explicit authorization; Python never starts a physical output.

| Function | Description |
|---|---|
| `workspace(project) -> dict` |  |
| `set_workspace(project, value: dict) -> None` |  |
| `list_experiments(project)` |  |
| `validate(project, experiment_id: str) -> dict` |  |

### resonant.measurements

Measurement records of a project and comparison of measured with simulated values.

| Function | Description |
|---|---|
| `records(project, experiment_id: str = None)` |  |
| `compare(measured, simulated) -> dict` | Error metrics of two equal-length sequences of real values on the same points: rms error, largest absolute error and the rms relative to the measured rms. Only compatible quantities in the same unit may be compared. |

### resonant.automation

Workflows: repeatable procedures made of registered actions ('resonant.workflow' documents).

| Function | Description |
|---|---|
| `templates()` |  |
| `template(workflow_id: str) -> dict` |  |
| `actions()` |  |
| `run(project, workflow, parameters: dict = None, output_dir: str = '', progress=None, cancel=None) -> dict` | Run a workflow (an id or a workflow dict); returns the run record. A failed required step stops the run (status 'failed'); outputs are by step id. |
| `action(project, action_id: str, params: dict = None, output_dir: str = '') -> dict` | Run one action directly. |
| `workflow(workflow_id: str, name: str, steps, parameters=None, description: str = '') -> dict` | Build a workflow document; steps are dicts {id, action, params, when?, repeat?, dependsOn?}. |

### resonant.analysis

Analysis helpers on results (NumPy).

| Function | Description |
|---|---|
| `peak(field: dict) -> dict` | Largest \|p\| of a field result and its grid index (w, v, u). |
| `width_6db(values, spacing: float) -> float` | Full width (same unit as spacing) of \|values\| >= peak / 2 around the peak of a 1D cut; NaN when the half level is not reached on both sides. |

### resonant.reporting

Reports: tables, CSV and HTML (standard library); plots with matplotlib when installed.

| Function | Description |
|---|---|
| `write_csv(path: str, rows) -> None` |  |
| `write_html(path: str, title: str, sections) -> None` | sections: [(heading, value)], values are text, dicts, lists or image file names ending in .png. |
| `plot_field(field: dict, path: str = None, title: str = '\|p\|')` | An image of \|p\| of a plane field result (matplotlib); saved to `path` when given. |

### resonant.visualization

Plots of results (matplotlib, optional).

| Function | Description |
|---|---|
| `plot_pattern_cut(pattern: dict, phi_index: int = 0, path: str = None, title: str = 'Pattern cut')` | \|E\| (dB) along theta at one phi column of a pattern result. |

### resonant.io

Data export and import (CSV, JSON).

| Function | Description |
|---|---|
| `export_field_csv(field: dict, path: str) -> None` | x, y, z, re, im, \|p\| for every sample of a field result. |
| `read_csv(path: str)` |  |
| `write_json(path: str, value) -> None` |  |

### resonant.plugins

The Python plugin API (plugin API version 1).

| Function | Description |
|---|---|
| `PluginApi(plugin_id: str)` |  |
| `load_plugin(directory: str) -> resonant.plugins.PluginApi` | Imports a Python plugin's entry module from its directory and calls ``register``. |
| `describe(api: resonant.plugins.PluginApi)` | The extensions of a loaded plugin without their functions (for the application). |

### resonant.app

The bridge between a script in the Resonant console and the application.

| Function | Description |
|---|---|
| `apply(project, description: str = 'Script changes') -> None` | Hand the (changed) project back to the application as one undoable change. |
| `pull() -> None` | Ask the application for a fresh copy of the open project (arrives as `project`). |
| `message(text: str) -> None` | Show a line in the application's status bar. |
| `running_in_console() -> bool` |  |
| `stdout_is_console() -> bool` |  |

## Versioning

The API version is major.minor. A minor version only adds; a major version may change or remove,
and is announced in the release notes. Scripts state what they need with `resonant.require(major, minor)`.
