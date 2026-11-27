---
id: ui.analysis-panels
title: Analysis panels
category: User Interface Reference
order: 80
status: available
summary: The Analysis panel's tabs: the Console log, Diagnostics, Simulation Status of background jobs, Field Analysis of acoustic results and Time Signals of full-wave runs.
keywords: [analysis, console, diagnostics, jobs, field analysis, time signals, export]
related: [acoustics.field-analysis, acoustics.fdtd, sim.results, sim.comparison, ui.panels]
---

The **Analysis** panel along the bottom of the window collects what the
application reports and what the acoustic solvers produced. It has five tabs:
**Console**, **Diagnostics**, **Simulation Status**, **Field Analysis** and
**Time Signals**. Results of the other domains are shown in their own panels:
mechanics in the Mechanics panel, antennas in the Electromagnetics panel,
measurements in the Experimental Hardware panel and candidates in the
Optimization Studio ([Panels](panels.md)).

## Console

Every message of the application in time order, one per line:
`date time.milliseconds [level] category: message`. Opening and saving
projects, scene changes and undo steps, jobs queued, started and finished,
refused operations, warnings and errors all appear
here. **Show** filters by level: *Everything*, *Information and above* (the
default at every start), *Warnings and errors* or *Errors only*. **Clear**
(or **Tools > Clear Console**) empties the log. When a project opens with
warnings, the status bar says so and the warnings are listed here.

## Diagnostics

A read-only list for checking the installation and for problem reports:

- **Application**: version, project file format, Qt version, operating system, CPU architecture, logical processors, Debug or Release build.
- **Compute**: CPU threads for solvers, VOLK vector kernels, CUDA and each CUDA device with its memory and whether it is usable.
- **Graphics**: the OpenGL context obtained, and the required OpenGL 4.5 Core.
- **Conventions**: internal SI units, axes, rotation order.
- **Project**: name, file, id, unsaved changes, number of objects, materials and undo steps.

**Refresh** reads everything again.

## Simulation Status

Every background job of the session: **Job**, **State** (queued, running,
completed, failed, cancelled), **Progress**, **Elapsed** time and the job's
**Message**. Select a row and press **Cancel Selected Job** to stop a queued
or running job; **Simulation > Cancel All Jobs** stops all of them. The tab
comes forward by itself when a calculation starts, and the status bar reports
when a job ends. Calculations never block the window: you can keep working
while a job runs.

## Field Analysis

Comes forward when an acoustic field result is displayed. On the left, a
summary of the displayed result: frequency, medium, wavelength, active
emitters, region size and sampling, maximum and mean pressure magnitude, the
location of the maximum, the solver and its time, what the colours show, and
the model's assumptions. On the right, two tabs:

- **Probes and Targets**: one row per probe and target point with its position, pressure magnitude (Pa, peak), Phase (deg), RMS (Pa), SPL (dB), active intensity (W/m²), the desired pressure of a target, and a note when a value is not valid (inside a source) or only approximate. Untick **Include targets** to list probes only.
- **Result Comparison**: one row per result of the session: model, peak pressure and its position, the width at half the peak pressure (-6 dB) along each sampled axis, the distance from the peak to the first target point (or a reflector's intended focus), the pressure there, the sidelobe-to-peak ratio and the solver time. A width of 0 means the half level is not reached inside the region.

**Refresh** updates the tables; **Export Points...** writes them to
a CSV or JSON file ([Field analysis](../acoustics/field-analysis.md),
[Comparing simulations](../simulation/comparison.md)).

## Time Signals

Comes forward when a full-wave (FDTD) run has finished. Choose the **Run** and
the **Probe**, then **Show**: *Pressure over time*, *Spectrum magnitude*,
*Spectrum phase* or *Energy over time* (the acoustic energy in the domain).
**Window** (*Hann* or *Rectangular*) is applied before the spectrum. The text
beside the plot states the run (steps, time step, simulated time, speed),
the signal, the response peaks of a spectrum and the frequency above which
the grid does not resolve the waves. Below the plot, the **Recorded instant**
slider shows a stored snapshot of the pressure in the viewport, and **Show
Steady State** shows the steady-state field of a continuous run. **Export
Signals...** writes the probe signals to a CSV file
([Acoustic full-wave (FDTD) simulation](../acoustics/fdtd.md)).

Probes and target points inside the region are recorded at every time step.
A run without one records no signal: add a probe and run again.

## Worked example

1. Open `examples/advanced_acoustics/e-resonant-cavity.resonant` (a rigid 40 x 30 mm cavity excited by a broadband pulse) and press **F6** (**Simulation > Run Full-Wave Simulation**).
2. **Simulation Status** shows the job running with its progress; the Console records that it was queued and started.
3. When it completes, **Time Signals** comes forward. Set **Show** to *Spectrum magnitude*: the peaks are the cavity's resonances, listed strongest first in the text beside the plot.
4. Press **Export Signals...** to save the probe signal as CSV for your own analysis.

## Limitations

- Plots cannot be zoomed or panned; export the data to examine it elsewhere.
- The Console shows the messages of the current session only, and its filter is not remembered.
- Field Analysis covers acoustic field results; the other domains report in their own panels.

## Related topics

[Field analysis](../acoustics/field-analysis.md) · [Acoustic full-wave (FDTD) simulation](../acoustics/fdtd.md) ·
[Result storage](../simulation/results.md) · [Comparing simulations](../simulation/comparison.md) · [Panels](panels.md)
