---
id: ref.project-format
title: Project file format
category: Technical Reference
order: 50
status: available
summary: The structure of a .resonant project file, how it is validated when opened, how its version and migrations work, and what is stored beside it.
keywords: [project, file format, JSON, schema version, migration, extensions]
related: [gs.saving, ref.data-formats, ref.units, ref.coordinates, trouble.build-runtime]
---

A Resonant project is one **JSON** document with the extension
`.resonant`. It holds the scene, the materials, the solver settings, the
records of the runs and the definitions of experiments and optimization
problems. Large numerical data does not belong in it: calculated fields are
kept in memory for the session and recalculated, and measurement data is
stored in a folder beside the project ([Data formats](data-formats.md)).

## The document

```json
{
  "format": "resonant-project",
  "schemaVersion": 1,
  "application": {"name": "Resonant", "version": "0.1.0"},
  "project": {"id": "...", "name": "...", "createdUtc": "...", "modifiedUtc": "..."},
  "units": {"length": "m", "time": "s", "frequency": "Hz", "mass": "kg", "angle": "rad",
            "rotation": "intrinsic Z-Y-X Euler angles stored as [x, y, z]",
            "axes": "right-handed; X horizontal, Y depth, Z vertical (up)"},
  "materials": [ ... ],
  "scene": {"objects": [ ... ]},
  "simulations": [ ... ],
  "extensions": { ... }
}
```

| Key | Content |
|---|---|
| `format` | always `resonant-project`; a file without it is not a project |
| `schemaVersion` | the file format version, an integer; 1 is the current and first one |
| `application` | the program and version that saved the file |
| `project` | id, name, and creation and modification times (UTC, ISO 8601) |
| `units` | the conventions of every number in the file, stated so that no reader has to guess ([Units](units.md), [Coordinate conventions](coordinates.md)) |
| `materials` | the project's material library |
| `scene.objects` | the top-level scene objects, each with its `children` |
| `simulations` | simulation configurations: id, name, domain, solver id, parameters |
| `extensions` | the settings and records of the domains, kept as found |

## Materials

Each material has an `id`, a `name`, a `description` and `domains`: an
object with up to three property sets, `acoustic`, `mechanical` and
`electromagnetic`. Every property is
`{"value": ..., "provenance": ..., "source": "..."}`, optionally with a
`validity` range (`minFrequencyHz`, `maxFrequencyHz`, `minTemperatureK`,
`maxTemperatureK`) and a `frequencyTable` of `[frequencyHz, value]` rows.
The provenance is one of `measured`, `manufacturer`, `literature`,
`theoretical` or `illustrative`.

| Domain | Property keys |
|---|---|
| acoustic | `density`, `soundSpeed`, `attenuation`, `absorption`, `referenceTemperature` |
| mechanical | `density`, `youngsModulus`, `poissonsRatio`, `lossFactor` |
| electromagnetic | `relativePermittivity`, `relativePermeability`, `conductivity`, `lossTangent` |

## Scene objects

| Key | Content |
|---|---|
| `id` | unique in the file |
| `type` | `geometry`, `group`, or the kind of a domain object (`acoustic.emitter`, `acoustic.array`, `acoustic.region`, `em.antenna`, `mechanical.structure`, `wave.manipulator`, ...) |
| `name`, `visible` | as in the scene tree |
| `transform` | `position` (m), `rotation` (rad, `[x, y, z]`), `scale`, relative to the parent |
| `geometry` | `kind` (`box`, `sphere`, `reflector.parabolic`, `lens.spherical`, `rod.horn`, ...) and its `parameters` in SI units |
| `material` | the id of a material of the project |
| `component` | the domain data: `type` and `data` |
| `children` | the objects inside a group |

## Extensions

| Key | Content |
|---|---|
| `acoustics` | frequency, medium, field model, propagation options, memory limit, reflector surface resolution |
| `fdtd` | acoustic full-wave settings: signal, resolution, boundaries, recording, compute backend, memory limit |
| `mechanics` | mechanical analysis settings |
| `electromagnetics`, `electromagneticFullWave` | electromagnetic study and Yee solver settings |
| `acousticRuns`, `mechanicsRuns`, `electromagneticRuns` | records of the runs: solver, version, settings, warnings, time |
| `mechanicsMeasurements`, `electromagneticMeasurements` | imported measured responses and one-port data |
| `experiments` | experiments, calibration records, measurement records, spatial data, comparisons (version 1) |
| `optimization` | problems, runs, candidates and promoted revisions (versioned) |

A project without experiments or optimization has no such key. Keys that
this version does not know are kept unchanged when the project is saved.

## Validation on opening

The whole document is checked before anything is replaced: a project is
loaded completely or not at all, and the error names the entry, for example
`scene.objects[2] ("Dish").geometry: Parabolic reflector has no parameter "radius"`.
Unknown geometry kinds and component types, duplicate object ids, values
that are not finite, and invalid material properties are refused. Two
problems are repaired with a warning in the Console instead: a reference to
a material that is not in the project is removed, and a geometry parameter
that a later version added keeps its default when the file does not have it.

## Versions and migrations

`schemaVersion` changes only when the format changes incompatibly. A file
saved by a newer version is refused:

```text
the project was saved by a newer version of Resonant (file format 2, this version reads up to 1). Update Resonant to open it.
```

An older file is converted step by step, one version at a time, each step
logged as `The project was converted from file format N to N+1.`; saving
writes the current version. Format 1 is the first, so no conversion step
exists yet. **Analysis > Diagnostics** shows the format this version reads.

## Saving

Saving writes the document, indented, to a temporary file beside the
project and then replaces the project with it, so a failed save never
leaves a truncated file. The modification time is updated with each save.

## Editing a project by hand

A project file can be read and changed with any text editor or JSON tool,
and some settings (the `memoryLimitBytes` of the `acoustics`, `fdtd` and
`mechanics` entries) have no field in the panels yet. Work on a copy, keep the units of the
file, and open the copy: the validation reports any mistake with its
location and leaves the current project untouched.

## Related topics

- [Saving and loading projects](../getting-started/saving.md)
- [Data formats](data-formats.md)
- [Units](units.md)
- [Build and runtime problems](../troubleshooting/build-runtime.md)
