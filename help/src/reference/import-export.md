---
id: ref.import-export
title: File import and export
category: Technical Reference
order: 100
status: available
summary: Every place in Resonant that reads or writes a file other than the project, with its format, where the data goes and what is not supported.
keywords: [import, export, CSV, Touchstone, STL, archive, report]
related: [ref.data-formats, ref.project-format, exp.measurements, opt.studio, sim.results]
---

Besides the project file itself, Resonant reads and writes a small set of
open formats. Imported data is always kept apart from calculated data: it
is labelled as measured or imported, and nothing imported is ever mixed into
a model result. The structure of each file is described on
[Data formats](data-formats.md).

## Export

| What | Where | Format |
|---|---|---|
| field values at probes and targets | **Analysis > Field Analysis**, **Export Points...** | CSV, or JSON when the file name ends in `.json` |
| pressure signals of the probes of a full-wave result | **Analysis > Time Signals**, **Export Signals...** | CSV |
| calculated one-port data of an antenna | **Electromagnetics** panel, **Impedance** tab, **Export .s1p...** | Touchstone 1 (GHz, RI) |
| an experiment's report | **Experimental Hardware**, **Measurements** tab, **Save Report...** | HTML |
| an experiment with its data | **Measurements** tab, **Export Archive...** (asks for a new folder name) | folder with `experiment.json` and `data/` |
| an optimization run | **Optimization Studio**, **Candidates** tab, **Export Run...** | run record JSON, or candidates CSV |
| an optimization candidate | **Candidates** tab, **Export...** (asks for a folder) | STL and OBJ per changed object, and `<candidate>.json` |
| a topology design | **Topology** tab, **Export STL...** | ASCII STL |
| an image of the main window | command line `resonant --screenshot <file> [project]` | image file named by its extension, for example PNG |

The point export uses the scene as it is now; the probe signals are those of
the result chosen on the tab. **Save Report...** needs a run of the
experiment in this session (`The experiment has no report yet: run it first.`). A full-wave result without probes has nothing
to export: `the result has no probe recordings: add probes inside the region before running`.

## Import

| What | Where | Format | Kept as |
|---|---|---|---|
| a measured frequency response | **Mechanics** panel, **Frequency Response** tab, **Import Measured Response...** | CSV | a measured response in the project, plotted beside the calculated one |
| measured one-port data of an antenna | **Electromagnetics** panel, **Impedance** tab, **Import Measured .s1p...** | Touchstone `.s1p` | *Measured data* for the sweep plot, never mixed with the model |
| network parameters of any port count | **Experimental Hardware**, **Measurements** tab, **Import Touchstone...** | Touchstone `.s1p` to `.s4p` (1.1 and 2.0) | a raw measurement, marked imported rather than acquired |
| a calibration record | **Calibration** tab, **Import Calibration...** | JSON | a new record in the append-only calibration library |
| an experiment archive | **Measurements** tab, **Import Archive...** (choose the folder) | folder from **Export Archive...** | the experiment, its calibrations and records merged into the project |

Imports are undoable project changes and are saved with the project. Each
import checks the whole file first and reports the first problem with its
line (for CSV and Touchstone), for example
`line 12: frequencies must increase` or
`only S parameters are read (the option line names Z)`. The
Electromagnetics panel reads one-port files only (`line N: more than one
port; only .s1p files are read`); larger networks go through
**Import Touchstone...**.

## Worked example

1. Open `examples/electromagnetics/a-half-wave-dipole.resonant` and select
   the dipole on the **Impedance** tab of the Electromagnetics panel.
2. Set **From**, **To** and **Frequencies**, press **Calculate Sweep** to
   see it, then **Export .s1p...**: the file holds the calculated reflection
   coefficient over that range, referred to the antenna's reference
   impedance, with a comment that it is an analytical model, not a
   measurement. Patch, horn and helical antennas have no calculated
   impedance: `no impedance is calculated for this antenna kind (design estimates only)`.
3. **Import Measured .s1p...** reads it back as *Measured data*; choose it
   under **Measured data** to plot it beside the model.

The two-port file `examples/experiments/rlc_antenna_and_line.s2p` is
synthetic data for **Import Touchstone...** in the Experimental Hardware
panel.

## Measurement data next to the project

Experiments do not import or export their raw data through dialogs: a run
records it into `<project name>.data/` beside the project file (HDF5 or
Resonant's binary format), read-only. Copy that folder together with the
project file, and keep its name matching the project's name.

## Not supported in this version

- Geometry import from CAD or mesh files (STEP, IGES, STL, OBJ, Gmsh):
  geometry is built from Resonant's own primitives and parametric objects.
- STEP export; field maps and volumes as files (only point values and
  probe signals are exported); result files in HDF5; multi-port Touchstone
  export.
- Import or export of material libraries.

Python scripts can read and write the same files, and read exported
optimization runs with `examples/optimization/read_run.py`; see
[The Python API](../python/api.md).

## Related topics

- [Data formats](data-formats.md)
- [Project file format](project-format.md)
- [Measurements](../experiments/measurements.md)
- [The Optimization Studio](../optimization/studio.md)
