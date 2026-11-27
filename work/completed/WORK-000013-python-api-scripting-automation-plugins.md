# WORK-000013: Python API, scripting, automation, plugins, in-app help and Demo Studio

**Status:** completed
**Type:** FEATURE
**Request:** requests/processing/2026-11-26-prompt-14-resonant-python-api--req-chat-20261126-010.md
**Requirements:** RES-PY-001, RES-PY-002, RES-PY-003, RES-PY-004, RES-PY-005, RES-PY-006, RES-PY-007, RES-PY-008, RES-PY-009, RES-PY-010, RES-PY-011, RES-AUTO-001, RES-AUTO-002, RES-AUTO-003, RES-AUTO-004, RES-AUTO-005, RES-AUTO-006, RES-AUTO-007, RES-AUTO-008, RES-PLUG-001, RES-PLUG-002, RES-PLUG-003, RES-PLUG-004, RES-PLUG-005, RES-PLUG-006, RES-PLUG-007, RES-PLUG-008, RES-HELP-001, RES-HELP-002, RES-HELP-003, RES-HELP-004, RES-HELP-005, RES-HELP-006, RES-HELP-007, RES-HELP-008, RES-HELP-009, RES-HELP-010, RES-HELP-011, RES-HELP-012, RES-DEMO-001, RES-DEMO-002, RES-DEMO-003, RES-DEMO-004, RES-DEMO-005, RES-DEMO-006, RES-DEMO-007, RES-DEMO-008, RES-DEMO-009, RES-DEMO-010
**Decision:** ADR-000215
**Evidence:** VER-000013
**Test-Run:** 20261127T142308-3323701, 20261127T142934-3381583
**Test-Suites:** core, app, cmake, layering, audits

## Scope

Python API, scripting, automation, plugins, in-app help and Demo Studio (Prompt 14, milestone M9): a documented Python API over the
core, a console and script editor in a worker process, JSON workflows of registered actions, explicit plugins, a 120-topic offline
help with real screenshots and context help, Demo Studio with presenter mode and demonstrations A-H, packaging, and a read-only audit
of RF_IQ_Analyst.

## Implementation

- Python: `src/python/ResonantModule.cpp` (pybind11 module `_resonant`: projects in a shared holder with a mutex, id-based
  ObjectRef, component patches through the components' JSON readers, calculations with the GIL released and callbacks
  reacquiring it); `python/resonant/` (the documented package, API 1.0, by domain; `app` bridge; `worker` JSON-lines protocol;
  `plugins` Python plugin API); `python/examples/` (seven scripts), `python/templates/`; build files `python.pri`,
  `src/python/python.pro`, `src/python/CMakeLists.txt`, `project.pro`, `CMakeLists.txt` (RESONANT_WITH_PYTHON).
- Automation: `src/core/automation/` (Workflow model, engine, ActionRegistry, WorkflowLibrary, WorkflowJob, history),
  `src/core/coupling/WorkflowActions.cpp` (actions for every domain), `WorkflowTemplates.cpp` (five templates),
  `src/core/optimization/Expression` (comparisons and logic for workflow conditions), `src/core/commands/ProjectChange`
  (a changed copy as one undoable composite).
- Plugins: `src/core/plugins/PluginManager`, `PluginApi.h` (C ABI); `plugins/examples/` (four), `tests/data/plugins/`
  (deliberately broken), `tests/data/screenshot-config/`.
- Help: `help/src/` (120 pages, categories, images), `help/screenshots/manifest.json`, `tools/help/build_help.py`,
  `generate_api_reference.py`, `coverage.py`, `help/help.pro`, `help/CMakeLists.txt`; `src/core/help/HelpIndex` (navigation,
  ranked search); `src/app/HelpWindow` (`forms/HelpWindow.ui`, `forms/HelpImageViewer.ui`), `src/app/HelpTopics` (F1 registry).
- Demonstrations: `src/core/demo/Demo` (formats, validation, library, SHA-256), `src/app/DemoController` (engine, captures),
  `DemoStudioPanel`, `PresenterOverlay` (`.ui` forms), `demos/` (eight profiles, sequences, presenter notes, manifest).
