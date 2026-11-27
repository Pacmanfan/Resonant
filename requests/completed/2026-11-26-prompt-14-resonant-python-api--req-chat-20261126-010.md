# Request: # PROMPT 14 — RESONANT: PYTHON API, SCRIPTING, AUTOMATION, EXTENSIBILITY, COMPRE

**Request ID:** REQ-CHAT-20261126-010
**Source:** chat
**Type:** FEATURE
**Priority:** NORMAL
**Created:** 2026-11-26T15:53:25-05:00
**Status:** completed
**Attachments:** - none

## Original User Statement

> # PROMPT 14 — RESONANT: PYTHON API, SCRIPTING, AUTOMATION, EXTENSIBILITY, COMPREHENSIVE HTML HELP AND DEMONSTRATION SYSTEM

You are continuing development of **RESONANT — Wave Field Engineering Studio**.

This milestone establishes the extensibility, automation, documentation, and demonstration infrastructure that will make Resonant easier to develop, use, teach, showcase, and extend.

This is a major product milestone.

Resonant must evolve from a collection of engineering capabilities into a coherent, extensible engineering platform with:

1. A documented Python scripting API.
2. An interactive scripting environment.
3. Repeatable experiment and simulation automation.
4. A plugin and extension architecture.
5. User-created workflows.
6. A comprehensive in-app HTML help system.
7. An integrated screenshot and documentation-generation workflow.
8. A dedicated demonstration environment.
9. Reproducible demonstrations based on actual application capabilities.
10. A systematic analysis of the existing RF_IQ_Analyst application as a reference for the demonstration system.

Do not weaken or replace existing physics, geometry, simulation, measurement, or optimization functionality.

The objective is to make Resonant extensible and demonstrable without requiring users or developers to modify the application core for every new workflow.

---

# PART 1 — ANALYZE THE EXISTING RF_IQ_ANALYST APPLICATION

## 1. Inspect the existing Analyst project

Before designing the Resonant demonstration environment, inspect:

`/home/dev/RF_IQ_Analyst`

This is an existing engineering application. Treat it as a reference implementation for engineering-oriented user interfaces, workflows, visualization, and technical demonstrations.

Do not modify, rename, delete, or otherwise alter the Analyst project.

The purpose of this inspection is to understand what makes a sophisticated engineering application useful and demonstrable.

Perform an audit-first inspection.

Examine:

- Repository structure.
- Existing documentation.
- Build system.
- Application architecture.
- Qt interface.
- Main window organization.
- Dockable panels.
- Toolbars.
- Menus.
- Visualization components.
- Plotting and analysis interfaces.
- Data-loading workflows.
- Live-data workflows.
- Signal-processing workflows.
- Configuration management.
- User interaction patterns.
- Error reporting.
- Status indicators.
- Technical terminology.
- Existing demo or test data.
- Screenshot or presentation capabilities, if any.

Pay particular attention to how the application presents technical information without overwhelming the user.

Study the relationship between:
- Main visualization.
- Data sources.
- Analysis tools.
- Configuration panels.
- Status information.
- Results.
- Workflow progression.

Identify which interface patterns could improve Resonant.

## 2. Produce an Analyst reference audit

Create:

`docs/reference/ANALYST_REFERENCE_AUDIT.md`

Include:

- What Analyst does well.
- Its most useful interface patterns.
- Its visualization patterns.
- Its engineering workflow conventions.
- Its information hierarchy.
- Its strengths for demonstrations.
- Its limitations as a demonstration reference.
- Which ideas should be adapted for Resonant.
- Which ideas should not be copied.
- Potential implementation implications.

Do not simply reproduce the Analyst interface.

Resonant has a broader scope involving three physical wave domains, 3D geometry, numerical simulation, experiments, and optimization.

The objective is to learn from Analyst's engineering presentation and workflow design while maintaining Resonant's own identity.

If the path does not exist or cannot be accessed, report that clearly and continue using the documented Resonant requirements. Do not invent observations about Analyst.

---

# PART 2 — PYTHON API AND SCRIPTING ARCHITECTURE

## 3. Establish a public Python API

Python must become a first-class extension and automation interface.

The core application remains C++20.

Use the existing Qt6, CMake, and pybind11 architecture.

The Python API should expose stable, documented interfaces for:

- Projects.
- Scenes.
- Geometry.
- Materials.
- Emitters.
- Arrays.
- Wave manipulators.
- Sensors.
- Actuators.
- Experiments.
- Simulations.
- Simulation results.
- Field sampling.
- Optimization.
- Measurements.
- Visualization configuration.
- Reports.
- Data export.

Do not expose arbitrary internal C++ implementation details as the public API.

Create a deliberate Python-facing abstraction layer.

Use consistent naming and predictable object lifetimes.

