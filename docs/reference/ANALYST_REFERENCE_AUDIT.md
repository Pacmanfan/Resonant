# RF_IQ_Analyst reference audit

**Request:** REQ-CHAT-20261126-010 (Part 1). **Decision:** ADR-000215.
**Method:** a read-only inspection of the RF_IQ_Analyst repository (a sibling checkout named in the request) (its sources,
`.ui` forms, documentation, demo and documentation tooling). Nothing in that
repository was modified, renamed, deleted, built or run. It is a reference,
not a code base to merge: no code was copied into Resonant.

## 1. What the application is

RF_IQ_Analyst is a Qt 6 / C++17 desktop application for recorded and live
radio-frequency IQ data. Its layers are a Qt-free signal-processing engine
(`RF_Engine`, `libiqfile`), a Qt-free application library (`AnalystCore`),
the GUI (`Analyst`, `Analyst/gui_library`), a command-line client
(`AnalystCLI`) and companion applications. qmake is canonical; a Qt-free
headless CMake build compiles the same source lists. Documentation lives in
`docs/` (user manual, architecture, requirements, decisions) with `evidence/`
records, a process like Resonant's (AGENTS.md, requirements, ADRs).

## 2. Main window and interface structure

| Element | How Analyst does it | Source |
|---|---|---|
| Central view | one fixed OpenGL surface (spectrum + waterfall with a draggable divider) in a central workspace; a CPU renderer fallback (`--display-renderer=cpu`) | `Analyst/applicationwindow.cpp`, `docs/architecture/display-layout.md` |
| Toolbars | an icon-only main toolbar in task groups (source, playback, record, pipeline, views, mission, transmit, settings, playbooks, tools; help at the right); a second, context row that appears only while a live tunable radio is connected | `desktop_commands.cpp` |
| Docks | a collapsible configuration dock on the left (sections), an analysis "tool chest" on the right that slides away behind an edge handle, scope / constellation / events / markers docks | `DockSlideOut`, `PanelContainer` |
| Menus | File, View, Tools, Playbooks, Help; large tools open as single-instance windows (acquisition, pipeline manager, mission workspace, map, settings with 16 tabs, ...) | `ToolWindowManager` |
| Status | status-bar messages plus permanent indicator chips (health dots, analysis progress, transmit, web UI) that open the matching window | `WgtHealthIndicator`, `WgtAnalysisIndicator` |
| Errors | commands that cannot run are greyed with the reason as tooltip (from a capability context); failures in the status bar with the cause; modal message boxes still common | `qt_command_binder.cpp` |

## 3. Workflows and their relationships

- **Load → look → tune → process → keep**: a source feeds a processing
  pipeline whose output is the central display; the left dock tunes the
  current pipeline, the right dock analyses; detections, signals and markers
  are unified into one activity model that feeds markers, missions, the map
  and reports.
- **Data provenance**: the file inspector shows where each value came from
  (SigMF metadata, file header, file name, user entry) and never guesses a
  missing value; "Configure IQ File..." asks only for what is missing.
- **Every action is a registered command** reachable from the toolbar, the
  menus, a Ctrl+K command palette, playbooks, the automation interface and the
  CLI. Controls record who changed them (user, automation, AI, sweep).
- **Configuration**: atomic JSON settings, remembered panel states, playbooks
  (21 JSON recipes with preconditions and success criteria), band and sweep
  plans; no general preset system.

## 4. Terminology and progressive disclosure

Engineering vocabulary (RBW, FFT rate, dBFS / dBm with calibration states,
CA-CFAR, PRI) with long tooltips; a "primary control" per tool; collapsible
sections, a closed tool chest, folded "Advanced" groups and hidden unsupported
controls. User pages follow "What it is / Why / Basic operation /
Troubleshooting / Related", with concept primers and a glossary; a lint tool
rejects developer terms in user documentation.

## 5. Demonstration, screenshots, help and scripting

