---
id: trouble.python
title: Python errors
category: Troubleshooting
order: 100
status: available
summary: What the console's and scripts' messages mean when Python cannot start, the package cannot be imported, or a script fails.
keywords: [python, error, traceback, import, worker, console]
related: [py.installation, py.console, py.editor, trouble.plugins]
---

## The console does not start

| Message | Cause | What to do |
|---|---|---|
| `Python 3 was not found` | no `python3` on the `PATH` | install Python 3, or set `RESONANT_PYTHON` to the interpreter |
| `The resonant Python package was not found` | the package folder is not where Resonant looks | set `RESONANT_PYTHON_PACKAGE_DIR` to the folder that contains `resonant/` |
| `The resonant package could not be imported: ImportError ...` | the native module `_resonant` is missing, or was built for another Python version | build the module (see [installation](../python/installation.md)); its file name names the Python it needs, for example `cpython-312` |
| `ImportError: numpy` | NumPy is not installed | install NumPy for that Python |
| `the Python worker ended (exit status N)` / `crashed` | the worker process stopped | your project is unaffected; press **Restart** |

## A script fails

The console and the editor print Python's traceback in red; the editor also lists
the error under **Problems** with its line.

| You see | Means |
|---|---|
| `resonant.Error: ...` | Resonant refused, with its reason: an invalid value (`width must be positive`), an unknown object (`no object 'Array'`), a solver that cannot run (`no active source at the analysis frequency`) |
| `NameError: name 'project' is not defined` | outside the console there is no `project`; create one (`resonant.Project()`) or open one (`resonant.project.open(path)`) |
| `RuntimeError: not running inside the Resonant console` | `app.apply` was called from a stand-alone Python; check `app.running_in_console()` first |
| `KeyboardInterrupt` | you pressed **Stop**; the worker and its variables are kept |
| `resonant.Error: API ... is older than required` | `resonant.require()` asked for a newer API than this Resonant has |
| nothing changes in the application | the console works on a copy: call `app.apply(project, "...")` |

## Using an object after removing it

An `ObjectRef` refers to its object by id. After the object is removed,
`obj.valid` is False and using it raises `resonant.Error: the object ... no longer
exists`. Look it up again with `project.object("name")`.

## Slow scripts

The worker runs one request at a time: while a script runs, the console waits.
Long calculations report progress through `progress=` and stop through `cancel=`
or **Stop**. Prefer one calculation over many tiny ones (fields over a region rather
than `pressure()` point by point).