The API must have documented versioning and compatibility expectations.

## 4. Python package organization

Create a structured Python package, provisionally:

`resonant`

Organize it into logical modules, for example:

- `resonant.project`
- `resonant.scene`
- `resonant.geometry`
- `resonant.materials`
- `resonant.acoustics`
- `resonant.mechanics`
- `resonant.electromagnetics`
- `resonant.sources`
- `resonant.arrays`
- `resonant.simulation`
- `resonant.experiments`
- `resonant.measurements`
- `resonant.optimization`
- `resonant.visualization`
- `resonant.analysis`
- `resonant.reporting`
- `resonant.automation`
- `resonant.io`

The exact organization may be adjusted based on the existing codebase.

Do not create duplicate implementations of core physics in Python.

Python should orchestrate the C++ engines and provide scientific workflow flexibility.

## 5. Core Python API capabilities

Support workflows such as:

**Project management**
- Create a project.
- Open a project.
- Save a project.
- Inspect project contents.
- Export results.

**Scene construction**
- Create geometry.
- Place emitters.
- Configure arrays.
- Assign materials.
- Create targets.
- Create sensors.

**Simulation**
- Configure a solver.
- Run a simulation.
- Monitor progress.
- Cancel a simulation.
- Access results.
- Sample fields.

**Optimization**
- Define design variables.
- Define objectives.
- Define constraints.
- Run optimization.
- Inspect candidates.
- Promote a candidate.

**Experiments**
- Define an experiment.
- Configure sensors.
- Configure acquisition.
- Import measurements.
- Apply calibration.
- Compare against simulation.

**Reporting**
- Generate plots.
- Generate tables.
- Export results.
- Generate reports.
- Save visualizations.

Provide complete examples rather than exposing functions without practical usage documentation.

## 6. Interactive scripting console

Implement a Python console integrated into Resonant.

It should support:

- Command execution.
- Multiline input.
- Syntax highlighting.
- Basic autocomplete.
- Command history.
- Error reporting.
- Object inspection.
- Help lookup.
- Script execution.
- Script file loading.
- Result inspection.

Expose the active project and supported application interfaces in a controlled way.

Do not expose unsafe internal application state unnecessarily.

The console must not block the Qt GUI during long-running simulations.

Support background jobs and asynchronous operations.

Provide clear handling of exceptions and application errors.

## 7. Script editor

Provide a practical script editor.

Support:

- Python syntax highlighting.
- Line numbers.
- Indentation.
- Basic autocomplete.
- Save and open.
- Run current script.
- Run selected code.
- Stop execution where safe.
- Error navigation.
- Script templates.

Allow scripts to be associated with projects.

Support user scripts stored separately from application code.

Provide a useful initial collection of example scripts.

## 8. Python execution architecture

Determine the safest and most reliable execution model.

Evaluate:

- Embedded Python.
- Dedicated Python worker process.
- Hybrid execution.

Consider long-running scientific workloads, application stability, object lifetime, thread safety, and third-party library compatibility.

Do not execute long-running Python jobs on the GUI thread.

Define explicit data-transfer and error-reporting interfaces.

Do not allow Python garbage collection to invalidate C++ objects still in use.

Define clear ownership and lifetime rules.

Document which APIs are thread-safe and which require application-thread access.

If Python is unavailable or disabled, the core C++ application must remain fully functional.

---

# PART 3 — AUTOMATION AND WORKFLOW SYSTEM

## 9. Unified automation framework

Introduce a reusable automation system.

Support automation of:

- Project setup.
- Geometry generation.
- Material assignment.
- Emitter configuration.
- Array construction.
- Simulation execution.
- Parameter sweeps.
- Optimization.
- Measurement import.
- Calibration.
- Result processing.
- Report generation.
- Demonstration sequences.

Automation must use the same underlying application APIs as normal user interactions.

Avoid implementing a separate automation-only physics or geometry engine.

## 10. Workflow definitions

Introduce a structured workflow representation.

A workflow should contain:

- Stable ID.
- Name.
- Description.
- Version.
- Required capabilities.
- Inputs.
- Parameters.
- Execution steps.
- Dependencies.
- Outputs.
- Error handling.
- Cancellation behavior.
- Expected runtime.
- Documentation references.

Support reusable workflow templates.

Allow users to execute workflows manually or through Python.

Prepare for future visual workflow editing, but do not let a visual editor delay the underlying workflow engine.

## 11. Workflow execution

Support:

- Sequential steps.
- Conditional execution.
- Parameterized execution.
- Repeated execution.
- Simulation dependencies.
- Result dependencies.
- Failure handling.
- Cancellation.
- Progress reporting.
- Checkpointing where appropriate.

Keep workflow execution asynchronous.

Store execution history and outputs.

Allow users to rerun a workflow with modified parameters.

