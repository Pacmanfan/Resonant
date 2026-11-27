---
id: gs.saving
title: Saving and loading projects
category: Getting Started
order: 60
status: available
summary: Save, open and protect your work - what a .resonant file contains, what stays next to it, and what has to be recalculated after reopening.
keywords: [save, open, project file, resonant file, unsaved changes, load, json]
related: [ref.project-format, gs.new-project, sim.results, ref.data-formats, ref.import-export]
---

A Resonant project is one file with the extension `.resonant`. It is a
readable JSON document that holds the scientific content of your work. It
does not hold window layouts, the camera or the selection.

## Commands

| Command | Shortcut | What it does |
|---|---|---|
| **File > Save** | Ctrl+S | writes the project to its file; asks for a name the first time |
| **File > Save As...** | Ctrl+Shift+S | writes the project to a new file, which becomes the project's file |
| **File > Open Project...** | Ctrl+O | replaces the open project with a file (after asking about unsaved changes) |
| **File > New Project** | Ctrl+N | an empty Untitled project (after asking about unsaved changes) |
| `resonant <file>` | | opens a project when the application starts |

The file dialogs start in the folder of the current project, else in the
folder you used last, else in your documents folder. If you type a name
without an extension, `.resonant` is added.

## Unsaved changes

Every edit that changes the project marks it as modified: the window title
gets an asterisk. Undoing back to the saved state clears it again. Before
**New**, **Open** or closing the window would discard changes, Resonant asks:

- **Save** saves first (and asks for a name if the project has none);
- **Discard** throws the changes away;
- **Cancel** keeps the project open and does nothing.

There is no automatic saving and no list of recent files: save often.

## What is in the file

| Stored in the project | Not stored |
|---|---|
| the scene: every object with its id, name, type, transform, visibility, group, geometry, material and domain settings | sampled fields, full-wave snapshots, mode shapes and other result data (recalculated) |
| the project's material library | window layout, panel arrangement, camera, selection |
| each domain's solver settings (medium, frequency, field model, full-wave and mechanical settings, compute backend) | the display length unit and the grid and axes switches (they are your settings) |
| records of completed runs: their configuration and metadata | run-time states of experiments and optimizations |
| experiments, calibrations and measurement records; optimization problems and run records | |
| the format and application version and the unit conventions (SI) | |
| data of features this build does not know, kept unchanged | |

After reopening a project, run its solvers again to see fields: for
example **Acoustics > Run Field Simulation** (F5) for an acoustic example.
Because each run record keeps the full configuration, the same inputs give
the same result.

## Files next to the project

Two kinds of data are too large for the project file and are written
beside it, named after the project:

- `<name>.data/measurements/...` holds measured data of experiments (HDF5 files, or Resonant's binary format when the build has no HDF5) and experiment reports. Raw measurements are written once and made read-only. An experiment needs a saved project before it can record anything.
- `<name>.optimization/` holds optimization checkpoints, written only when a run saves one, so a stopped run can be resumed.

Copy or move these folders together with the `.resonant` file.

## Opening a file safely

A file is checked completely before it replaces the open project. If
anything is wrong - the file is missing, is not valid JSON, has an invalid
value or comes from a newer format version - a message box *Could not open
the project* gives the reason and the current project stays as it was.

Some problems are repaired with a warning instead, for example an object
that refers to a material missing from the library loses that reference.
The status bar then says *Opened ... with N warning(s); see the Console*,
and the warnings are in **Analysis > Console**.

Older files are migrated step by step to the current format. The current
format is version 1, the first one, so no migration has been needed yet.

## Saving safely

Saving is atomic: the project is written to a temporary file in the same
folder, which then replaces the old file. A failed save (a full disk, a
folder you cannot write to) reports *Could not save the project* and
leaves the previous file intact.

## Worked example

1. **File > Open Project...** and choose `examples/acoustics/c-focused-planar-array.resonant`.
2. The scene appears and is framed; the field plane is empty, because fields are not stored.
3. Press F5: the field is recalculated. Its 241 x 321 samples and 64 sources take well under a second in a Release build (0.23 s even on a single thread).
4. **File > Save As...** and save it under a new name in your own folder. Saving does not add the field to the file; it adds the run record.

## Common mistakes

- **Saving over an example.** The examples are reference projects checked by the tests; keep your changes in a copy (Save As).
- **Moving only the `.resonant` file** of a project with experiments or optimizations: its measurement and checkpoint folders stay behind.
- **Expecting the layout to travel with the project.** Layouts are per user; use **View > Reset Layout** if a layout looks wrong.

The file format itself is described in
[Project file format](../reference/project-format.md).