- App: `PythonSession`, `PythonConsole`, `ScriptEditor`, `CodeEditor`, `PythonHighlighter`, `WorkflowsDialog`, `PluginsDialog`,
  `ResourcePaths` (all with `.ui` forms where they have a layout); MainWindow(.ui): docks, Tools and Help menus, F1, presenter
  mode, `applyProjectCopy`; `main.cpp`: `--run-demo`, `--capture-screenshots`, `--run-workflow`, `--check-help`, `--presenter`,
  `--window-size`; install rules (main.pro, help.pro, python.pro, CMake).
- Shutdown fix: OptimizationController / ExperimentsController no longer use the deleted DocumentController in their destructors
  (QPointer guard; found by the Demo Studio tests).
- `tools/check_layering.py`: automation, help, demo and plugins include no physics domain.

## Tests

- Core: `tests/core/test_automation.cpp` (10), `test_plugins.cpp` (5), `test_project_change.cpp` (1), `test_help_index.cpp` (4: strict
  help build, broken-reference detection, index and search, missing build), `test_demo.cpp` (4), `test_python_api.cpp` (runs
  `python/tests`: 27 Python tests - API, worker protocol, example plugins, every example script, the generated API reference and
  the coverage report), `test_optimization.cpp` (comparison and logic operators).
- App: `tests/app/test_python_console.cpp` (5), `test_help_ui.cpp` (5: navigation, search, images and maths, F1 context help,
  installed layout), `test_plugins_ui.cpp` (3), `test_demo_studio.cpp` (7: every demonstration from a clean state, reproducibility,
  presenter mode, pauses and restart, stop, failure and time limit, captures).
- Also run by hand: all eight demonstrations with `--run-demo`, the 22 screenshots with `--capture-screenshots`, `make install` into
  a staging prefix with `--check-help` and demonstrations B and H from the installed binary.

## Regression impact and documentation

- The help build is part of `make`: a broken help reference fails the build (by design). MainWindow gained docks and menus (the
  existing GUI tests pass). The expression language gained operators (existing optimization tests pass). Controller destructors
  are safer at shutdown.
- Documentation: `docs/requirements/automation.md`, ADR-000215, `docs/python/` (3), `docs/automation/` (2), `docs/plugins/` (3),
  `docs/help/` (5, coverage generated), `docs/demos/` (6), `docs/reference/ANALYST_REFERENCE_AUDIT.md`, the 120 help pages,
  `python/README.md`, `plugins/README.md`, `demos/README.md`, `IMPLEMENTATION_STATUS.md`, `DECISIONS.md`, `docs/README.md`,
  `README.md`, `BUILDING.md`, manifests.

## Traceability

REQ-CHAT-20261126-010 -> RES-PY-001, RES-PY-002, RES-PY-003, RES-PY-004, RES-PY-005, RES-PY-006, RES-PY-007, RES-PY-008, RES-PY-009, RES-PY-010, RES-PY-011, RES-AUTO-001, RES-AUTO-002, RES-AUTO-003, RES-AUTO-004, RES-AUTO-005, RES-AUTO-006, RES-AUTO-007, RES-AUTO-008, RES-PLUG-001, RES-PLUG-002, RES-PLUG-003, RES-PLUG-004, RES-PLUG-005, RES-PLUG-006, RES-PLUG-007, RES-PLUG-008, RES-HELP-001, RES-HELP-002, RES-HELP-003, RES-HELP-004, RES-HELP-005, RES-HELP-006, RES-HELP-007, RES-HELP-008, RES-HELP-009, RES-HELP-010, RES-HELP-011, RES-HELP-012, RES-DEMO-001, RES-DEMO-002, RES-DEMO-003, RES-DEMO-004, RES-DEMO-005, RES-DEMO-006, RES-DEMO-007, RES-DEMO-008, RES-DEMO-009, RES-DEMO-010 -> WORK-000013 -> evidence (pending)

## Transitions

- 2026-11-27T19:08:15.898147+00:00 created in active by agent-dev-20261126151713-76f632
- 2026-11-27T19:34:01.095839+00:00 active -> completed by agent-dev-20261126151713-76f632