Do not silently continue after a required step fails.

## 12. Reusable engineering templates

Create workflow templates for all three physics domains.

Examples:

**Acoustic**
- Create a linear array.
- Configure transducers.
- Calculate a focal field.
- Generate field slices.
- Compare active and passive focusing.
- Optimize a reflector.
- Generate an engineering report.

**Mechanical**
- Create a structural model.
- Assign materials.
- Configure excitation.
- Run modal analysis.
- Run forced-response analysis.
- Compare multiple designs.
- Generate a report.

**Electromagnetic**
- Create a dipole.
- Calculate a radiation pattern.
- Generate an antenna array.
- Steer a beam.
- Calculate sidelobes.
- Compare array configurations.
- Export radiation-pattern data.

Templates must call actual implemented capabilities.

Do not include steps that silently produce fabricated or placeholder results.

---

# PART 4 — PLUGIN AND EXTENSION ARCHITECTURE

## 13. Plugin system

Introduce a modular plugin architecture.

The objective is to allow new capabilities without changing the core application.

Potential plugin categories:

- Physics domains.
- Solvers.
- Geometry generators.
- Materials.
- Visualization layers.
- Measurement importers.
- Instrument drivers.
- Optimization algorithms.
- Analysis tools.
- Reporting tools.
- Python packages.
- Workflow templates.

Not every category needs to use the same plugin mechanism.

Choose appropriate interfaces for native C++ plugins and Python extensions.

## 14. Plugin lifecycle

Support:

- Discovery.
- Metadata inspection.
- Version validation.
- Dependency checking.
- Loading.
- Initialization.
- Capability registration.
- Error reporting.
- Unloading where safe.
- Disabling.
- Removal.

A plugin should declare:

- ID.
- Name.
- Version.
- Author.
- Description.
- Supported Resonant versions.
- Dependencies.
- Required capabilities.
- License.
- Entry point.
- Permissions where relevant.

Do not load arbitrary plugins without a clear user or administrator configuration.

Do not imply that loading a native plugin is a secure sandbox.

## 15. Plugin API stability

Define stable plugin interfaces.

Provide:

- Plugin API versioning.
- Compatibility rules.
- Example plugins.
- Plugin development documentation.
- Test harnesses.
- Error isolation.

Avoid allowing plugins to modify arbitrary global application state.

Plugins must use supported extension points.

Document binary compatibility considerations for C++ plugins.

## 16. Example plugins

Create at least three demonstration extensions:

1. A Python-based geometry generator.
2. A custom analysis or visualization extension.
3. A workflow or measurement importer extension.

Each must demonstrate the intended extension API rather than bypassing it.

---

# PART 5 — COMPREHENSIVE IN-APP HTML HELP SYSTEM

## 17. Build a complete integrated help system

This is a major product requirement.

Resonant must include a comprehensive, professional, searchable HTML help system integrated directly into the desktop application.

This must not be a minimal README viewer.

The help system will serve three purposes:

1. User documentation.
2. Technical reference documentation.
3. The foundation of the product's demonstration and training system.

Use local HTML, CSS, JavaScript, images, and other appropriate static resources.

The help system must work offline.

Do not require an internet connection.

Evaluate an appropriate Qt6 integration such as Qt WebEngine or Qt Help/QHelpEngine.

Choose based on existing dependencies, platform support, security, licensing, and the need for rich HTML content.

Do not introduce a large dependency without justification.

## 18. Help system interface

Create a dedicated Help window or workspace.

Include:

- Navigation tree.
- Search field.
- Table of contents.
- Article viewer.
- Previous and next navigation.
- Breadcrumbs.
- Related topics.
- Image viewer.
- Screenshot zoom.
- Code examples.
- Mathematical notation.
- Cross-references.
- Keyboard shortcut reference.
- Context-sensitive help.

Support opening help from:

- Main menus.
- Toolbar buttons.
- Workspace panels.
- Property editors.
- Simulation dialogs.
- Solver configuration.
- Optimization Studio.
- Experiment workflows.
- Error messages.

Provide context-sensitive help links where practical.

For example, opening the help action from an antenna-array editor should navigate to the antenna-array documentation rather than the help home page.

## 19. Help content structure

Create a comprehensive documentation hierarchy.

At minimum include:

**Getting Started**
- Introduction to Resonant.
- Installation and first launch.
- Project creation.
- Interface overview.
- Navigation and selection.
- Saving and loading projects.
- Units and coordinate systems.

**Core Concepts**
- Scene architecture.
- Geometry.
- Materials.
- Emitters and sources.
- Arrays.
- Targets.
- Wave manipulators.
- Sensors.
- Simulation regions.
- Meshes.
- Solver selection.

