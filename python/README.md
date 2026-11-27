# The resonant Python package

The documented Python API of Resonant (API 1.0, REQ-CHAT-20261126-010,
ADR-000215). It drives Resonant's C++ core through the native module
`_resonant` (built from `src/python/` when Python headers and pybind11 are
found); no physics is reimplemented in Python.

```text
resonant/      the package: project, scene, geometry, materials, acoustics, mechanics,
               electromagnetics, simulation, optimization, experiments, measurements,
               automation, analysis, reporting, visualization, io, plugins, app, worker
examples/      seven runnable example scripts (python/tests runs every one)
templates/     starting points for the script editor
tests/         the package's tests (run by the core suite against the same build)
```

Use it from your own Python:

```bash
export PYTHONPATH=$PWD/python RESONANT_PYTHON_MODULE_DIR=$PWD/build/debug/src/python
python3 python/examples/01_focus_array.py
python3 -m unittest discover -s python/tests
```

In the application, **Tools > Python Console** and **Tools > Script Editor**
run Python in a worker process with the open project as `project`.

Documentation: the in-app help (topics `py.api`, `ref.api`), `docs/python/`.
