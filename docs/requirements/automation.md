# Python API, Scripting, Automation, Plugins, In-App Help and Demonstrations

**Owner of:** `RES-PY-001..011`, `RES-AUTO-001..008`, `RES-PLUG-001..008`, `RES-HELP-001..012`, `RES-DEMO-001..010`
**Source:** REQ-CHAT-20261126-010 (Prompt 14, "Python API, scripting, automation, extensibility, comprehensive HTML help and demonstrations")
**Refines:** `product.md` (scripting and automation), `optimization.md` RES-OPT-024 (JSON / CSV exchange for Python, kept)
**Decisions:** ADR-000215
**Documents:** `docs/python/`, `docs/automation/`, `docs/plugins/`, `docs/help/`, `docs/demos/`, `docs/reference/ANALYST_REFERENCE_AUDIT.md`
**Status:** normative; implementation status in `IMPLEMENTATION_STATUS.md`.

## Python API

### RES-PY-001 — A documented, versioned public Python API

The `resonant` package (`python/resonant/`) is the public API, version
major.minor (1.0), with `require(major, minor)`. It is organized by domain
(project, scene, geometry, materials, acoustics, mechanics, electromagnetics,
simulation, optimization, experiments, measurements, automation, analysis,
reporting, visualization, io, plugins, app). Python stays optional: the
application and the core build and run without it.
*Verification:* `ApiBasics.test_versions_and_requirements`; `PythonApi.ThePackageTestsPassAgainstThisBuildsModule` (skipped with the reason when not built).

### RES-PY-002 — No physics in Python

Every calculation the API offers runs in the C++ core through the native
module `_resonant` (pybind11); Python adds conveniences only.
*Verification:* `Simulations.*` compare the API's results with the core's known values.

### RES-PY-003 — Safe object lifetimes

Objects are referred to by id and looked up on every access; a removed
object raises `resonant.Error`; a reference keeps its project alive; copies
are deep. *Verification:* `ApiBasics.test_objects_have_safe_lifetimes`.

### RES-PY-004 — Construction, configuration, simulation and results

Python constructs every scene object type, configures components through
validated patches and settings, runs acoustic fields and pressures, the
full-wave solver, modal and harmonic analyses, antenna analysis, patterns,
steering and tapers, optimization (with promotion), workflows and experiment
validation, and reads results as NumPy arrays and dicts.
*Verification:* `ApiBasics.*`, `Simulations.*`, `OptimizationAndAutomation.*`, `Reporting.*`.

### RES-PY-005 — Errors, cancellation, threads

Core failures raise `resonant.Error` with the core's message; long calls
take progress and cancel callbacks and release the GIL; independent projects
run in parallel threads. *Verification:* `Simulations.test_errors_are_reported_not_hidden`, `test_long_running_simulations_can_be_cancelled`, `Threads.*`.

### RES-PY-006 — Changes to the open project are one undoable step

A script's changed copy of the project is applied as one composite of the
ordinary commands; undo restores the project exactly; what has no command is
reported. *Verification:* `ProjectChange.AChangedCopyBecomesOneUndoableStep`; `PythonUiTest.AppApplyIsOneUndoableChangeOfTheOpenProject`.

### RES-PY-007 — Interactive console

A console with the open project (a copy), the API and `app`: command
execution, multi-line input, highlighting, completion, history, error
reporting, inspection, help, script files, results.
*Verification:* `PythonUiTest.TheConsoleRunsCodeWithMultiLineInputErrorsAndInspection`.

### RES-PY-008 — Execution in a worker process; the interface never blocks

Console, editor and Python plugins run in a separate worker process; a long
script never blocks the interface; Stop interrupts it; a dead worker leaves the
application and the project intact. *Verification:* `PythonUiTest.ALongScriptIsInterruptedAndNeverBlocksTheInterface`, `AWorkerThatDiesLeavesTheApplicationAndProjectIntact`; `WorkerProtocol.*`.

### RES-PY-009 — Script editor