**Acoustic Engineering**
- Acoustic physics.
- Acoustic materials.
- Sources and transducers.
- Arrays.
- Focusing.
- Reflectors.
- Lenses.
- Waveguides.
- Acoustic FDTD.
- Field analysis.
- Acoustic optimization.

**Mechanical Engineering**
- Mechanical materials.
- Structural objects.
- Actuators.
- Boundary conditions.
- Modal analysis.
- Forced response.
- Mechanical waves.
- Resonators.
- Mechanical optimization.

**Electromagnetic Engineering**
- Maxwell equations.
- RF materials.
- Antennas.
- Antenna arrays.
- Radiation patterns.
- Beamforming.
- Impedance.
- S-parameters.
- Transmission lines.
- Waveguides.
- Near-field and far-field analysis.
- Electromagnetic solvers.

**Simulation**
- Solver architecture.
- Solver selection.
- Mesh resolution.
- Numerical accuracy.
- Convergence.
- Boundary conditions.
- Resource estimation.
- Result storage.
- Simulation comparison.

**Experiments and Hardware**
- Hardware discovery.
- Sensors.
- Actuators.
- Acquisition.
- Synchronization.
- Calibration.
- Measurements.
- Uncertainty.
- Simulation correlation.
- Safety.

**Optimization**
- Design variables.
- Objectives.
- Constraints.
- Algorithms.
- Inverse design.
- Geometry optimization.
- Topology optimization.
- Robust optimization.
- Multi-fidelity optimization.

**Python and Automation**
- Python installation and configuration.
- Python API.
- Scripting console.
- Script editor.
- Workflow automation.
- Plugin development.
- Example scripts.

**User Interface Reference**
- Main window.
- Toolbars.
- Panels.
- Scene hierarchy.
- Property editors.
- Viewport controls.
- Visualization settings.
- Analysis panels.
- Keyboard shortcuts.
- Preferences.

**Troubleshooting**
- Installation problems.
- Build and runtime problems.
- Missing dependencies.
- Solver failures.
- Mesh problems.
- Memory issues.
- Device connection failures.
- Acquisition errors.
- Calibration problems.
- Python errors.
- Plugin failures.

**Technical Reference**
- Units.
- Coordinate conventions.
- Phase conventions.
- Data formats.
- Project format.
- Solver assumptions.
- Numerical limitations.
- API reference.
- Plugin API.
- File import/export.

Do not document unsupported features as if they are implemented.

Each page must identify its implementation status where relevant:

- Available.
- Experimental.
- Planned.
- Not implemented.

## 20. Screenshot and illustration system

The help system must include real screenshots from the actual Resonant application.

Do not fill the documentation with generic stock images or generated images pretending to be application screenshots.

Use screenshots captured from the running application.

Create a repeatable screenshot-generation workflow.

Screenshots should show:

- Main application window.
- Main 3D viewport.
- Scene hierarchy.
- Property editors.
- Material editor.
- Geometry workflows.
- Acoustic simulation.
- Mechanical simulation.
- Electromagnetic simulation.
- Array configuration.
- Field visualization.
- Radiation patterns.
- Simulation controls.
- Optimization Studio.
- Experiment workspace.
- Python console.
- Help system itself.

Use meaningful example projects and consistent UI state.

Capture both overview and detail screenshots.

Where useful, annotate screenshots with numbered callouts or explanatory overlays.

Do not obscure critical application content.

Keep original unannotated screenshots available.

Use consistent image dimensions and sensible compression.

## 21. Screenshot automation

Implement a screenshot capture tool or automated process.

It should be possible to:

1. Launch Resonant.
2. Open a designated example project.
3. Select a workspace.
4. Set a known viewport state.
5. Select an object or panel.
6. Configure relevant display options.
7. Capture a screenshot.
8. Save the screenshot with a descriptive filename.
9. Associate it with a documentation page.

Use stable screenshot names, for example:

- `main-window-overview.png`
- `scene-hierarchy.png`
- `acoustic-array-setup.png`
- `acoustic-focal-field.png`
- `mechanical-modal-analysis.png`
- `antenna-radiation-pattern.png`
- `optimization-studio.png`
- `experiment-workspace.png`
- `python-console.png`

Screenshots must be generated from the real application state.

Do not claim screenshot coverage for panels or features that do not exist.

If a feature is not implemented, document its current status instead of fabricating a screenshot.

## 22. Help content quality

Documentation must explain how to use Resonant, not merely describe internal class names.

For each major feature, include:

- What it does.
- When to use it.
- How to configure it.
- Important parameters.
- Expected results.
- Common mistakes.
- Limitations.
- Related features.
- Relevant screenshots.
- Practical examples.

Include equations where they help explain physics.

Use readable mathematical notation.

Provide worked examples.

Keep terminology consistent across the application and documentation.

## 23. Help system search and navigation

Implement full-text local search.

Support:

