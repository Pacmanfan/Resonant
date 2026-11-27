# Python execution architecture

**Decision:** ADR-000215. **Requirement:** RES-PY-008.

## Options evaluated

| Option | Responsiveness | Isolation | Access to the project | Verdict |
|---|---|---|---|---|
| Embedded interpreter in the GUI thread | blocks the GUI | a crash kills the application | direct | rejected |
| Embedded interpreter in a worker thread | GIL contention with callbacks into Qt; Ctrl+C is process-wide | a crash kills the application | direct, needs locking of live objects | rejected |
| **Separate worker process** (`python3 -m resonant.worker`) | the GUI never waits | a crash ends only the worker | a JSON snapshot; changes come back as a diff | **chosen for the console, editor and Python plugins** |
| In-process module for external Python (pybind11) | the caller's own process | — | direct | **the API** (`import resonant`) |

## The worker protocol (version 1)

One JSON object per line on stdin / stdout. Requests carry an `id`; every
request ends with `{"type": "done", "ok": ...}`; the application side also
synthesizes `done` for requests a dead worker cannot answer.

| Request | Reply messages |
|---|---|
| `exec` (`code`, `mode` single / block / exec, `filename`) | `stdout`, `stderr`, `incomplete`, `error` (type, message, traceback, line in the submitted code), `apply`, `message`, `pull` |
| `complete` (`text`) | `completions` |
| `inspect` (`expression`) | `inspect` (valueType, repr, doc) |
| `set_project` (`project`: the project JSON) | — |
| `load_plugins` (`plugins`: directories) | `plugins` (per plugin: ok, extensions or error) |
| `call` (`plugin`, `extension`, `params`) | `result`, `apply` |
| `reset` | — |

Start-up sends `ready` (protocol, API and core versions, or the import error).

## Application side

`PythonSession` (src/app) owns the `QProcess`: environment (`PYTHONPATH` to the
package, `RESONANT_PYTHON_MODULE_DIR` to the module of the build or
installation, unbuffered UTF-8), parsing, busy tracking, SIGINT for Stop,
crash detection (pending requests fail, the console says so, the project is
untouched). The console, the editor, Python plugins and Demo Studio share one
session.

## Safety

The worker is not a sandbox: it runs with the user's permissions, as the UI
and help say. It has no access to the application's devices or output
authority: a script or plugin cannot start a physical output.

## Locating Python and the package

`ResourcePaths` (src/app): environment variables, then an installation
(`<prefix>/share/resonant/python`), then the build tree and the source tree.
`RESONANT_PYTHON` chooses the interpreter.