| Area | Analyst |
|---|---|
| Demonstrations | outside the application: 41 YAML scenarios (demo / test / exploration) run by a Python driver against the automation interface; semantic targets (`command:`, `widget:`, `tab:`) found by inspecting the widget tree; X11 XTEST input for clicks; a PySide6 overlay for captions, highlights and cursor; modes clean / guided / presentation / debug; `wait_for` on live state and `expected_results`; a video standard (1080p30, durations); a catalog of feature → demonstration coverage |
| Screenshots | a capture tool with 48 scenarios, generated (not committed) |
| Help | a QTextBrowser help window over a manual generated from Markdown (contents, incremental search, F1, chapters in the Help menu), tutorials, presentation decks from one source |
| Scripting | a protobuf automation protocol over a local socket, an MCP bridge, the CLI, playbooks and an automation workbench |
| Extensions | external-process decoders described by JSON only; no in-process plugin SDK |

## 6. Assessment

**Strong patterns** (worth adapting):

1. One command model behind every way of acting (menus, palette, automation).
2. Honest availability: a disabled command says why.
3. Value provenance shown to the user.
4. Demonstrations that drive the real application through semantic targets
   and wait for real state, on real data, with expected results.
5. Documentation, screenshots, demonstrations and decks from one source.
6. A fixed central view, a configuration dock and a slide-away analysis dock;
   a context toolbar row only when a live device is connected; status chips
   for background work.

**Weaknesses** (not to copy):

1. Recording bound to X11 pixel input (XTEST) and screen grabs.
2. Demo fixtures outside the repository; scenario text drifting from the UI
   (a walkthrough names a removed panel).
3. A crowded icon-only toolbar and many top-level windows.
4. Modal message boxes that block automation.
5. Requirement identifiers in user-visible strings.

## 7. What Resonant adopted (and how), and what it did not

| Analyst pattern | In Resonant (ADR-000215) |
|---|---|
| Demonstrations driving the application by semantic targets, waiting on live state | Demo Studio runs `resonant.demo-sequence` steps that call the application's own commands by stable object ids, panel and tab names (no screen coordinates, no synthetic input) and wait for job completion and controller signals, never for a fixed delay (`src/app/DemoController.cpp`) |
| Expected results, modes, presenter use | each profile states its expected results, completion criteria, story and limitations; presenter mode with step instructions and notes; a run manifest of every run (`docs/demos/`) |
| Fixtures | every demonstration uses a versioned example project in the repository; synthetic data is labelled in its own header and in the profile |
| Feature → demonstration catalog | `demos/manifest.json`, `docs/demos/DEMO_CATALOG.md`, `docs/help/DOCUMENTATION_COVERAGE.md` |
| Help window over a generated manual with F1 | QTextBrowser help over HTML generated from `help/src` Markdown, offline search with ranking and highlighting, breadcrumbs, reading order, a central help-topic registry for F1 (`src/app/HelpTopics.cpp`) |
| Screenshots from scenarios | the screenshot manifest's entries are Demo Studio sequences; captures use the application's own widget grab (the 3D viewport renders into it), and are committed so the help works offline |
| Honest availability, value provenance | every help page carries a status (available / experimental / planned / not implemented); plugins show their state and why; demonstrations say what is partial |
| Concept primers, page structure | the help's content structure (what / when / how / parameters / results / mistakes / limits / related) |
| Automation interface | an in-process Python API (pybind11) plus a separate worker process for the console; workflows as JSON (`docs/python/`, `docs/automation/`) |

Not adopted: X11 input injection and screen grabs; an external overlay
process; a window per tool; an icon-only toolbar; a socket protocol (the
Python package calls the core directly; the console talks to its own worker
over standard input and output).

## 8. Implications for Resonant's architecture

- Stable object names for panels and tabs, and stable object ids in example
  projects, are part of the demonstration and help interface; renaming one is
  a change to demonstrations and help (the tests check every topic and step).
- The viewport must be capturable by the application itself; it is
  (`QWidget::grab` renders the OpenGL viewport into the image).
- Physics stays in the Qt-free core so the same calculations run headless,
  in Python, in workflows and in demonstrations.
- Formats are versioned (`resonant.workflow`, `resonant.demo-profile`,
  `resonant.demo-sequence`, `resonant.demo-run`, `resonant.help-index`,
  `plugin.json` with `apiVersion`).
