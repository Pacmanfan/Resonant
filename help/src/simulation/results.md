---
id: sim.results
title: Result storage
category: Simulation
order: 80
status: available
summary: Where simulation results live, what a result records about how it was made, what the project file keeps, and how to export values.
keywords: [results, metadata, run record, export, csv, json, reproducibility]
related: [sim.architecture, sim.comparison, acoustics.field-analysis, ref.project-format, ref.data-formats]
---

A simulation result is a snapshot: the calculated data plus a complete
record of what produced it. Results live **in memory for the session**. The
project file keeps a **run record** of every completed run, not the sampled
data; opening the project again and running the same region reproduces the
field.

## Results of the session

- **Acoustic fields** (analytical runs and the steady-state fields of
  full-wave runs): **Acoustics** panel, tab **Simulation**, group **Results
  of this session**. Each line gives the name, the peak and the solver time,
  in the form "Field plane @ 40000 Hz, run 2 - peak ... Pa, ... s".
  Select a line to display it; **Remove** drops it; **Show field** hides or
  shows the field in the viewport.
- **Full-wave runs** (probe signals, snapshots, energy): **Analysis > Time
  Signals**, **Run** list. A full-wave run is named "<region>, full-wave
  run N".
- **Mechanical, electromagnetic and optimization results**: in their own
  panels (Mechanics, Electromagnetics, Optimization Studio).

A result never changes after it is made. Editing the scene, the medium or the
settings afterwards does not touch it, and the Field Analysis summary shows
the frequency and medium **of the result**, which can differ from the current
settings.

## What a result records

| Item | Example |
|---|---|
| Solver id and version | `acoustic.freefield.analytical`, version 1 |
| Software version, start time (UTC), elapsed time | |
| Assumptions | "Objects in the scene do not affect the calculated field." |
| Configuration | settings, region, every source with position, frequency and amplitude, targets, reflectors with their boundary and surface elements |
| Full-wave plan | grid, cell size, time step, steps, Courant number, cells per wavelength, phase-velocity error, how each object was represented, warnings, compute backend |

The Field Analysis summary lists the solver, its version, the solver time and
the assumptions of the displayed result.

## What the project file keeps

The project stores the scene, the materials and the settings of each domain
(medium, model, frequency, full-wave settings, mechanical and
electromagnetic study settings), and a run record of each completed run in
its `extensions` section: `acousticRuns`, `mechanicsRuns` and
`electromagneticRuns`. Run records are kept apart from the settings, so
undoing a settings change never removes one. The sampled fields, probe
signals and snapshots are **not** saved. See
[Project file format](../reference/project-format.md).

Optimization problems, run records and revisions are kept in the project;
checkpoints of a running optimization are written next to a saved project,
so a run can be resumed ([The Optimization Studio](../optimization/studio.md)).
Measurements of the experiments workspace are stored next to the project in
an HDF5 or binary store ([Measurements](../experiments/measurements.md)).

## Exporting values

| What | Where | Format |
|---|---|---|
| Values at probes and targets | Analysis > Field Analysis, **Export Points...** | CSV or JSON |
| Probe signals of a full-wave run | Analysis > Time Signals, **Export Signals...** | CSV |
| One-port data of an antenna | Electromagnetics panel, **Export .s1p...** | Touchstone |
| Optimization runs and candidates | Optimization Studio, **Export Run...**, **Export...**, **Export STL...** | JSON, CSV, STL |

The point CSV has one header line and one line per point:

```text
name,x_m,y_m,z_m,pressure_real_Pa,pressure_imag_Pa,magnitude_Pa_peak,phase_rad,rms_Pa,spl_dB,
spl_reference_Pa,intensity_x_W_m2,intensity_y_W_m2,intensity_z_W_m2,intensity_exact,near_source
```

`intensity_exact` is 0 when the directivity gradient was neglected;
`near_source` is 1 inside a source's radius. The JSON form
(`"format": "resonant-point-report"`) adds the frequency, medium, model,
time convention, and the desired pressure of targets. The probe-signal CSV
starts with a comment line naming the run, the solver and the time step,
then a `time_s` column and one pressure column (Pa) per probe. Details:
[Data formats](../reference/data-formats.md).

## Worked example

1. Open `examples/acoustics/a-two-source-interference.resonant` and press
   **F5**.
2. Save the project and reopen it: the results list is empty, but the run
   record is in the file.
3. Press **F5** again: the same field is calculated from the stored scene
   and settings.
4. **Analysis > Field Analysis > Export Points...** writes the two probes to
   `points.csv`.

## Not available

Saving sampled fields in the project, HDF5 output of simulation fields,
checkpointing and resuming of full-wave runs, storing a region of interest
or full time histories of a field, and particle-velocity fields of the
full-wave solver.