- Search indexing.
- Search result ranking.
- Highlighted search terms.
- Topic categories.
- Related topic suggestions.
- Breadcrumb navigation.
- Cross-page links.

Search must work offline.

Provide a useful landing page with major engineering domains and common workflows.

## 24. Context-sensitive help integration

Add a consistent help mechanism to relevant application controls.

Examples:

- F1 opens context-sensitive help.
- A help icon on complex dialogs.
- Help links in solver configuration.
- Help links in material editors.
- Help links in geometry editors.
- Help links in experiment configuration.
- Help links in optimization setup.

Use stable documentation topic IDs.

Do not hardcode brittle file paths into dozens of unrelated UI components.

Create a central help-topic registry.

---

# PART 6 — DEMONSTRATION AND SHOWCASE SYSTEM

## 25. Create a dedicated demonstration environment

Resonant must have a first-class demonstration system.

This is not simply a collection of example projects.

It should allow a developer, presenter, or user to launch a polished, repeatable demonstration of the software's capabilities.

The demo system will be used to produce future demonstration videos.

It must be designed so that individual demonstrations can be executed consistently, recorded, and repeated.

Create a dedicated **Demo Studio** workspace or equivalent.

Keep demonstration infrastructure separate from the core physics engines.

Do not create fake simulation results.

All demonstrations must use real implemented capabilities, validated example datasets, or explicitly labeled imported measurements.

## 26. Demonstration profiles

Support named demonstration profiles.

A profile should include:

- Profile ID.
- Name.
- Description.
- Target audience.
- Physics domain.
- Example project.
- Initial workspace.
- Required features.
- Required data.
- Demonstration sequence.
- Expected results.
- Camera configuration.
- Visualization configuration.
- Screenshot references.
- Narration or presenter notes.
- Estimated duration.
- Completion criteria.

Support profiles for:

- Acoustic engineering.
- Mechanical engineering.
- Electromagnetic engineering.
- Cross-domain capabilities.
- Optimization.
- Experimental workflows.
- Python automation.
- General product overview.

## 27. Demonstration sequence engine

Create a structured demonstration sequence system.

A demonstration should consist of explicit steps.

Possible actions:

- Open project.
- Change workspace.
- Select scene object.
- Change camera.
- Configure visualization.
- Run simulation.
- Wait for simulation completion.
- Display result.
- Change field component.
- Move clipping plane.
- Display plot.
- Compare candidates.
- Open help topic.
- Display measurement.
- Capture screenshot.
- Pause for narration.

Each step should have:

- Stable ID.
- Description.
- Action type.
- Parameters.
- Expected state.
- Timeout.
- Failure behavior.
- Optional presenter notes.
- Optional screenshot reference.

Allow sequences to be saved and replayed.

Do not use arbitrary screen-coordinate clicking as the primary automation mechanism.

Use application-level commands and stable object IDs wherever possible.

## 28. Presenter mode

Introduce a presenter-friendly demonstration mode.

Support:

- Clean presentation layout.
- Reduced visual clutter.
- Consistent panel arrangement.
- Larger visualization area.
- Optional step instructions.
- Optional presenter notes.
- Step progression.
- Pause and resume.
- Restart.
- Progress indication.

Allow demonstrations to run without exposing development diagnostics or unrelated panels.

Provide a way to exit demonstration mode without losing the project.

## 29. Deterministic demonstration behavior

Demonstrations must be repeatable.

Use:

- Fixed example project versions.
- Known initial conditions.
- Fixed simulation parameters.
- Recorded random seeds where randomness is involved.
- Known camera positions.
- Known viewport settings.
- Stable object IDs.
- Defined execution sequences.

Do not rely on timing assumptions alone.

A demonstration must wait for actual application events, such as simulation completion or data availability.

If a simulation fails, report the failure rather than displaying a previous run's result.

## 30. Demo asset library

Create a dedicated example and demo asset structure.

For example:

`examples/`
- `acoustics/`
- `mechanics/`
- `electromagnetics/`
- `optimization/`
- `experiments/`
- `cross_domain/`

`demos/`
- `profiles/`
- `sequences/`
- `presenter_notes/`
- `assets/`
- `screenshots/`
- `reports/`

Keep demo assets versioned.

Do not store large temporary simulation caches in source control unless there is a clear reason.

Include a manifest describing each demo's dependencies and expected outputs.

## 31. Demonstration quality standards

Every demonstration must answer:

1. What engineering problem are we solving?
2. What does the user configure?
3. What does Resonant calculate?
4. What does the result mean physically?
5. How can the result be inspected?
6. What makes this capability valuable?
7. What are its assumptions and limitations?

Demonstrations should tell a coherent technical story.

Avoid demonstrations that merely click through menus.

Prioritize visual clarity and engineering substance.

## 32. Initial demonstration set