Highlighting, line numbers, indentation, completion, open / save, run, run
selection, stop, error navigation, templates, examples, scripts stored in the
project, user scripts outside the application's files.
*Verification:* `PythonUiTest.TheScriptEditorRunsScriptsAndSelectionsAndPointsAtErrors`.

### RES-PY-010 — Not a sandbox; no hardware

Scripts and plugins run with the user's permissions and are described as
such; the worker cannot start a physical output.
*Verification:* review of `docs/python/PYTHON_EXECUTION_ARCHITECTURE.md`; the worker has no device interface (`python/resonant/worker.py`).

### RES-PY-011 — Example scripts that run

At least seven example scripts with real results; every one runs to the end.
*Verification:* `ExampleScripts.test_every_example_runs`.

## Automation

### RES-AUTO-001 — One automation framework, domain independent

Workflows and actions live in `src/core/automation` (no physics domain);
actions are registered in `src/core/coupling`.
*Verification:* `tools/check_layering.py`.

### RES-AUTO-002 — Workflow definitions

`resonant.workflow` v1: identity, version, requirements, typed parameters
with units, steps (action, parameters, condition, repetition, dependencies,
failure behaviour), outputs, expected duration, help topics.
*Verification:* `Automation.WorkflowsRoundTripAndInvalidOnesAreRefused`.

### RES-AUTO-003 — Execution semantics

Parameter and output substitution with types, conditions, repetition,
dependencies, stop or continue on failure, cancellation between steps.
*Verification:* `Automation.ParametersAndReferencesResolveWithTheirTypes`, `ConditionsDependenciesRepetitionAndFailuresAreHonoured`, `CancellationStopsBetweenSteps`.

### RES-AUTO-004 — Background runs on a snapshot, recorded

Workflows run as jobs on a copy of the project; the result is applied as one
undoable change; every run is recorded in the project (last 50).
*Verification:* `Automation.AJobWorksOnItsOwnSnapshotAndHistoryIsKept`; `PluginsUiTest.AWorkflowFromAPluginRunsAsOneUndoableRecordedChange`.

### RES-AUTO-005 — Reusable engineering templates

Acoustic, mechanical, electromagnetic and optimization templates that run
with real actions. *Verification:* `Automation.TheAcousticTemplate...`, `TheMechanicalTemplate...`, `TheElectromagneticTemplates...`, `TheReflectorTemplate...`.

### RES-AUTO-006 — Front ends

Workflows run from Python, the Workflows dialog, the command line
(`--run-workflow`) and Demo Studio. *Verification:* `OptimizationAndAutomation.test_workflows_run_with_real_actions`; the GUI tests above.

### RES-AUTO-007 — Every scene object type from automation

Actions add and change every scene object type with validated component
patches. *Verification:* `Automation.SceneObjectsOfEveryTypeCanBeAddedAndChanged`.

### RES-AUTO-008 — No hardware from automation

No action drives hardware; experiments with outputs run only from the
Experiments panel with authorization. *Verification:* review of `WorkflowActions.cpp`; `DemoController` never authorizes.

## Plugins

### RES-PLUG-001 — Plugin model and manifest

Plugins are folders with `plugin.json` (id, name, version, author,
description, license, supported Resonant versions, plugin API version, kind,
entry, dependencies, requirements, extension points, declared permissions).
*Verification:* `Plugins.ManifestsAreChecked`.

### RES-PLUG-002 — Discovery and validation

Search paths; validation of manifests, versions, API version, Resonant range,
requirements, entries, duplicate ids and dependencies, with a state and a
reason per plugin. *Verification:* `Plugins.DiscoveryValidatesEveryPluginAndLoadsNothingUnasked`.

### RES-PLUG-003 — Explicit enabling

Nothing loads unless the user enabled it (persisted configuration; effective
on reload). *Verification:* `Plugins.DiscoveryValidatesEveryPluginAndLoadsNothingUnasked`; `PluginsUiTest.TheDialogShowsEveryPluginAndLoadsOnlyWhatIsEnabled`.

### RES-PLUG-004 — Extension points

