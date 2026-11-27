---
id: py.installation
title: Python installation and configuration
category: Python and Automation
order: 10
status: available
summary: What the Python API needs, how the build finds it, and how the console, scripts and your own Python find the resonant package.
keywords: [python, install, pybind11, numpy, module, configuration, environment]
related: [py.api, py.console, trouble.python, gs.installation]
---

Resonant works without Python. Python adds the API, the [console](console.md),
the [script editor](editor.md) and Python [plugins](plugins.md). The
physics always runs in Resonant's C++ core: Python only drives it.

## What you need

| Component | Needed for | Notes |
|---|---|---|
| Python 3.10 or newer (`python3`) | running scripts, the console, Python plugins | also builds the help |
| Python development headers (`python3-dev`) and pybind11 (`pybind11-dev`) | building the native module `_resonant` | only at build time |
| NumPy | field results as arrays | required by the package |
| matplotlib | plots in reports and examples | optional |

## Building the module

The build looks for Python and pybind11 itself:

- **qmake**: `python.pri` enables the module when `python3-config` and the pybind11
  headers are present; `qmake6 CONFIG+=no_python` builds without it.
- **CMake**: the option `RESONANT_WITH_PYTHON` (on by default); the configure output
  says `Resonant Python API module: ON` or `OFF`.

The module is written to `<build tree>/src/python/_resonant.cpython-*.so`. It must
be built by the same Python version that runs it (the file name says which).

## How Python finds the package

The console and the editor start `python3 -m resonant.worker` with the package and
the module on its path; nothing has to be installed. To use the API from your own
Python, put the package directory on `PYTHONPATH` and tell it where the module is:

```python
# In a shell:
#   export PYTHONPATH=<source tree>/python
#   export RESONANT_PYTHON_MODULE_DIR=<build tree>/src/python
import resonant
resonant.require(1, 0)          # fails clearly if the API is older
print(resonant.API_VERSION, resonant.CORE_VERSION)
```

In an installation the package (with its module) is in `share/resonant/python`.

## Environment variables

| Variable | Meaning |
|---|---|
| `RESONANT_PYTHON` | the interpreter the console starts (default: `python3` on the `PATH`) |
| `RESONANT_PYTHON_PACKAGE_DIR` | the directory that contains the `resonant` package |
| `RESONANT_PYTHON_MODULE_DIR` | the directory that contains `_resonant*.so` (when it is not in the package) |
| `RESONANT_PLUGIN_PATH` | extra plugin directories, separated by `:` ([Plugins](plugins.md)) |

## Checking it works

Open **Tools > Python Console**. The first line says `Python ready: resonant API 1.0`.
If it says the package could not be imported, see [Python errors](../troubleshooting/python.md).

> **Warning** Scripts and plugins are programs that run with your permissions. They are
> not sandboxed. Run only code you trust. They cannot start a physical output: outputs
> need your authorization in the Experiments panel ([Safety](../experiments/safety.md)).