Create an initial set of polished demonstrations based on implemented features.

**Demo A — Resonant Overview**
- Open a representative project.
- Introduce the interface.
- Show scene construction.
- Show the 3D viewport.
- Show the three physics domains.
- Demonstrate project management.

**Demo B — Acoustic Focusing**
- Configure an emitter array.
- Define a target focus.
- Calculate the field.
- Visualize the focus in 3D.
- Inspect field slices.
- Compare excitation configurations.

**Demo C — Passive Acoustic Focusing**
- Create a reflector.
- Assign material properties.
- Configure a source.
- Simulate supported geometry interactions.
- Visualize the resulting field.
- Explain the physical behavior.

**Demo D — Mechanical Resonance**
- Create a structural model.
- Assign a mechanical material.
- Configure boundary conditions.
- Run modal analysis.
- Visualize mode shapes.
- Explain resonance.

**Demo E — RF Antenna Engineering**
- Create a dipole antenna.
- Configure frequency.
- Calculate its radiation pattern.
- Visualize 3D directivity.
- Compare a simple array.
- Demonstrate beam steering.

**Demo F — Inverse Design**
- Define a target field.
- Select design variables.
- Define objectives and constraints.
- Run optimization.
- Compare candidates.
- Promote a candidate.

**Demo G — Experimental Correlation**
- Load a supported measurement dataset.
- Apply calibration.
- Compare measured and simulated results.
- Display error and uncertainty.

**Demo H — Python Automation**
- Open the scripting console.
- Execute a real workflow.
- Create or configure a design.
- Run a simulation.
- Export results.

Only include steps that the current implementation can actually execute.

If a demo is partially implemented, label it accordingly and document what remains.

## 33. Video production preparation

Design the demonstration system to support future video production.

Prepare for:

- 1080p and 4K recording.
- Consistent application window dimensions.
- Clean presenter layouts.
- Stable camera movements.
- Reproducible simulation playback.
- Scripted demonstration sequences.
- Screenshot capture.
- Presenter notes.
- Optional narration scripts.

Do not attempt to build a full video editor.

Provide a clean and reliable application environment for external screen recording.

Support exporting a demonstration run manifest containing:

- Demo profile.
- Sequence version.
- Application version.
- Project version.
- Simulation configuration.
- Screenshot filenames.
- Presenter notes.
- Execution status.
- Timestamps.

Future prompts will develop individual demonstration videos, so make this system reusable and extensible.

---

# PART 7 — HELP AND DEMO CONTENT GENERATION PIPELINE

## 34. Single-source documentation architecture

Avoid maintaining separate, contradictory versions of documentation.

Establish a structured documentation source system.

Consider Markdown or another suitable authoring format with generated HTML output.

The documentation source should be version-controlled.

The build process should produce:

- In-app HTML help.
- Search index.
- Navigation structure.
- Screenshot references.
- API reference.
- Plugin documentation.
- Example workflow documentation.

Keep content modular.

Allow individual documentation topics to be referenced by stable IDs.

## 35. Documentation generation

Implement a repeatable build process.

It should:

1. Validate documentation structure.
2. Validate internal links.
3. Check screenshot references.
4. Generate HTML.
5. Generate navigation.
6. Generate search indexes.
7. Copy required static assets.
8. Package the help system for the application.

Detect broken image links.

Detect missing pages.

Detect documentation references to unavailable features where practical.

Do not silently omit failed documentation generation steps.

## 36. Documentation coverage tracking

Create a documentation coverage report.

Track:

- Implemented feature.
- Related help topic.
- Screenshot coverage.
- Example project.
- Demo profile.
- Python API documentation.
- Known limitations.

The goal is to make documentation coverage measurable.

A major user-facing feature should not be considered complete without an appropriate help topic.

Do not require screenshots for every trivial control, but ensure every major workflow has illustrated documentation.

## 37. Demo and documentation consistency

Demonstrations and help pages must describe the same actual application behavior.

Where possible, use the same example project and screenshots for both.

When a feature changes:

- Identify affected help pages.
- Identify affected screenshots.
- Identify affected demo sequences.
- Update the relevant content.
- Validate the generated help.

Do not let screenshots become stale without warning.

Include application version or documentation build metadata where appropriate.

---

# PART 8 — QUALITY, SECURITY AND TESTING

## 38. API testing

Test:

- Python module imports.
- Object lifetimes.
- API versioning.
- Project operations.
- Scene construction.
- Simulation execution.
- Result access.
- Optimization access.
- Experiment access.
- Error handling.
- Script cancellation.
- Long-running operations.

Use automated Python tests.

Test compatibility between the Python package and the C++ core.

## 39. Plugin testing

Test:

- Discovery.
- Metadata validation.
- Dependency checks.
- Loading.
- Initialization.
- Registration.
- Failure handling.
- Version incompatibility.
- Disabled plugins.