Workflow templates (workflow packs, native plugins), geometry generators,
analyses and measurement importers (Python plugins).
*Verification:* `Plugins.AnEnabledWorkflowPluginAddsWorkflowsThatRunWithRealActions`; `PythonPlugins.*`.

### RES-PLUG-005 — Failures are reported and contained

A failing plugin (broken manifest, wrong API, missing dependency, bad
library, raising `register`) is reported with its reason and leaves the
project and the other plugins untouched; deliberately failing test plugins
exist. *Verification:* `Plugins.AFailingNativePluginIsReportedAndOthersStillLoad`; `PluginsUiTest.*`; `PythonPlugins.test_a_failing_plugin_raises_with_its_reason`.

### RES-PLUG-006 — Stable interfaces

Plugin API version 1; a C ABI host table versioned by size; versioning rules
documented. *Verification:* `docs/plugins/PLUGIN_API_REFERENCE.md`; `Plugins.DiscoveryValidates...` (wrong API refused).

### RES-PLUG-007 — Example plugins

A Python geometry generator, an analysis, a measurement importer and a
workflow pack. *Verification:* `PythonPlugins.*`; `Plugins.AnEnabledWorkflowPlugin...`.

### RES-PLUG-008 — Security stated, hardware excluded

Plugins are not sandboxed and the UI says so; no plugin interface starts a
physical output. *Verification:* the Plugins dialog's warning (`forms/PluginsDialog.ui`); review.

## In-app help

### RES-HELP-001 — An integrated, offline HTML help

A help window built into the application, working without a network.
*Verification:* `HelpUiTest.*`.

### RES-HELP-002 — One source, generated output

Markdown sources in `help/src` with stable topic ids; HTML, navigation and
search are generated by the build (both build systems); generated output is
never edited or committed. *Verification:* `Help.TheStrictBuildOfTheHelpSourcesPasses`.

### RES-HELP-003 — Navigation

Contents, breadcrumbs, back / forward, previous / next, sections, related
topics, cross-references. *Verification:* `HelpUiTest.NavigationBreadcrumbsHistoryAndReadingOrder`; `Help.TheIndexAnswersLookupsNavigationAndSearch`.

### RES-HELP-004 — Search

Full-text, offline, ranked, with highlighted terms and prefix matching.
*Verification:* `HelpUiTest.SearchRanksAndHighlightsOffline`; `Help.TheIndexAnswersLookupsNavigationAndSearch`.

### RES-HELP-005 — Images, mathematics, code

Screenshots load from the help's files and open full size; equations are
readable; code examples are shown verbatim. *Verification:* `HelpUiTest.ImagesMathematicsAndAMissingTopic`.

### RES-HELP-006 — A missing topic and a missing build are handled

*Verification:* `HelpUiTest.ImagesMathematicsAndAMissingTopic`, `TheHelpWorksFromAnInstalledLayout`; `Help.AMissingBuildIsReportedClearly`.

### RES-HELP-007 — Content

Pages for every category of the request (getting started, core concepts,
acoustics, mechanics, electromagnetics, simulation, experiments, optimization,
Python and automation, UI reference, troubleshooting, technical reference),
each with an implementation status; feature pages explain use, parameters,
results, mistakes, limitations and related topics.
*Verification:* the strict build (status required); `docs/help/DOCUMENTATION_COVERAGE.md`.

### RES-HELP-008 — Context-sensitive help from a central registry

F1 opens the topic of the panel, tab, editor or dialog in use; topics come
from one registry (`src/app/HelpTopics.cpp`), never from file paths in UI
code; failure messages lead to troubleshooting topics.
*Verification:* `HelpUiTest.ContextHelpOpensTheTopicOfThePanelInUse`.

### RES-HELP-009 — Real screenshots, captured reproducibly

Screenshots are captured from the running application by a manifest of
Demo Studio steps, with stable names, page associations, annotated copies
beside the originals, and no private data.
*Verification:* `DemoStudioTest.ACaptureWritesTheImageAndItsAnnotatedCopy`; `resonant --capture-screenshots` (evidence).

