# ADR-000215: Python API, automation, plugins, in-app help and Demo Studio — a pybind11 package over the core, a worker process for the console, JSON workflows of registered actions, explicit plugins, generated offline help with a topic registry, demonstrations as application-level step sequences

**Status:** ACCEPTED
**Date:** 2026-11-27
**Type:** architecture
**Requirements:** `docs/requirements/automation.md` RES-PY-001..011, RES-AUTO-001..008, RES-PLUG-001..008, RES-HELP-001..012, RES-DEMO-001..010
**Request:** REQ-CHAT-20261126-010
**Builds on:** ADR-000205 (layering), ADR-000214 (optimization), ADR-000213 (experiments and output authorization)

## Context

Resonant had to become scriptable, automatable, extensible and
self-documenting: a Python API, a console and editor, workflows, plugins, a
complete offline help with real screenshots, and repeatable demonstrations
for presentations and videos, all on real calculations, without blocking the
interface, without claiming isolation that does not exist, and without ever
starting a physical output on its own. RF_IQ_Analyst was inspected as a
reference (`docs/reference/ANALYST_REFERENCE_AUDIT.md`).

## Decisions

1. **Python API = a pybind11 module over `resonant_core` plus a pure-Python
   package.** `_resonant` binds projects (shared holder with a mutex),
   id-based object references, component patches through the components' own
   JSON readers, and the calculations; `python/resonant` is the documented,
   versioned surface (API 1.0). No physics in Python. Long calls release the
   GIL; callbacks reacquire it. Built when Python headers and pybind11 are
   found (`python.pri`, `RESONANT_WITH_PYTHON`); Python stays optional.
2. **The console, editor and Python plugins run in a worker process**
   (`python3 -m resonant.worker`, JSON lines). Alternatives: an embedded
   interpreter in the GUI thread (blocks), or in a thread (a crash kills the
   application, Ctrl+C is process-wide, live objects need locking). The worker
   gets a JSON snapshot; changes come back as a whole project and are applied
   as one composite of the ordinary commands (`projectChangeCommand`), so undo
   is exact and listeners see ordinary scene events. SIGINT interrupts.
3. **Workflows are JSON (`resonant.workflow` v1) of registered actions.** The
   engine (`src/core/automation`) is domain independent; actions are registered
   in `src/core/coupling`; conditions reuse the optimization expression
   language (extended with comparisons and logic); runs happen on snapshots as
   jobs; the run is recorded in the project.
4. **Plugins are explicit.** Manifests with an API version and a supported
   Resonant range; three kinds: workflow packs (data), Python plugins (in the
   worker), native plugins (a C ABI host table, `dlopen`, never unloaded).
   Nothing loads unless enabled; every plugin has a state and a reason; no
   sandbox is claimed; `permissions` are declarations.
5. **Help: one Markdown source, generated at build time, shown in
   QTextBrowser.** QtWebEngine is not available on the build platforms and the
   help needs no scripting; the build tool (standard library Python) validates
   everything (links, anchors, related topics, images, screenshot manifest,
   statuses) and fails on any error; mathematics is readable Unicode; style is
   inlined. A Qt-free `HelpIndex` does navigation and ranked search. F1 uses a
   central registry keyed by panel class and tab object name.
6. **Screenshots are Demo Studio sequences.** The manifest's entries are
   steps run by the same engine as demonstrations; the capture uses the
   application's own widget grab (the OpenGL viewport renders into it); images
   are committed so the help works offline; annotated copies sit beside the
   originals.
7. **Demonstrations are application-level step sequences**
   (`resonant.demo-profile`, `resonant.demo-sequence`, `resonant.demo-run`),
   executed by `DemoController` with the panels' and controllers' own
   functions and stable object ids — no screen coordinates or input injection.
   Waiting is event-based (jobs started by the step, controller completion
   signals, the worker's reply, the presenter); time limits bound failure only.
   A failing step fails the run; nothing old is shown as new.
8. **No physical output from any of it.** The worker, plugins, workflows and
   demonstrations have no path to output authorization; a demonstration that
   would need outputs stays partial (G) instead.
9. **Packaging**: install rules in both build systems put the help,
   demonstrations, examples, example plugins and the Python package under
   `<prefix>/share/resonant`; `ResourcePaths` finds them from an installation
   or a build tree; `--check-help` verifies an installation.

## Consequences

- Panel class names, tab object names and example object ids are now part of
  the help and demonstration interface; the tests check every registry topic,
  every demonstration and every screenshot reference.
- The console works on a copy: results calculated in Python appear in the
  application only after `app.apply` and a recalculation (documented).
- Native plugins can crash the application (documented); Python plugins can
  only crash the worker.
- `QTextBrowser` limits the help's styling (no scripts, simple CSS), accepted
  for an offline help without a web engine.
- Teardown order: controllers that hold jobs now check that the document still
  exists (found by the Demo Studio tests).