Include a deliberately failing test plugin.

The application must report the failure without corrupting project state.

## 40. Help system testing

Test:

- Help startup.
- Offline operation.
- Search.
- Navigation.
- Context-sensitive links.
- Image loading.
- Mathematical rendering.
- Internal links.
- Missing-page handling.
- Packaging.

Verify that help works from the installed application, not just the development build.

## 41. Demo system testing

Test:

- Profile loading.
- Project opening.
- Sequence execution.
- Event-based waiting.
- Simulation completion.
- Screenshot capture.
- Presenter mode.
- Restart.
- Cancellation.
- Failure recovery.

Run every initial demonstration from a clean application state.

Verify that the result is reproducible.

## 42. Security and reliability

Treat user scripts and plugins as executable code.

Do not claim that Python execution or native plugins are sandboxed unless a real security boundary exists.

Provide clear configuration and warnings for enabling extensions.

Keep plugin execution, project loading, and script execution from silently activating physical hardware.

All physical output must continue to require explicit authorization through the existing experiment safety architecture.

Do not expose credentials, private paths, or sensitive data in generated reports or screenshots.

---

# PART 9 — DOCUMENTATION AND PROJECT DELIVERABLES

## 43. Required documentation

Create or update:

**Python and automation**
- PYTHON_API.md
- PYTHON_SCRIPTING.md
- PYTHON_EXECUTION_ARCHITECTURE.md
- AUTOMATION_ARCHITECTURE.md
- WORKFLOW_SYSTEM.md

**Plugins**
- PLUGIN_ARCHITECTURE.md
- PLUGIN_DEVELOPMENT.md
- PLUGIN_API_REFERENCE.md

**Help**
- HELP_SYSTEM_ARCHITECTURE.md
- HELP_CONTENT_STRUCTURE.md
- SCREENSHOT_CAPTURE_SYSTEM.md
- DOCUMENTATION_BUILD.md
- DOCUMENTATION_COVERAGE.md

**Demonstrations**
- DEMO_SYSTEM_ARCHITECTURE.md
- DEMO_PROFILE_FORMAT.md
- DEMO_SEQUENCE_FORMAT.md
- PRESENTER_MODE.md
- VIDEO_PRODUCTION_GUIDE.md
- DEMO_CATALOG.md

**Analyst reference**
- reference/ANALYST_REFERENCE_AUDIT.md

**Project status**
- IMPLEMENTATION_STATUS.md
- DECISIONS.md

In addition, create the actual user-facing HTML help content.

The Markdown architecture documents are not substitutes for the in-app HTML help.

---

# PART 10 — IMPLEMENTATION SEQUENCE

## 44. Development phases

**Phase A — Analyst reference audit**
- Inspect `/home/dev/RF_IQ_Analyst`.
- Document useful patterns.
- Identify relevant demo-system ideas.
- Do not modify Analyst.

**Phase B — Python API**
- Define public interfaces.
- Implement bindings.
- Package Python modules.
- Add API tests.

**Phase C — Scripting environment**
- Interactive console.
- Script editor.
- Execution management.
- Example scripts.

**Phase D — Automation**
- Workflow model.
- Workflow execution.
- Reusable templates.
- History and results.

**Phase E — Plugin architecture**
- Plugin interfaces.
- Discovery.
- Lifecycle.
- Example plugins.

**Phase F — Help system foundation**
- Select HTML integration.
- Build help window.
- Navigation.
- Search.
- Topic registry.

**Phase G — Help content**
- Populate documentation.
- Add examples.
- Add mathematical explanations.
- Add troubleshooting.
- Generate API references.

**Phase H — Screenshot system**
- Automated capture.
- Naming conventions.
- Screenshot catalog.
- Documentation integration.

**Phase I — Demo Studio**
- Demo profiles.
- Sequence engine.
- Presenter mode.
- Demo asset library.

**Phase J — Initial demonstrations**
- Overview.
- Acoustic.
- Mechanical.
- Electromagnetic.
- Optimization.
- Experimental.
- Python.

**Phase K — Integration and verification**
- Offline testing.
- Packaging.
- Documentation validation.
- Demo reproducibility.
- API and plugin regression tests.

Do not attempt to implement everything simultaneously.

Prioritize the foundational APIs and help architecture, then build demonstration functionality around real capabilities.

---

# PART 11 — ACCEPTANCE CRITERIA

## 45. Required completion criteria

This milestone is complete only when:

1. `/home/dev/RF_IQ_Analyst` has been inspected if accessible.
2. The Analyst reference audit exists.
3. Analyst has not been modified.
4. A documented Python API exists.
5. Python can construct and configure supported Resonant objects.
6. Python can run supported simulations.
7. Python can access simulation results.
8. A scripting console exists.
9. A script editor exists.
10. Long-running scripts do not block the GUI.
11. A reusable automation framework exists.
12. Workflow templates can be executed.
13. A plugin architecture exists.
14. Example plugins demonstrate supported extension points.
15. An offline HTML help system is integrated into the application.
16. Help search works.
17. Context-sensitive help works for major workflows.
18. Major implemented features have practical documentation.
19. Actual application screenshots appear in the help.
20. Screenshot capture is reproducible.
21. Documentation builds automatically.
22. Broken internal links and missing screenshot references are detected.
23. A Demo Studio or equivalent demonstration environment exists.
24. Demonstration profiles can be loaded.
25. Demonstration sequences can execute reproducibly.
26. Presenter mode works.
27. At least the initial set of implemented demonstrations is available.
28. Demonstrations use actual implemented functionality.
29. Demonstration runs can be recorded in a manifest.
30. Existing physics, simulation, measurement, and optimization functionality remains intact.
31. Automated tests pass.
32. The application can be packaged with its local help content and demonstration assets.

---

# PART 12 — CRITICAL DEVELOPMENT RULES

- Do not modify RF_IQ_Analyst.
- Inspect it as a reference, not as a codebase to merge into Resonant.
- Keep Resonant's architecture independent.
- Keep Python optional for core operation.
- Do not duplicate physics implementations in Python.
- Do not block the GUI with scripts or automation.
- Treat plugins and scripts as executable code.
- Do not claim security isolation without a real security boundary.
- Do not silently activate hardware.
- Keep help available offline.
- Use real screenshots from the actual application.
- Never fabricate screenshots or claim unimplemented functionality exists.
- Maintain one authoritative documentation source.
- Keep demonstration sequences deterministic and reproducible.
- Wait for actual application events rather than relying on arbitrary delays.
- Use real simulation results and clearly labeled datasets.
- Keep demo assets separate from temporary build and simulation output.
- Preserve existing project data and functionality.
- Record architectural decisions and limitations.

**The goal is to make Resonant a complete, extensible, self-documenting engineering platform. Users should be able to extend it with Python, automate complex workflows, develop plugins, learn through comprehensive in-app help, and experience its capabilities through polished, reproducible demonstrations.**

**The demonstration system and HTML help are foundational product capabilities, not optional developer conveniences. They must be designed so that future development can use them to produce professional, technically credible demonstration videos without rebuilding the presentation infrastructure each time.**

## Classification

- Type: FEATURE (final; validated by `scripts/request_classification.py`, REQ-DEVOPS-045)
- Priority: NORMAL
- Source: chat
- classification:
  - proposed_type: FEATURE (override)
  - final_type: FEATURE
  - heuristic_type: FEATURE (confidence high)
  - validation: PASS
  - classifier_reason: request statement is framed as feature: Add, Create
  - evidence: 'milestone' [body]; 'extend' [body]; 'milestone' [body]; 'evolve' [body]; 'capabilities' [body]

## Inferred Intent (agent interpretation — not user statement)

Intent: implement requested capability per classification; derive requirement, implement, test, verify.
Conversational context: Tenth ticket, submitted by the user in chat while REQ-CHAT-20261126-002 was being processed, with the instruction to process it in order (after REQ-CHAT-20261126-009). Python API, automation, plugins, in-app HTML help and demonstration system on top of all earlier milestones. It names an external reference project, /home/dev/RF_IQ_Analyst, to be inspected read-only and never modified. Standing operator instructions for all tickets: keep qmake support in addition to CMake; Qt user interfaces use 

## Observed / Unknown

- Observed: see original statement above.
- Unknown: exact scope — agent to derive
- Follow-ups will be appended under ## Follow-up Context

## Follow-up Context

Tenth ticket, submitted by the user in chat while REQ-CHAT-20261126-002 was being processed, with the instruction to process it in order (after REQ-CHAT-20261126-009). Python API, automation, plugins, in-app HTML help and demonstration system on top of all earlier milestones. It names an external reference project, /home/dev/RF_IQ_Analyst, to be inspected read-only and never modified. Standing operator instructions for all tickets: keep qmake support in addition to CMake; Qt user interfaces use .ui files.

## Processing Notes

- This is the canonical persistent request artifact. Conversation history is contextual evidence; this file is authoritative.
- After intake, workflow is `requests/inbox -> processing -> intent -> requirement -> work -> implementation -> test -> verification -> documentation -> complete` per `docs/process/state-model.md` and `docs/process/autonomous-operating-model.md`.
- Next: agent claims via `scripts/claim.py`, then `scripts/preflight.py`, then requirement/work linkage.

## Traceability

- Intent: pending (created from this request)
- Requirement: pending
- Work: WORK-000013
- Evidence: pending