### RES-HELP-010 — Validation of links and screenshots

Broken links, missing anchors, unknown related topics, missing images,
screenshots outside the manifest, manifest entries for missing pages and
screenshots on unavailable features are build errors.
*Verification:* `Help.BrokenLinksMissingImagesAndScreenshotsAreDetected`.

### RES-HELP-011 — Generated API reference

The Python API reference page is generated from the package and kept current.
*Verification:* `ApiReference.test_the_generated_reference_is_current`.

### RES-HELP-012 — Measured documentation coverage; packaged with the application

A coverage report per major feature (topic, status, screenshots, examples,
demonstrations, Python API), kept current; the help is installed with the
application and works from the installation.
*Verification:* `ApiReference.test_the_documentation_coverage_report_is_current`; `HelpUiTest.TheHelpWorksFromAnInstalledLayout`; `resonant --check-help` from an installation (evidence).

## Demonstrations

### RES-DEMO-001 — Demo Studio

A demonstration environment in the application, separate from the physics.
*Verification:* `tools/check_layering.py`; `DemoStudioTest.*`.

### RES-DEMO-002 — Profiles

Profiles with id, story (problem, configuration, calculation, meaning,
inspection, value, assumptions), audience, domain, status, project, workspace,
requirements, data, sequence, expected results, camera and visualization,
screenshots, notes, duration, completion criteria, limitations.
*Verification:* `Demo.EveryBundledDemonstrationLoadsAndTellsItsStory`.

### RES-DEMO-003 — Sequences of application-level steps

Explicit steps with stable ids, description, action, parameters, expected
state, time limit, failure behaviour, notes and screenshot; no screen
coordinates. *Verification:* `Demo.BrokenSequencesAndProfilesAreRefusedWithTheReason`.

### RES-DEMO-004 — Event-based waiting; failures never hidden

Steps wait for jobs, controller signals, the worker or the presenter, never a
fixed delay; a failed simulation fails its step; a time limit fails a step.
*Verification:* `DemoStudioTest.AFailingSimulationFailsItsStepAndATimeLimitIsEnforced`, `StopCancelsTheRunningSimulation`.

### RES-DEMO-005 — Reproducible

The same demonstration from a clean state calculates the same values.
*Verification:* `DemoStudioTest.ARunIsReproducibleAndRecordedInItsManifest`.

### RES-DEMO-006 — Real capabilities only

Every result is calculated in the run; synthetic data is labelled; partial
demonstrations say what remains. *Verification:* `DemoStudioTest.EveryDemonstrationRunsFromACleanStateWithRealResults`; `Demo.EveryBundledDemonstration...` (G partial).

### RES-DEMO-007 — Presenter mode

A clean layout with step instructions, notes, progress, pause / resume, next,
restart and exit that keeps the project.
*Verification:* `DemoStudioTest.PresenterModeHidesAndRestoresTheLayoutAndKeepsTheProject`, `NarrationPausesWaitForThePresenterAndRestartRunsAgain`.

### RES-DEMO-008 — Run manifests

Profile, sequence version, application version, project and its SHA-256,
solver settings, screenshots, notes, status, timestamps, per-step outcomes.
*Verification:* `Demo.RunManifestsCarryWhatARecordingNeeds`; `DemoStudioTest.ARunIsReproducibleAndRecordedInItsManifest`.

### RES-DEMO-009 — The initial demonstrations A–H

Overview, acoustic focusing, passive focusing, mechanical resonance, RF
antennas, inverse design, experimental correlation (partial), Python
automation. *Verification:* `DemoStudioTest.EveryDemonstrationRunsFromACleanStateWithRealResults`.

### RES-DEMO-010 — Asset library and recording preparation

`demos/` (profiles, sequences, presenter notes, assets, screenshots, reports,
manifest); fixed window sizes and unattended runs for recording.
*Verification:* `Demo.EveryBundledDemonstration...`; `docs/demos/VIDEO_PRODUCTION_GUIDE.md`.
