---
id: trouble.build-runtime
title: Build and runtime problems
category: Troubleshooting
order: 20
status: available
summary: Fix a build that breaks after a change, an application that does not start or shows no 3D view, and projects that do not open or save.
keywords: [build, startup, OpenGL, display, project, log, diagnostics]
related: [trouble.installation, trouble.dependencies, trouble.memory, gs.saving, ui.panels]
---

This page collects the problems that appear after Resonant has been built
once: rebuilding, starting, showing the 3D view, and opening and saving
projects. Every message quoted here is the text the application shows.

## Where to look first

- **Analysis > Console.** Every log entry of the session, filtered by the
  **Show** list (*Everything*, *Information and above*, *Warnings and
  errors*, *Errors only*). Warnings from loading a project appear here.
- **Analysis > Diagnostics.** The version, the project file format it reads,
  the Qt version, the operating system, the build type (Debug or Release),
  the compute capabilities (CPU threads, VOLK, CUDA and each CUDA device),
  the graphics context and the units and axis conventions. Press
  **Refresh** after a change.
- **Standard error.** Everything that is logged is also written to the
  terminal, so a failure before the window appears is still visible. The
  first lines name the operating system, the Qt platform plugin, the
  library versions and the compute capabilities.

## The build fails after adding or removing a file

Both builds collect the sources of each target from its folder. CMake
notices new and deleted files by itself. qmake does not: run
`qmake6 -r ../../project.pro` in the build tree (the `-r` matters; without it
only the top-level Makefile is regenerated), then `make` again.

Other rebuild problems:

- **CUDA errors from `nvcc`.** Configure without the GPU backend
  (`-DRESONANT_WITH_CUDA=OFF`, qmake `CONFIG+=no_cuda`) or set the GPU
  architectures your toolkit supports (`RESONANT_CUDA_ARCHITECTURES`,
  qmake `RESONANT_CUDA_ARCHS`).
- **Warnings in Resonant's own code** are not expected: it builds without
  warnings at `-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor`. A new
  warning usually points at a real mistake in a local change.

## The application does not start

- **No display.** On a machine without a desktop session Qt cannot open a
  window. Run it on a virtual display:
  `xvfb-run -a -s "-screen 0 1600x1000x24" resonant`.
- **Shared libraries are missing.** The program links Qt and, when they were
  found at build time, VOLK, HDF5, ALSA and SoapySDR dynamically. Install
  the same libraries on the machine that runs it
  ([Missing dependencies](dependencies.md)).
- **It starts but closes at once** with `--screenshot <file>`: that option
  saves an image of the main window and exits on purpose.

## The 3D viewport stays empty

Resonant draws with OpenGL 4.5 Core and never falls back silently to
something else. If the system offers less, a dialog titled **3D graphics are
unavailable** says, for example:

```text
OpenGL 4.5 Core is required, but the system provided OpenGL 3.3 Core.
Update the graphics driver or run on hardware that supports OpenGL 4.5.

The viewport will stay empty. Projects can still be opened, edited
through the panels and saved.
```

Everything except the viewport keeps working. To get the view back,
update the graphics driver. On Linux, Mesa's software renderer (llvmpipe)
provides OpenGL 4.5 Core; it is what the application tests run on, and it is
slow but correct. **Analysis > Diagnostics > Graphics** shows the context
that was created.

> **Note** The behaviour without OpenGL 4.5 has not yet been seen on a
> machine that lacks it; the message path exists but is untested.

## A project does not open

The dialog **Could not open the project** gives the reason. The current
project stays open: a project is loaded completely or not at all.

| Message (start) | Meaning |
|---|---|
| `could not open <file>. The file may have been moved or deleted.` | the path is wrong or not readable |
| `<file> is not a valid project file (it is not well-formed JSON).` | the file is damaged or not a project |
| `this is not a Resonant project file (missing format tag "resonant-project")` | a JSON file of another kind |
| `the project was saved by a newer version of Resonant (file format N, this version reads up to M). Update Resonant to open it.` | update the application |
| `<file> could not be loaded: scene.objects[3] ("Lens"): ...` | one entry is invalid; the path names it |

When a project opens with repairs (for example a reference to a material
that is not in the project, which is removed), the status bar says
**Opened ... with N warning(s); see the Console**. The file format and its
checks are described in [Project file format](../reference/project-format.md).

## A project does not save

The dialog **Could not save the project** gives one of:

- `could not write to <file>. Check that the folder exists and is writable.`
- `could not replace <file>: the file may be in use or read-only.`

Saving writes a temporary file next to the project and then replaces the
project, so a failed save never leaves a half-written file. Choose
**File > Save As...** with a writable folder.

## The window layout is broken

Panels can be docked, floated and closed. **View > Reset Layout** restores
the default arrangement, and **View > Panels** shows a closed panel again.
The layout is remembered between sessions.

## A calculation does not finish

Long calculations run as background jobs. **Analysis > Simulation Status**
lists each job with its state, progress, elapsed time and message;
**Cancel Selected Job** stops one, **Simulation > Cancel All Jobs** stops
every unfinished job. If a run is much slower than its preflight estimate,
check that you are running a Release build and see
[Memory issues](memory.md).

## Related topics

- [Installation problems](installation.md)
- [Missing dependencies](dependencies.md)
- [Saving and loading projects](../getting-started/saving.md)
- [Panels](../ui/panels.md)
