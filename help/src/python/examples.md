---
id: py.examples
title: Example scripts
category: Python and Automation
order: 70
status: available
summary: Seven runnable scripts that focus an array, sweep frequency, compare beam thicknesses, analyse antennas, optimize, run a workflow and write a report.
keywords: [examples, scripts, python, tutorial, sweep, report]
related: [py.editor, py.console, py.api, py.workflows]
---

Open them with **File > Open Example** in the [script editor](editor.md), or
run them with any Python that finds the package ([installation](installation.md)).
Each prints its results; files go to the folder in `RESONANT_EXAMPLE_OUTPUT` (or a
temporary folder). The numbers below are what this version calculates.

| Script | What it shows |
|---|---|
| `01_focus_array.py` | Focus a 16-element 40 kHz array 6 cm away: the pressure at the focus rises from 81.7 Pa to 253.9 Pa (3.1 times); the −6 dB focal width is 10.4 mm. Exports the plane as CSV. In the console it adds the array to your project (undoable). |
| `02_frequency_sweep.py` | The same focused array from 20 to 60 kHz: the focal pressure stays the same (the element waves add in phase at any frequency) while the −6 dB width falls from 22 mm to 7.6 mm, about 1.3 λz/D. |
| `03_beam_modes.py` | An aluminium cantilever of 3, 5 and 8 mm: f1 = 9.80, 16.33 and 26.13 Hz, growing in proportion to the thickness as beam theory says. |
| `04_antenna_array.py` | A half-wave dipole (73.2 + 43.0j Ω, 2.15 dBi), then a 10-element array: −13.1 dB sidelobes uniform, −30.0 dB with a 30 dB Dolph-Chebyshev taper, 11.8 dBi steered 30°. |
| `05_optimize_and_promote.py` | Runs the "array phase focus" optimization template and promotes the best candidate (recorded as a design revision). |
| `06_run_workflow.py` | Lists the workflow templates and runs `em.array-steering`, printing each step. |
| `07_html_report.py` | A focused-array field study written as an HTML report with a plot (when matplotlib is installed). |

## The pattern of a script

```python
import resonant
from resonant import app

p = project if "project" in globals() else resonant.Project("Study")  # the console's copy, or a new project
# ... build, calculate, print ...
if app.running_in_console():
    app.apply(p, "What this script changed")       # one undoable step
```

The scripts are checked by Resonant's tests: each must run to the end.
